#!/usr/bin/env python3
"""tools/mst_passes.py TRACE [FPS]: GPU time per frame by render pass kind (BB_PASS_LABELS=1 runs),
compute/blit encoders by type, and GPU idle inside and between command buffers."""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mst_summary as m


def main():
    trace = sys.argv[1]
    labels, kinds = {}, {}
    for r in m.rows(m.export(trace, 'metal-application-intervals')):
        if 'bb-probe' in r['process'][1] and 'encoder-id' in r and ' - :' in r['event-label'][1]:
            labels[r['encoder-id'][0]] = r['event-label'][1].split(' - :', 1)[1].split(' (bb:')[0]
    for r in m.rows(m.export(trace, 'metal-application-encoders-list')):
        if 'bb-probe' in r['process'][1]:
            kinds[r['encoder-id'][0]] = re.sub(r'\d+', 'N', r['encoder-label'][1])
    total = collections.defaultdict(collections.Counter)
    encoders = collections.defaultdict(set)
    per_cb = collections.defaultdict(list)
    t0, t1 = None, 0
    for r in m.rows(m.export(trace, 'metal-gpu-intervals')):
        if 'bb-probe' not in r['process'][1]:
            continue
        s, d = int(r['start'][0]), int(r['duration'][0])
        t0 = s if t0 is None else min(t0, s)
        t1 = max(t1, s + d)
        e = r['encoder-id'][0]
        key = re.sub(r' vs [0-9a-f]+ ps [0-9a-f]+', '', labels.get(e) or f'[{kinds.get(e, "?")}]')
        total[key][r['channel-name'][1]] += d
        encoders[key].add(e)
        per_cb[r['cmdbuffer-id'][0]].append((s, s + d))
    span = (t1 - t0) / 1e9
    fps = float(sys.argv[2]) if len(sys.argv) > 2 else None
    frames = span * fps if fps else 1
    unit = 'ms/frame' if fps else 'ms'
    print(f'{span:.1f} s traced' + (f', ~{frames:.0f} frames' if fps else ''))
    ranked = sorted(total.items(), key=lambda kv: -sum(kv[1].values()))
    for key, ch in ranked[:int(os.environ.get('TOP', 25))]:
        print(f"{sum(ch.values()) / 1e6 / frames:7.2f} {unit} (frag {ch['Fragment'] / 1e6 / frames:5.2f}"
              f" vtx {ch['Vertex'] / 1e6 / frames:5.2f} cs {ch['Compute'] / 1e6 / frames:5.2f})"
              f" {len(encoders[key]) / frames:6.1f} enc  {key[:100]}")
    intervals = sorted(x for v in per_cb.values() for x in v)
    busy, cur = 0, None
    for s, e in intervals:
        if cur is None or s > cur[1]:
            busy += cur[1] - cur[0] if cur else 0
            cur = [s, e]
        else:
            cur[1] = max(cur[1], e)
    busy += cur[1] - cur[0]
    inner = 0
    for v in per_cb.values():
        v.sort()
        u, c = 0, None
        for s, e in v:
            if c is None or s > c[1]:
                u += c[1] - c[0] if c else 0
                c = [s, e]
            else:
                c[1] = max(c[1], e)
        u += c[1] - c[0]
        inner += max(e for _, e in v) - v[0][0] - u
    whole = (t1 - t0)
    print(f'GPU busy {busy / 1e6 / frames:.1f} {unit}; idle inside command buffers {inner / 1e6 / frames:.1f},'
          f' between them {(whole - busy - inner) / 1e6 / frames:.1f}')


if __name__ == '__main__':
    main()
