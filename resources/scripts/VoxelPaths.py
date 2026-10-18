"""
Routes for the biters in the voxel sample: a way across the terrain, on the block grid, round a wall that stands between a
biter and the player.

A creature can stand in a cell when the cell and the one above it are air and the cell below is ground (a block that is not air
or water or lava). From a cell it can go to a neighbour across, one block down, or one block up (up needs the cell above its
head to be air, as a jump does). The search is breadth first, and it stops at RADIUS cells from the creature, so a room with no
way in gives no route, and the creature keeps wandering instead.

Pure Python with no engine calls: block_at is a function (x, y, z) -> block id, so tools/test_voxel_paths.py checks the search on a
small grid of its own.
"""

from collections import deque

import VoxelGen

RADIUS = 14             # cells: a route stays this close to the creature's cell (in x and z)
MAX_CELLS = 2000        # cells the search looks at, at most: a cap on what one search costs
STEPS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def stands(block_at, x, y, z):
    """True when a creature can stand with its feet in cell (x, y, z): the cell and the one above are air, and the cell below is ground."""
    if block_at(x, y, z) != VoxelGen.AIR or block_at(x, y + 1, z) != VoxelGen.AIR:
        return False
    below = block_at(x, y - 1, z)
    return below != VoxelGen.AIR and below not in VoxelGen.LIQUIDS


def find_path(block_at, start, is_goal):
    """The cells a creature walks from start (x, y, z) to the first cell where is_goal(x, y, z) is true, nearest first: the cells
    after start, the goal last. [] when start is already a goal, None when no route is found within RADIUS and MAX_CELLS."""
    if is_goal(*start):
        return []
    # Each cell is read by several of its neighbours, so a search reads each block once: the world's lookup is the cost of a search.
    seen = {}
    read = block_at

    def block(x, y, z):
        key = (x, y, z)
        found = seen.get(key)
        if found is None:
            found = read(x, y, z)
            seen[key] = found
        return found
    block_at = block
    sx, sy, sz = start
    came = {start: None}
    queue = deque([start])
    while queue and len(came) < MAX_CELLS:
        cell = queue.popleft()
        x, y, z = cell
        for dx, dz in STEPS:
            nx, nz = x + dx, z + dz
            if abs(nx - sx) > RADIUS or abs(nz - sz) > RADIUS:
                continue
            for ny in (y, y - 1, y + 1):
                nxt = (nx, ny, nz)
                if nxt in came:
                    continue
                if ny == y + 1 and block_at(x, y + 2, z) != VoxelGen.AIR:
                    continue    # no room over the creature's head to step up
                if not stands(block_at, nx, ny, nz):
                    continue
                came[nxt] = cell
                if is_goal(*nxt):
                    return _route(came, nxt)
                queue.append(nxt)
    return None


def _route(came, goal):
    """The cells from the step after the start to the goal, read back through the search's came-from links."""
    cells = []
    cell = goal
    while came[cell] is not None:
        cells.append(cell)
        cell = came[cell]
    cells.reverse()
    return cells
