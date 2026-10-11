"""
Checks the voxel terrain generator (resources/scripts/VoxelGen.py) without the engine.
Usage (from the repo root): python -I tools/test_voxel_gen.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelGen as g  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


cells = [(x, z) for x in range(-24, 24) for z in range(-24, 24)]
heights = [g.column_height(x, z, 7) for x, z in cells]

check("the same seed gives the same heights", heights == [g.column_height(x, z, 7) for x, z in cells])
check("another seed gives other heights", heights != [g.column_height(x, z, 8) for x, z in cells])
check("heights stay between 8 and 52", all(8 <= h <= 52 for h in heights))
check("the land has hills: heights vary", max(heights) - min(heights) >= 10)

noise = [g.value_noise(x * 0.37, z * 0.37, 3) for x in range(-40, 40) for z in range(-40, 40)]
check("value noise stays in [0, 1]", all(0.0 <= n <= 1.0 for n in noise))

solid_ok = True
water_ok = True
for x, z in cells:
    h = g.column_height(x, z, 7)
    runs = g.column_runs(x, z, 7)
    solid = [r for r in runs if r[2] != g.WATER]
    water = [r for r in runs if r[2] == g.WATER]
    # Solid runs stack from the bottom of the world up to the surface, with no gaps.
    edge = 0
    for y_from, y_to, block in solid:
        if y_from != edge or y_to <= y_from or block == g.AIR:
            solid_ok = False
        edge = y_to
    if edge != h:
        solid_ok = False
    # Water only fills columns whose surface is below sea level, from the surface up to sea level.
    for y_from, y_to, block in water:
        if not (h < g.SEA_LEVEL and y_from == h and y_to == g.SEA_LEVEL):
            water_ok = False
check("every column is solid from the bottom up to its surface", solid_ok)
check("water fills only the low columns, up to sea level", water_ok)
check("no run goes above the world height", all(y_to <= g.HEIGHT for x, z in cells for _, y_to, _ in g.column_runs(x, z, 7)))
check("every block the terrain uses has a colour", all(b in g.COLOURS for x, z in cells for _, _, b in g.column_runs(x, z, 7) if b != g.AIR))

columns = g.chunk_columns(2, -3, 7)
check("a chunk has one column per block of its floor", len(columns) == g.CHUNK * g.CHUNK)
check("chunk columns carry world coordinates",
      all(2 * g.CHUNK <= wx < 3 * g.CHUNK and -3 * g.CHUNK <= wz < -2 * g.CHUNK for wx, wz, _ in columns))

png = os.path.join(HERE, "..", "resources", "voxel_noise.png")
with open(png, "rb") as f:
    check("the voxel texture is a PNG", f.read(8) == b"\x89PNG\r\n\x1a\n")

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel generator checks passed.")
