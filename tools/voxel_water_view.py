"""
Puts the player on the bank of the nearest water surface of the voxel sample, looking across it, for tools/voxel_view_shot.ps1 to
screenshot. Run inside the game through the control channel (run_script). The search covers the 40 blocks around the player.
"""
import gc
import math

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
found = None
for radius in range(2, 40):
    for dx in range(-radius, radius + 1):
        for dz in (-radius, radius) if abs(dx) != radius else range(-radius, radius + 1):
            x, z = px + dx, pz + dz
            for y in range(VoxelGen.HEIGHT - 1, 0, -1):
                if world.get_block(x, y, z) != VoxelGen.AIR:
                    if world.get_block(x, y, z) == VoxelGen.WATER:
                        found = (x, y, z)
                    break
            if found:
                break
        if found:
            break
    if found:
        break
if found is None:
    print("WATER no water within 40 blocks")
else:
    wx, wy, wz = found
    # Hover over the bank 6 blocks west of the water, 4 blocks up, and look down across the surface (the player flies).
    x, y, z = wx - 6.5, wy + 4.0, wz + 0.5
    game.flying = True
    game.vel[1] = 0.0
    game.teleport(x, y, z)
    game.yaw = 90.0
    game.pitch = -25.0
    print("WATER water at %d %d %d, player at %.1f %.1f %.1f" % (wx, wy, wz, x, y, z))
