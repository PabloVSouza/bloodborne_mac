#!/usr/bin/env python3
"""tools/mst_shaders.py TRACE FRAMES: GPU time per frame by shader pair, from a BB_PASS_LABELS=1
BB_DRAW_LABELS=1 run (one labeled pass per draw). Fragment and vertex time, draws per frame,
and the pass the draw renders into."""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mst_summary as m

trace, frames = sys.argv[1], float(sys.argv[2])
labels = {}
for r in m.rows(m.export(trace, 'metal-application-intervals')):
    if 'bb-probe' in r['process'][1] and 'encoder-id' in r and ' - :' in r['event-label'][1]:
        labels[r['encoder-id'][0]] = r['event-label'][1].split(' - :', 1)[1].split(' (bb:')[0]
total = collections.defaultdict(collections.Counter)
draws = collections.Counter()
target = {}
for r in m.rows(m.export(trace, 'metal-gpu-intervals')):
    if 'bb-probe' not in r['process'][1]:
        continue
    label = labels.get(r['encoder-id'][0])
    if not label:
        continue
    found = re.search(r'vs ([0-9a-f]+) ps ([0-9a-f]+)', label)
    if not found:
        continue
    key = (found.group(1)[-8:], found.group(2)[-8:])
    total[key][r['channel-name'][1]] += int(r['duration'][0])
    draws[(key, r['encoder-id'][0])] = 1
    target[key] = re.sub(r' vs .*', '', label).replace('R8G8B8A8Unorm', 'RGBA8')[:70]
per = collections.Counter()
for (key, _), _ in draws.items():
    per[key] += 1
grand = sum(sum(c.values()) for c in total.values())
print(f'{grand / 1e6 / frames:.1f} ms/frame in labeled draws')
for key, c in sorted(total.items(), key=lambda kv: -sum(kv[1].values()))[:int(os.environ.get('TOP', 30))]:
    t = sum(c.values()) / 1e6 / frames
    print(f"{t:6.2f} ms (frag {c['Fragment'] / 1e6 / frames:5.2f} vtx {c['Vertex'] / 1e6 / frames:5.2f})"
          f" {per[key] / frames:6.1f} draws  vs {key[0]} ps {key[1]}  {target[key]}")
