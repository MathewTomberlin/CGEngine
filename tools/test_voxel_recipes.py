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
ALL_NAMES = ["Torch", "Pickaxe", "Iron pickaxe", "Sword", "Shield", "Bone blade", "Shovel", "Bucket", "Bow", "Arrow"]

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
inv.add(IRON, 3)   # iron teaches the recipes that take iron (the bucket takes only iron)
check("holding wood and iron teaches the recipes that take them", crafting.learn(inv) == ALL_NAMES and crafting.known == set(ALL_NAMES))
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
check("the Tools category lists the pickaxes, the sword, the shield, the bone blade, the shovel and the bucket", [r["name"] for r in crafting.visible(creative=False)] == ["Pickaxe", "Iron pickaxe", "Sword", "Shield", "Bone blade", "Shovel", "Bucket", "Bow", "Arrow"])
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

# Shaped recipes (30.1): a pattern in the file gives the items it takes, and the grid matches the pattern exactly.
pick = [r for r in VoxelRecipes.RECIPES if r["name"] == "Pickaxe"][0]
check("the pickaxe is shaped, and takes the same items its pattern counts", pick["pattern"] is not None and pick["takes"] == {STONE: 3, WOOD: 2})
check("a shapeless recipe has no pattern", [r for r in VoxelRecipes.RECIPES if r["name"] == "Torch"][0]["pattern"] is None)
E = None
grid = [[STONE, STONE, STONE], [E, WOOD, E], [E, WOOD, E]]
check("the pickaxe's pattern in the grid matches", VoxelRecipes.matches(pick, grid))
# A two-high pattern of wood can sit at the bottom right of the grid, and the cells above and to its left must be empty.
two_wood = {"name": "Two wood", "pattern": ((WOOD,), (WOOD,))}
check("a two-high pattern matches at the bottom right of the grid", VoxelRecipes.matches(two_wood, [[E, E, E], [E, E, E], [E, WOOD, WOOD]]) is False
      and VoxelRecipes.matches(two_wood, [[E, E, E], [E, E, WOOD], [E, E, WOOD]]))
check("a two-high pattern is not a match with an item beside it", not VoxelRecipes.matches(two_wood, [[E, E, E], [E, WOOD, E], [E, E, WOOD]]))
check("an empty grid is not a match for a pattern", not VoxelRecipes.matches(pick, [[E, E, E], [E, E, E], [E, E, E]]))
check("a stone in the wrong place is not a match", not VoxelRecipes.matches(pick, [[STONE, STONE, E], [E, WOOD, E], [E, WOOD, STONE]]))
check("an extra item outside the pattern is not a match", not VoxelRecipes.matches(pick, [[STONE, STONE, STONE], [E, WOOD, E], [WOOD, WOOD, E]]))
check("a mirrored pattern is not a match", not VoxelRecipes.matches(pick, [[STONE, STONE, STONE], [E, E, WOOD], [E, E, WOOD]]) and
      not VoxelRecipes.matches(pick, [[STONE, STONE, STONE], [WOOD, E, E], [WOOD, E, E]]))
check("a shapeless recipe never matches a grid", not VoxelRecipes.matches([r for r in VoxelRecipes.RECIPES if r["name"] == "Torch"][0], grid))
bad_shapes = {
    "a row of a different length": {"pattern": ["SS", "W"], "key": {"S": "stone", "W": "wood"}},
    "a character the key does not name": {"pattern": ["SX"], "key": {"S": "stone"}},
    "a key to an unknown item": {"pattern": ["S"], "key": {"S": "gold"}},
    "a pattern bigger than the grid": {"pattern": ["SSSS"], "key": {"S": "stone"}},
    "a shaped recipe that also lists takes": {"pattern": ["S"], "key": {"S": "stone"}, "takes": {"stone": 1}},
}
for label, extra in bad_shapes.items():
    recipe = {"name": "Bad", "category": "Tools", "makes": {"torch": 1}}
    recipe.update(extra)
    path = os.path.join(tempfile.gettempdir(), "voxel_shape_bad_test.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"recipes": [recipe]}, f)
    try:
        VoxelRecipes.load(path)
        refused = False
    except ValueError:
        refused = True
    os.remove(path)
    check("a bad shape is refused: " + label, refused)

# The crafting grid: items go in one at a time, the pattern makes the output, and the items come back out.
PICK = [STONE, STONE, STONE, None, WOOD, None, None, WOOD, None]   # the pickaxe's pattern, from the top left
grid_inv = VoxelInventory.Inventory()
grid_crafting = VoxelRecipes.Crafting()
grid_inv.add(STONE, 3)
grid_inv.add(WOOD, 2)
grid_crafting.learn(grid_inv)
grid_inv.slots = [None] * VoxelInventory.SLOTS
grid_inv.held = [STONE, 3]
for index in (0, 1, 2):
    grid_crafting.click_cell(index, grid_inv)
check("a left click puts one held item in an empty cell, and the stack empties", grid_crafting.grid[:3] == [STONE] * 3
      and grid_inv.held is None)
grid_inv.held = [WOOD, 2]
grid_crafting.click_cell(4, grid_inv)
grid_crafting.click_cell(7, grid_inv)
check("the held wood fills two cells and the stack empties", grid_crafting.grid[4] == WOOD and grid_crafting.grid[7] == WOOD
      and grid_inv.held is None)
check("the grid matches the pickaxe, named by grid_recipe", grid_crafting.grid == PICK
      and grid_crafting.grid_recipe(False) is pickaxe)
grid_crafting.grid[5] = STONE
check("a stone on a cell the pattern does not use stops the match", grid_crafting.grid_recipe(False) is None)
grid_crafting.grid[5] = None
grid_inv.held = [IRON, 1]
grid_crafting.click_cell(0, grid_inv)
check("a different item in hand does not replace a cell's item", grid_crafting.grid[0] == STONE and grid_inv.held == [IRON, 1])
grid_inv.held = None
grid_crafting.click_cell(0, grid_inv)
check("with the hand empty, a left click takes the cell's item into the hand", grid_inv.held == [STONE, 1] and grid_crafting.grid[0] is None)
grid_inv.return_held()
grid_crafting.grid[0] = STONE
stone_before = grid_inv.count(STONE)
grid_crafting.click_cell(0, grid_inv, secondary=True)
check("the right button returns the cell's item to the inventory", grid_crafting.grid[0] is None
      and grid_inv.count(STONE) == stone_before + 1)
grid_crafting.grid = list(PICK)
grid_inv.slots = [None] * VoxelInventory.SLOTS
made = grid_crafting.craft_grid(grid_inv, False)
check("the matched pattern's output is made and the grid empties", made is pickaxe and grid_inv.count(PICKAXE) == 1
      and grid_crafting.grid == [None] * 9)
grid_crafting.grid = list(PICK)
full_inv = VoxelInventory.Inventory()
full_inv.start([VoxelGen.DIRT] * VoxelInventory.SLOTS, 1)
check("an output that does not fit is refused and the grid keeps its items",
      grid_crafting.craft_grid(full_inv, False) is None and grid_crafting.grid == PICK)
grid_crafting.grid = list(PICK)
check("a recipe the player has not learned is not offered from the grid", VoxelRecipes.Crafting().grid_recipe(False) is None)
check("creative mode offers the grid's recipe without learning it", VoxelRecipes.Crafting().grid_recipe(True) is None)
fresh = VoxelRecipes.Crafting()
fresh.grid = list(PICK)
check("creative mode offers the grid's recipe, known or not", fresh.grid_recipe(True) is pickaxe)
grid_crafting.grid = [STONE, None, None, None, WOOD, None, None, WOOD, None]
grid_inv.slots = [None] * VoxelInventory.SLOTS
grid_crafting.return_grid(grid_inv)
check("returning the grid puts its items back in the inventory", grid_crafting.grid == [None] * 9
      and grid_inv.count(STONE) == 1 and grid_inv.count(WOOD) == 2)
full_again = VoxelInventory.Inventory()
full_again.start([VoxelGen.DIRT] * VoxelInventory.SLOTS, 1)
grid_crafting.grid[0] = STONE
grid_crafting.return_grid(full_again)
check("an item that does not fit when the grid returns stays in its cell", grid_crafting.grid[0] == STONE)
tool_inv = VoxelInventory.Inventory()
tool_inv.held = [PICKAXE, 1, VoxelGen.TOOLS[PICKAXE]["durability"]]
grid_crafting.grid = [None] * 9
grid_crafting.click_cell(0, tool_inv)
check("a tool does not go in the grid", grid_crafting.grid[0] is None and tool_inv.held is not None)
check("a shaped recipe's pattern is shown until toggled off", grid_crafting.toggle_pattern("Pickaxe") is True
      and grid_crafting.pattern_recipe() is pickaxe and grid_crafting.toggle_pattern("Pickaxe") is False
      and grid_crafting.pattern is None)

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel recipe checks passed.")
sys.exit(0)
