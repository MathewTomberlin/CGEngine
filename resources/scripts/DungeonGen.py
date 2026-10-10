"""
Procedural dungeon layout for the dungeon sample game (docs/games/dungeon.md).

Pure Python: no engine imports, so it can be tested without a GPU (tools/test_dungeon_gen.py).

The level is a grid of rooms. Rooms are linked by a random spanning tree, plus a couple of extra
links so the map has loops. Every room is ROOM_W x ROOM_H tiles including its border. Neighbouring
rooms share a two-tile-thick border; a link opens the border at the middle row or column.

Tiles use global coordinates (gx, gy). Room (rx, ry) covers gx in [rx*ROOM_W, rx*ROOM_W + ROOM_W - 1]
and gy in [ry*ROOM_H, ry*ROOM_H + ROOM_H - 1]. Each tile is either a floor or a wall.
"""
import random
from collections import deque

ROOM_W = 13
ROOM_H = 9
GRID_W = 3
GRID_H = 3
EXTRA_LINKS = 2  # loops added on top of the spanning tree
MAX_ENEMIES_PER_ROOM = 5


class Level:
    def __init__(self, seed, grid_w, grid_h, room_w, room_h):
        self.seed = seed
        self.grid_w = grid_w
        self.grid_h = grid_h
        self.room_w = room_w
        self.room_h = room_h
        self.links = set()        # frozensets of two room coordinates
        self.floors = set()       # passable global tiles
        self.walls = set()        # solid global tiles
        self.room_depth = {}      # room -> number of links from the start room
        self.start = None         # global tile where the player starts
        self.goal = None          # global tile of the goal
        self.goal_room = None
        self.enemy_spawns = {}    # room -> list of global tiles for initial enemies

    def room_of(self, gx, gy):
        return (gx // self.room_w, gy // self.room_h)

    def in_bounds(self, gx, gy):
        return 0 <= gx < self.grid_w * self.room_w and 0 <= gy < self.grid_h * self.room_h

    def passable(self, gx, gy):
        return (gx, gy) in self.floors

    def rooms(self):
        return [(rx, ry) for ry in range(self.grid_h) for rx in range(self.grid_w)]

    def room_tiles(self, room):
        """Floor and wall tiles of one room, in global coordinates."""
        rx, ry = room
        floors, walls = [], []
        for ly in range(self.room_h):
            for lx in range(self.room_w):
                tile = (rx * self.room_w + lx, ry * self.room_h + ly)
                (floors if tile in self.floors else walls).append(tile)
        return floors, walls

    def room_centre(self, room):
        rx, ry = room
        return (rx * self.room_w + self.room_w // 2, ry * self.room_h + self.room_h // 2)


def _neighbours(room, grid_w, grid_h):
    rx, ry = room
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = rx + dx, ry + dy
        if 0 <= nx < grid_w and 0 <= ny < grid_h:
            yield (nx, ny)


def _link_tree(rng, grid_w, grid_h):
    """Randomised depth-first spanning tree over the room grid, from the top-left room."""
    start = (0, 0)
    visited = {start}
    links = set()
    stack = [start]
    while stack:
        options = [n for n in _neighbours(stack[-1], grid_w, grid_h) if n not in visited]
        if not options:
            stack.pop()
            continue
        nxt = rng.choice(options)
        visited.add(nxt)
        links.add(frozenset((stack[-1], nxt)))
        stack.append(nxt)
    return links


def _depths(links, start):
    """Number of links from start to each room (breadth-first over the link graph)."""
    graph = {}
    for pair in links:
        a, b = tuple(pair)
        graph.setdefault(a, []).append(b)
        graph.setdefault(b, []).append(a)
    depth = {start: 0}
    queue = deque([start])
    while queue:
        room = queue.popleft()
        for other in graph.get(room, []):
            if other not in depth:
                depth[other] = depth[room] + 1
                queue.append(other)
    return depth


def generate(seed, grid_w=GRID_W, grid_h=GRID_H, room_w=ROOM_W, room_h=ROOM_H):
    """Build a level for this seed. The same seed always gives the same level."""
    rng = random.Random(seed)
    level = Level(seed, grid_w, grid_h, room_w, room_h)

    # Links: a spanning tree, then a few extra links for loops.
    level.links = _link_tree(rng, grid_w, grid_h)
    candidates = []
    for room in level.rooms():
        for other in _neighbours(room, grid_w, grid_h):
            pair = frozenset((room, other))
            if pair not in level.links:
                candidates.append(pair)
    rng.shuffle(candidates)
    for pair in candidates[:EXTRA_LINKS]:
        level.links.add(pair)

    # Tiles: border walls with doors where linked, then pillars inside each room.
    door_row = room_h // 2
    door_col = room_w // 2
    def has_link(a, b):
        return frozenset((a, b)) in level.links

    for room in level.rooms():
        rx, ry = room
        for ly in range(room_h):
            for lx in range(room_w):
                gx = rx * room_w + lx
                gy = ry * room_h + ly
                border = lx in (0, room_w - 1) or ly in (0, room_h - 1)
                is_door = False
                if border:
                    if lx == room_w - 1 and ly == door_row and has_link(room, (rx + 1, ry)):
                        is_door = True
                    elif lx == 0 and ly == door_row and has_link(room, (rx - 1, ry)):
                        is_door = True
                    elif ly == room_h - 1 and lx == door_col and has_link(room, (rx, ry + 1)):
                        is_door = True
                    elif ly == 0 and lx == door_col and has_link(room, (rx, ry - 1)):
                        is_door = True
                if is_door or not border:
                    level.floors.add((gx, gy))
                else:
                    level.walls.add((gx, gy))

    # Pillars sit on a sparse lattice that never touches the centre row or column, so they cannot
    # cut a room off from its doors or from its centre. Isolated single tiles never disconnect a grid.
    pillar_cells = [(3, 2), (9, 2), (3, 6), (9, 6)]
    for room in level.rooms():
        rx, ry = room
        for lx, ly in pillar_cells:
            if lx >= room_w - 1 or ly >= room_h - 1:
                continue
            if rng.random() < 0.7:
                gx = rx * room_w + lx
                gy = ry * room_h + ly
                level.floors.discard((gx, gy))
                level.walls.add((gx, gy))

    # Start, goal and enemies.
    level.start = level.room_centre((0, 0))
    depth = _depths(level.links, (0, 0))
    level.room_depth = depth
    farthest = max(depth.values())
    far_rooms = sorted(room for room, d in depth.items() if d == farthest)
    level.goal_room = rng.choice(far_rooms)
    level.goal = level.room_centre(level.goal_room)

    for room in level.rooms():
        if room == (0, 0):
            level.enemy_spawns[room] = []
            continue
        count = min(MAX_ENEMIES_PER_ROOM, 1 + rng.randint(0, 2) + depth[room] // 2)
        rx, ry = room
        spots = []
        for ly in range(2, room_h - 2):
            for lx in range(2, room_w - 2):
                gx = rx * room_w + lx
                gy = ry * room_h + ly
                if (gx, gy) in level.floors and (gx, gy) != level.goal:
                    spots.append((gx, gy))
        rng.shuffle(spots)
        level.enemy_spawns[room] = spots[:count]

    return level
