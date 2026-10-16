"""
Shows the armour side bar, for tools/voxel_view_shot.ps1 -View equipment: the inventory is opened with a cap and a tunic worn, and the
other armour in the inventory, so the bar names the pieces and the empty slots.
"""
import gc

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
for item in (VoxelGen.LEATHER_CAP, VoxelGen.LEATHER_TUNIC, VoxelGen.LEATHER_BOOTS):
    game.inventory.add(item, 1)
game.equipment.wear(VoxelGen.LEATHER_CAP)
game.equipment.wear(VoxelGen.LEATHER_TUNIC)
game.inventory_open = True
