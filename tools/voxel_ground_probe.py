"""
Measures whether placing water on a grass top moves the ground's mesh, inside the running voxel sample (its own block definitions,
not the check world's). It finds a grass top near the player with air above, takes the ground's vertices near it, places water
there (a spring, then a full volume), takes them again, and writes the vertices that moved to cg_control/ground_probe.json.
Run through the control channel by tools/voxel_ground_probe.ps1.
"""
import gc
import json
import math

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))

spot = None
for radius in range(2, 30):
    for dx in range(-radius, radius + 1):
        for dz in range(-radius, radius + 1):
            if max(abs(dx), abs(dz)) != radius or spot is not None:
                continue
            x, z = px + dx, pz + dz
            top = next((y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(x, y, z)), None)
            if top is not None and world.get_block(x, top, z) == VoxelGen.GRASS and world.get_block(x, top + 1, z) == VoxelGen.AIR:
                spot = (x, top, z)

result = {"ok": spot is not None}
if spot is not None:
    x, y, z = spot
    cx, cz = x // VoxelGen.CHUNK, z // VoxelGen.CHUNK
    centre = (x + 0.5, y + 1.0, z + 0.5)

    def near(vertices):
        # The ground's vertices within 1.5 blocks of the grass top's centre, by their position and attributes.
        return [v for v in vertices if sum((v[i] - centre[i]) ** 2 for i in range(3)) < 1.5 * 1.5]

    before = near(world.ground_mesh(cx, cz))
    results = {}
    for name, block in (("spring", VoxelGen.WATER), ("volume", VoxelGen.WATER_VOLUME_FULL)):
        world.set_block(x, y + 1, z, block)
        after = near(world.ground_mesh(cx, cz))
        world.set_block(x, y + 1, z, VoxelGen.AIR)
        moved = []
        for a, b in zip(before, after):
            dy = b[1] - a[1]
            if abs(dy) > 1e-4 or any(abs(a[i] - b[i]) > 1e-4 for i in range(3, 7)):
                moved.append({"at": a[:3], "dy": round(dy, 4), "colour_before": a[3:6], "colour_after": b[3:6]})
        # The water's own mesh over the grass: how far its surface dips below the grass's top face, which is where the grass ends.
        world.set_block(x, y + 1, z, block)
        water = near(world.ground_mesh(cx, cz, VoxelGen.WATER))
        world.set_block(x, y + 1, z, VoxelGen.AIR)
        top = y + 1.0
        # Over the grass block's own footprint only: the water's sides beside a drop legitimately run down to the lower ground.
        over = [v for v in water if x <= v[0] <= x + 1 and z <= v[2] <= z + 1]
        lowest = min([v[1] for v in over] or [top])
        results[name] = {"vertices": len(after), "same_count": len(after) == len(before), "moved": len(moved),
                         "max_rise": round(max([m["dy"] for m in moved] or [0.0]), 4),
                         "examples": moved[:4],
                         "water_vertices": len(over), "water_lowest_y": round(lowest, 4), "grass_top_y": top,
                         "water_dips_into_grass": round(top - lowest, 4)}
    result.update({"spot": list(spot), "vertices_near": len(before), "results": results})

with open("cg_control/ground_probe.json", "w") as f:
    json.dump(result, f, indent=1)
print("GROUND PROBE", json.dumps(result)[:300])
