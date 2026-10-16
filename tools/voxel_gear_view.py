"""
For tools/voxel_view_shot.ps1 -View gear: wears a cap and a tunic (the bonuses show at the top left, in play), and tests the armour
clicks in the inventory on the boots, through the game's own click_equipment: a piece held in the cursor goes on its slot, a click
on a worn slot with nothing held takes the piece off into the cursor, and a piece for another slot does not go on. The results are
written to C:/Users/admin/AppData/Local/Temp/gear_test.json.
"""
import gc
import json

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
results = {}
game.equipment.wear(VoxelGen.LEATHER_CAP)
game.equipment.wear(VoxelGen.LEATHER_TUNIC)

game.inventory.held = [VoxelGen.LEATHER_CAP, 1]
game.click_equipment("feet")
results["wrong_slot_refused"] = game.equipment.worn["feet"] is None and game.inventory.held == [VoxelGen.LEATHER_CAP, 1]
game.inventory.held = [VoxelGen.LEATHER_BOOTS, 1]
game.click_equipment("feet")
results["goes_on"] = game.equipment.worn["feet"] == VoxelGen.LEATHER_BOOTS and game.inventory.held is None
game.click_equipment("feet")
results["taken_off_into_cursor"] = game.equipment.worn["feet"] is None and game.inventory.held == [VoxelGen.LEATHER_BOOTS, 1]
game.inventory.held = None
game.equipment.wear(VoxelGen.LEATHER_BOOTS)
results["summary"] = game.equipment.summary()
with open("C:/Users/admin/AppData/Local/Temp/gear_test.json", "w") as f:
    json.dump(results, f)
