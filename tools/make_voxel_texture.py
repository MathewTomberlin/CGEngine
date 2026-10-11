"""
Writes resources/voxel_noise.png: a 64 by 64 light grey noise. The voxel sample tints it per block type (see
VoxelGen.COLOURS), so the noise only adds grain. Pure Python (zlib and struct), so no image library is needed.

Usage (from the repo root): python -I tools/make_voxel_texture.py
"""
import os
import random
import struct
import zlib

SIZE = 64
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources", "voxel_noise.png")


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def main():
    rng = random.Random(7)   # fixed, so the file is the same every time it is written
    rows = bytearray()
    for _ in range(SIZE):
        rows.append(0)       # filter type 0 (none) for each scanline
        for _ in range(SIZE):
            grey = rng.randint(200, 255)
            rows += bytes((grey, grey, grey))
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)   # 8-bit RGB
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    with open(OUT, "wb") as f:
        f.write(png)
    print("wrote", os.path.normpath(OUT), len(png), "bytes")


if __name__ == "__main__":
    main()
