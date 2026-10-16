"""
Checks whether water is drawn at each level of detail of the voxel sample: a full water volume on a stone floor in a near chunk and in
a far chunk, the water's mesh vertices over it, and the detail each chunk is meshed at. Writes cg_control/lod_probe.json. Run
through the control channel by tools/voxel_lod_probe.ps1.
"""
import gc
import json
import math

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
result = {}


def place(name, x0, z0, y0):
    cx0, cz0 = x0 // VoxelGen.CHUNK, z0 // VoxelGen.CHUNK
    before_count = len(world.ground_mesh(cx0, cz0))
    before_floor = len([v for v in world.ground_mesh(cx0, cz0) if abs(v[1] - (y0 + 1.0)) < 0.2])
    for dx in range(-2, 3):
        for dz in range(-2, 3):
            world.set_block(x0 + dx, y0, z0 + dz, VoxelGen.STONE)
            for dy in range(1, 4):
                world.set_block(x0 + dx, y0 + dy, z0 + dz, VoxelGen.AIR)
    world.set_block(x0, y0 + 1, z0, VoxelGen.WATER_VOLUME_FULL)
    cx, cz = x0 // VoxelGen.CHUNK, z0 // VoxelGen.CHUNK
    # Mesh positions are relative to the chunk's corner, so the centre is too.
    centre = (x0 + 0.5 - cx * VoxelGen.CHUNK, y0 + 1.0, z0 + 0.5 - cz * VoxelGen.CHUNK)
    water = [v for v in world.ground_mesh(cx, cz, VoxelGen.WATER)
             if sum((v[i] - centre[i]) ** 2 for i in range(3)) < 4.0]
    ground = [v for v in world.ground_mesh(cx, cz)
              if sum((v[i] - centre[i]) ** 2 for i in range(3)) < 4.0]
    result[name + "_water_vertices"] = [[round(c, 3) for c in v[:11]] for v in water]
    result[name] = {"chunk": [cx, cz], "before_count": before_count, "before_floor": before_floor, "after_floor": len([v for v in world.ground_mesh(cx, cz) if abs(v[1] - (y0 + 1.0)) < 0.2]),
                    "water_vertices_near": len(water), "ground_vertices_near": len(ground),
                    "ground_in_chunk": len(world.ground_mesh(cx, cz)), "nearest_ground": round(min([sum((v[i] - centre[i]) ** 2 for i in range(3)) for v in world.ground_mesh(cx, cz)] or [-1.0]) ** 0.5, 3),
                    "floor_block": world.get_block(x0, y0, z0), "air_above": world.get_block(x0, y0 + 1, z0),
                    "water_top_y": round(max([v[1] for v in water] or [0.0]), 3),
                    "water_lowest_y": round(min([v[1] for v in water] or [0.0]), 3)}


# A near spot (about one chunk away) and a far one (about five chunks away), both on flat stone.
near_x, near_z = (px // 16 + 1) * 16 + 4, px + 0
far_x, far_z = (px // 16 + 5) * 16 + 4, pz
place("near", near_x, near_z, 60)
place("far", far_x, far_z, 60)
with open("cg_control/lod_probe.json", "w") as f:
    json.dump(result, f, indent=1)
print("LOD PROBE", json.dumps(result))






