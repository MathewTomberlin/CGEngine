"""
Puts the player on the shore of the nearest sea of the voxel sample, looking out over it, for tools/voxel_view_shot.ps1 -View sea to
screenshot (roadmap 40.5). Run inside the game through the control channel (run_script). The search covers the sea columns out to
SEARCH blocks from the player (the biome is worked out for each column, so the chunks need not be loaded for the search); the player
is then moved there, and the chunks there load while the shot waits.
"""
import gc
import math

import cg_engine_bindings as cge
import VoxelGen

SEARCH = 160     # blocks: the sea looked for, from the player
STEP = 4         # blocks between the columns looked at

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
px, pz = math.floor(game.pos[0]), math.floor(game.pos[2])
# A world of the same seed, used only for its biome (VoxelWorld.biome_at needs no chunk).
probe = cge.VoxelWorld(VoxelGen.CHUNK, VoxelGen.HEIGHT, "voxel")
probe.set_terrain(seed=game.seed, sea_level=VoxelGen.SEA_LEVEL, base_height=8, height_range=44, snow_level=VoxelGen.SNOW_LEVEL,
                  tree_chance=VoxelGen.TREE_CHANCE, cave_width=0.07)
found = None
for radius in range(0, SEARCH, STEP):
    for dx in range(-radius, radius + 1, STEP):
        for dz in range(-radius, radius + 1, STEP):
            if max(abs(dx), abs(dz)) != radius:
                continue
            if probe.biome_at(px + dx, pz + dz) == "sea":
                found = (px + dx, pz + dz)
                break
        if found:
            break
    if found:
        break
if found is None:
    print("SEA no sea within %d blocks" % SEARCH)
else:
    sx, sz = found
    # Stand on the shore, 5 blocks above the sea level, on the land side (-x), looking out over the water (+x), and a little down.
    x, y, z = sx - 9.5, VoxelGen.SEA_LEVEL + 5.0, sz + 0.5
    game.flying = True
    game.vel[1] = 0.0
    game.teleport(x, y, z)
    game.yaw = 90.0
    game.pitch = -12.0
    print("SEA sea column at %d %d, player at %.1f %.1f %.1f" % (sx, sz, x, y, z))
