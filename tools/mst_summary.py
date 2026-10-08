#!/usr/bin/env python3
"""tools/mst_summary.py TRACE [PROCESS]: GPU time of a Metal System Trace (xctrace) by channel
(vertex, fragment, compute, blit) and by encoder, for the game (bb-probe by default).

Record one while the game runs (Xcode needed; xcode-select may point at the command line tools):
  DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcrun xctrace record \
      --template 'Metal System Trace' --attach $(pgrep -x bb-probe) --time-limit 6s --output t.trace
"""
import collections
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

DEV = os.environ.get('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer')


def export(trace, schema):
    xpath = f'/trace-toc/run[@number="1"]/data/table[@schema="{schema}"]'
    out = subprocess.run(['xcrun', 'xctrace', 'export', '--input', trace, '--xpath', xpath],
                         env={**os.environ, 'DEVELOPER_DIR': DEV}, check=True, capture_output=True).stdout
    return ET.fromstring(out)


def rows(root):
    """Rows as lists of (text, fmt) per column, with id/ref back-references resolved."""
    seen = {}
    node = root.find('node')
    cols = [c.findtext('mnemonic') for c in node.find('schema').findall('col')]
    for row in node.findall('row'):
        values = {}
        for i, el in enumerate(row):
            mnemonic = cols[i] if i < len(cols) else f'extra{i - len(cols)}'
            if 'ref' in el.attrib:
                el = seen[el.attrib['ref']]
            for sub in el.iter():
                if 'id' in sub.attrib:
                    seen[sub.attrib['id']] = sub
            values[mnemonic] = (el.text or '', el.attrib.get('fmt', ''))
        yield values


def print_labels(trace, process, by_encoder):
    """GPU time per debug label (BB_PASS_LABELS=1 names each render pass), joined by encoder."""
    label_of = {}
    for r in rows(export(trace, 'metal-application-intervals')):
        if process not in r['process'][1] or 'encoder-id' not in r:
            continue
        text = r['event-label'][1]
        if ' - :' in text:
            label_of[r['encoder-id'][0]] = text.split(' - :', 1)[1].rsplit(' (', 2)[0]
    if not label_of:
        return
    total = collections.defaultdict(collections.Counter)
    count = collections.Counter()
    for encoder, channels in by_encoder.items():
        label = label_of.get(encoder, '(no label)')
        total[label].update(channels)
        count[label] += 1
    print('GPU time per label (Fragment, Vertex, Compute), most expensive first:')
    ranked = sorted(total.items(), key=lambda kv: -sum(kv[1].values()))
    for label, ch in ranked[:int(os.environ.get('TOP', 30))]:
        print(f"  {ch['Fragment'] / 1e6:8.1f} {ch['Vertex'] / 1e6:7.1f} {ch['Compute'] / 1e6:7.1f} ms"
              f" {count[label]:6} enc  {label[:140]}")


def main():
    trace = sys.argv[1]
    process = sys.argv[2] if len(sys.argv) > 2 else 'bb-probe'
    by_channel = collections.Counter()
    by_label = collections.Counter()
    count_label = collections.Counter()
    t0 = t1 = None
    cmdbufs = set()
    by_encoder = collections.defaultdict(collections.Counter)
    for r in rows(export(trace, 'metal-gpu-intervals')):
        if process not in r['process'][1]:
            continue
        start, dur = int(r['start'][0]), int(r['duration'][0])
        t0 = start if t0 is None else min(t0, start)
        t1 = start + dur if t1 is None else max(t1, start + dur)
        channel = r['channel-name'][1]
        by_channel[channel] += dur
        label = r['event-label'][1].strip()
        by_label[(channel, label)] += dur
        count_label[(channel, label)] += 1
        cmdbufs.add(r['cmdbuffer-id'][0])
        by_encoder[r['encoder-id'][0]][channel] += dur
    if t0 is None:
        sys.exit(f'no GPU intervals of {process}')
    if os.environ.get('LABELS', '1') == '1':
        print_labels(trace, process, by_encoder)
    span = (t1 - t0) / 1e6
    print(f'{process}: {span:.0f} ms traced, {len(cmdbufs)} command buffers')
    for channel, ns in by_channel.most_common():
        print(f'  {channel:12} {ns / 1e6:9.1f} ms  ({100 * ns / 1e6 / span:.0f}% of the span)')
    print('top encoders/intervals (summed):')
    for (channel, label), ns in by_label.most_common(int(os.environ.get('TOP', 30))):
        print(f'  {ns / 1e6:8.1f} ms {count_label[(channel, label)]:6}x  {channel:10} {label[:110]}')


if __name__ == '__main__':
    main()
