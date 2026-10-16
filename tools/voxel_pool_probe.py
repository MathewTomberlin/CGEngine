"""
Finds the pools of water over stone in the voxel world around a column, inside the running voxel sample: the swimming test
(test_swimming.ps1) needs a pool whose floor is solid at a known height. It writes cg_control/pool_probe.json: for each column in
the box, the top solid block, the water above it, the biome, and whether the column is a pool (water at least two deep over a solid
floor). Run through the control channel by tools/voxel_pool_probe.ps1.
"""
import gc
import json

import VoxelGen

CX, CZ = 0, -31
RADIUS = 14
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world

columns = []
for x in range(CX - RADIUS, CX + RADIUS + 1):
    for z in range(CZ - RADIUS, CZ + RADIUS + 1):
        top = next((y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(x, y, z)), None)
        if top is None:
            continue
        water = 0
        y = top + 1
        while y < VoxelGen.HEIGHT and world.get_block(x, y, z) in VoxelGen.WATER_BLOCKS:
            water += 1
            y += 1
        columns.append({"x": x, "z": z, "floor": top, "water": water, "block": world.get_block(x, top, z),
                        "biome": world.biome_at(x, z)})

pools = [c for c in columns if c["water"] >= 2]
with open("cg_control/pool_probe.json", "w") as f:
    json.dump({"columns": columns, "pools": pools}, f)
print("pool probe: %d columns, %d pools" % (len(columns), len(pools)))
