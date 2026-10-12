"""Creatures that wander the voxel terrain. Each is a box mesh moved by the engine's mover (world.step_body), so it falls
onto the ground, stops against walls, and does not pass through terrain that has not loaded. A creature keeps a heading,
turns after a few seconds or when it meets a wall (jumping a block if it can), and is put back at its spawn if it falls
out of the world."""
import math
import random

import cg_engine_bindings as cge
import VoxelGen
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays

HALF_WIDTH = 0.3            # the creature is 0.6 wide and 1.2 tall
HEIGHT = 1.2
PACE = 1.4                  # blocks per second
RESPAWN_DEPTH = -10.0       # below this a creature is put back at its spawn point
CHUNK = VoxelGen.CHUNK
PALETTE = [(0.92, 0.90, 0.85), (0.55, 0.36, 0.22), (0.20, 0.20, 0.24), (0.85, 0.62, 0.30)]


class Walker:
    def __init__(self, world, name, spawn, colour, rng):
        self.world = world
        self.name = name
        self.spawn = spawn
        self.rng = rng
        self.pos = list(spawn)
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.heading = rng.uniform(0.0, 2.0 * math.pi)
        self.timer = rng.uniform(1.0, 4.0)
        arrays = boxes_arrays([((-HALF_WIDTH, 0.0, -HALF_WIDTH), (HALF_WIDTH, HEIGHT, HALF_WIDTH), colour)])
        cge.create_mesh_body(name, *arrays, materials=["voxel"], position=cge.Vector3f(0.0, 0.0, 0.0))
        self.mesh = cge.find_body(name).get_mesh()
        self.mesh.set_position(cge.Vector3f(*spawn))

    def loaded(self):
        return self.world.is_loaded(math.floor(self.pos[0] / CHUNK), math.floor(self.pos[2] / CHUNK))

    def update(self, dt):
        if self.pos[1] < RESPAWN_DEPTH:
            self.pos, self.vel = list(self.spawn), [0.0, 0.0, 0.0]
            self.mesh.set_position(cge.Vector3f(*self.pos))
            return
        if not self.loaded():
            return
        self.timer -= dt
        if self.timer <= 0.0:
            self.heading += self.rng.uniform(-1.2, 1.2)
            self.timer = self.rng.uniform(1.0, 4.0)
        self.vel[0] = math.cos(self.heading) * PACE
        self.vel[2] = math.sin(self.heading) * PACE
        before = list(self.pos)
        pos, vel, self.on_ground = self.world.step_body(tuple(self.pos), tuple(self.vel), dt, False, HALF_WIDTH, HEIGHT)
        # A wall stops the creature short of the distance it meant to walk: it turns, and jumps a block if it stands.
        walked = math.hypot(pos[0] - before[0], pos[2] - before[2])
        if walked < 0.5 * PACE * dt:
            self.heading += self.rng.choice((-1.0, 1.0)) * self.rng.uniform(math.pi / 3, 2.0 * math.pi / 3)
            if self.on_ground:
                vel = (vel[0], physics.JUMP_SPEED, vel[2])
        self.pos, self.vel = list(pos), list(vel)
        self.mesh.set_position(cge.Vector3f(*self.pos))


class Walkers:
    """A herd of creatures spread over the loaded terrain near centre, each standing on the ground where it is put."""

    def __init__(self, world, centre, count, seed, prefix="creature"):
        self.walkers = []
        rng = random.Random(seed)
        for i in range(count):
            spot = self.find_ground(world, centre, rng)
            if spot is None:
                continue
            colour = rng.choice(PALETTE)
            self.walkers.append(Walker(world, "%s_%d" % (prefix, i), spot, colour, rng))

    @staticmethod
    def find_ground(world, centre, rng, tries=40):
        """A point near centre standing on the top solid block of its column, on loaded terrain above the water."""
        for _ in range(tries):
            x = math.floor(centre[0] + rng.uniform(-12.0, 12.0))
            z = math.floor(centre[1] + rng.uniform(-12.0, 12.0))
            if not world.is_loaded(math.floor(x / CHUNK), math.floor(z / CHUNK)):
                continue
            for y in range(VoxelGen.HEIGHT - 2, 0, -1):
                if world.is_solid(x, y, z):
                    if world.get_block(x, y + 1, z) == 0 and world.get_block(x, y + 2, z) == 0 and y + 1 > VoxelGen.SEA_LEVEL:
                        return (x + 0.5, float(y + 1), z + 0.5)
                    break
        return None

    def update(self, dt):
        for walker in self.walkers:
            walker.update(dt)
