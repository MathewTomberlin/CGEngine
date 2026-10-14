"""
Checks the reaction rule files (resources/scripts/VoxelRules.py) without the engine: the example file parses, names, ids and
families work, touches can be a list, a timed reaction keeps its seconds, the built-in reactions parse, bad entries are reported
with their index, and a bad file leaves the world's reactions as they were.
Usage (from the repo root): python -I tools/test_voxel_rules.py
Exits 0 on success, 1 on any failed check.
"""
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "resources", "scripts"))

import VoxelGen  # noqa: E402
import VoxelRules  # noqa: E402

failures = []


def check(name, condition):
    print(("PASS  " if condition else "FAIL  ") + name)
    if not condition:
        failures.append(name)


def rejects(data, *words):
    try:
        VoxelRules.parse(data)
    except ValueError as error:
        return all(word in str(error) for word in words)
    return False


class FakeWorld:
    """Records the calls a rule file makes, so the test sees what would reach the engine."""

    def __init__(self):
        self.calls = []

    def clear_rules(self):
        self.calls.append(("clear",))

    def add_reaction(self, block, kinds, becomes, chance, seconds):
        self.calls.append(("add", block, kinds, becomes, chance, seconds))


example = os.path.join(ROOT, "resources", "voxel_rules_example.json")
with open(example, encoding="utf-8") as f:
    check("the example file parses to its one rule (touching the water family)",
          VoxelRules.parse(json.load(f)) == [(1, sorted(VoxelGen.WATER_BLOCKS), 2, 0.5, 0.0)])

check("names and ids are the same rule",
      VoxelRules.parse({"rules": [{"block": "grass", "touches": "sand", "becomes": "dirt"}]})
      == VoxelRules.parse({"rules": [{"block": 1, "touches": 4, "becomes": 2}]}))
check("chance is 1 when it is left out", VoxelRules.parse({"rules": [{"block": 1, "touches": 5, "becomes": 2}]})[0][3] == 1.0)
check("a name shared by several ids is not a name", rejects({"rules": [{"block": "flowing water", "touches": 1, "becomes": 2}]},
                                                             "rule 0 block", "flowing water"))
check("an id above 255 is refused", rejects({"rules": [{"block": 256, "touches": 1, "becomes": 2}]}, "rule 0 block", "256"))
check("a boolean is not an id", rejects({"rules": [{"block": True, "touches": 1, "becomes": 2}]}, "rule 0 block"))
check("a chance of 0 is refused", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2, "chance": 0}]}, "chance"))
check("a chance above 1 is refused", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2, "chance": 1.5}]}, "chance"))
check("a boolean chance is refused", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2, "chance": True}]}, "chance"))
check("an unknown key is named", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2, "speed": 3}]}, "unknown keys", "speed"))
check("a missing key is named", rejects({"rules": [{"block": 1, "touches": 5}]}, "missing", "becomes"))
check("an entry that is not an object is numbered", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2}, 7]}, "rule 1"))
check("a file without a rules list is refused", rejects({"reactions": []}, "rules"))
check("a file that is a list is refused", rejects([], "rules"))

# Families, lists and time.
water = sorted(VoxelGen.WATER_BLOCKS)
check("the family water is every water block, the source and its flows",
      VoxelRules.parse({"rules": [{"block": 1, "touches": "water", "becomes": 2}]})[0][1] == water)
check("a list of touches is their union", VoxelRules.parse({"rules": [{"block": 1, "touches": [5, "lava"], "becomes": 2}]})[0][1]
      == sorted([5] + list(VoxelGen.LAVA_BLOCKS)))
check("a family as the block gives one rule for each of its blocks",
      [r[0] for r in VoxelRules.parse({"rules": [{"block": "lava", "touches": "water", "becomes": "obsidian"}]})]
      == list(VoxelGen.LAVA_BLOCKS))
check("an empty list of touches is refused", rejects({"rules": [{"block": 1, "touches": [], "becomes": 2}]}, "rule 0", "touches"))
check("seconds are kept as a time", VoxelRules.parse({"rules": [{"block": 22, "touches": 5, "becomes": 30, "seconds": 4}]})[0][4] == 4.0)
check("seconds of 0 keep the chance per tick", VoxelRules.parse({"rules": [{"block": 1, "touches": 5, "becomes": 2}]})[0][4] == 0.0)
check("negative seconds are refused", rejects({"rules": [{"block": 1, "touches": 5, "becomes": 2, "seconds": -1}]}, "seconds"))
check("a built-in reaction parses to the water family", all(r[1] == water and r[2] == VoxelGen.OBSIDIAN and r[4] == 4.0
                                                             for r in VoxelRules.parse({"rules": VoxelGen.REACTIONS})))

# Installing and appending.
world = FakeWorld()
count = VoxelRules.install(world, VoxelGen.REACTIONS)
check("install replaces the reactions with the built-in ones",
      count == len(VoxelGen.LAVA_BLOCKS) and world.calls[0] == ("clear",) and len(world.calls) == 1 + count)

world = FakeWorld()
with tempfile.TemporaryDirectory() as folder:
    good = os.path.join(folder, "good.json")
    with open(good, "w", encoding="utf-8") as f:
        json.dump({"rules": [{"block": "grass", "touches": "water", "becomes": "dirt", "chance": 0.5}]}, f)
    count = VoxelRules.load(world, good)
    check("a good file replaces the rules and reports the count",
          count == 1 and world.calls == [("clear",), ("add", 1, water, 2, 0.5, 0.0)])

    world = FakeWorld()
    count = VoxelRules.add_file(world, good)
    check("a rule file adds to the reactions without clearing them", count == 1 and world.calls == [("add", 1, water, 2, 0.5, 0.0)])

    world = FakeWorld()
    bad = os.path.join(folder, "bad.json")
    with open(bad, "w", encoding="utf-8") as f:
        json.dump({"rules": [{"block": 1, "touches": 5, "becomes": 2}, {"block": 1, "touches": 5}]}, f)
    try:
        VoxelRules.load(world, bad)
        raised = False
    except ValueError as error:
        raised = "rule 1" in str(error)
    check("a bad file raises before the world's rules change", raised and world.calls == [])

    world = FakeWorld()
    try:
        VoxelRules.add_file(world, bad)
        raised = False
    except ValueError:
        raised = True
    check("a bad file added to the reactions adds nothing", raised and world.calls == [])

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel rule checks passed.")
