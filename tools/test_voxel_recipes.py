"""
Checks the voxel construction rules (resources/scripts/VoxelRecipes.py) without the engine: a recipe is learned when the player
holds one of its items, unknown recipes are not listed, a known recipe is disabled while an item is missing, crafting takes the
items and puts the output in, a full inventory refuses a craft and keeps its items, creative mode knows and counts everything,
the known recipes load back from the save file, and the recipes come from resources/voxel_recipes.json (a bad file is an error).
Usage (from the repo root): python -I tools/test_voxel_recipes.py
Exits 0 on success, 1 on any failed check.
"""
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelGen  # noqa: E402
import VoxelInventory  # noqa: E402
import VoxelRecipes  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


WOOD, TORCH, STONE, IRON = VoxelGen.WOOD, VoxelGen.TORCH, VoxelGen.STONE, VoxelGen.IRON
PICKAXE, IRON_PICKAXE = VoxelGen.PICKAXE, VoxelGen.IRON_PICKAXE
BY_NAME = {recipe["name"]: recipe for recipe in VoxelRecipes.RECIPES}
torch, pickaxe, iron_pickaxe = BY_NAME["Torch"], BY_NAME["Pickaxe"], BY_NAME["Iron pickaxe"]
ALL_NAMES = ["Torch", "Pickaxe", "Iron pickaxe"]

check("the recipes come from the data file, in file order", [r["name"] for r in VoxelRecipes.RECIPES] == ALL_NAMES)
check("the pickaxe takes stone and wood and makes a pickaxe",
      pickaxe["takes"] == {STONE: 3, WOOD: 2} and pickaxe["makes"] == {PICKAXE: 1})
check("the iron pickaxe takes iron and wood and makes an iron pickaxe",
      iron_pickaxe["takes"] == {IRON: 3, WOOD: 2} and iron_pickaxe["makes"] == {IRON_PICKAXE: 1})

# A new player: nothing in the inventory, no recipe known.
inv = VoxelInventory.Inventory()
crafting = VoxelRecipes.Crafting()
check("a new player knows no recipe", crafting.known == set())
check("an unknown recipe is not listed", crafting.visible(creative=False) == [])
check("learning from an empty inventory teaches nothing", crafting.learn(inv) == [])

# Picking up wood teaches every recipe that takes wood, once.
inv.add(WOOD, 3)
check("holding wood teaches the recipes that take wood", crafting.learn(inv) == ALL_NAMES and crafting.known == set(ALL_NAMES))
check("a recipe is learned only once", crafting.learn(inv) == [])
check("a known recipe is listed", [r["name"] for r in crafting.visible(creative=False)] == ALL_NAMES)

# Missing items disable the recipe; a made recipe takes its items and gives its output.
empty = VoxelInventory.Inventory()
check("a known recipe with its item missing is listed but cannot be made",
      crafting.visible(creative=False) and crafting.missing(torch, empty, False) == {WOOD: 1})
check("crafting with a missing item fails and changes nothing", crafting.craft(torch, empty, False) is False
      and empty.slots == [None] * VoxelInventory.SLOTS)
check("the pickaxe lists what it lacks: stone and wood", crafting.missing(pickaxe, inv, False) == {STONE: 3})
wood_before = inv.count(WOOD)
check("crafting with the item present succeeds", crafting.craft(torch, inv, False) is True)
check("crafting takes one wood and gives one torch", inv.count(WOOD) == wood_before - 1 and inv.count(TORCH) == 1)

# A full inventory refuses the craft and keeps every item it would have taken.
full = VoxelInventory.Inventory()
for index in range(VoxelInventory.SLOTS):
    full.slots[index] = [VoxelGen.STONE, VoxelInventory.STACK_LIMIT]
full.slots[0] = [WOOD, 2]   # taking one leaves a wood stack in the slot, so no room is freed for the torch
snapshot = [None if s is None else list(s) for s in full.slots]
check("a craft whose output does not fit is refused", crafting.craft(torch, full, False) is False)
check("a refused craft keeps the wood it would have taken", full.slots == snapshot)

# The iron pickaxe is made from iron ore and wood; a tool is made with its full durability.
ironed = VoxelInventory.Inventory()
ironed.add(IRON, 3)
ironed.add(WOOD, 2)
check("the iron pickaxe is made from iron and wood", crafting.craft(iron_pickaxe, ironed, False) is True
      and ironed.count(IRON) == 0 and ironed.count(IRON_PICKAXE) == 1)
check("the made tool has its full durability", ironed.slots[0][2] == VoxelGen.TOOLS[IRON_PICKAXE]["durability"])

# Category filter.
check("the dropdown lists All and the categories of the recipes", VoxelRecipes.CATEGORIES == ("All", "Light", "Tools"))
crafting.category = "Light"
check("the Light category lists the torch", crafting.visible(creative=False) == [torch])
crafting.category = "Tools"
check("the Tools category lists the pickaxes", [r["name"] for r in crafting.visible(creative=False)] == ["Pickaxe", "Iron pickaxe"])
crafting.category = VoxelRecipes.ALL
fresh_tools = VoxelRecipes.Crafting()
fresh_tools.category = "Tools"
check("an unknown category's recipes are not listed", fresh_tools.visible(creative=False) == [])

# Creative mode knows every recipe and counts each item as 99; crafting takes nothing from the inventory.
fresh = VoxelRecipes.Crafting()
none = VoxelInventory.Inventory()
check("creative mode lists every recipe, known or not", [r["name"] for r in fresh.visible(creative=True)] == ALL_NAMES)
check("creative mode counts a missing item as 99", fresh.missing(torch, none, True) == {})
check("creative mode crafts with nothing in the inventory", fresh.craft(torch, none, True) is True)
check("creative crafting adds the output and takes no wood", none.count(TORCH) == 1 and none.count(WOOD) == 0)
check("creative crafting does not teach a recipe", fresh.known == set())

# The known recipes save and load; names that are not recipes are dropped.
saved = crafting.to_list()
loaded = VoxelRecipes.Crafting()
loaded.load_list(saved + ["Gold ring"])
check("the known recipes load back, and an unknown name is dropped", loaded.known == set(ALL_NAMES))

# A recipe file that names an item the game does not know is an error, and the error names it.
bad = os.path.join(tempfile.gettempdir(), "voxel_recipes_bad_test.json")
with open(bad, "w", encoding="utf-8") as f:
    json.dump({"recipes": [{"name": "Gold", "category": "Tools", "takes": {"gold": 1}, "makes": {"torch": 1}}]}, f)
try:
    VoxelRecipes.load(bad)
    raised = ""
except ValueError as error:
    raised = str(error)
os.remove(bad)
check("a recipe that takes an unknown item is refused, and the error names it", "gold" in raised)

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel recipe checks passed.")
sys.exit(0)
