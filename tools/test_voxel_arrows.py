"""
Checks the arrows (resources/scripts/VoxelArrows.py) without the engine: the draw's charge, the speed and damage a charge gives, an arrow
that falls under gravity along its arc, an arrow that lands at the first solid block it reaches, and a creature's box that an arrow
hits. Usage (from the repo root): python -I tools/test_voxel_arrows.py   Exits 0 on success, 1 on any failed check.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

import VoxelArrows as arrows  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


def open_air(x, y, z):
    return False


check("a draw of no time has no charge, a half draw half, and a long draw full",
      arrows.charge_of(0.0) == 0.0 and abs(arrows.charge_of(arrows.DRAW_SECONDS / 2) - 0.5) < 1e-9
      and arrows.charge_of(arrows.DRAW_SECONDS * 5) == 1.0)
check("the speed grows from SPEED_MIN at no draw to SPEED_MAX at a full draw",
      abs(math.hypot(*arrows.launch((0, 0, 0), (1.0, 0.0, 0.0), 0.0)[1]) - arrows.SPEED_MIN) < 1e-9
      and abs(math.hypot(*arrows.launch((0, 0, 0), (1.0, 0.0, 0.0), 1.0)[1]) - arrows.SPEED_MAX) < 1e-9)
check("the arrow starts at the origin and flies along the look direction",
      arrows.launch((1, 2, 3), (0.0, 0.0, 1.0), 0.5)[0] == [1, 2, 3]
      and arrows.launch((1, 2, 3), (0.0, 0.0, 1.0), 0.5)[1][0] == 0.0)
check("the damage grows from DAMAGE_MIN to DAMAGE_MAX (a full draw kills a 3-health creature)",
      arrows.damage_of_charge(0.0) == arrows.DAMAGE_MIN and arrows.damage_of_charge(1.0) == arrows.DAMAGE_MAX == 3.0)

# A flat shot, no gravity to speak of over one tenth of a second: it moves about speed times the time, along x.
pos, vel, landed = arrows.step((0.0, 10.0, 0.0), (20.0, 0.0, 0.0), 0.1, open_air)
check("an arrow in open air flies on along its velocity", landed is None and abs(pos[0] - 2.0) < 1e-9)
check("an arrow in open air loses height to gravity over the flight",
      vel[1] < 0.0 and abs(vel[1] + arrows.GRAVITY * 0.1) < 1e-9)

# The arc: a shot straight up comes back down, and is higher in the middle than at the start.
pos, vel = [0.0, 0.0, 0.0], [0.0, 10.0, 0.0]
heights = []
for _ in range(120):
    pos, vel, landed = arrows.step(pos, vel, 0.05, open_air)
    heights.append(pos[1])
check("an arrow shot up rises, then falls back under gravity", max(heights) > 4.0 and heights[-1] < max(heights))

# A wall: an arrow flying along +x stops at the first solid block (x = 5), and lands at the last open point before it.
def wall(x, y, z):
    return x >= 5
pos, vel, landed = arrows.step((0.0, 0.5, 0.5), (30.0, 0.0, 0.0), 1.0, wall)
check("an arrow stops at a solid block", landed is not None and landed[0] < 5.0 and landed[0] > 4.5)
check("the arrow lands at the open point before the block, not inside it", landed is not None and math.floor(landed[0]) == 4)

check("a point inside a creature's box is a hit, and a point just outside it is not",
      arrows.hits_box((1.0, 1.0, 1.0), (0.5, 0.5, 0.5), (1.5, 1.5, 1.5))
      and not arrows.hits_box((2.0, 1.0, 1.0), (0.5, 0.5, 0.5), (1.5, 1.5, 1.5)))
check("the arrow's life is a few seconds", 1.0 <= arrows.LIFE_SECONDS <= 10.0)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel arrow checks passed.")
