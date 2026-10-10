"""
Offline checks for the dungeon generator (resources/scripts/DungeonGen.py). No GPU or engine needed.

Usage (from the repo root):
    python -I tools/test_dungeon_gen.py
Exits 0 when every check passes for every seed tested, 1 otherwise.
"""
import os
import sys
from collections import deque

sys.dont_write_bytecode = True  # keep __pycache__ out of resources/scripts
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "resources", "scripts"))

import DungeonGen as dg  # noqa: E402

SEEDS = range(300)
failures = []


def check(seed, name, ok):
    if not ok:
        failures.append(f"seed {seed}: {name}")


def reachable(level, start):
    seen = {start}
    queue = deque([start])
    while queue:
        gx, gy = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nxt = (gx + dx, gy + dy)
            if nxt in level.floors and nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return seen


for seed in SEEDS:
    level = dg.generate(seed)

    # Determinism: the same seed gives the same level.
    again = dg.generate(seed)
    check(seed, "same seed gives same floors", level.floors == again.floors)

    # Every tile is either a floor or a wall, and nothing is outside the grid.
    total = level.grid_w * level.room_w * level.grid_h * level.room_h
    check(seed, "floors and walls partition the grid", len(level.floors) + len(level.walls) == total)
    check(seed, "floors and walls do not overlap", not (level.floors & level.walls))

    # Start and goal are floors, and the goal is not in the start room.
    check(seed, "start is a floor", level.start in level.floors)
    check(seed, "goal is a floor", level.goal in level.floors)
    check(seed, "goal is not in the start room", level.goal_room != (0, 0))

    # Every floor tile is reachable from the start, so every room and the goal can be reached.
    seen = reachable(level, level.start)
    check(seed, "every floor is reachable from the start", seen == level.floors)
    check(seed, "goal is reachable", level.goal in seen)

    # Links: both sides of each door are floors, and each room's border has walls except at doors.
    for pair in level.links:
        a, b = tuple(pair)
        ax, ay = a
        bx, by = b
        if ax != bx:
            left, right = (a, b) if ax < bx else (b, a)
            row = left[1] * level.room_h + level.room_h // 2
            x_a = left[0] * level.room_w + level.room_w - 1
            x_b = right[0] * level.room_w
            check(seed, "door tiles are floors", (x_a, row) in level.floors and (x_b, row) in level.floors)
        else:
            top, bottom = (a, b) if ay < by else (b, a)
            col = top[0] * level.room_w + level.room_w // 2
            y_a = top[1] * level.room_h + level.room_h - 1
            y_b = bottom[1] * level.room_h
            check(seed, "door tiles are floors", (col, y_a) in level.floors and (col, y_b) in level.floors)

    for room in level.rooms():
        rx, ry = room
        for lx in range(level.room_w):
            for ly in (0, level.room_h - 1):
                tile = (rx * level.room_w + lx, ry * level.room_h + ly)
                if tile in level.floors:
                    check(seed, "floor on a room border is only a door", lx == level.room_w // 2)
        for ly in range(level.room_h):
            for lx in (0, level.room_w - 1):
                tile = (rx * level.room_w + lx, ry * level.room_h + ly)
                if tile in level.floors:
                    check(seed, "floor on a room border is only a door", ly == level.room_h // 2)

    # Pillars never touch the centre row or column of a room.
    for room in level.rooms():
        rx, ry = room
        # Interior only: the border is open just where a link leaves the room.
        for lx in range(1, level.room_w - 1):
            tile = (rx * level.room_w + lx, ry * level.room_h + level.room_h // 2)
            check(seed, "centre row stays open", tile in level.floors)
        for ly in range(1, level.room_h - 1):
            tile = (rx * level.room_w + level.room_w // 2, ry * level.room_h + ly)
            check(seed, "centre column stays open", tile in level.floors)

    # Enemies: on floors, inside the room's interior, none in the start room, and a sensible count.
    for room, spots in level.enemy_spawns.items():
        if room == (0, 0):
            check(seed, "no enemies in the start room", spots == [])
        check(seed, "enemy count is 0..5", 0 <= len(spots) <= 5)
        for gx, gy in spots:
            check(seed, "enemy stands on a floor", (gx, gy) in level.floors)
            check(seed, "enemy stands inside its room", level.room_of(gx, gy) == room)
            check(seed, "enemy is not on the room border",
                  1 < gx % level.room_w < level.room_w - 2 and 1 < gy % level.room_h < level.room_h - 2)

    # Different seeds should usually give different layouts.
    if seed > 0:
        check(seed, "neighbouring seeds differ", dg.generate(seed - 1).floors != level.floors)

if failures:
    print(f"{len(failures)} check(s) failed across {len(SEEDS)} seeds:")
    for line in failures[:40]:
        print("  - " + line)
    sys.exit(1)

print(f"All dungeon generator checks passed for {len(SEEDS)} seeds.")
