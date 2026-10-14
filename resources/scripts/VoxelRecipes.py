"""
Construction in the voxel sample: recipes that turn items into items. The crafting panel on the inventory screen (I) lists them.

A recipe is known once the player holds one of the items it takes, and creative mode knows them all. A known recipe is listed
under its category, and it is disabled while the inventory lacks an item it takes. A recipe that is not known is not listed.
In creative mode each item counts as CREATIVE_COUNT for the check, and crafting takes nothing from the inventory.

The recipes are data: resources/voxel_recipes.json (or the file named by VOXEL_RECIPES) lists them by item name.

Pure Python with no engine calls, so tools/test_voxel_recipes.py checks it without a window.
"""

import json
import os

import VoxelGen

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
        recipes.append({"name": entry["name"], "category": entry["category"],
                        "takes": items(entry["takes"], entry["name"]), "makes": items(entry["makes"], entry["name"])})
    return tuple(recipes)


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

    def to_list(self):
        """The known recipes for the save file, by name."""
        return sorted(self.known)

    def load_list(self, names):
        """Replace the known recipes with a saved list. Names that are not recipes are dropped."""
        valid = {recipe["name"] for recipe in RECIPES}
        self.known = {name for name in names if name in valid}
