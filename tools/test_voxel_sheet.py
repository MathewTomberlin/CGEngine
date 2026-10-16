"""
Checks the console's sheet command (resources/scripts/VoxelConsole.py) without the engine: it names the health, the held item with
its damage and uses left, the shield, and the recipes known, from a stand-in game. Usage (from the repo root):
python -I tools/test_voxel_sheet.py   Exits 0 on success, 1 on any failed check.
"""
import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
sys.modules["cg_engine_bindings"] = types.ModuleType("cg_engine_bindings")

import VoxelConsole  # noqa: E402
import VoxelEquipment  # noqa: E402
import VoxelGen  # noqa: E402
import VoxelInventory  # noqa: E402
import VoxelRecipes  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


class Crafting:
    def __init__(self, known):
        self.known = set(known)


class Game:
    """The fields the sheet reads: health, the mode, the hotbar slot and its inventory, the shield and the recipes."""
    def __init__(self, slots, slot, offhand, raised):
        self.health = 7.5
        self.creative = False
        self.inventory = VoxelInventory.Inventory()
        self.inventory.slots = slots + [None] * (VoxelInventory.SLOTS - len(slots))
        self.slot = slot
        self.offhand = offhand
        self.offhand_uses = VoxelGen.SHIELD_USES
        self._raised = raised
        self.crafting = Crafting(["Sword"])
        self.equipment = VoxelEquipment.Equipment(VoxelGen.EQUIPMENT)
        self.equipment.wear(VoxelGen.LEATHER_CAP)

    def selected(self):
        return self.inventory.item(self.slot)

    def selected_name(self):
        return VoxelGen.NAMES.get(self.selected(), "nothing")

    def shield_up(self):
        return self._raised


def sheet(game):
    return "\n".join(VoxelConsole.cmd_sheet(types.SimpleNamespace(game=game), []))


SWORD = VoxelGen.SWORD
text = sheet(Game([[SWORD, 1, 45]], 0, VoxelGen.SHIELD, True))
check("the sheet names health, mode and the held sword with its damage and uses left",
      "Health 7.5 of 10; survival mode" in text and "sword, striking for 2.5, 45 of 1280 uses left" in text)
check("the sheet says the shield is raised while Q holds it", "Shield: in the left hand, raised" in text)
check("the sheet counts the recipes known", "Recipes known: 1 of %d" % len(VoxelRecipes.RECIPES) in text)
text = sheet(Game([[VoxelGen.STONE, 3]], 0, None, False))
check("an empty offhand and a block in hand: no shield, the block does 1", "Shield: none" in text
      and "striking for 1" in text and "uses left" not in text)
text = sheet(Game([], 0, None, False))
check("an empty hand says so", "Holding: nothing" in text)
check("the sheet lists the armour worn, and the bonuses it adds", "Hd leather cap" in text and "speed +0%  jump +2%" in text)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel sheet checks passed.")
