"""
Movement and block picking for the voxel sample (docs/games/voxels.md). Pure Python: every function takes
solid(x, y, z) -> bool, so tools/test_voxel_physics.py can run them without the engine.

The player is a box, PLAYER_HALF_WIDTH wide, PLAYER_HEIGHT tall, with its feet at pos. Blocks are unit cubes whose
corner is at integer coordinates.
"""
import math

PLAYER_HALF_WIDTH = 0.3
PLAYER_HEIGHT = 1.8
EYE_HEIGHT = 1.6
GRAVITY = 28.0          # blocks per second squared
JUMP_SPEED = 9.0        # blocks per second, upwards
MAX_FALL = 40.0         # terminal fall speed
REACH = 6.0             # blocks from the eye that a block can be picked
_EPSILON = 1e-6
_SUBSTEP = 0.1          # largest move along one axis in one go, so the box cannot pass through a block


def overlaps(solid, x, y, z):
    """True if the player's box with its feet at (x, y, z) touches a solid block."""
    x0 = math.floor(x - PLAYER_HALF_WIDTH)
    x1 = math.floor(x + PLAYER_HALF_WIDTH - _EPSILON)
    y0 = math.floor(y)
    y1 = math.floor(y + PLAYER_HEIGHT - _EPSILON)
    z0 = math.floor(z - PLAYER_HALF_WIDTH)
    z1 = math.floor(z + PLAYER_HALF_WIDTH - _EPSILON)
    for bx in range(x0, x1 + 1):
        for by in range(y0, y1 + 1):
            for bz in range(z0, z1 + 1):
                if solid(bx, by, bz):
                    return True
    return False


def _touching(value, axis, direction):
    """The position on this axis where the box just touches the solid block it ran into (moving in direction)."""
    if axis == 1:
        if direction < 0:
            return math.floor(value) + 1.0                                       # feet on top of the block
        return math.floor(value + PLAYER_HEIGHT - _EPSILON) - PLAYER_HEIGHT      # head under the block
    if direction < 0:
        return math.floor(value - PLAYER_HALF_WIDTH) + 1.0 + PLAYER_HALF_WIDTH   # side against the block
    return math.floor(value + PLAYER_HALF_WIDTH - _EPSILON) - PLAYER_HALF_WIDTH


def _move_axis(solid, pos, axis, delta):
    """
    Move the box along one axis by delta, in small steps. When a step would overlap a solid block, the box is set
    flush against that block, so it does not hover a fraction of a step above the ground. Returns (position, blocked).
    """
    pos = list(pos)
    steps = max(1, int(math.ceil(abs(delta) / _SUBSTEP)))
    part = delta / steps
    for _ in range(steps):
        candidate = list(pos)
        candidate[axis] += part
        if overlaps(solid, *candidate):
            pos[axis] = _touching(candidate[axis], axis, part)
            return pos, True
        pos = candidate
    return pos, False


def cell_in_player(cell, pos):
    """True if the unit block at integer cell overlaps the player's box at pos. Touching faces do not count."""
    x, y, z = pos
    return (cell[0] < x + PLAYER_HALF_WIDTH and cell[0] + 1 > x - PLAYER_HALF_WIDTH
            and cell[1] < y + PLAYER_HEIGHT and cell[1] + 1 > y
            and cell[2] < z + PLAYER_HALF_WIDTH and cell[2] + 1 > z - PLAYER_HALF_WIDTH)


def step(solid, pos, vel, dt, flying=False):
    """
    One physics step. vel is (vx, vy, vz) in blocks per second: the horizontal part is set by the caller each frame
    (the input), and the vertical part falls under gravity unless flying. Returns (pos, vel, on_ground).
    """
    vx, vy, vz = vel
    if not flying:
        vy = max(-MAX_FALL, vy - GRAVITY * dt)
    on_ground = False
    pos, blocked = _move_axis(solid, pos, 0, vx * dt)
    if blocked:
        vx = 0.0
    pos, blocked = _move_axis(solid, pos, 2, vz * dt)
    if blocked:
        vz = 0.0
    pos, blocked = _move_axis(solid, pos, 1, vy * dt)
    if blocked:
        if vy < 0.0:
            on_ground = True
        vy = 0.0
    return tuple(pos), (vx, vy, vz), on_ground


def raycast(solid, origin, direction, max_distance=REACH):
    """
    The first solid block along a ray (Amanatides and Woo's grid walk). Returns (block, normal) where block is the
    integer cell hit and normal points from the block back towards the ray, so block + normal is the air cell in
    front of the face that was hit. Returns None if nothing is hit within max_distance.
    """
    length = math.sqrt(sum(c * c for c in direction))
    if length == 0.0:
        return None
    d = [c / length for c in direction]
    cell = [math.floor(c) for c in origin]
    step_axis = [1 if c > 0 else -1 for c in d]
    t_delta = [abs(1.0 / c) if c != 0.0 else math.inf for c in d]
    t_max = []
    for axis in range(3):
        if d[axis] == 0.0:
            t_max.append(math.inf)
        elif d[axis] > 0.0:
            t_max.append((cell[axis] + 1 - origin[axis]) / d[axis])
        else:
            t_max.append((origin[axis] - cell[axis]) / -d[axis])

    if solid(*cell):
        return tuple(cell), (0, 0, 0)
    t = 0.0
    while t <= max_distance:
        axis = min(range(3), key=lambda a: t_max[a])
        t = t_max[axis]
        if t > max_distance:
            break
        cell[axis] += step_axis[axis]
        t_max[axis] += t_delta[axis]
        if solid(*cell):
            normal = [0, 0, 0]
            normal[axis] = -step_axis[axis]
            return tuple(cell), tuple(normal)
    return None
