"""
Checks the block textures without the engine: resources/voxel_tiles.png is a strip of TILE_SIZE squares, one per name in
VoxelGen.TILE_NAMES (in the same order as tools/make_voxel_tiles.py), and every textured block points at a layer that exists.
Usage (from the repo root): python -I tools/test_voxel_tiles.py
Exits 0 on success, 1 on any failed check.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "resources", "scripts"))

import VoxelGen  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


def png_size(path):
    """The width and height in a PNG's IHDR chunk (the first chunk after the 8-byte signature)."""
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", head[16:24])


tiles_path = os.path.join(ROOT, "resources", "voxel_tiles.png")
size = png_size(tiles_path) if os.path.exists(tiles_path) else None
layers = len(VoxelGen.TILE_NAMES)
check("the tile strip exists and is a PNG", size is not None)
if size is not None:
    check("the strip is one %d-pixel tile wide" % VoxelGen.TILE_SIZE, size[0] == VoxelGen.TILE_SIZE)
    check("the strip has one tile per name (%d)" % layers, size[1] == layers * VoxelGen.TILE_SIZE)

generator = os.path.join(HERE, "make_voxel_tiles.py")
with open(generator, encoding="utf-8") as f:
    names_in_generator = re.findall(r'"([a-z_]+)"', re.search(r"TILE_NAMES = \[(.*?)\]", f.read(), re.S).group(1))
check("the generator writes the same tiles in the same order as VoxelGen.TILE_NAMES",
      names_in_generator == VoxelGen.TILE_NAMES)

check("every textured block names three layers that exist", all(
    len(layers3) == 3 and all(0 <= layer < layers for layer in layers3) for layers3 in VoxelGen.TILES.values()))
check("every block with a tile is a block the world defines (has a colour)", set(VoxelGen.TILES) <= set(VoxelGen.COLOURS))
check("the water family shares the water tile", all(VoxelGen.TILES[b] == VoxelGen.TILES[VoxelGen.WATER]
                                                    for b in VoxelGen.WATER_BLOCKS))
check("a wood block's top is its rings and its sides are bark", VoxelGen.TILE_NAMES[VoxelGen.TILES[VoxelGen.WOOD][0]] == "wood_rings"
      and VoxelGen.TILE_NAMES[VoxelGen.TILES[VoxelGen.WOOD][1]] == "bark")

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel tile checks passed.")
