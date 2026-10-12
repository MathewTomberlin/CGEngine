"""
Movement and block picking for the voxel sample (docs/games/voxels.md). Pure Python: every function takes
solid(x, y, z) -> bool, the occupancy of a block, so tools/test_voxel_physics.py can run them without the engine.

The player is a box, PLAYER_HALF_WIDTH wide, PLAYER_HEIGHT tall, with its feet at pos. A body collides with the skin the
terrain is drawn with, not with the blocks: density() is the same trilinear occupancy the mesh is built from, and a point is
inside the skin where it is above SKIN_ISO. The C++ mover (src/Core/Voxel/VoxelBody.cpp) does the same, operation for
operation, so tools/voxel_world_check.py can compare them. Picking (raycast) still walks the blocks.
"""
import math

PLAYER_HALF_WIDTH = 0.3
PLAYER_HEIGHT = 1.8
EYE_HEIGHT = 1.6
GRAVITY = 28.0          # blocks per second squared
JUMP_SPEED = 9.0        # blocks per second, upwards
MAX_FALL = 40.0         # terminal fall speed
REACH = 6.0             # blocks from the eye that a block can be picked
SKIN_ISO = 0.3          # the skin is where the density is this (VoxelSkinIso in VoxelMesher.h)
STEP_UP = 1.0           # a blocked walk on the ground climbs this high (a block), then snaps back to the surface
_SUBSTEP = 0.1          # largest move along one axis in one go
_BISECT_STEPS = 12      # how exactly a blocked move is set flush against the skin


def density(solid, x, y, z):
    """The skin's density at (x, y, z): the occupancy of the eight block centres around the point, weighted trilinearly."""
    bx = x - 0.5
    by = y - 0.5
    bz = z - 0.5
    fx = math.floor(bx)
    fy = math.floor(by)
    fz = math.floor(bz)
    tx = bx - fx
    ty = by - fy
    tz = bz - fz
    x0 = int(fx)
    y0 = int(fy)
    z0 = int(fz)
    value = 0.0
    for c in range(8):
        dx = c & 1
        dy = (c >> 1) & 1
        dz = (c >> 2) & 1
        w = (tx if dx else 1.0 - tx) * (ty if dy else 1.0 - ty) * (tz if dz else 1.0 - tz)
        if solid(x0 + dx, y0 + dy, z0 + dz):
            value += w
    return value


def inside(solid, pos):
    """True if any point of the body (its feet, middle, head and eye) is inside the skin."""
    x, y, z = pos
    hw = PLAYER_HALF_WIDTH
    heights = (y, y + PLAYER_HEIGHT * 0.5, y + PLAYER_HEIGHT - 0.02, y + EYE_HEIGHT)
    across = ((-hw, -hw), (hw, -hw), (-hw, hw), (hw, hw), (0.0, 0.0))
    for h in heights:
        for ox, oz in across:
            if density(solid, x + ox, h, z + oz) > SKIN_ISO:
                return True
    return False


def _move_axis(solid, pos, axis, delta):
    """
    Move the body along one axis by delta, in steps of at most _SUBSTEP. When a step would enter the skin, the body is set
    flush against it by bisection. Returns (position, blocked).
    """
    pos = list(pos)
    steps = max(1, int(math.ceil(abs(delta) / _SUBSTEP)))
    part = delta / steps
    for _ in range(steps):
        candidate = list(pos)
        candidate[axis] += part
        if inside(solid, candidate):
            free = 0.0
            blocked = 1.0
            for _ in range(_BISECT_STEPS):
                mid = (free + blocked) * 0.5
                probe = list(pos)
                probe[axis] += part * mid
                if inside(solid, probe):
                    blocked = mid
                else:
                    free = mid
            pos[axis] += part * free
            return pos, True
        pos = candidate
    return pos, False


def _horizontal(a, b):
    dx = b[0] - a[0]
    dz = b[2] - a[2]
    return math.sqrt(dx * dx + dz * dz)


def cell_in_player(cell, pos):
    """True if the unit block at integer cell overlaps the player's box at pos. Touching faces do not count."""
    x, y, z = pos
    return (cell[0] < x + PLAYER_HALF_WIDTH and cell[0] + 1 > x - PLAYER_HALF_WIDTH
            and cell[1] < y + PLAYER_HEIGHT and cell[1] + 1 > y
            and cell[2] < z + PLAYER_HALF_WIDTH and cell[2] + 1 > z - PLAYER_HALF_WIDTH)


def step(solid, pos, vel, dt, flying=False, on_ground=False):
    """
    One physics step. vel is (vx, vy, vz) in blocks per second: the horizontal part is set by the caller each frame
    (the input), and the vertical part falls under gravity unless flying. on_ground is whether the body stood on the
    skin at the last step (it lets the body climb a step). Returns (pos, vel, on_ground).
    """
    vx, vy, vz = vel
    rising = vy > 0.0
    if not flying:
        vy = max(-MAX_FALL, vy - GRAVITY * dt)
    # A body that starts inside the skin (it was placed there, or the ground rose under it) is lifted out first.
    start = list(pos)
    for _ in range(40):
        if not inside(solid, start):
            break
        start[1] += 0.05

    # The walk: straight on, x then z. If that is blocked on the ground (and not rising), try again a step higher, and keep
    # that walk if it went further. Then snap back down onto whatever surface is under the new place.
    flat, blocked_x = _move_axis(solid, start, 0, vx * dt)
    flat, blocked_z = _move_axis(solid, flat, 2, vz * dt)
    chosen = flat
    new_on_ground = False
    if (blocked_x or blocked_z) and on_ground and not flying and not rising:
        raised = list(start)
        raised[1] += STEP_UP
        if not inside(solid, raised):
            up, up_x = _move_axis(solid, raised, 0, vx * dt)
            up, up_z = _move_axis(solid, up, 2, vz * dt)
            up, snapped = _move_axis(solid, up, 1, -(STEP_UP + 0.05))
            if _horizontal(start, up) > _horizontal(start, flat):
                chosen = up
                blocked_x = up_x
                blocked_z = up_z
                new_on_ground = snapped
    if blocked_x:
        vx = 0.0
    if blocked_z:
        vz = 0.0

    # The fall or the jump. Landing (a blocked move downwards) is standing on the ground.
    pos, blocked_y = _move_axis(solid, chosen, 1, vy * dt)
    if blocked_y:
        if vy < 0.0:
            new_on_ground = True
        vy = 0.0
    return tuple(pos), (vx, vy, vz), new_on_ground


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
