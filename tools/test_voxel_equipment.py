"""
Checks the armour (resources/scripts/VoxelEquipment.py, resources/voxel_equipment.json) without the engine: a piece goes on its slot
and replaces the one there, a piece is taken off, the speed and jump bonuses are sums clamped to their limits, the side bar names the
pieces, the worn pieces save and load back (a piece in the wrong slot is dropped), and the data file names real items.
Usage (from the repo root): python -I tools/test_voxel_equipment.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelEquipment as eq  # noqa: E402
import VoxelGen  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


TABLE = {
    50: {"slot": "head", "speed": 0.0, "jump": 0.02, "name": "cap"},
    51: {"slot": "head", "speed": 0.05, "jump": 0.0, "name": "helm"},
    52: {"slot": "feet", "speed": 0.04, "jump": 0.0, "name": "boots"},
    53: {"slot": "chest", "speed": -0.2, "jump": -0.2, "name": "plate"},
    54: {"slot": "chest", "speed": 0.7, "jump": 0.3, "name": "silk"},
}
worn = eq.Equipment(TABLE)

check("nothing worn: both factors are 1", worn.speed_factor() == 1.0 and worn.jump_factor() == 1.0)
check("the side bar lists every slot, empty ones too", len(worn.lines()) == len(eq.SLOTS) and worn.lines()[0] == "Hd -")
check("armour is what the table lists", worn.is_armour(50) and not worn.is_armour(999))
check("a slot is the piece's own", worn.slot_of(52) == "feet" and worn.slot_of(999) is None)
try:
    worn.wear(999)
    check("wearing a non-armour item raises", False)
except ValueError:
    check("wearing a non-armour item raises", True)

check("a piece goes on its slot, and nothing is replaced", worn.wear(50) is None and worn.worn["head"] == 50)
check("a second piece for the slot replaces the first, which comes back", worn.wear(51) == 50 and worn.worn["head"] == 51)
check("the speed bonus is the sum over the worn pieces", abs(worn.total("speed") - 0.05) < 1e-9)
check("the speed factor follows the worn speed bonus", abs(worn.speed_factor() - 1.05) < 1e-9)
worn.wear(52)
check("two pieces add their speed", abs(worn.speed_factor() - 1.09) < 1e-9)
check("a piece taken off is returned, and its slot is empty", worn.take_off("feet") == 52 and worn.worn["feet"] is None)
check("taking off an empty slot returns nothing", worn.take_off("feet") is None)

heavy = eq.Equipment(TABLE)
heavy.wear(53)
check("a heavy suit slows the walk, within the limit", abs(heavy.speed_factor() - 0.8) < 1e-9)
check("a heavy suit's jump is clamped above the one-block step", abs(heavy.jump_factor() - 0.9) < 1e-9)
light = eq.Equipment(TABLE)
light.wear(54)
check("a light suit's speed is clamped at the most it adds", abs(light.speed_factor() - (1.0 + eq.SPEED_LIMITS[1])) < 1e-9)
check("a light suit's jump is clamped at the most it adds", abs(light.jump_factor() - (1.0 + eq.JUMP_LIMITS[1])) < 1e-9)

line = worn.lines()[eq.SLOTS.index("head")]
check("the side bar names the piece in its slot", line == "Hd helm")
check("the side bar's last line totals the bonuses", worn.summary() == "speed +5%  jump +0%")

saved = worn.to_list()
loaded = eq.Equipment(TABLE)
loaded.load_list(saved)
check("the worn pieces load back", loaded.worn == worn.worn)
bad = eq.Equipment(TABLE)
bad.load_list([52, None, None, None, None])   # 52 is boots: not for the head slot, so it is dropped
check("a piece in the wrong slot is dropped on load", bad.worn["head"] is None and bad.worn["feet"] is None)
bad.load_list("junk")
check("a damaged save leaves nothing worn", all(v is None for v in bad.worn.values()))

# The data file: every item it names is a constant of VoxelGen, with a slot of its own and a name the recipes use.
check("the data file's pieces are VoxelGen's armour", set(VoxelGen.EQUIPMENT) == {VoxelGen.LEATHER_CAP, VoxelGen.LEATHER_TUNIC,
      VoxelGen.LEATHER_LEGGINGS, VoxelGen.LEATHER_BOOTS, VoxelGen.LEATHER_GLOVES})
check("each piece's slot is one the equipment has", all(e["slot"] in eq.SLOTS for e in VoxelGen.EQUIPMENT.values()))
check("each piece has its name in the game", all(VoxelGen.NAMES.get(item) == e["name"] for item, e in VoxelGen.EQUIPMENT.items()))
g = eq.Equipment(VoxelGen.EQUIPMENT)
check("nothing worn: no strike bonus line", g.combat_summary() == "" and g.damage_factor() == 1.0 and g.break_factor() == 1.0)
g.wear(VoxelGen.LEATHER_GLOVES)
check("gloves add to the strike against creatures", abs(g.damage_factor() - 1.15) < 1e-9)
check("gloves add to the blocks a strike breaks", abs(g.break_factor() - 1.1) < 1e-9)
check("the strike line names the gloves' bonuses", g.combat_summary() == "hit +15%  dig +10%")
h = eq.Equipment(TABLE)
h.table[60] = {"slot": "gloves", "speed": 0.0, "jump": 0.0, "damage": 2.0, "break": -2.0, "name": "giant"}
h.wear(60)
check("strike bonuses are clamped too", abs(h.damage_factor() - 2.0) < 1e-9 and abs(h.break_factor() - 0.5) < 1e-9)
check("nothing worn: no shot bonus", g.shot_factor() == 1.0 and "shot" not in g.combat_summary())
c = eq.Equipment(VoxelGen.EQUIPMENT)
c.wear(VoxelGen.LEATHER_CAP)
check("the cap adds to the arrows' damage", abs(c.shot_factor() - 1.2) < 1e-9)
check("the cap's strike line names the arrows", c.combat_summary() == "shot +20%")
c.wear(VoxelGen.LEATHER_GLOVES)
check("gloves and cap show together on the strike line", c.combat_summary() == "hit +15%  dig +10%  shot +20%")
k = eq.Equipment(TABLE)
k.table[61] = {"slot": "head", "speed": 0.0, "jump": 0.0, "shot": 5.0, "name": "sniper"}
k.wear(61)
check("a cap with no strike bonus reads as none, not as an error", abs(c.damage_factor() - 1.15) < 1e-9 and abs(eq.Equipment(TABLE).damage_factor() - 1.0) < 1e-9)
check("the arrow bonus is clamped", abs(k.shot_factor() - 2.0) < 1e-9)
check("every slot has a piece to wear", {e["slot"] for e in VoxelGen.EQUIPMENT.values()} == set(eq.SLOTS))

if failures:
    print("\n%d check(s) failed" % len(failures))
    sys.exit(1)
print("\nAll voxel equipment checks passed.")
