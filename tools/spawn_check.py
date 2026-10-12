"""
Checks that the player lands when the game starts, for many worlds: a fresh world from each seed, the chunks around the
spawn loaded, and the body dropped from the spawn height with the same mover and steps as the game. It reports every
seed whose body does not land on the ground near the surface. Run inside the engine (run_script, as test_voxels.ps1 does).
Writes cg_control/spawn_check.json.
"""
import json
import os
import time

import cg_engine_bindings as cge

CHUNK = 16
HEIGHT = 64
SPAWN = (8.5, 60.0, 8.5)
DT = 0.05   # the game caps its frame step at 0.05 s; a slow start hits that cap
SECONDS = 12.0
SEEDS = list(range(1, 25))


def load(world, chunks, timeout=60.0):
    for chunk in chunks:
        world.request_chunk(*chunk)
    deadline = time.time() + timeout
    while time.time() < deadline and not all(world.is_loaded(*c) for c in chunks):
        world.update(5.0, 0, 0)
        time.sleep(0.005)
    return all(world.is_loaded(*c) for c in chunks)


def drop(world):
    pos = SPAWN
    vel = (0.0, 0.0, 0.0)
    on_ground = False
    lowest = pos[1]
    for _ in range(int(SECONDS / DT)):
        pos, vel, on_ground = world.step_body(pos, vel, DT, False)
        lowest = min(lowest, pos[1])
        if on_ground:
            break
    return pos, on_ground, lowest


def run(out_path="cg_control/spawn_check.json"):
    results = {"seeds": {}, "failed": []}
    chunks = [(x, z) for x in range(-2, 2) for z in range(-2, 2)]
    for seed in SEEDS:
        world = cge.VoxelWorld(CHUNK, HEIGHT, "voxel")
        world.set_terrain(seed=seed, sea_level=14, base_height=8, height_range=44, snow_level=40,
                          tree_chance=0.01, cave_width=0.05)
        if not load(world, chunks):
            results["seeds"][str(seed)] = "chunks did not load"
            results["failed"].append(seed)
            world.clear()
            continue
        pos, on_ground, lowest = drop(world)
        results["seeds"][str(seed)] = {"landed": on_ground, "y": round(pos[1], 3), "lowest": round(lowest, 3)}
        if not on_ground or pos[1] < 5.0:
            results["failed"].append(seed)
        world.clear()
    results["ok"] = not results["failed"]
    tmp = out_path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(results, f, indent=1)
    os.replace(tmp, out_path)
    return results


# run_script executes this file inside the engine, so the check runs when the file is loaded.
run()
