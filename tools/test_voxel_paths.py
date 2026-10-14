"""
Checks the biters' routes (resources/scripts/VoxelPaths.py) without the engine: a route goes round a wall by its gap, climbs a
block but not two, does not cross water, does not go past its radius, and finds no way into a closed room, so a biter outside
it has no route in.
Usage (from the repo root): python -I tools/test_voxel_paths.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelGen  # noqa: E402
import VoxelPaths  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


def world(ground=0, blocks=None, floor_y=None):
    """A grid: solid ground up to and including y = ground (stone), air above it, and the given blocks over that."""
    cells = dict(blocks or {})

    def block_at(x, y, z):
        if (x, y, z) in cells:
            return cells[(x, y, z)]
        return VoxelGen.STONE if y <= ground else VoxelGen.AIR
    return block_at


def wall(x, z_from, z_to, height=3, gap=None):
    """A wall across x, from z_from to z_to, above the ground (y = 1 to height), with an optional gap at the z given."""
    out = {}
    for z in range(z_from, z_to + 1):
        if z == gap:
            continue
        for y in range(1, height + 1):
            out[(x, y, z)] = VoxelGen.STONE
    return out


def at(cell):
    return lambda x, y, z: (x, y, z) == cell


def walks_legally(start, route, block_at):
    """Each step of the route moves to a neighbour in x or z, by at most one block up or down, to a cell that stands."""
    here = start
    for nxt in route:
        if abs(nxt[0] - here[0]) + abs(nxt[2] - here[2]) != 1 or abs(nxt[1] - here[1]) > 1:
            return False
        if not VoxelPaths.stands(block_at, *nxt):
            return False
        here = nxt
    return True


# Open ground: a straight route, nearest first, ending at the goal.
open_ground = world()
route = VoxelPaths.find_path(open_ground, (0, 1, 0), at((5, 1, 0)))
check("open ground gives a route to a cell five blocks away", route is not None and route[-1] == (5, 1, 0) and len(route) == 5)
check("every step of the open route is a legal move", route is not None and walks_legally((0, 1, 0), route, open_ground))
check("a start that is already a goal gives an empty route", VoxelPaths.find_path(open_ground, (0, 1, 0), lambda x, y, z: True) == [])

# A wall with a gap: the route goes through the gap, not over or through the wall.
gapped = world(blocks=wall(3, -4, 4, gap=2))
route = VoxelPaths.find_path(gapped, (0, 1, 0), at((6, 1, 0)))
check("a route goes round a wall by its gap", route is not None and (3, 1, 2) in route and route[-1] == (6, 1, 0))
check("the route through the gap is a legal walk", route is not None and walks_legally((0, 1, 0), route, gapped))

# A closed room: a wall with no gap all round; the player inside has no route for a biter outside.
room = {}
for x in range(2, 9):
    for z in range(2, 9):
        if x in (2, 8) or z in (2, 8):
            for y in range(1, 4):
                room[(x, y, z)] = VoxelGen.STONE
closed = world(blocks=room)
check("a closed room gives a biter outside no route in", VoxelPaths.find_path(closed, (0, 1, 0), at((5, 1, 5))) is None)
room_gapped = dict(room)
del room_gapped[(2, 1, 5)], room_gapped[(2, 2, 5)], room_gapped[(2, 3, 5)]
check("the same room with a gap in its wall gives a route in", VoxelPaths.find_path(world(blocks=room_gapped), (0, 1, 0), at((5, 1, 5))) is not None)

# Steps: one block up is a route, two blocks up is not.
step = {(x, 1, z): VoxelGen.STONE for x in range(4, 9) for z in range(-1, 2)}
route = VoxelPaths.find_path(world(blocks=step), (0, 1, 0), at((6, 2, 0)))
check("a route climbs one block up", route is not None and route[-1] == (6, 2, 0))
ledge = {(x, y, z): VoxelGen.STONE for x in range(4, 9) for z in range(-1, 2) for y in (1, 2)}
check("a route does not climb two blocks", VoxelPaths.find_path(world(blocks=ledge), (0, 1, 0), at((6, 3, 0))) is None)
# A low ceiling over the ground round a shelf: a creature cannot step up onto the shelf from any side, so it has no route.
# Without the ceiling it climbs the shelf's edge.
shelf = {(x, 1, z): VoxelGen.STONE for x in range(1, 6) for z in range(-5, 6)}
low_ceiling = {(x, 3, z): VoxelGen.STONE for x in range(-2, 8) for z in range(-7, 8)
               if not (1 <= x <= 5 and -5 <= z <= 5)}
check("a ceiling over the step stops the climb onto the shelf", VoxelPaths.find_path(
    world(blocks={**shelf, **low_ceiling}), (0, 1, 0), at((3, 2, 0))) is None)
check("the same shelf without the ceiling is climbed", VoxelPaths.find_path(
    world(blocks=shelf), (0, 1, 0), at((3, 2, 0))) is not None)

# Water is not ground: a pool across the way is not walked, and it is too long to go round within the radius.
pool = {(x, 0, z): VoxelGen.WATER for x in range(3, 5) for z in range(-16, 17)}
check("water does not stand a creature, so a pool across the way gives no route", VoxelPaths.find_path(
    world(blocks=pool), (0, 1, 0), at((6, 1, 0))) is None)
check("a cell over water is not a place to stand", not VoxelPaths.stands(world(blocks={(3, 0, 0): VoxelGen.WATER}), 3, 1, 0))

# Radius: a goal beyond RADIUS is not found, however open the ground.
far = VoxelPaths.find_path(open_ground, (0, 1, 0), at((VoxelPaths.RADIUS + 3, 1, 0)))
check("a goal beyond the search radius gives no route", far is None)

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel path checks passed.")
sys.exit(0)
