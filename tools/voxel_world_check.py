"""
Checks the voxel world from inside the engine: the caves the generator carves, and the block edits (saved, loaded,
kept across an unload). test_voxels.ps1 runs it through the control channel (run_script) and reads its result.

It makes its own cge.VoxelWorld, so it does not touch the sample game's world. The result goes to
cg_control/voxel_check.json as {"check": {name: bool}, "caveAir": n, "underground": n}.
"""
import json
import math
import os
import time

import cg_engine_bindings as cge

SIZE = 16
HEIGHT = 64
BLOCK_CHECK = 7   # wood: an id that the terrain never makes next to the edited block
VOXEL_WOOD = 7
VOXEL_LEAVES = 8
VOXEL_STONE = 3
VOXEL_LAMP = 17   # VoxelGen.LAMP


def load_chunks(world, chunks, timeout=60.0):
    for chunk in chunks:
        world.request_chunk(*chunk)
    deadline = time.time() + timeout
    while time.time() < deadline and not all(world.is_loaded(*c) for c in chunks):
        world.update(5.0, 0, 0)
        time.sleep(0.005)
    return all(world.is_loaded(*c) for c in chunks)


def wait_meshed(world, chunks, timeout=60.0):
    """True once every chunk has a mesh that is up to date (none dirty or meshing)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        world.update(5.0, 0, 0)
        stats = world.stats()
        if stats["dirty"] == 0 and stats["meshing"] == 0 and stats["loaded"] == len(chunks):
            return True
        time.sleep(0.005)
    return False


SAND = 4
GRASS = 1
DIRT = 2
STONE = 3
WATER = 5
WOOD_ID = 7
FLOW_IDS = range(9, 16)


def run_simulation(world, seconds):
    """Advance the block physics by the given seconds, a frame at a time."""
    for _ in range(int(seconds / 0.05)):
        world.simulate(0.05, 20.0)


def liquid_count(world, x0, x1, z0, z1):
    liquids = set([WATER]) | set(FLOW_IDS)
    return sum(1 for x in range(x0, x1) for z in range(z0, z1) for y in range(HEIGHT)
               if world.get_block(x, y, z) in liquids)


def surface_y(world, x, z):
    """The height of the highest solid block in the column, or -1."""
    for y in range(HEIGHT - 1, -1, -1):
        if world.is_solid(x, y, z):
            return y
    return -1


def check_body_parity(world, checks):
    """The engine's body mover (step_body) and resources/scripts/VoxelPhysics.step agree on random moves."""
    import random
    import VoxelPhysics as physics
    rng = random.Random(7)
    mismatches = 0
    for _ in range(300):
        pos = (rng.uniform(-6, 6), rng.uniform(12, 60), rng.uniform(-6, 6))
        vel = (rng.uniform(-6, 6), rng.uniform(-30, 10), rng.uniform(-6, 6))
        dt = rng.uniform(0.005, 0.05)
        flying = rng.random() < 0.2
        on_ground = rng.random() < 0.5
        py_pos, py_vel, py_ground = physics.step(world.is_solid, pos, vel, dt, flying, on_ground)
        cpp_pos, cpp_vel, cpp_ground = world.step_body(pos, vel, dt, flying, on_ground=on_ground)
        gap = max(abs(a - b) for a, b in zip(py_pos + py_vel, cpp_pos + cpp_vel))
        if gap > 1e-9 or py_ground != cpp_ground:
            mismatches += 1
    checks["body physics matches the Python version"] = mismatches == 0


def check_detail(checks):
    """The smooth skin is finer near the focus: the chunks within one of it have twice the samples per block, so more quads."""
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.set_terrain(seed=5, sea_level=14, base_height=8, height_range=44, snow_level=40,
                      tree_chance=0.01, cave_width=0.05)
    world.set_smoothing(1)
    world.set_detail(1, 2)
    chunks = [(x, z) for x in range(-3, 3) for z in range(-3, 3)]
    for chunk in chunks:
        world.request_chunk(*chunk)
    deadline = time.time() + 120.0
    while time.time() < deadline:
        world.update(5.0, 0, 0)
        stats = world.stats()
        if stats["loaded"] == len(chunks) and stats["dirty"] == 0 and stats["meshing"] == 0:
            break
        time.sleep(0.005)
    near = [world.chunk_quads(x, z) for x in (-1, 0, 1) for z in (-1, 0, 1)]
    far = [world.chunk_quads(-3, -3), world.chunk_quads(2, 2)]
    checks["near chunks have finer skins than far ones (near %d, far %d quads)" % (
        sum(near) // len(near), sum(far) // len(far))] = (
        all(q > 0 for q in near + far) and sum(near) / len(near) > 1.5 * sum(far) / len(far))
    world.clear()


def check_motion(world, checks):
    """A falling block takes time: it is in flight for a while, then lands in the world."""
    world.set_move_seconds(0.4)
    world.set_block(-10, HEIGHT - 2, 10, SAND)
    run_simulation(world, 0.3)
    moving = [move for move in world.moving_blocks() if move[0] == SAND]
    checks["sand is in flight while it falls"] = len(moving) == 1
    if moving:
        _, src, dst, progress = moving[0]
        checks["the cell it leaves is empty, and the move is part way"] = (
            world.get_block(src[0], src[1], src[2]) == 0 and 0.0 <= progress < 1.0 and dst[1] == src[1] - 1)
    run_simulation(world, 3.0)
    checks["the sand lands in the world"] = world.get_block(-10, HEIGHT - 2, 10) == 0 and any(
        world.get_block(-10, y, 10) == SAND for y in range(HEIGHT))
    world.settle_moves()
    checks["settling leaves nothing in flight"] = len(world.moving_blocks()) == 0
    world.set_move_seconds(0.0)


def check_physics(world, checks):
    # Sand falls from the top of its column onto the first solid block.
    world.set_block(5, HEIGHT - 2, 5, SAND)
    run_simulation(world, 4.0)
    sand = [y for y in range(HEIGHT) if world.get_block(5, y, 5) == SAND]
    checks["sand falls to the ground"] = (len(sand) == 1 and sand[0] < 40 and
                                          (sand[0] == 0 or world.is_solid(5, sand[0] - 1, 5)))

    # Water poured from a source falls, then spreads and stops.
    world.set_block(7, HEIGHT - 2, 7, WATER)
    run_simulation(world, 4.0)
    after_four = liquid_count(world, 5, 10, 5, 10)
    checks["water falls and spreads"] = after_four > 5
    run_simulation(world, 10.0)
    checks["water settles"] = liquid_count(world, 5, 10, 5, 10) <= after_four + 2

    # A reaction rule: grass with water beside it becomes dirt.
    world.add_rule(GRASS, WATER, DIRT, 1.0)
    world.set_block(-6, 62, -6, GRASS)
    world.set_block(-6, 63, -6, WATER)
    run_simulation(world, 1.0)
    checks["rule turns grass to dirt"] = world.get_block(-6, 62, -6) == DIRT
    world.clear_rules()

    # The hook replaces a stone it is shown.
    world.set_tick_hook(lambda x, y, z, block: WOOD_ID if (x, y, z) == (-9, 62, -9) and block == STONE else None)
    world.set_block(-9, 62, -9, STONE)
    run_simulation(world, 0.5)
    checks["hook replaces a block"] = world.get_block(-9, 62, -9) == WOOD_ID
    world.set_tick_hook(None)

    # With everything settled, nothing is queued: a quiet world costs nothing.
    run_simulation(world, 2.0)
    checks["quiet world queues nothing"] = world.active_count() == 0


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


def check_light(world, checks, chunks, results):
    """Sky light: open ground is full, and the air far under the surface is darker, but never fully dark."""
    lo = min(cx for cx, _ in chunks) * SIZE + 2
    hi = (max(cx for cx, _ in chunks) + 1) * SIZE - 2
    tops, deep = [], []
    for x in range(lo, hi, 4):
        for z in range(lo, hi, 4):
            top = None
            for y in range(HEIGHT - 2, 0, -1):
                if world.get_block(x, y, z) != 0 and world.get_block(x, y + 1, z) == 0:
                    top = y + 1
                    break
            if top is None:
                continue
            tops.append(world.light_at(x, top, z))
            for y in range(top - 10, 1, -3):
                if world.get_block(x, y, z) == 0:
                    deep.append(world.light_at(x, y, z))
    results["light"] = {"surfaceSamples": len(tops), "deepSamples": len(deep),
                        "deepMean": round(sum(deep) / max(1, len(deep)), 1)}
    checks["open ground is full light"] = bool(tops) and all(v == 255 for v in tops)
    checks["air far underground is darker than open ground"] = bool(deep) and sum(deep) / len(deep) < 0.5 * 255
    checks["air is never fully dark"] = bool(deep) and min(deep) >= 50


def check_walkers(world, checks, results):
    """Creatures stand on the ground, walk about, and never end up inside a block. The engine moves them through
    cge.step_voxel_walker here, since this check runs inside a frame and no frames pass while it works."""
    import VoxelWalkers
    herd = VoxelWalkers.Walkers(world, (0.0, 0.0), 4, 5, prefix="check_creature")
    walkers = herd.walkers
    checks["walkers spawn on the ground"] = len(walkers) == 4 and all(w.state()[2] is False or w.state()[0][1] >= 1.0 for w in walkers)
    spawns = [list(w.state()[0]) for w in walkers]
    dt = 1.0 / 60.0
    for _ in range(240):   # 4 seconds at 60 frames a second
        herd.update(dt)
        for w in walkers:
            cge.step_voxel_walker(w.name, dt)

    import VoxelPhysics as physics
    stands = True
    inside = False
    moved = 0.0
    for w, spawn in zip(walkers, spawns):
        pos, vel, on_ground = w.state()
        x, z = pos[0], pos[2]
        # A creature on the ground stands on the skin: it is outside the skin, and nothing moves it on.
        results.setdefault("walkers", []).append({"pos": [round(c, 3) for c in pos], "vel": [round(c, 3) for c in vel],
                                                 "onGround": on_ground, "inside": physics.inside(world.is_solid, pos)})
        if not on_ground and abs(vel[1]) < 1e-6:
            stands = False
        if on_ground and physics.inside(world.is_solid, pos):
            stands = False
        for y in (pos[1] + 0.1, pos[1] + VoxelWalkers.HEIGHT - 0.1):
            if world.is_solid(int(x // 1), int(y // 1), int(z // 1)):
                inside = True
        moved = max(moved, ((x - spawn[0]) ** 2 + (z - spawn[2]) ** 2) ** 0.5)
    checks["walkers stand on the skin after 4 s"] = stands
    checks["walkers never end up inside a block"] = not inside
    checks["walkers wander"] = moved > 0.5
    for w in walkers:
        cge.remove_voxel_walker(w.name)
        cge.remove_mesh_body(w.name)


def check_skin_surface(world, checks):
    """The surface the view meets is the drawn skin: looking straight down, it lies about 0.2 of a block above the top block of
    the column (the skin sits out from each face), and the outline's block is the one under it."""
    good = 0
    total = 0
    for x in range(-20, 20, 5):
        for z in range(-20, 20, 5):
            top = max([y for y in range(HEIGHT - 1) if world.is_solid(x, y, z)] or [-1]) + 1
            if top <= 0:
                continue
            surface = world.skin_surface((x + 0.5, top + 20.0, z + 0.5), (0.0, -1.0, 0.0), 40.0)
            total += 1
            if surface is not None and abs(surface[1] - (top + 0.2)) < 0.25:
                good += 1
    checks["the view meets the drawn skin (about 0.2 of a block above the top block)"] = total > 0 and good >= 0.9 * total


def check_lamp(world, checks):
    """A light block lights the air round it: full at its cell, and 0.8 less for each block walked through air. Tested in a
    sealed pocket of stone, which the sky cannot reach, so the air is at the sky floor (0.2) until the lamp is placed. The
    pocket is put back afterwards, so the checks after it see the same world."""
    world.define_block(VOXEL_LAMP, cge.Vector3f(1.0, 0.8, 0.42), solid=True, emits=255)
    cx, cy, cz = 4, 20, 4
    saved = {}
    for x in range(cx - 2, cx + 3):
        for y in range(cy - 2, cy + 3):
            for z in range(cz - 2, cz + 3):
                saved[(x, y, z)] = world.get_block(x, y, z)
                inside = max(abs(x - cx), abs(y - cy), abs(z - cz)) <= 1
                world.set_block(x, y, z, 0 if inside else VOXEL_STONE)
    dark = world.light_at(cx, cy, cz)
    world.set_block(cx, cy, cz, VOXEL_LAMP)
    near = world.light_at(cx + 1, cy, cz)            # one block through air: 0.8
    far = world.light_at(cx + 1, cy + 1, cz + 1)     # three blocks through air: 0.8 cubed, about 0.5
    checks["a sealed pocket is at the sky floor before the lamp"] = abs(dark - 51) <= 2
    checks["a lamp is a block, and it is not walkable air"] = world.get_block(cx, cy, cz) == VOXEL_LAMP and world.is_solid(cx, cy, cz)
    checks["one block from a lamp is 0.8 of full light"] = abs(near - 204) <= 2
    checks["three blocks from a lamp, through air, is 0.8 cubed"] = abs(far - 131) <= 2
    for (x, y, z), block in saved.items():
        world.set_block(x, y, z, block)
    # Putting the pocket back is recorded as edits too; forget them, so the edit checks below see only their own.
    world.clear_edits()


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
    check_light(world, results["check"], chunks, results)
    check_walkers(world, results["check"], results)
    check_skin_surface(world, results["check"])
    check_lamp(world, results["check"])
    biomes ={world.biome_at(x, z) for x in range(-200, 200, 10) for z in range(-200, 200, 10)}
    results["check"]["biomes vary: plains, forest and desert occur"] = biomes == {"plains", "forest", "desert"}
    results["biomes"] = sorted(biomes)

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

    # The smooth skin meshes the same chunks: every loaded chunk gets a mesh, and switching back to cubes works too.
    world.set_smoothing(1)
    results["check"]["smooth skin meshes"] = wait_meshed(world, chunks)
    results["check"]["smooth skin has quads"] = world.stats()["quads"] > 0
    world.set_smoothing(0)
    results["check"]["cubes again"] = wait_meshed(world, chunks) and world.stats()["quads"] > 0

    # The block physics: falling sand, flowing water, a rule and a hook.
    check_body_parity(world, results["check"])
    check_physics(world, results["check"])
    check_motion(world, results["check"])
    check_detail(results["check"])

    results["ok"] = all(results["check"].values())
    tmp = out_path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(results, f, indent=1)
    os.replace(tmp, out_path)
    world.clear()
    return results


# run_script executes this file inside the engine, so the check runs when the file is loaded.
run()
