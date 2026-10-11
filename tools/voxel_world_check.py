"""
Checks the voxel world from inside the engine: the caves the generator carves, and the block edits (saved, loaded,
kept across an unload). test_voxels.ps1 runs it through the control channel (run_script) and reads its result.

It makes its own cge.VoxelWorld, so it does not touch the sample game's world. The result goes to
cg_control/voxel_check.json as {"check": {name: bool}, "caveAir": n, "underground": n}.
"""
import json
import os
import time

import cg_engine_bindings as cge

SIZE = 16
HEIGHT = 64
BLOCK_CHECK = 7   # wood: an id that the terrain never makes next to the edited block
VOXEL_WOOD = 7
VOXEL_LEAVES = 8


def load_chunks(world, chunks, timeout=60.0):
    for chunk in chunks:
        world.request_chunk(*chunk)
    deadline = time.time() + timeout
    while time.time() < deadline and not all(world.is_loaded(*c) for c in chunks):
        world.update(5.0, 0, 0)
        time.sleep(0.005)
    return all(world.is_loaded(*c) for c in chunks)


def count_caves(world, chunks):
    """Air below the surface, column by column: air between y = 2 and the top solid block minus 3."""
    air = 0
    underground = 0
    for cx, cz in chunks:
        for lx in range(SIZE):
            for lz in range(SIZE):
                x = cx * SIZE + lx
                z = cz * SIZE + lz
                top = HEIGHT - 1
                while top > 0 and world.get_block(x, top, z) == 0:
                    top -= 1
                for y in range(2, top - 3):
                    underground += 1
                    if world.get_block(x, y, z) == 0:
                        air += 1
    return air, underground


def count_floating_leaves(world, chunks):
    """Leaves with no wood under them in their column, away from the loaded edge (a tree's trunk is in its own chunk, so
    a leaf next to the edge may have its trunk outside the loaded area)."""
    low = min(cx for cx, _ in chunks) * SIZE + 4
    high = (max(cx for cx, _ in chunks) + 1) * SIZE - 4
    floating = []
    for x in range(low, high):
        for z in range(low, high):
            for y in range(1, HEIGHT - 1):
                if world.get_block(x, y, z) != VOXEL_LEAVES:
                    continue
                # A leaf belongs to a trunk within 2 blocks sideways and up to 3 blocks below it.
                supported = any(world.get_block(x + dx, y - k, z + dz) == VOXEL_WOOD
                                for dx in range(-2, 3) for dz in range(-2, 3) for k in range(0, 4))
                if not supported:
                    floating.append((x, y, z))
    return floating


def run(out_path="cg_control/voxel_check.json"):
    results = {"check": {}}
    chunks = [(x, z) for x in range(-1, 1) for z in range(-1, 1)]
    edit_file = "cg_control/voxel_check_edits.bin"
    os.makedirs("cg_control", exist_ok=True)

    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.set_terrain(seed=3, sea_level=14, base_height=8, height_range=44, snow_level=40,
                      tree_chance=0.01, cave_width=0.05)
    results["check"]["loads"] = load_chunks(world, chunks)
    results["floatingLeaves"] = count_floating_leaves(world, chunks)[:20]
    results["check"]["no floating leaves"] = len(results["floatingLeaves"]) == 0
    air, underground = count_caves(world, chunks)
    results["caveAir"] = air
    results["underground"] = underground
    results["check"]["caves carved"] = air > underground * 0.01

    # An edit survives an unload of its chunk, and a save and load round trip.
    world.set_block(3, 20, 3, BLOCK_CHECK)
    results["check"]["edit reads back"] = world.get_block(3, 20, 3) == BLOCK_CHECK
    world.unload_chunk(0, 0)
    results["check"]["edit gone while unloaded"] = world.get_block(3, 20, 3) == 0
    load_chunks(world, [(0, 0)])
    results["check"]["edit back after reload"] = world.get_block(3, 20, 3) == BLOCK_CHECK
    results["check"]["save writes"] = world.save_edits(edit_file)
    world.clear_edits()
    results["check"]["clear_edits empties"] = world.edit_count() == 0
    results["check"]["load reads"] = world.load_edits(edit_file)
    results["check"]["edit count after load"] = world.edit_count() == 1
    results["check"]["loaded edit applies at once"] = world.get_block(3, 20, 3) == BLOCK_CHECK

    results["ok"] = all(results["check"].values())
    tmp = out_path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(results, f, indent=1)
    os.replace(tmp, out_path)
    world.clear()
    return results


# run_script executes this file inside the engine, so the check runs when the file is loaded.
run()
