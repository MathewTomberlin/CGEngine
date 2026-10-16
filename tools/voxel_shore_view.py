"""
Builds a shore in the voxel sample for tools/voxel_view_shot.ps1 -View shore: a stone floor at the level of the water, a raised stone
bank along one side of a water pool, the pool filled with water, and the player on the lower side looking at the bank's edge.
Run inside the game through the control channel (run_script).
"""
import gc
import math

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
top = next(y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(px, y, pz))
floor = top + 2
cx, cz = px + 4, pz
for dx in range(-8, 9):
    for dz in range(-8, 9):
        for dy in range(0, 8):
            world.set_block(cx + dx, floor + dy, cz + dz, VoxelGen.AIR)
        world.set_block(cx + dx, floor, cz + dz, VoxelGen.STONE)
# The bank: a raised stone wall along x = cx + 2, three blocks high.
for dz in range(-8, 9):
    for dy in range(1, 4):
        world.set_block(cx + 2, floor + dy, cz + dz, VoxelGen.STONE)
# The pool: water (a full volume) in the cells west of the bank.
for dx in range(-2, 2):
    for dz in range(-3, 4):
        world.set_block(cx + dx, floor + 1, cz + dz, VoxelGen.WATER_VOLUME_FULL)
game.flying = True
game.vel[1] = 0.0
game.teleport(cx - 5.5, floor + 4.0, cz + 0.5)
game.yaw = 90.0
game.pitch = -15.0
print("SHORE floor at y %d, bank at x %d" % (floor, cx + 2))
