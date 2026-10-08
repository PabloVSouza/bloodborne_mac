#!/usr/bin/env python3
"""tools/raw2png.py FILE.raw...: 8-bit rgba/bgra dumps (fNNN_<name>_<w>x<h>_<rgba|bgra>.raw, from
BB_PRESENT_DUMP_TRIGGER) to PNG next to them, without numpy/Pillow (tools/dump_view.py does
the other formats). Halves the size with SCALE=2 (default)."""
import os
import re
import struct
import sys
import zlib


def png(path, width, height, rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    raw = b''.join(b'\0' + r for r in rows)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


for name in sys.argv[1:]:
    m = re.search(r'_(\d+)x(\d+)_(rgba|bgra)\.raw$', name)
    if not m:
        print('skipped', name)
        continue
    w, h, fmt = int(m[1]), int(m[2]), m[3]
    data = open(name, 'rb').read()
    scale = int(os.environ.get('SCALE', '2'))
    rows = []
    for y in range(0, h, scale):
        line = data[y * w * 4:(y + 1) * w * 4]
        px = bytearray()
        for x in range(0, w, scale):
            b0, b1, b2 = line[x * 4], line[x * 4 + 1], line[x * 4 + 2]
            px += bytes((b2, b1, b0)) if fmt == 'bgra' else bytes((b0, b1, b2))
        rows.append(bytes(px))
    out = name[:-4] + '.png'
    png(out, (w + scale - 1) // scale, len(rows), rows)
    print(out)
