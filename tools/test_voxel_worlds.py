"""
Checks the named worlds of the voxel sample (VOXEL_WORLD, resources/scripts/VoxelConfig.py): a named world keeps its edits and
inventory in cg_control/worlds/<name>, the default world keeps them in cg_control as before, and a name with a path or a space
is refused, so a world cannot write outside its folder. The module is read in a child process with its own environment, as the
game reads it. The game's save and load (save_world, load_world) use these paths and the clock; tools/test_voxels.ps1 runs them.
Usage (from the repo root): python -I tools/test_voxel_worlds.py   Exits 0 on success, 1 on any failed check.
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(HERE, "..", "resources", "scripts")

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


def paths(world):
    """The save paths VoxelConfig makes with VOXEL_WORLD set to world (None means not set), as the game sees them."""
    env = dict(os.environ)
    env.pop("VOXEL_WORLD", None)
    if world is not None:
        env["VOXEL_WORLD"] = world
    code = ("import json, sys; sys.path.insert(0, %r); import VoxelConfig as c; "
            "print(json.dumps([c.WORLD_FOLDER, c.EDITS_FILE, c.INVENTORY_FILE]))" % SCRIPTS)
    out = subprocess.run([sys.executable, "-I", "-c", code], cwd=HERE, env=env, capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError(out.stderr)
    return json.loads(out.stdout.strip().splitlines()[-1])


default = paths(None)
check("with no world named, the files stay in cg_control",
      [p.replace("\\", "/") for p in default] == ["cg_control", "cg_control/voxel_edits.bin", "cg_control/voxel_inventory.json"])
named = paths("trial-1")
check("a named world keeps its files in cg_control/worlds/<name>",
      named[0].replace("\\", "/") == "cg_control/worlds/trial-1" and named[1].replace("\\", "/") == "cg_control/worlds/trial-1/voxel_edits.bin")
check("a named world has its own inventory file", named[2].replace("\\", "/") == "cg_control/worlds/trial-1/voxel_inventory.json")
for bad in ["../escape", "a b", "x/y"]:
    check("the world name %r is refused, so the default world is used" % bad, paths(bad) == default)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel world checks passed.")
