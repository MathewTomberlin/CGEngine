"""
Times the connection check (VoxelWorld.check_support, roadmap 39.1) in the running voxel sample, on a block that was just broken
beside the spawn, and on a detached slab of 2048 blocks built in the air above the spawn. Writes cg_control/support_probe.json with
the verdicts, the blocks each fill took in, and the milliseconds per call. Run through the control channel by
tools/voxel_support_probe.ps1.
"""
import gc
import json
import math
import time

import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
results = {}


def timed(fn, repeats=5):
    best = None
    out = None
    for _ in range(repeats):
        t0 = time.perf_counter()
        out = fn()
        ms = (time.perf_counter() - t0) * 1000.0
        best = ms if best is None else min(best, ms)
    return out, best


# 1. A block on the ground: break it, check, and put it back.
ground = next(y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(px + 2, y, pz))
bx, by, bz = px + 2, ground, pz
original = world.get_block(bx, by, bz)
world.set_block(bx, by, bz, VoxelGen.AIR)
(status, visited), ms = timed(lambda: world.check_support(bx, by, bz, 16384))
world.set_block(bx, by, bz, original)
results["ground_block"] = {"status": status, "visited": visited, "ms": ms}

# 2. A slab of 16 by 16 by 8 blocks, 56 to 63 up, in the air above the spawn; break one block under it, and check.
x0, z0, y0 = px - 8, pz - 8, 56
for x in range(x0, x0 + 16):
    for z in range(z0, z0 + 16):
        for y in range(y0, y0 + 8):
            world.set_block(x, y, z, VoxelGen.STONE)
sx, sz = x0 + 8, z0 + 8
world.set_block(sx, y0, sz, VoxelGen.AIR)
(status, visited), ms = timed(lambda: world.check_support(sx, y0, sz, 16384))
results["slab_2048"] = {"status": status, "visited": visited, "ms": ms}
(status, visited), ms = timed(lambda: world.check_support(sx, y0, sz, 500))
results["slab_capped_500"] = {"status": status, "visited": visited, "ms": ms}

with open("cg_control/support_probe.json", "w") as f:
    json.dump(results, f)
