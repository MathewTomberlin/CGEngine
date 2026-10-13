"""
Checks the voxel construction rules (resources/scripts/VoxelRecipes.py) without the engine: a recipe is learned when the player
holds one of its items, unknown recipes are not listed, a known recipe is disabled while an item is missing, crafting takes the
items and puts the output in, a full inventory refuses a craft and keeps its items, creative mode knows and counts everything,
and the known recipes load back from the save file.
Usage (from the repo root): python -I tools/test_voxel_recipes.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

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


WOOD, TORCH = VoxelGen.WOOD, VoxelGen.TORCH
torch = VoxelRecipes.RECIPES[0]
assert torch["name"] == "Torch"

# A new player: nothing in the inventory, no recipe known.
inv = VoxelInventory.Inventory()
crafting = VoxelRecipes.Crafting()
check("a new player knows no recipe", crafting.known == set())
check("an unknown recipe is not listed", crafting.visible(creative=False) == [])
check("learning from an empty inventory teaches nothing", crafting.learn(inv) == [])

# Picking up wood teaches the torch recipe, once.
inv.add(WOOD, 3)
check("holding wood teaches the torch recipe", crafting.learn(inv) == ["Torch"] and "Torch" in crafting.known)
check("a recipe is learned only once", crafting.learn(inv) == [])
check("a known recipe is listed", crafting.visible(creative=False) == [torch])

# Missing items disable the recipe; a made recipe takes its items and gives its output.
empty = VoxelInventory.Inventory()
check("a known recipe with its item missing is listed but cannot be made",
      crafting.visible(creative=False) == [torch] and crafting.missing(torch, empty, False) == {WOOD: 1})
check("crafting with a missing item fails and changes nothing", crafting.craft(torch, empty, False) is False
      and empty.slots == [None] * VoxelInventory.SLOTS)
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

# Category filter.
check("the dropdown lists All and the categories of the recipes", VoxelRecipes.CATEGORIES == ("All", "Light"))
crafting.category = "Light"
check("the Light category lists the torch", crafting.visible(creative=False) == [torch])
crafting.category = "Tools"
check("an empty category lists nothing", crafting.visible(creative=False) == [])
crafting.category = VoxelRecipes.ALL

# Creative mode knows every recipe and counts each item as 99; crafting takes nothing from the inventory.
fresh = VoxelRecipes.Crafting()
none = VoxelInventory.Inventory()
check("creative mode lists every recipe, known or not", fresh.visible(creative=True) == [torch])
check("creative mode counts a missing item as 99", fresh.missing(torch, none, True) == {})
check("creative mode crafts with nothing in the inventory", fresh.craft(torch, none, True) is True)
check("creative crafting adds the output and takes no wood", none.count(TORCH) == 1 and none.count(WOOD) == 0)
check("creative crafting does not teach a recipe", fresh.known == set())

# The known recipes save and load; names that are not recipes are dropped.
saved = crafting.to_list()
loaded = VoxelRecipes.Crafting()
loaded.load_list(saved + ["Gold ring"])
check("the known recipes load back, and an unknown name is dropped", loaded.known == {"Torch"})

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel recipe checks passed.")
sys.exit(0)
