"""
Counts the iron ore in the loaded voxel world around the player, inside the running voxel sample, for roadmap 28.4 (is iron found,
not only mined deep?). It writes cg_control/ore_probe.json: the iron blocks in the box (radius RADIUS blocks around the player),
how many touch air (a cave wall or the surface, so a player sees them), and how many lie within NEAR blocks of their column's top
(a shallow dig reaches them). Run through the control channel by tools/voxel_ore_probe.ps1.
"""
import gc
import json
import math

import VoxelGen

RADIUS = 40
NEAR = 8
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))

iron = exposed = near = columns_with = columns = 0
for x in range(px - RADIUS, px + RADIUS + 1):
    for z in range(pz - RADIUS, pz + RADIUS + 1):
        top = next((y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(x, y, z)), None)
        if top is None:
            continue
        columns += 1
        column_has = False
        for y in range(1, top + 1):
            if world.get_block(x, y, z) != VoxelGen.IRON:
                continue
            iron += 1
            if not world.is_solid(x + 1, y, z) or not world.is_solid(x - 1, y, z) or not world.is_solid(x, y + 1, z) \
                    or not world.is_solid(x, y - 1, z) or not world.is_solid(x, y, z + 1) or not world.is_solid(x, y, z - 1):
                exposed += 1
            if top - y <= NEAR:
                near += 1
                column_has = True
        columns_with += column_has

with open("cg_control/ore_probe.json", "w") as f:
    json.dump({"radius": RADIUS, "near": NEAR, "columns": columns, "iron": iron, "exposed": exposed,
               "near_top": near, "columns_with_near_top": columns_with}, f)
