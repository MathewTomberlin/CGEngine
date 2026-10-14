"""
Construction in the voxel sample: recipes that turn items into items. The crafting panel on the inventory screen (I) lists them.

A recipe is known once the player holds one of the items it takes, and creative mode knows them all. A known recipe is listed
under its category, and it is disabled while the inventory lacks an item it takes. A recipe that is not known is not listed.
In creative mode each item counts as CREATIVE_COUNT for the check, and crafting takes nothing from the inventory.

The recipes are data: resources/voxel_recipes.json (or the file named by VOXEL_RECIPES) lists them by item name.

A shaped recipe is made from the 3 by 3 crafting grid: the player puts items in its cells, one at a time, and when the grid holds
the pattern the output is offered. The items in the grid came out of the inventory; they go back when the screen closes.
Shapeless recipes are still made from the recipe's row, by counts.

Pure Python with no engine calls, so tools/test_voxel_recipes.py checks it without a window.
"""

import json
import os

import VoxelGen
import VoxelInventory

CREATIVE_COUNT = 99     # what creative mode counts each item as when it checks a recipe
ALL = "All"             # the category that lists every recipe

# A recipe: its name, its category, the items it takes and the items it makes, each as {item id: count}.
ITEM_IDS = {name: item for item, name in VoxelGen.NAMES.items()}


def load(path):
    """The recipes in a JSON file, with the item names turned into ids. A name the game does not know is an error, named."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    def items(counts, recipe):
        out = {}
        for name, count in counts.items():
            if name not in ITEM_IDS:
                raise ValueError("recipe %r: %r is not an item the game knows" % (recipe, name))
            out[ITEM_IDS[name]] = int(count)
        return out

    recipes = []
    for entry in data["recipes"]:
        name = entry["name"]
        if "pattern" in entry:
            pattern, takes = shape(entry, name)
        else:
            pattern, takes = None, items(entry["takes"], name)
        recipes.append({"name": name, "category": entry["category"], "takes": takes,
                        "makes": items(entry["makes"], name), "pattern": pattern})
    return tuple(recipes)


GRID = 3    # the crafting grid is GRID by GRID cells; a pattern is at most that big


def shape(entry, name):
    """A shaped recipe's pattern as rows of item ids (None for an empty cell), and the items it takes, counted from the pattern.
    The rows are a list of strings; a space is an empty cell and every other character is a key to an item name."""
    if "takes" in entry:
        raise ValueError("recipe %r: a shaped recipe takes its items from its pattern, not from 'takes'" % name)
    rows = entry["pattern"]
    if not isinstance(rows, list) or not rows or len(rows) > GRID:
        raise ValueError("recipe %r: a pattern is between 1 and %d rows" % (name, GRID))
    width = len(rows[0]) if isinstance(rows[0], str) else 0
    if not 1 <= width <= GRID or any(not isinstance(row, str) or len(row) != width for row in rows):
        raise ValueError("recipe %r: every row of a pattern is the same length, from 1 to %d" % (name, GRID))
    key = entry.get("key", {})
    if not isinstance(key, dict) or any(len(char) != 1 or char == " " for char in key):
        raise ValueError("recipe %r: a key maps one character (not a space) to an item name" % name)
    for char, item_name in key.items():
        if item_name not in ITEM_IDS:
            raise ValueError("recipe %r: %r is not an item the game knows" % (name, item_name))
    pattern, takes = [], {}
    for row in rows:
        cells = []
        for char in row:
            if char == " ":
                cells.append(None)
                continue
            if char not in key:
                raise ValueError("recipe %r: the pattern uses %r, which the key does not name" % (name, char))
            item = ITEM_IDS[key[char]]
            cells.append(item)
            takes[item] = takes.get(item, 0) + 1
        pattern.append(tuple(cells))
    return tuple(pattern), takes


def matches(recipe, grid):
    """True when a shaped recipe's pattern is in the crafting grid: grid is GRID rows of GRID item ids (None for an empty cell).
    The pattern may sit anywhere in the grid, but every cell it does not cover must be empty, so a shape must be exact.
    A recipe that is not shaped never matches here."""
    pattern = recipe.get("pattern")
    if not pattern:
        return False
    height, width = len(pattern), len(pattern[0])
    for top in range(GRID - height + 1):
        for left in range(GRID - width + 1):
            fits = True
            for row in range(GRID):
                for col in range(GRID):
                    inside = top <= row < top + height and left <= col < left + width
                    want = pattern[row - top][col - left] if inside else None
                    if grid[row][col] != want:
                        fits = False
                        break
                if not fits:
                    break
            if fits:
                return True
    return False


def data_file():
    """VOXEL_RECIPES if it is set; else voxel_recipes.json beside this script (the game folder) or in resources (the repository)."""
    if os.environ.get("VOXEL_RECIPES"):
        return os.environ["VOXEL_RECIPES"]
    here = os.path.dirname(os.path.abspath(__file__))
    for path in (os.path.join(here, "voxel_recipes.json"), os.path.join(here, os.pardir, "voxel_recipes.json")):
        if os.path.isfile(path):
            return path
    raise FileNotFoundError("voxel_recipes.json was not found beside the scripts or in resources")


RECIPES = load(data_file())
CATEGORIES = (ALL,) + tuple(sorted({recipe["category"] for recipe in RECIPES}))


def describe(items):
    """The items as a phrase, such as '2 wood, 1 torch'."""
    return ", ".join("%d %s" % (count, VoxelGen.NAMES.get(item, "?")) for item, count in items.items())


class Crafting:
    def __init__(self):
        self.known = set()          # the names of the recipes the player has learned
        self.category = ALL         # the category the dropdown shows
        self.dropdown_open = False
        self.grid = [None] * (GRID * GRID)   # the crafting grid, row by row: the item in each cell, or None
        self.pattern = None         # the name of the shaped recipe whose pattern the grid shows as a guide, or None

    def learn(self, inventory):
        """Learn each recipe whose items the inventory holds one of. Returns the names learned now, so the game can say so."""
        learned = []
        for recipe in RECIPES:
            if recipe["name"] in self.known:
                continue
            if any(inventory.count(item) > 0 for item in recipe["takes"]):
                self.known.add(recipe["name"])
                learned.append(recipe["name"])
        return learned

    def visible(self, creative):
        """The recipes the panel lists: the known ones (every one in creative mode), in the chosen category."""
        return [recipe for recipe in RECIPES
                if (creative or recipe["name"] in self.known)
                and (self.category == ALL or recipe["category"] == self.category)]

    def stock(self, item, inventory, creative):
        """How much of an item the check counts: the inventory's, or CREATIVE_COUNT in creative mode."""
        return CREATIVE_COUNT if creative else inventory.count(item)

    def missing(self, recipe, inventory, creative):
        """The items the inventory lacks for a recipe, as {item: how many more}. Empty when the recipe can be made."""
        lacking = {}
        for item, count in recipe["takes"].items():
            short = count - self.stock(item, inventory, creative)
            if short > 0:
                lacking[item] = short
        return lacking

    def craft(self, recipe, inventory, creative):
        """Make a recipe: take its items out of the inventory and put its output in. Returns False, and changes nothing, when
        an item is missing or the output would not fit. Creative mode takes nothing from the inventory."""
        if self.missing(recipe, inventory, creative):
            return False
        saved = [None if slot is None else list(slot) for slot in inventory.slots]
        if not creative:
            for item, count in recipe["takes"].items():
                inventory.take_item(item, count)
        for item, count in recipe["makes"].items():
            if inventory.add(item, count) > 0:
                inventory.slots = saved   # the output does not fit: the inventory is as it was
                return False
        return True

    def grid_recipe(self, creative):
        """The first recipe the crafting grid matches, among those the player can make (known, or any in creative mode), or None."""
        rows = [self.grid[r * GRID:(r + 1) * GRID] for r in range(GRID)]
        for recipe in RECIPES:
            if (creative or recipe["name"] in self.known) and matches(recipe, rows):
                return recipe
        return None

    def pattern_recipe(self):
        """The shaped recipe the grid shows as a guide, or None."""
        for recipe in RECIPES:
            if recipe["name"] == self.pattern:
                return recipe
        return None

    def toggle_pattern(self, name):
        """Show the pattern of a shaped recipe in the grid, or hide it when it is already shown. Returns True when it is shown."""
        self.pattern = None if self.pattern == name else name
        return self.pattern is not None

    def click_cell(self, index, inventory, secondary=False):
        """A click on a grid cell. The left button puts one item from the held stack into an empty cell, or takes the cell's item
        into the held stack (onto the same item). The right button puts the cell's item back into the inventory. A tool does not
        go in the grid: it has its own uses, which a grid cell does not keep."""
        item = self.grid[index]
        if secondary:
            if item is not None and inventory.add(item) == 0:
                self.grid[index] = None
            return
        held = inventory.held
        if item is None:
            if held is None or held[0] in VoxelGen.TOOLS:
                return
            self.grid[index] = held[0]
            held[1] -= 1
            if held[1] == 0:
                inventory.held = None
            return
        if held is None:
            inventory.held = [item, 1]
        elif held[0] == item and held[1] < VoxelInventory.STACK_LIMIT:
            held[1] += 1
        else:
            return
        self.grid[index] = None

    def craft_grid(self, inventory, creative):
        """Make the recipe the grid matches: its output goes into the inventory and the grid empties. Returns the recipe made, or
        None when the grid matches none or the output does not fit (then the grid and the inventory are as they were)."""
        recipe = self.grid_recipe(creative)
        if recipe is None:
            return None
        saved = [None if slot is None else list(slot) for slot in inventory.slots]
        for item, count in recipe["makes"].items():
            if inventory.add(item, count) > 0:
                inventory.slots = saved
                return None
        self.grid = [None] * (GRID * GRID)
        return recipe

    def return_grid(self, inventory):
        """Put the grid's items back into the inventory (when the screen closes, or before a save). An item that does not fit stays
        in its cell, so none is lost."""
        for index, item in enumerate(self.grid):
            if item is not None and inventory.add(item) == 0:
                self.grid[index] = None

    def to_list(self):
        """The known recipes for the save file, by name."""
        return sorted(self.known)

    def load_list(self, names):
        """Replace the known recipes with a saved list. Names that are not recipes are dropped."""
        valid = {recipe["name"] for recipe in RECIPES}
        self.known = {name for name in names if name in valid}
