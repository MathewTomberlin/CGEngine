"""
Checks the block table (roadmap 29.3): resources/voxel_blocks.json is the source of the per-block rules in resources/scripts/VoxelGen.py
(colours, tiles, hits, falls, emits, own and cube meshes, materials, joins, transparency and the splat channels). The checks read the
module as the game does, and load broken copies of the file to see that each kind of mistake is refused with a clear error. No
window is needed. Usage (from the repo root): python -I tools/test_voxel_blocks.py   Exits 0 on success, 1 on any failed check.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(HERE, "..", "resources", "scripts")
TABLE = os.path.join(HERE, "..", "resources", "voxel_blocks.json")
sys.path.insert(0, SCRIPTS)

import VoxelGen  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


def load_with(path):
    """Imports VoxelGen in a fresh process with VOXEL_BLOCKS set to path; returns its error text, or '' when it loads."""
    code = "import sys; sys.path.insert(0, %r); import VoxelGen" % SCRIPTS
    env = dict(os.environ, VOXEL_BLOCKS=path)
    result = subprocess.run([sys.executable, "-I", "-c", code], env=env, capture_output=True, text=True)
    return "" if result.returncode == 0 else result.stderr.strip().splitlines()[-1]


with open(TABLE, encoding="utf-8") as f:
    entries = json.load(f)["blocks"]

# The table is the source: every block id the game has a colour for is in the table, and its name is the display name.
ids_in_table = set()
for entry in entries:
    ids_in_table.update(range(entry["ids"][0], entry["ids"][1] + 1) if "ids" in entry else [entry["id"]])
check("every block with a colour is in the table", set(VoxelGen.COLOURS) <= ids_in_table)
check("the table's blocks are the names the game shows", all(VoxelGen.NAMES[b] for b in ids_in_table))
check("each splat channel is its block in SPLAT", all(VoxelGen.SPLAT[e["splat"]] == e["id"] for e in entries if "splat" in e))
check("the splat channels run from 0 to 8 (nine blocks)", len(VoxelGen.SPLAT) == 9)
check("every tile name is a texture layer", all(0 <= t < len(VoxelGen.TILE_NAMES) for tiles in VoxelGen.TILES.values() for t in tiles))
check("a flowing block takes its source's colour and tiles",
      VoxelGen.COLOURS[9] == VoxelGen.COLOURS[VoxelGen.WATER] and VoxelGen.TILES[9] == VoxelGen.TILES[VoxelGen.WATER])
check("lava's flows glow as the lava does", all(VoxelGen.EMITS[b] == VoxelGen.EMITS[VoxelGen.LAVA] for b in range(23, 30)))
check("the cube blocks are in their own meshes", all(b in VoxelGen.OWN for b in VoxelGen.CUBES))
check("every liquid is in its own mesh", all(b in VoxelGen.OWN for b in VoxelGen.LIQUIDS))
check("the wood joins the leaves above it", VoxelGen.JOINS.get(VoxelGen.WOOD) == VoxelGen.LEAVES)
check("every ground block has its own footstep", set(VoxelGen.GROUND) <= set(VoxelGen.STEPS))
check("every footstep names a sound file that exists", all(
    os.path.exists(os.path.join(HERE, "..", "resources", "sounds", name + ".wav")) for name in VoxelGen.STEPS.values()))

# Each kind of mistake is refused, with the file's name in the error.
with tempfile.TemporaryDirectory() as tmp:
    def broken(name, change):
        data = json.loads(json.dumps({"blocks": entries}))
        change(data["blocks"])
        path = os.path.join(tmp, name + ".json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f)
        return load_with(path)

    error = broken("wrong_key", lambda b: b[1].update({"key": "STONE"}))   # entry 1 is grass, so its id is not STONE's
    check("an id whose key names another constant is refused", "STONE" in error and "not" in error)

    error = broken("duplicate", lambda b: b.append({"id": 3, "name": "stone again"}))
    check("a block listed twice is refused", "listed twice" in error)

    error = broken("liquid", lambda b: b[5].update({"liquid": False}))   # water is a liquid by the id ranges
    check("a liquid the id ranges do not give is refused", "liquid blocks" in error)

    error = broken("splat", lambda b: b[2].update({"splat": 40}))
    check("splat channels that are not 0 to n-1 are refused", "splat channels" in error)

check("the shipped table loads", load_with(TABLE) == "")

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel block table checks passed.")
