"""
For tools/voxel_view_shot.ps1 -View scroll: knows every recipe, opens the inventory with the list scrolled down three rows, and writes
the scroll state to C:/Users/admin/AppData/Local/Temp/scroll_test.json: the offset, the range, and that the scroll stays in range.
"""
import gc
import json

import VoxelRecipes

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
game.crafting.load_list([recipe["name"] for recipe in VoxelRecipes.RECIPES])
game.crafting.category = "All"
game.inventory_open = True
game.scroll_crafting(-3)
count = len(game.crafting.visible(game.creative))
results = {
    "count": count,
    "offset_after_down_3": game.craft_scroll,
    "range": game.craft_scroll_range(count),
    "down_past_end_clamped": None,
}
game.scroll_crafting(-100)
results["down_past_end_clamped"] = game.craft_scroll == game.craft_scroll_range(count)
game.scroll_crafting(100)
results["up_past_top_clamped"] = game.craft_scroll == 0
with open("C:/Users/admin/AppData/Local/Temp/scroll_test.json", "w") as f:
    json.dump(results, f)
game.scroll_crafting(-3)
