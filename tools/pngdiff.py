#!/usr/bin/env python3
"""tools/pngdiff.py A.png B.png: mean absolute difference (0-255) and the share of values that
differ by more than 24, for frame grabs of the same scene (tools/grab.sh). No dependencies."""
import struct
import sys
import zlib


def load(path):
    data = open(path, 'rb').read()
    i, idat = 8, b''
    while i < len(data):
        n = struct.unpack('>I', data[i:i + 4])[0]
        kind, chunk = data[i + 4:i + 8], data[i + 8:i + 8 + n]
        if kind == b'IHDR':
            w, h, _, ct = struct.unpack('>IIBB', chunk[:10])
        elif kind == b'IDAT':
            idat += chunk
        i += 12 + n
    raw, bpp = zlib.decompress(idat), (3 if ct == 2 else 4)
    stride, out, prev, p = w * bpp, bytearray(), bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[p], bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b, c = prev[x], (prev[x - bpp] if x >= bpp else 0)
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                q = a + b - c
                pa, pb, pc = abs(q - a), abs(q - b), abs(q - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        prev = line
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
d = [abs(x - y) for x, y in zip(a, b)]
print(f'mean {sum(d) / len(d):.2f}, over 24: {100 * sum(1 for v in d if v > 24) / len(d):.2f}%')
