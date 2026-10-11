"""
Checks the voxel player physics and block picking (resources/scripts/VoxelPhysics.py) without the engine.
Usage (from the repo root): python -I tools/test_voxel_physics.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelPhysics as p  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


def floor_world(x, y, z):
    """Ground: every block below y = 5, and a wall at x = 3 standing on it."""
    return y < 5 or (x == 3 and 5 <= y < 9)


def run(solid, pos, vel, seconds, flying=False, dt=1.0 / 60.0):
    on_ground = False
    for _ in range(int(seconds / dt)):
        pos, vel, on_ground = p.step(solid, pos, vel, dt, flying)
    return pos, vel, on_ground


# Falling lands on the ground: the feet stop at the top of the floor (y = 5).
pos, vel, on_ground = run(floor_world, (0.5, 10.0, 0.5), (0.0, 0.0, 0.0), 2.0)
check("a falling player lands on the ground", on_ground and abs(pos[1] - 5.0) < 1e-6)
check("landing stops the fall", vel[1] == 0.0)

# Walking into the wall stops at its face: the box's near side touches x = 3 from the left.
pos, vel, _ = run(floor_world, (1.0, 5.0, 0.5), (8.0, 0.0, 0.0), 1.0)
check("a wall stops the player", abs(pos[0] - (3.0 - p.PLAYER_HALF_WIDTH)) < 0.11)
check("a wall stops the horizontal speed", vel[0] == 0.0)

# A jump leaves the ground and comes back down to the same height.
pos, vel, on_ground = run(floor_world, (0.5, 5.0, 0.5), (0.0, p.JUMP_SPEED, 0.0), 0.1)
check("a jump leaves the ground", not on_ground and pos[1] > 5.2)
top = pos[1]
pos, vel, on_ground = run(floor_world, pos, (0.0, 0.0, 0.0), 2.0)
check("the player falls back to the ground after a jump", on_ground and abs(pos[1] - 5.0) < 1e-6 and top > 5.0)

# Flying ignores gravity.
pos, vel, on_ground = run(floor_world, (0.5, 12.0, 0.5), (0.0, 0.0, 0.0), 1.0, flying=True)
check("flying does not fall", abs(pos[1] - 12.0) < 1e-6)

# A box overlaps blocks it stands in and touches, not blocks beside it.
check("a box inside the ground overlaps", p.overlaps(floor_world, 0.5, 4.0, 0.5))
check("a box standing on the ground does not overlap", not p.overlaps(floor_world, 0.5, 5.0, 0.5))
check("a box next to the wall does not overlap", not p.overlaps(floor_world, 2.0, 5.0, 0.5))


def one_block(x, y, z):
    return (x, y, z) == (3, 0, 0)


# Picking: a ray along +x from inside the cell at (0, 0, 0) hits the block at (3, 0, 0) from the -x side.
hit = p.raycast(one_block, (0.5, 0.5, 0.5), (1.0, 0.0, 0.0))
check("a ray hits the first solid block", hit is not None and hit[0] == (3, 0, 0))
check("the hit reports the face it came through", hit is not None and hit[1] == (-1, 0, 0))
check("a block beyond reach is not picked", p.raycast(one_block, (0.5, 0.5, 0.5), (1.0, 0.0, 0.0), max_distance=2.0) is None)

# Looking down at the floor from above hits its top face.
hit = p.raycast(floor_world, (0.5, 7.5, 0.5), (0.0, -1.0, 0.0))
check("looking down picks the floor's top face", hit is not None and hit[0][1] == 4 and hit[1] == (0, 1, 0))

# Diagonal rays walk every cell they cross and still hit the first solid one.
hit = p.raycast(floor_world, (0.5, 7.0, 0.5), (1.0, -1.0, 0.0), max_distance=20.0)
check("a diagonal ray hits the floor", hit is not None and hit[0][1] == 4)

# A block cannot be placed on the player: a cell the box stands in is refused, one in front of it is allowed.
check("a cell the player stands in is refused", p.cell_in_player((0, 5, 0), (0.5, 5.0, 0.5)))
check("the cell in front of the player is allowed", not p.cell_in_player((1, 5, 0), (0.5, 5.0, 0.5)))
check("the cell touching the feet from below is allowed", not p.cell_in_player((0, 4, 0), (0.5, 5.0, 0.5)))

# Sky is not picked.
check("looking at the sky picks nothing", p.raycast(floor_world, (0.5, 20.0, 0.5), (0.0, 1.0, 0.0)) is None)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel physics checks passed.")
