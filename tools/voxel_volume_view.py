"""
Builds a flat stone floor in the voxel sample, drops a full water volume on it (8 units, one block: VoxelGen.WATER_VOLUME_FULL),
and puts the player in the air beside it, looking down across it, for tools/voxel_view_shot.ps1 -View volume to screenshot. Run
inside the game through the control channel (run_script). The volume spreads and levels out while the screenshot waits.
"""
import gc
import math

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
# The floor is 2 blocks above the highest solid block in the spawn column, so it is clear of the terrain around it.
top = next(y for y in range(VoxelGen.HEIGHT - 1, 0, -1) if world.is_solid(px, y, pz))
floor = top + 2
cx, cz = px + 4, pz
for dx in range(-7, 8):
    for dz in range(-7, 8):
        for dy in range(0, 6):
            world.set_block(cx + dx, floor + dy, cz + dz, VoxelGen.AIR)
        world.set_block(cx + dx, floor, cz + dz, VoxelGen.STONE)
world.set_block(cx, floor + 1, cz, VoxelGen.WATER_VOLUME_FULL)
# Hover over the west bank of the floor, 5 blocks up, and look down across the volume.
game.flying = True
game.vel[1] = 0.0
game.teleport(cx - 5.5, floor + 5.0, cz + 0.5)
game.yaw = 90.0
game.pitch = -35.0
print("VOLUME floor at y %d, volume at %d %d %d" % (floor, cx, floor + 1, cz))
