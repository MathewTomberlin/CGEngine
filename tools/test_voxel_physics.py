"""
Checks the voxel player physics and block picking (resources/scripts/VoxelPhysics.py) without the engine. The body collides
with the skin the terrain is drawn with: with a solid block under it, the surface is SKIN_ISO (0.3) of a block above the
block's top face, and beside a wall it is 0.2 of a block out from the wall's face.
Usage (from the repo root): python -I tools/test_voxel_physics.py
Exits 0 on success, 1 on any failed check.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelPhysics as p  # noqa: E402

failures = []
BULGE = 1.0 - 2.0 * (p.SKIN_ISO + p.TERRAIN_SHIFT)   # the terrain's skin sits this far out from a face (0.1 of a block)
TOP = 5.0 + BULGE                # the skin above a floor whose top face is at y = 5
WALL = 3.0 - BULGE              # the skin in front of a wall whose face is at x = 3


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
        pos, vel, on_ground = p.step(solid, pos, vel, dt, flying, on_ground)
    return pos, vel, on_ground


# Falling lands on the ground: the feet stop on the skin above the floor.
pos, vel, on_ground = run(floor_world, (0.5, 10.0, 0.5), (0.0, 0.0, 0.0), 2.0)
check("a falling player lands on the ground's skin", on_ground and abs(pos[1] - TOP) < 1e-3)
check("landing stops the fall", vel[1] == 0.0)

# Walking into the wall stops at its skin: the box's near side touches the skin in front of x = 3.
pos, vel, _ = run(floor_world, (1.0, TOP + 0.1, 0.5), (8.0, 0.0, 0.0), 1.0)
check("a wall stops the player at its skin", abs(pos[0] + p.PLAYER_HALF_WIDTH - WALL) < 1e-3)
check("a wall stops the horizontal speed", vel[0] == 0.0)

# A jump leaves the ground and comes back down to the same height.
pos, vel, on_ground = run(floor_world, (0.5, TOP, 0.5), (0.0, p.JUMP_SPEED, 0.0), 0.1)
check("a jump leaves the ground", not on_ground and pos[1] > TOP + 0.2)
top = pos[1]
pos, vel, on_ground = run(floor_world, pos, (0.0, 0.0, 0.0), 2.0)
check("the player falls back to the ground after a jump", on_ground and abs(pos[1] - TOP) < 1e-3 and top > TOP)

# Flying ignores gravity.
pos, vel, on_ground = run(floor_world, (0.5, 12.0, 0.5), (0.0, 0.0, 0.0), 1.0, flying=True)
check("flying does not fall", abs(pos[1] - 12.0) < 1e-6)


# A ramp the body can walk up: the ground rises one block for every two blocks along x (block columns, so the skin makes a
# slope of about one block in two). The body walks along +x and must climb it, not stop at the first riser.
def ramp(x, y, z):
    return y < 5 + x // 2


# The body drops onto the ramp and keeps walking in the same run: a run that starts in the air does not climb.
pos, vel, on_ground = run(ramp, (2.5, 8.0, 0.5), (2.0, 0.0, 0.0), 5.0, flying=False)
check("a player walks up a ramp of blocks", pos[0] > 6.0 and pos[1] > 5.0 + 0.3 * (pos[0] - 2.0) and on_ground)
check("walking up a ramp keeps the body on its surface (outside the skin, on the ground)",
      on_ground and not p.inside(ramp, pos))

# A ledge one block high can be climbed by walking into it; a wall four blocks high cannot.
def ledge(x, y, z):
    return y < 5 or (3 <= x < 6 and y < 6)


pos, vel, on_ground = run(ledge, (1.0, TOP, 0.5), (3.0, 0.0, 0.0), 1.2)
check("a player walks up onto a one-block ledge (its top is at 6, the skin on it)",
      pos[0] > 4.0 and abs(pos[1] - (6.0 + BULGE)) < 1e-3 and on_ground)
pos, vel, on_ground = run(floor_world, (1.0, TOP, 0.5), (3.0, 0.0, 0.0), 2.0)
# The corners of a wall fill in: its foot rises as a one-block fillet, so the body stands on the fillet against the wall's
# face, below the wall's top (9). It does not get over the wall.
check("a four-block wall is not climbed", pos[0] < WALL and pos[1] < 7.0)

# A box inside the ground is inside the skin, and one standing on the ground is not.
check("a box inside the ground is inside the skin", p.inside(floor_world, (0.5, 4.0, 0.5)))
check("a box standing on the ground is outside the skin", not p.inside(floor_world, (0.5, TOP + 1e-3, 0.5)))
check("a box next to the wall is outside the skin", not p.inside(floor_world, (1.0, TOP + 1e-3, 0.5)))
check("the eye is part of the body: a box with its eye in a wall is inside", p.inside(floor_world, (2.9, 5.5, 0.5)))


def one_block(x, y, z):
    return (x, y, z) == (3, 0, 0)


# The terrain's density is the larger of its share of the 2 by 2 by 2 blocks at each corner, less 0.15, interpolated between the
# corners, and 0.6 times the occupancy of the block centres (VoxelCentreWeight): a slab of terrain has 0.85 deep inside, 0.35 on its
# top face, and the skin (the iso, 0.3) 0.1 of a block out from the face. A single block is a skin of its own, its faces on the iso.
def slab(x, y, z):
    return p.TERRAIN_SKIN if y <= 0 else 0


check("the density is 0.85 deep inside a slab of terrain", abs(p.density(slab, 3.5, -2.5, 0.5) - 0.85) < 1e-9)
check("the density is 0.35 on a slab's top face", abs(p.density(slab, 3.5, 1.0, 0.5) - 0.35) < 1e-9)
check("the skin is 0.1 of a block above a slab's top face", abs(p.density(slab, 3.5, 1.0 + BULGE, 0.5) - p.SKIN_ISO) < 1e-9)
check("the density is under the iso above a slab", p.density(slab, 3.5, 2.5, 0.5) < p.SKIN_ISO)
check("a single block of terrain is inside its own skin", p.density(one_block, 3.5, 0.5, 0.5) > p.SKIN_ISO)
check("a single block's face is on the skin (half way between block centres)", abs(p.density(one_block, 3.0, 0.5, 0.5) - p.SKIN_ISO) < 1e-9)


# A wall one block thick, standing on the slab: its middle stays inside the skin (a wider smoothing would thin it away).
def thin_wall(x, y, z):
    return p.TERRAIN_SKIN if (y <= 0 or x == 3) else 0


check("a one-block wall of terrain is solid through its middle", p.density(thin_wall, 3.5, 2.5, 0.5) > p.SKIN_ISO)

# Skins (VoxelWorld::skinAt): a cube's skin is the cell it is in, so the density is 1 inside and 0 at the next cell's edge.
def one_brick(x, y, z):
    return p.CUBE_SKIN + 18 if (x, y, z) == (3, 0, 0) else 0


check("a cube's density is 1 inside its cell", p.density(one_brick, 3.9999, 0.5, 0.5) == 1.0)
check("a cube's density is under the iso in the next cell", p.density(one_brick, 4.0, 0.5, 0.5) < p.SKIN_ISO)


# Each smooth skin is its own: a leaf above a slab of terrain has density 1 at its centre, which is the larger of its own skin and
# the terrain's, not their sum.
def slab_and_leaf(x, y, z):
    if (x, y, z) == (1, 1, 0):
        return p.OWN_SKIN + 8
    return slab(x, y, z)


check("skins take the largest density, they do not add up", abs(p.density(slab_and_leaf, 1.5, 1.5, 0.5) - 1.0) < 1e-9)


# A body lands on a brick at the brick's drawn top face (y = 6), not 0.2 above it as a smooth block would.
def brick_on_floor(x, y, z):
    if y < 5:
        return p.TERRAIN_SKIN
    return p.CUBE_SKIN + 18 if (x, y, z) == (3, 5, 0) else 0


pos, vel, on_ground = run(brick_on_floor, (3.5, 10.0, 0.5), (0.0, 0.0, 0.0), 2.0)
check("a falling player stops on a brick's top face", on_ground and abs(pos[1] - 6.0) < 1e-3)

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

# A creature (a box, half width CREATURE_HALF_WIDTH) walks at 2 blocks a second toward a still player, the fastest pace at the
# frame cap (dt 0.05). It must never overlap the player's box, and it must come to the contact distance and stop there.
def overlaps(a, b):
    return abs(a[0] - b[0]) < p.PLAYER_HALF_WIDTH + p.CREATURE_HALF_WIDTH and abs(a[1] - b[1]) < p.PLAYER_HALF_WIDTH + p.CREATURE_HALF_WIDTH


player_xz = (5.0, 0.0)
never_overlaps = True
creature_xz = (0.0, 0.0)
closest = math.inf
for _ in range(200):
    vx, vz = p.keep_out(2.0, 0.0, player_xz[0] - creature_xz[0], player_xz[1] - creature_xz[1])
    creature_xz = (creature_xz[0] + vx * 0.05, creature_xz[1] + vz * 0.05)
    closest = min(closest, math.hypot(creature_xz[0] - player_xz[0], creature_xz[1] - player_xz[1]))
    never_overlaps = never_overlaps and not overlaps(creature_xz, player_xz)
check("a creature walking at a still player never overlaps its box", never_overlaps)
check("a creature walking at a still player stops at the contact distance", p.CONTACT - 0.01 <= closest <= p.CONTACT + p.CONTACT_SLACK + 0.01)

# A creature already in contact can walk off: a velocity away from the player is kept.
vx, vz = p.keep_out(-2.0, 0.0, 0.5, 0.0)
check("a creature in contact keeps its move away from the player", vx == -2.0 and vz == 0.0)

# A player moving into a creature is stopped at its side; a move that gains distance is not, so a body in contact can step off.
creature = (5.0, 0.0)
check("a player walking into a creature is blocked", p.blocks_move((0.0, 0.0), (4.6, 0.0), creature))
check("a player stepping away from a creature in contact is not blocked", not p.blocks_move((4.5, 0.0), (4.2, 0.0), creature))
check("a player far from a creature is not blocked", not p.blocks_move((0.0, 0.0), (1.0, 0.0), creature))

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel physics checks passed.")
