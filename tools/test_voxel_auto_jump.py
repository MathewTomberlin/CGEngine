"""
Checks the Auto jump rule (resources/scripts/VoxelPhysics.py: wall_ahead, auto_jump_wanted) without the engine: a walk stopped by a
wall of 1 to 4 blocks at the feet makes the player jump, a clear way or a wall of 5 blocks or more does not, and the wall is the one
in the direction of the walk. Usage (from the repo root): python -I tools/test_voxel_auto_jump.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

import VoxelPhysics as physics  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


def world_with_wall(height, wall_x=1, feet_row=0):
    """A solid column of blocks, height high, at x = wall_x (every z), from the feet row up; everything else is air."""
    def solid(x, y, z):
        return x == wall_x and feet_row <= y < feet_row + height
    return solid


PLAYER = (0.7, 0.0, 0.5)     # against the face of block (1, 0, 0) (the body is 0.3 across), feet on the floor
EAST = (1.0, 0.0)            # walking towards +x

for height in (1, 2, 3, 4):
    solid = world_with_wall(height)
    check("a wall of %d block(s) at the feet makes Auto jump jump" % height,
          physics.auto_jump_wanted(solid, PLAYER, EAST))

check("a wall of 5 blocks is too high to jump", not physics.auto_jump_wanted(world_with_wall(5), PLAYER, EAST))
check("a wall of 4 blocks counts as 4", physics.wall_ahead(world_with_wall(4), PLAYER, EAST) == 4)
check("a clear way ahead does not jump", not physics.auto_jump_wanted(lambda x, y, z: False, PLAYER, EAST))
check("a wall behind does not jump", not physics.auto_jump_wanted(world_with_wall(2, wall_x=-1), PLAYER, EAST))
check("a wall only above the feet does not jump (the feet row is clear)",
      not physics.auto_jump_wanted(world_with_wall(3, feet_row=1), PLAYER, EAST))
check("the wall is found in the direction of the walk (north, -z)",
      physics.auto_jump_wanted(lambda x, y, z: z == -1 and y == 0, (0.5, 0.0, 0.3), (0.0, -1.0)))

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel auto jump checks passed.")
