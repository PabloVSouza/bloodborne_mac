#!/usr/bin/env python3
"""tools/mst_sequence.py TRACE [START_MS] [SPAN_MS]: the Metal encoders of a Metal System Trace in GPU
order (BB_PASS_LABELS=1 runs name the render passes): start, GPU time, kind or pass. By default the
62 ms from the middle of the trace (about a frame at 16 FPS). Shows what splits a render pass into
several encoders (each reloads and stores its attachments on Apple's tile-based GPUs)."""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mst_summary as m


def short(label):
    label = re.sub(r' \(bb:[^)]*\)', '', label)
    label = re.sub(r'R8G8B8A8Unorm R8G8B8A8Unorm R8G8B8A8Srgb R8G8B8A8Unorm B10G11R11UfloatPack32 R16G16B16A16Sfloat',
                   'GBUFFER6', label)
    label = label.replace('B10G11R11UfloatPack32', 'R11G11B10').replace('R16G16B16A16Sfloat', 'RGBA16F')
    label = label.replace('R8G8B8A8Unorm', 'RGBA8').replace('D32SfloatS8Uint', 'D32S8')
    return label


def main():
    trace = sys.argv[1]
    labels, kinds = {}, {}
    for r in m.rows(m.export(trace, 'metal-application-intervals')):
        if 'bb-probe' in r['process'][1] and 'encoder-id' in r and ' - :' in r['event-label'][1]:
            labels[r['encoder-id'][0]] = r['event-label'][1].split(' - :', 1)[1]
    for r in m.rows(m.export(trace, 'metal-application-encoders-list')):
        if 'bb-probe' in r['process'][1]:
            kinds[r['encoder-id'][0]] = r['encoder-label'][1]
    spans = {}
    gpu = collections.Counter()
    for r in m.rows(m.export(trace, 'metal-gpu-intervals')):
        if 'bb-probe' not in r['process'][1]:
            continue
        s, d, e = int(r['start'][0]), int(r['duration'][0]), r['encoder-id'][0]
        a, b = spans.get(e, (s, s + d))
        spans[e] = (min(a, s), max(b, s + d))
        gpu[e] += d
    if not spans:
        print('no encoders')
        return
    t0 = min(a for a, _ in spans.values())
    t1 = max(b for _, b in spans.values())
    start = float(sys.argv[2]) * 1e6 + t0 if len(sys.argv) > 2 else (t0 + t1) / 2
    end = start + (float(sys.argv[3]) if len(sys.argv) > 3 else 62) * 1e6
    previous_end = None
    for e, (a, b) in sorted(spans.items(), key=lambda kv: kv[1][0]):
        if a < start or a >= end:
            continue
        gap = (a - previous_end) / 1e6 if previous_end is not None else 0
        previous_end = max(previous_end or 0, b)
        name = short(labels.get(e) or f'[{kinds.get(e, "?")}]')
        print(f'{(a - t0) / 1e6:9.3f} ms  {gpu[e] / 1e6:6.3f} ms  gap {gap:6.3f}  {name[:150]}')


main()
