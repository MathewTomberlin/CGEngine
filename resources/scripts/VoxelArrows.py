"""
Arrows in the voxel sample: the bow's draw and the flight of an arrow, with no engine calls, so tools/test_voxel_arrows.py checks
them without a window. VoxelGame draws each arrow as a body, moves it with step(), and asks it what it hit.

A bow is drawn while the place button is held: the draw grows over DRAW_SECONDS, and letting go fires an arrow whose speed and
damage grow with the draw. An arrow falls under gravity; it stops when it reaches a solid block or a creature, or after LIFE_SECONDS.
"""
import math

DRAW_SECONDS = 1.0          # a full draw takes this long (seconds of game time)
SPEED_MIN = 12.0            # blocks a second at no draw
SPEED_MAX = 30.0            # blocks a second at a full draw
DAMAGE_MIN = 1.0            # a creature's health taken at no draw (the hand takes 1)
DAMAGE_MAX = 3.0            # at a full draw: a creature with 3 health dies in one arrow
GRAVITY = 9.8               # blocks a second squared, downwards
LIFE_SECONDS = 6.0          # an arrow that has not landed by then is gone
SUBSTEP = 0.1               # the most blocks an arrow moves between two tests, so a thin wall is not passed through


def charge_of(seconds):
    """How far a draw has gone, 0 (just drawn) to 1 (full): the draw time over DRAW_SECONDS, capped."""
    return max(0.0, min(1.0, seconds / DRAW_SECONDS))


def launch(origin, look, charge):
    """The position and velocity an arrow fired at this charge starts with: along the look direction (a unit vector)."""
    speed = SPEED_MIN + (SPEED_MAX - SPEED_MIN) * charge
    velocity = [look[i] * speed for i in range(3)]
    return list(origin), velocity


def damage_of_charge(charge):
    """The health an arrow takes from a creature at this charge."""
    return DAMAGE_MIN + (DAMAGE_MAX - DAMAGE_MIN) * charge


def step(pos, velocity, dt, solid):
    """Move an arrow by dt seconds under gravity, a substep at a time. solid(x, y, z) says whether a block stops it. Returns
    (pos, velocity, landed_at) where landed_at is the last open point before a solid block (the arrow lands there), or None while it
    is still flying. The velocity is updated by gravity over the step."""
    pos = list(pos)
    velocity = list(velocity)
    left = dt
    while left > 1e-9:
        # A substep is at most SUBSTEP blocks long, so a fast arrow is tested as often as a slow one.
        speed = math.sqrt(sum(v * v for v in velocity))
        sub = min(SUBSTEP / speed if speed > 0.0 else left, left)
        left -= sub
        velocity[1] -= GRAVITY * sub
        before = list(pos)
        for i in range(3):
            pos[i] += velocity[i] * sub
        cell = (math.floor(pos[0]), math.floor(pos[1]), math.floor(pos[2]))
        if solid(*cell):
            return before, velocity, before
    return pos, velocity, None


def hits_box(pos, lo, hi, margin=0.05):
    """Whether the point pos is inside the box lo..hi (its corners), grown by margin on every side."""
    return all(lo[i] - margin <= pos[i] <= hi[i] + margin for i in range(3))
