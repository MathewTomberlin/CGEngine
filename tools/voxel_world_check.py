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
VOXEL_BRICK = 18  # VoxelGen.BRICK
VOXEL_COAL = 20   # VoxelTerrain::Coal
VOXEL_IRON = 21   # VoxelTerrain::Iron
VOXEL_LAVA = 22   # VoxelTerrain::Lava: a producing lava source (it makes volume)
VOXEL_ENDLESS_LAVA = 32   # the endless lava source: the cave lava (VoxelTerrain::EndlessLava), which makes flows
VOXEL_ENDLESS_WATER = 31   # the endless water source (its flows spread as the old springs' did)


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
    liquids = set([WATER, VOXEL_ENDLESS_WATER]) | set(FLOW_IDS)
    return sum(1 for x in range(x0, x1) for z in range(z0, z1) for y in range(HEIGHT)
               if world.get_block(x, y, z) in liquids)


def surface_y(world, x, z):
    """The height of the highest solid block in the column, or -1."""
    for y in range(HEIGHT - 1, -1, -1):
        if world.is_solid(x, y, z):
            return y
    return -1


def body_parity(world, seed, count, lo, hi):
    """How many of count random moves, from positions in the box lo..hi, differ between the engine's mover (step_body) and
    resources/scripts/VoxelPhysics.step, which reads the same skins (skin_at)."""
    import random
    import VoxelPhysics as physics
    rng = random.Random(seed)
    mismatches = 0
    for _ in range(count):
        pos = tuple(rng.uniform(lo[axis], hi[axis]) for axis in range(3))
        vel = (rng.uniform(-6, 6), rng.uniform(-30, 10), rng.uniform(-6, 6))
        dt = rng.uniform(0.005, 0.05)
        flying = rng.random() < 0.2
        on_ground = rng.random() < 0.5
        py_pos, py_vel, py_ground = physics.step(world.skin_at, pos, vel, dt, flying, on_ground)
        cpp_pos, cpp_vel, cpp_ground = world.step_body(pos, vel, dt, flying, on_ground=on_ground)
        gap = max(abs(a - b) for a, b in zip(py_pos + py_vel, cpp_pos + cpp_vel))
        if gap > 1e-9 or py_ground != cpp_ground:
            mismatches += 1
    return mismatches


def check_body_parity(world, checks):
    """The engine's body mover (step_body) and resources/scripts/VoxelPhysics.step agree on random moves."""
    checks["body physics matches the Python version"] = body_parity(world, 7, 300, (-6, 12, -6), (6, 60, 6)) == 0


def check_groups(checks):
    """Blocks in their own meshes (BlockType::own) and cube blocks (BlockType::cube). A leaf is in its own mesh and not the
    terrain's, a brick is six cube faces, and ground is in the terrain's mesh. Collision sees each skin: a body lands on a
    brick at its drawn top, and the engine's mover agrees with the Python physics on a mixed patch of blocks."""
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    # Wood joins the leaves above it and the natural ground under it (stone 3, dirt 2, grass 1, sand 4, gravel 16, snow 6).
    world.define_block(VOXEL_WOOD, cge.Vector3f(0.44, 0.29, 0.15), group="own", join_above=VOXEL_LEAVES,
                       join_below=[3, 2, 1, 4, 16, 6])
    world.define_block(VOXEL_LEAVES, cge.Vector3f(0.20, 0.50, 0.16), group="own", material="voxel_leaves")
    world.define_block(VOXEL_BRICK, cge.Vector3f(0.66, 0.30, 0.22), shape="cube", group="own")
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)

    world.set_block(8, 8, 8, VOXEL_LEAVES)
    wait_meshed(world, chunk)
    groups = world.chunk_group_quads(0, 0)
    checks["a leaf alone is in its own mesh, not the terrain's"] = set(groups) == {VOXEL_LEAVES} and groups[VOXEL_LEAVES] > 0
    checks["a leaf is a skin of its own to collision"] = world.skin_at(8, 8, 8) == 256 + VOXEL_LEAVES

    world.set_block(8, 8, 8, VOXEL_BRICK)
    wait_meshed(world, chunk)
    checks["a brick alone is six cube faces in its own mesh"] = world.chunk_group_quads(0, 0) == {VOXEL_BRICK: 6}
    checks["a brick is a cube skin to collision"] = world.skin_at(8, 8, 8) == 512 + VOXEL_BRICK
    world.set_block(8, 8, 8, 0)

    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    wait_meshed(world, chunk)
    checks["ground is in the terrain mesh alone"] = set(world.chunk_group_quads(0, 0)) == {0} and world.chunk_group_quads(0, 0)[0] > 0
    checks["stone is the terrain skin to collision"] = world.skin_at(8, 5, 8) == 1 and world.skin_at(8, 6, 8) == 0

    world.set_block(8, 6, 8, VOXEL_BRICK)
    world.set_block(5, 6, 5, VOXEL_WOOD)
    world.set_block(10, 6, 10, VOXEL_LEAVES)
    world.set_block(6, 6, 9, VOXEL_BRICK)
    wait_meshed(world, chunk)
    groups = world.chunk_group_quads(0, 0)
    checks["on the ground, the terrain and each own mesh are separate"] = (
        set(groups) == {0, VOXEL_LEAVES, VOXEL_BRICK} and groups[VOXEL_BRICK] == 12)
    checks["wood standing on the ground joins the terrain's skin"] = world.skin_at(5, 6, 5) == 1

    # A body falls onto the brick and stands on its drawn top face, at y = 7 (the cell's top).
    state = ((8.5, 12.0, 8.5), (0.0, 0.0, 0.0), False)
    for _ in range(600):
        state = world.step_body(state[0], state[1], 1.0 / 60.0, False, on_ground=state[2])
        if state[2]:
            break
    checks["a body lands on a brick at its drawn top face"] = state[2] and abs(state[0][1] - 7.0) < 0.01

    checks["the engine's mover matches the Python physics on a mixed patch"] = body_parity(world, 11, 300, (4, 5, 4), (12, 9, 12)) == 0

    # Wood joins the leaves above it: a trunk top under a canopy belongs to the canopy's skin. Wood with air above, and wood
    # beside leaves, keep their own skin.
    world.set_block(8, 8, 8, VOXEL_WOOD)
    world.set_block(8, 9, 8, VOXEL_LEAVES)
    world.set_block(3, 8, 3, VOXEL_WOOD)
    world.set_block(7, 8, 12, VOXEL_WOOD)
    world.set_block(8, 8, 12, VOXEL_LEAVES)
    wait_meshed(world, chunk)
    checks["wood under leaves joins the leaves' skin"] = world.skin_at(8, 8, 8) == 256 + VOXEL_LEAVES
    checks["wood with air above keeps its own skin"] = world.skin_at(3, 8, 3) == 256 + VOXEL_WOOD
    checks["wood beside leaves keeps its own skin"] = world.skin_at(7, 8, 12) == 256 + VOXEL_WOOD
    world.set_block(6, 7, 9, VOXEL_WOOD)
    checks["wood standing on a brick keeps its own skin"] = world.skin_at(6, 7, 9) == 256 + VOXEL_WOOD
    groups = world.chunk_group_quads(0, 0)
    checks["a joined trunk top is drawn with the leaves' mesh"] = groups.get(VOXEL_LEAVES, 0) > 0 and groups.get(VOXEL_WOOD, 0) > 0
    world.clear()


def trunk_widths(join_below):
    """The drawn half-width of a trunk one block wide on stone, at heights just above the ground: the first skin crossing from 3
    blocks out (skin_surface). join_below is the terrain blocks a trunk base joins, or None for the sample's rule."""
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    if join_below is None:
        world.define_block(VOXEL_WOOD, cge.Vector3f(0.44, 0.29, 0.15), group="own")
    else:
        world.define_block(VOXEL_WOOD, cge.Vector3f(0.44, 0.29, 0.15), group="own", join_below=join_below)
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    for y in range(6, 11):
        world.set_block(6, y, 6, VOXEL_WOOD)
    widths = []
    for y in (6.25, 6.5, 6.75, 7.0, 7.25, 7.5):
        # The ray starts in loaded air: a cell of an unloaded chunk counts as solid, so the trunk stays away from the chunk's edge.
        surface = world.skin_surface((9.5, y, 6.5), (-1.0, 0.0, 0.0), 3.0)
        widths.append(None if surface is None else surface[0] - 6.5)
    world.clear()
    return widths


def check_trunk_base(results):
    """A trunk one block wide on stone, with its own skin and no join below (VoxelGen.JOINS_BELOW is empty in the sample). The
    drawn trunk must not pinch where it leaves the ground: a base joined to the terrain left a neck of 0.8 blocks against a trunk
    of 1.4 (roadmap 27.1). The control is the joined trunk, which must show that neck, so the check is known to see it."""
    widths = trunk_widths(None)
    joined = trunk_widths([3, 2, 1, 4, 16, 6])
    results["trunkWidths"] = {"plain": widths, "joined": joined}
    checks = results["check"]
    checks["a trunk on stone has no neck above its base (half-width at least 0.55 block)"] = (
        all(w is not None and w >= 0.55 for w in widths))
    checks["control: a trunk joined to the stone pinches (the neck the check above guards against)"] = (
        any(w is None or w < 0.55 for w in joined))


def check_falling(results):
    """A detached piece (roadmap 39.1): breaking the block under a 2 by 2 slab that stands on a pillar drops the slab one cell, and it
    is drawn while it falls (moving_blocks); it lands on the pillar. A pillar that stays whole holds the slab."""
    checks = results["check"]
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    for y in range(6, 10):
        world.set_block(8, y, 8, VOXEL_STONE)
    for x in (8, 9):
        for z in (8, 9):
            for y in (11, 12):
                world.set_block(x, y, z, VOXEL_STONE)
    world.set_block(8, 10, 8, 0)   # the break: the slab is no longer held
    checks["a broken block leaves the slab as a falling piece of 8 blocks"] = (
        world.falling_pieces() == 1 and world.falling_cells() == 8)
    world.simulate(0.2, 20.0)
    checks["the falling piece is drawn in flight as it moves down a cell"] = len(world.moving_blocks()) == 8
    world.simulate(1.0, 20.0)
    checks["the slab lands on the pillar, one cell lower"] = (
        world.get_block(8, 10, 8) == VOXEL_STONE and world.get_block(9, 10, 9) == VOXEL_STONE
        and world.get_block(8, 12, 8) == 0 and world.falling_pieces() == 0)
    # Control: the pillar whole, and its top broken, so the blocks below still hold to the floor and nothing falls.
    world.clear()
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    for y in range(6, 13):
        world.set_block(8, y, 8, VOXEL_STONE)
    world.set_block(8, 12, 8, 0)   # the top of the pillar: what is below it still reaches the floor
    checks["a break that holds nothing up starts no piece"] = world.falling_pieces() == 0
    world.clear()


def check_far_feature(results):
    """A far skin (stride 2) samples every second block, so a one-block pillar at an odd block was missed (roadmap 25.4). The skin
    of a stride-2 chunk must show the pillar, as the full-detail skin does (the control)."""
    checks = results["check"]
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    for y in range(6, 10):
        world.set_block(5, y, 5, VOXEL_STONE)   # one block wide, at odd x and z
    def top_near_pillar(stride):
        verts = world.ground_mesh(0, 0, 0, stride)
        return max((v[1] for v in verts if 4.5 <= v[0] <= 6.5 and 4.5 <= v[2] <= 6.5), default=None)
    full = top_near_pillar(1)
    far = top_near_pillar(2)
    checks["control: the full-detail skin shows a one-block pillar at an odd block"] = full is not None and full > 8.0
    checks["a far skin (stride 2) shows a one-block pillar at an odd block"] = far is not None and far > 8.0
    world.clear()


def check_reactions(results):
    """Reactions (VoxelWorld.add_reaction): a lava block that water touches takes damage as it goes and becomes obsidian after its
    seconds; if the water leaves first, it heals and stays lava. Dirt beside grass becomes grass after its second, and air beside
    leaves grows leaves after its second. A reaction without time is a chance per tick, and is not checked here."""
    checks = results["check"]
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.define_block(VOXEL_LAVA, cge.Vector3f(1.0, 0.30, 0.05), solid=False, liquid=True, hits=4, emits=120,
                       group="own", material="voxel_lava")
    world.define_block(5, cge.Vector3f(0.22, 0.45, 0.92), solid=False, liquid=True, group="own", material="voxel_water",
                       transparent=True)
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    world.add_reaction(VOXEL_LAVA, [5], 30, 1.0, 4.0)
    world.add_reaction(2, [1], 1, 1.0, 1.0)
    world.add_reaction(0, [VOXEL_LEAVES], VOXEL_LEAVES, 1.0, 1.0)
    world.set_block(8, 6, 8, VOXEL_LAVA)
    world.set_block(9, 6, 8, 5)
    world.set_block(12, 6, 12, VOXEL_LAVA)
    world.set_block(13, 6, 12, 5)
    world.set_block(5, 5, 4, 2)
    world.set_block(4, 5, 4, 1)
    world.set_block(2, 7, 14, VOXEL_LEAVES)
    world.simulate(0.5, 20.0)   # t = 0.5
    checks["a dirt block beside grass is still dirt before its second"] = world.get_block(5, 5, 4) == 2
    checks["air beside leaves is still air before its second"] = world.get_block(2, 6, 14) == 0
    world.simulate(1.5, 20.0)   # t = 2.0
    checks["a lava block that water touches takes damage as it goes"] = (
        world.damage_at(8, 6, 8) >= 1 and world.get_block(8, 6, 8) == VOXEL_LAVA)
    checks["dirt beside grass becomes grass after its second"] = world.get_block(5, 5, 4) == 1
    checks["air beside leaves grows leaves after its second"] = world.get_block(2, 6, 14) == VOXEL_LEAVES
    world.set_block(13, 6, 12, 0)   # the water leaves the second lava block
    world.simulate(1.0, 20.0)   # t = 3.0
    checks["lava the water has left heals and stays lava"] = world.damage_at(12, 6, 12) == 0 and world.get_block(12, 6, 12) == VOXEL_LAVA
    world.simulate(1.5, 20.0)   # t = 4.5
    checks["a lava block that water touches becomes obsidian after its seconds"] = world.get_block(8, 6, 8) == 30
    world.clear()


def check_lava(results):
    """Lava is a liquid like water: it is not solid, it is drawn in its own mesh, and an endless lava source (32) spreads into a flow of
    lava (ids 23 to 29) beside it, as an endless water source does (the control). A producing lava source (22) makes volume instead.
    The deep caves hold endless lava at or below the lava level, and nowhere above it."""
    checks = results["check"]
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.define_block(VOXEL_LAVA, cge.Vector3f(1.0, 0.30, 0.05), solid=False, liquid=True, emits=120,
                       group="own", material="voxel_lava")
    world.define_block(VOXEL_ENDLESS_LAVA, cge.Vector3f(1.0, 0.30, 0.05), solid=False, liquid=True, emits=120, group="own",
                       mesh=VOXEL_LAVA)
    world.define_block(VOXEL_ENDLESS_WATER, cge.Vector3f(0.22, 0.45, 0.92), solid=False, liquid=True, group="own",
                       material="voxel_water", transparent=True)
    for flow in range(23, 30):   # the flowing lava: its levels, drawn in the lava's mesh
        world.define_block(flow, cge.Vector3f(1.0, 0.30, 0.05), solid=False, liquid=True, emits=120, group="own", mesh=VOXEL_LAVA)
    world.define_block(5, cge.Vector3f(0.22, 0.45, 0.92), solid=False, liquid=True, group="own", material="voxel_water",
                       transparent=True)
    world.set_smoothing(1)
    chunk = [(0, 0)]
    load_chunks(world, chunk)
    for x in range(SIZE):
        for z in range(SIZE):
            world.fill_column(x, z, 0, HEIGHT, 0)
            world.fill_column(x, z, 0, 6, VOXEL_STONE)
    world.set_block(8, 6, 8, VOXEL_ENDLESS_LAVA)
    world.set_block(3, 6, 3, VOXEL_ENDLESS_WATER)
    for _ in range(40):
        world.simulate(1.0, 20.0)
    checks["lava is not solid to collision"] = not world.is_solid(8, 6, 8)
    checks["lava spreads like water: the cell beside its source is flowing lava"] = 23 <= world.get_block(9, 6, 8) <= 29
    checks["lava stays a source where it was set"] = world.get_block(8, 6, 8) == VOXEL_ENDLESS_LAVA
    checks["control: water beside a source spreads"] = world.get_block(4, 6, 3) != 0
    wait_meshed(world, chunk)
    checks["lava is drawn in its own mesh"] = world.chunk_group_quads(0, 0).get(VOXEL_LAVA, 0) > 0
    world.clear()

    # The deep caves: lava fills the caves at or below height 6 (VoxelTerrain::LavaLevel), so none is higher.
    terrain = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    terrain.set_terrain(seed=5, sea_level=14, base_height=8, height_range=44, snow_level=40, tree_chance=0.01, cave_width=0.05)
    cells = [(x, z) for x in range(-1, 2) for z in range(-1, 2)]
    load_chunks(terrain, cells)
    lava = [(x, y, z) for x in range(-16, 32) for z in range(-16, 32) for y in range(1, 30)
            if terrain.get_block(x, y, z) == VOXEL_ENDLESS_LAVA]
    checks["the deep caves hold lava"] = len(lava) > 0
    checks["lava is at or below height 6 and nowhere higher"] = bool(lava) and max(y for _x, y, _z in lava) <= 6
    terrain.clear()


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

    # Water poured from an endless source falls, then spreads and stops. (A producing spring, WATER, makes volume instead, and keeps
    # the queue busy for as long as it stands, so it is not used here.)
    world.set_block(7, HEIGHT - 2, 7, VOXEL_ENDLESS_WATER)
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
                                                 "onGround": on_ground, "inside": physics.inside(world.skin_at, pos)})
        if not on_ground and abs(vel[1]) < 1e-6:
            stands = False
        if on_ground and physics.inside(world.skin_at, pos):
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


BULGE = 0.1   # the terrain's skin sits this far out from each face (VoxelTerrainShift: 1 - 2 * (VoxelSkinIso + shift))


def check_skin_surface(world, checks):
    """The surface the view meets is the drawn skin: on flat ground, looking straight down, it lies BULGE of a block above the
    top block of the column (the skin sits out from each face). Only flat columns are checked: on a slope the column's top is not
    the surface under its centre. The outline's block is the one under the surface."""
    def top_at(x, z):
        return max([y for y in range(HEIGHT - 1) if world.is_solid(x, y, z)] or [-1]) + 1

    good = 0
    total = 0
    for x in range(-20, 20, 5):
        for z in range(-20, 20, 5):
            top = top_at(x, z)
            if top <= 0:
                continue
            if any(top_at(x + dx, z + dz) != top for dx in (-1, 0, 1) for dz in (-1, 0, 1)):
                continue
            surface = world.skin_surface((x + 0.5, top + 20.0, z + 0.5), (0.0, -1.0, 0.0), 40.0)
            total += 1
            if surface is not None and abs(surface[1] - (top + BULGE)) < 0.06:
                good += 1
    checks["the view meets the drawn skin on flat ground (BULGE above the top block)"] = total > 0 and good >= 0.9 * total


def check_props(world, checks, chunks):
    """A torch is a prop in an air cell: it lights its cell (0.8 of full light) and the air next to it (0.8 of that), it stays
    air for collision, it adds geometry to its chunk's terrain mesh, and removing it puts the light out. Tested in a sealed
    pocket, which is put back afterwards."""
    cx, cy, cz = 10, 22, 10
    saved = {}
    for x in range(cx - 2, cx + 3):
        for y in range(cy - 2, cy + 3):
            for z in range(cz - 2, cz + 3):
                saved[(x, y, z)] = world.get_block(x, y, z)
                inside = max(abs(x - cx), abs(y - cy), abs(z - cz)) <= 1
                world.set_block(x, y, z, 0 if inside else VOXEL_STONE)
    wait_meshed(world, chunks)
    dark = world.light_at(cx + 1, cy, cz)
    quads_before = world.chunk_group_quads(0, 0).get(0, 0)
    placed = world.set_prop(cx + 1, cy, cz, 0)
    wait_meshed(world, chunks)
    torch_light = world.light_at(cx + 1, cy, cz)
    next_light = world.light_at(cx, cy, cz)
    checks["a sealed pocket has no light before the torch"] = dark == 0
    checks["a torch is placed in an air cell, and listed"] = placed and world.prop_at(cx + 1, cy, cz) == 0         and (cx + 1, cy, cz, 0) in [tuple(p) for p in world.props()]
    checks["a torch gives 0.8 of full light in its cell"] = abs(torch_light - 204) <= 2
    checks["the air beside a torch gets 0.8 of that"] = abs(next_light - 163) <= 2
    checks["a torch cell stays air to collision"] = world.skin_at(cx + 1, cy, cz) == 0
    checks["a torch adds geometry to its chunk's terrain mesh"] = world.chunk_group_quads(0, 0).get(0, 0) > quads_before
    # The wall is stone at +x, so its skin is 0.3 from the torch's cell centre; the stick's outer face must be on that skin.
    boxes = [b for b in world.prop_boxes() if (b[0], b[1], b[2]) == (cx + 1, cy, cz)]
    stick_outer = boxes[0][4][0] + boxes[0][5][0] if boxes else None
    # The torch leans on the wall at cx + 2 (its face), so its base is on the skin BULGE out from that face.
    # The stick stands out of the skin along its normal, so its foot is 0.06 off the wall and its outer edge is within 0.1 of the skin.
    checks["a torch stands on the skin of its wall, not inside it"] = stick_outer is not None and abs(stick_outer - (cx + 2.0 - BULGE)) < 0.1
    world.remove_prop(cx + 1, cy, cz)
    checks["removing a torch removes it and its light"] = world.prop_at(cx + 1, cy, cz) == -1         and world.light_at(cx + 1, cy, cz) == 0
    for (x, y, z), block in saved.items():
        world.set_block(x, y, z, block)
    world.clear_edits()
    wait_meshed(world, chunks)


def measure_remesh(world, chunks, results):
    """How long an edit takes to reach the mesh, in milliseconds (the median of 9 edits): a block, and a torch, in a chunk with
    no light block, and the same block in a chunk with a lamp in it. The check runs inside the engine, so the build is the
    one the test runs (Release)."""
    import statistics
    edit_cell = (8, 40, 8)
    def settle():
        # A busy poll: wait_meshed sleeps between polls, which would be most of a 5 ms measurement.
        while True:
            world.update(5.0, 0, 0)
            stats = world.stats()
            if stats["dirty"] == 0 and stats["meshing"] == 0:
                return
    def timed(edit):
        samples = []
        for i in range(9):
            start = time.perf_counter()
            edit(i)
            settle()
            samples.append((time.perf_counter() - start) * 1000.0)
        return round(statistics.median(samples), 2)
    world.set_block(*edit_cell, 0)
    results["measure"] = {}
    results["measure"]["block edit, no light block in the chunk"] = timed(
        lambda i: world.set_block(edit_cell[0], edit_cell[1], edit_cell[2], VOXEL_STONE if i % 2 == 0 else 0))
    results["measure"]["torch placed and removed, no light block in the chunk"] = timed(
        lambda i: world.set_prop(edit_cell[0] + 1, edit_cell[1], edit_cell[2], 0) if i % 2 == 0 else world.remove_prop(edit_cell[0] + 1, edit_cell[1], edit_cell[2]))
    world.set_block(edit_cell[0] - 2, edit_cell[1], edit_cell[2], VOXEL_LAMP)
    results["measure"]["block edit, with a lamp in the chunk"] = timed(
        lambda i: world.set_block(edit_cell[0], edit_cell[1], edit_cell[2], VOXEL_STONE if i % 2 == 0 else 0))
    world.set_block(edit_cell[0] - 2, edit_cell[1], edit_cell[2], 0)
    world.set_block(edit_cell[0], edit_cell[1], edit_cell[2], 0)
    world.clear_edits()
    wait_meshed(world, chunks)


def check_sea(world, checks, results):
    """The sea (roadmap 40.5): a basin under the sea level, with its water on it and sand on its floor. It is a share of the map,
    not all of it or none, and a column in it has water above its floor and nothing else."""
    # Its own world: the chunks it loads must not change the checks that count the world's chunks after it.
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.set_terrain(seed=3, sea_level=14, base_height=8, height_range=44, snow_level=40, tree_chance=0.01, cave_width=0.05)
    grid = [(x, z) for x in range(-600, 600, 12) for z in range(-600, 600, 12)]
    sea = [(x, z) for x, z in grid if world.biome_at(x, z) == "sea"]
    share = len(sea) / len(grid)
    results["seaShare"] = round(share, 3)
    checks["the sea covers a share of the map, not all and not none"] = 0.02 <= share <= 0.4
    if not sea:
        checks["a sea column has water over a sand floor"] = False
        return
    x, z = sea[len(sea) // 2]
    chunk = (x // SIZE, z // SIZE)
    around = [(chunk[0] + dx, chunk[1] + dz) for dx in (-1, 0, 1) for dz in (-1, 0, 1)]
    loaded = load_chunks(world, around)
    floor = None
    for y in range(HEIGHT - 1, 0, -1):
        if world.get_block(x, y, z) not in (0, WATER, VOXEL_ENDLESS_WATER):
            floor = y
            break
    results["seaColumn"] = [x, z, floor]
    water_over = all(world.get_block(x, y, z) in (WATER, VOXEL_ENDLESS_WATER) for y in range(floor + 1, 14))
    checks["a sea column has water over a sand floor"] = (loaded and floor is not None and floor < 14
        and world.get_block(x, floor, z) == SAND and water_over)


def check_rocks(world, chunks, checks, results):
    """Boulders are stone on the ground. The terrain's own columns end in grass, sand, snow or dirt, so a column whose top block
    is stone is a boulder. Some are in the chunks checked, and none is in the sea."""
    sea_level = 14
    rocks = []
    xs = range(min(cx for cx, _ in chunks) * SIZE, (max(cx for cx, _ in chunks) + 1) * SIZE)
    zs = range(min(cz for _, cz in chunks) * SIZE, (max(cz for _, cz in chunks) + 1) * SIZE)
    for x in xs:
        for z in zs:
            top = None
            for y in range(HEIGHT - 1, 0, -1):
                if world.get_block(x, y, z) != 0:
                    top = y
                    break
            if top is not None and world.get_block(x, top, z) == VOXEL_STONE:
                rocks.append((x, top, z))
    results["boulders"] = len(rocks)
    checks["boulders are in the chunks checked"] = len(rocks) > 0
    checks["no boulder is in the sea"] = all(top > sea_level for _, top, _ in rocks)


def check_ores(world, chunks, checks):
    """Ore is in the stone: coal at any depth, and iron only deep (below the terrain's IronDepth of 20)."""
    coal = iron = iron_high = 0
    xs = range(min(cx for cx, _ in chunks) * SIZE, (max(cx for cx, _ in chunks) + 1) * SIZE)
    zs = range(min(cz for _, cz in chunks) * SIZE, (max(cz for _, cz in chunks) + 1) * SIZE)
    for x in xs:
        for z in zs:
            for y in range(1, 40):
                block = world.get_block(x, y, z)
                if block == VOXEL_COAL:
                    coal += 1
                elif block == VOXEL_IRON:
                    iron += 1
                    if y >= 20:
                        iron_high += 1
    checks["coal ore is in the stone"] = coal > 0
    checks["iron ore is in the stone, and only deep"] = iron > 0 and iron_high == 0


def check_damage(checks):
    """A block takes its hits to break (stone takes three here). Each hit before the last leaves a damage stage, the last one
    breaks it, and the stage goes with the block. A hit marks the chunk to be meshed again, so the stage shows."""
    world = cge.VoxelWorld(SIZE, HEIGHT, "voxel")
    world.define_block(VOXEL_STONE, cge.Vector3f(0.5, 0.5, 0.53), hits=3)
    load_chunks(world, [(0, 0)])
    world.set_block(4, 30, 4, VOXEL_STONE)
    wait_meshed(world, [(0, 0)])
    first = world.hit_block(4, 30, 4)
    stage = world.damage_at(4, 30, 4)
    dirty = world.stats()["dirty"]
    second = world.hit_block(4, 30, 4)
    third = world.hit_block(4, 30, 4)
    checks["a block takes its hits to break (stone: three)"] = first == 1 and second == 2 and third == -1 \
        and world.get_block(4, 30, 4) == 0
    checks["a hit leaves a damage stage, and the stage goes with the block"] = stage == 1 and world.damage_at(4, 30, 4) == 0
    checks["a hit marks the chunk to be meshed again"] = dirty > 0
    world.set_block(4, 31, 4, VOXEL_STONE)
    world.hit_block(4, 31, 4)
    world.hit_block(4, 31, 4)
    healed = [world.heal_block(4, 31, 4), world.heal_block(4, 31, 4), world.heal_block(4, 31, 4)]
    checks["a damaged block heals one stage at a time, and is whole again"] = healed == [1, 0, 0] and world.damage_at(4, 31, 4) == 0
    world.clear()


def check_lamp(world, checks):
    """A light block lights the air round it: full at its cell, and 0.8 less for each block walked through air. Tested in a
    sealed pocket of stone, which the sky cannot reach, so the air has no sky light until the lamp is placed. The
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
    checks["a sealed pocket has no sky light before the lamp"] = dark == 0
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


WATER_VOLUME = range(128, 136)   # units 1 to 8 of water (VoxelGen.WATER_VOLUME_FIRST and _FULL)
LAVA_VOLUME = range(136, 144)    # units 1 to 8 of lava
LAVA_SPRING = 22


def prepare_box(world, x0, x1, z0, z1, floor=49, top=57):
    """A flat stone floor at y = floor, with air above it up to top, over the box (inclusive)."""
    for x in range(x0, x1 + 1):
        for z in range(z0, z1 + 1):
            world.set_block(x, floor, z, STONE)
            for y in range(floor + 1, top):
                world.set_block(x, y, z, 0)


def settle_liquid(world, seconds):
    """Runs the block physics until nothing is queued, for at most the given seconds. True when it settled."""
    for _ in range(int(seconds / 0.05)):
        world.simulate(0.05, 20.0)
        if world.active_count() == 0:
            return True
    return False


def volume_units(world, cells, family):
    """The units each cell holds in the volume family (0 for a cell of any other block), as a dict."""
    units = {}
    for cell in cells:
        block = world.get_block(*cell)
        units[cell] = block - family.start + 1 if block in family else 0
    return units


def levels_out(world, units, layer, family):
    """True when no cell holds two or more units beside air: a volume that has spread has every air face at one unit or less."""
    for (x, y, z), u in units.items():
        if u < 2:
            continue
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if world.get_block(x + dx, y, z + dz) == 0:
                return False
    return True


def check_volume(world, checks, name, family, box, layer, drop_y=None):
    """A placed volume of 8 units spreads out and levels off (or falls onto the floor first, from drop_y). Units are moved, never
    made or lost, so the total stays 8 across the box."""
    x0, x1, z0, z1 = box
    # The floor and the air are prepared a margin wider than the box, and the units are counted over that margin: the volume may
    # spread past the box's edge, and a unit out there is still in the world.
    margin = 6
    prepare_box(world, x0 - margin, x1 + margin, z0 - margin, z1 + margin)
    cx, cz = (x0 + x1) // 2, (z0 + z1) // 2
    start_y = drop_y if drop_y is not None else layer
    world.set_block(cx, start_y, cz, family.start + 7)   # unit 8: a full volume
    settled = settle_liquid(world, 60.0)
    cells = [(x, y, z) for x in range(x0 - margin, x1 + margin + 1) for y in range(layer, layer + 8)
             for z in range(z0 - margin, z1 + margin + 1)]
    units = volume_units(world, cells, family)
    total = sum(units.values())
    on_layer = {c: u for c, u in units.items() if c[1] == layer}
    checks[name + " volume settles"] = settled
    checks[name + " volume keeps its 8 units"] = total == 8
    if drop_y is None:
        checks[name + " volume spreads out over several cells"] = sum(1 for u in on_layer.values() if u > 0) >= 4
    else:
        checks[name + " volume falls onto the floor"] = sum(on_layer.values()) == 8 and all(
            u == 0 for c, u in units.items() if c[1] > layer)
    checks[name + " volume levels out: no cell with two units has air beside it"] = levels_out(world, on_layer, layer, family)


def check_volume_step(world, checks):
    """A volume on a high floor flows off its edge and falls to the lower floor beside it: a step down at x = -8 (the floor is 47 at x
    >= -8, 49 before), so a unit is in the air over the drop, and the volume's units reach the low floor and stay there."""
    x0, x1, z0, z1 = -14, -4, -14, -4
    prepare_box(world, x0, x1, z0, z1)
    # A solid floor under the basin too: the terrain here has caves, and a unit that reached one would leave the counted box.
    for x in range(x0, x1 + 1):
        for z in range(z0, z1 + 1):
            for y in range(40, 49):
                world.set_block(x, y, z, STONE)
    # Walls round the basin, so no unit spills out of the counted box at the low edge.
    for y in range(47, 57):
        for k in range(-15, -2):
            for wall in ((k, y, -3), (k, y, -15), (-3, y, k), (-15, y, k)):
                world.set_block(wall[0], wall[1], wall[2], STONE)
    for x in range(-8, x1 + 1):
        for z in range(z0, z1 + 1):
            world.set_block(x, 49, z, 0)
            world.set_block(x, 48, z, 0)
            world.set_block(x, 47, z, STONE)
    world.set_block(-10, 50, -9, WATER_VOLUME[7])   # unit 8 on the high floor, two blocks from the edge
    settled = settle_liquid(world, 60.0)
    cells = [(x, y, z) for x in range(x0, x1 + 1) for y in range(44, 57) for z in range(z0, z1 + 1)]
    units = volume_units(world, cells, WATER_VOLUME)
    low = {c: u for c, u in units.items() if c[0] >= -8 and u > 0}
    checks["a volume on a step flows to the low floor (settles)"] = settled
    checks["the step's volume keeps its 8 units"] = sum(units.values()) == 8
    checks["the volume reaches the low side of the step"] = sum(low.values()) > 0
    checks["the volume on the low side lies on the low floor, not in the air"] = all(c[1] == 48 for c in low)


def close_vertex(a, b):
    return len(a) == len(b) and all(abs(p - q) < 1e-4 for p, q in zip(a, b))


def check_ground_skin(world, checks):
    """Placing water on the ground leaves the ground's skin as it was. The ground's smooth mesh is the same vertices (positions,
    colours and splat weights) with the water there: water is open like air, so it does not change the blocks around it."""
    # Water is defined as the game defines it (VoxelGame.setup): liquid, not solid, drawn in the water mesh, see-through.
    for block in [WATER] + list(FLOW_IDS) + list(WATER_VOLUME):
        world.define_block(block, cge.Vector3f(0.22, 0.45, 0.92), solid=False, liquid=True, group="own", mesh=WATER, transparent=True)
    # A flat stone floor at y = 30 in a clear patch of chunk (0, 0), so the test does not depend on what the terrain has.
    for x in range(2, 11):
        for z in range(2, 11):
            world.set_block(x, 30, z, STONE)
            for y in range(31, 40):
                world.set_block(x, y, z, 0)
    spot = (6, 30, 6)
    world.set_block(6, 30, 6, GRASS)   # grass in the stone, so a change of the block the skin takes its colour from shows
    x, y, z = spot
    before = list(world.ground_mesh(0, 0))
    world.set_block(x, y + 1, z, WATER_VOLUME[7])
    after = list(world.ground_mesh(0, 0))
    # A liquid's surface is drawn only in cells with liquid in them: no liquid surface on the ground or on wood beside it.
    liquid_ids = set([WATER] + list(FLOW_IDS) + list(WATER_VOLUME))
    water_surface = list(world.ground_mesh(0, 0, WATER))
    stray = sum(1 for v in water_surface if not any(
        world.get_block(int(v[0] // 1) + dx, int(v[1] // 1) + dy, int(v[2] // 1) + dz) in liquid_ids
        for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1)))
    checks["the liquid's surface is drawn only where there is liquid (%d vertices, %d stray)" % (len(water_surface), stray)] = len(water_surface) > 0 and stray == 0
    world.set_block(x, y + 1, z, 0)
    checks["placing water on the ground leaves its skin as it was"] = len(before) == len(after) and all(close_vertex(a, b) for a, b in zip(before, after))


def check_diagonal_reaction(world, checks):
    """A lava spring touching water only at a corner (no face shared) becomes obsidian; the rule's diagonal flag is what lets it."""
    x0, x1, z0, z1 = -14, -8, 10, 15
    prepare_box(world, x0, x1, z0, z1)
    world.set_block(-10, 50, 12, STONE)   # walls between the lava and the water, which share only a corner (the edge in the plane)
    world.set_block(-11, 50, 13, STONE)
    world.set_block(-9, 50, 13, STONE)
    world.set_block(-10, 50, 14, STONE)
    world.set_block(-10, 51, 13, STONE)
    world.set_block(-11, 50, 12, LAVA_SPRING)
    world.set_block(-10, 50, 13, WATER)
    world.add_reaction(LAVA_SPRING, [WATER], 30, 1.0, 4.0, True)
    settle_liquid(world, 8.0)
    checks["lava touching water only at a corner becomes obsidian (diagonal rule)"] = world.get_block(-11, 50, 12) == 30
    world.clear_rules()
    world.set_block(-11, 50, 12, 0)
    world.set_block(-10, 50, 13, 0)
    settle_liquid(world, 2.0)


def check_volumes(world, checks):
    """Placed volumes: water and lava spread and level out, and a volume dropped from above falls onto the floor first."""
    # The earlier checks left spring water and lava running over the land (their flows reach 7 blocks). Clear the liquid above
    # the sea, so these volumes meet only their own floors.
    liquids = set([WATER, LAVA_SPRING, VOXEL_ENDLESS_WATER, VOXEL_ENDLESS_LAVA]) | set(FLOW_IDS) | set(range(23, 30)) | set(WATER_VOLUME) | set(LAVA_VOLUME)
    for x in range(-SIZE, SIZE):
        for z in range(-SIZE, SIZE):
            for y in range(15, HEIGHT):
                if world.get_block(x, y, z) in liquids:
                    world.set_block(x, y, z, 0)
    settle_liquid(world, 3.0)
    check_volume(world, checks, "water", WATER_VOLUME, (-14, -4, -14, -4), 50)
    check_volume(world, checks, "lava", LAVA_VOLUME, (-14, -4, -2, 8), 50)
    check_volume(world, checks, "a falling water", WATER_VOLUME, (-14, -4, -14, -4), 50, drop_y=54)
    check_volume_step(world, checks)
    check_diagonal_reaction(world, checks)


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
    check_props(world, results["check"], chunks)
    check_ores(world, chunks, results["check"])
    check_rocks(world, chunks, results["check"], results)
    check_damage(results["check"])
    measure_remesh(world, chunks, results)
    check_groups(results["check"])
    check_trunk_base(results)
    check_lava(results)
    check_reactions(results)
    check_falling(results)
    check_far_feature(results)
    biomes = {world.biome_at(x, z) for x in range(-200, 200, 10) for z in range(-200, 200, 10)}
    results["check"]["biomes vary: plains, forest and desert occur"] = {"plains", "forest", "desert"} <= biomes
    results["biomes"] = sorted(biomes)
    check_sea(world, results["check"], results)

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
    # A load replaces the edits: an edit made after the save is undone, and its cell shows the terrain again.
    terrain_at_9 = world.get_block(9, 20, 9)
    world.set_block(9, 20, 9, VOXEL_LAMP)
    world.load_edits(edit_file)
    results["check"]["a load drops the edits made after the save"] = (world.get_block(9, 20, 9) == terrain_at_9
                                                                      and terrain_at_9 != VOXEL_LAMP
                                                                      and world.edit_count() == 1)
    # Torches are saved with the edits too: one goes out and comes back from the file.
    world.set_block(5, 30, 5, 0)
    world.set_prop(5, 30, 5, 2)
    world.save_edits(edit_file)
    world.remove_prop(5, 30, 5)
    world.clear_edits()
    world.load_edits(edit_file)
    results["check"]["a torch is saved and loaded with the edits"] = world.prop_at(5, 30, 5) == 2
    world.remove_prop(5, 30, 5)
    world.clear_edits()
    # Damage is saved with the edits too: a block hit once keeps its stage through a save and a load (stone takes three).
    world.define_block(VOXEL_STONE, cge.Vector3f(0.5, 0.5, 0.53), hits=3)
    world.set_block(7, 20, 7, VOXEL_STONE)
    world.hit_block(7, 20, 7)
    world.save_edits(edit_file)
    world.heal_block(7, 20, 7)
    results["check"]["the damage is healed before the load"] = world.damage_at(7, 20, 7) == 0
    world.load_edits(edit_file)
    results["check"]["a damaged block is saved and loaded with its stage"] = world.damage_at(7, 20, 7) == 1
    world.set_block(7, 20, 7, 0)
    world.clear_edits()

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
    check_volumes(world, results["check"])
    check_ground_skin(world, results["check"])
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
