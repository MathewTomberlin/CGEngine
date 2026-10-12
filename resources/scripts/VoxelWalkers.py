"""Creatures that wander the voxel terrain. Each is a box mesh that the engine's voxel walker moves once per update
(cge.add_voxel_walker): it falls onto the ground, stops against walls, and does not pass through terrain that has not loaded.
This script is the brain: it picks a heading and a pace each frame, turns when a wall stops a creature, and jumps a block
when it can. A creature is put back at its spawn if it falls out of the world."""
import math
import random

import cg_engine_bindings as cge
import VoxelGen
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays

HALF_WIDTH = 0.3            # the creature is 0.6 wide and 1.2 tall
HEIGHT = 1.2
PACE = 1.4                  # blocks per second
FLEE_RANGE = 4.5            # a creature this close to the player runs away from it
FLEE_PACE = 3.0             # blocks per second while it runs
BUMP_RANGE = 0.8            # two creatures this close turn away from each other
RESPAWN_DEPTH = -10.0       # below this a creature is put back at its spawn point
CHUNK = VoxelGen.CHUNK
PALETTE = [(0.92, 0.90, 0.85), (0.55, 0.36, 0.22), (0.20, 0.20, 0.24), (0.85, 0.62, 0.30)]


class Walker:
    def __init__(self, world, name, spawn, colour, rng):
        self.name = name
        self.spawn = spawn
        self.rng = rng
        self.heading = rng.uniform(0.0, 2.0 * math.pi)
        self.timer = rng.uniform(1.0, 4.0)
        self.last = None            # the feet last frame, to see how far the creature got
        arrays = boxes_arrays([((-HALF_WIDTH, 0.0, -HALF_WIDTH), (HALF_WIDTH, HEIGHT, HALF_WIDTH), colour)])
        cge.create_mesh_body(name, *arrays, materials=["voxel"], position=cge.Vector3f(0.0, 0.0, 0.0))
        cge.find_body(name).get_mesh().set_position(cge.Vector3f(*spawn))
        cge.add_voxel_walker(name, world, HALF_WIDTH, HEIGHT)

    def update(self, dt, player=None):
        pos, _vel, on_ground = cge.voxel_walker_state(self.name)
        if pos[1] < RESPAWN_DEPTH:
            cge.set_voxel_position(self.name, *self.spawn)
            self.last = None
            return
        pace = PACE
        self.timer -= dt
        if player is not None and math.hypot(pos[0] - player[0], pos[2] - player[1]) < FLEE_RANGE:
            # Startled: it runs straight away from the player, and keeps running until it is out of range.
            self.heading = math.atan2(pos[2] - player[1], pos[0] - player[0])
            self.timer = 1.0
            pace = FLEE_PACE
        elif self.timer <= 0.0:
            self.heading += self.rng.uniform(-1.2, 1.2)
            self.timer = self.rng.uniform(1.0, 4.0)
        # A wall stops the creature short of the distance it meant to walk (seen in last frame's move): it turns, and
        # jumps a block if it stands on the ground.
        if self.last is not None:
            walked = math.hypot(pos[0] - self.last[0], pos[2] - self.last[2])
            if walked < 0.5 * pace * dt and on_ground:
                self.heading += self.rng.choice((-1.0, 1.0)) * self.rng.uniform(math.pi / 3, 2.0 * math.pi / 3)
                cge.voxel_jump(self.name, physics.JUMP_SPEED)
        self.last = pos
        cge.set_voxel_velocity(self.name, math.cos(self.heading) * pace, math.sin(self.heading) * pace)

    def state(self):
        """The creature's feet, velocity and ground contact, as the engine has them now."""
        return cge.voxel_walker_state(self.name)

    def turn_away(self, x, z):
        """Head away from the point (x, z), a little off straight, so two creatures do not turn towards each other."""
        pos = self.state()[0]
        self.heading = math.atan2(pos[2] - z, pos[0] - x) + self.rng.uniform(-0.3, 0.3)
        self.timer = self.rng.uniform(1.0, 2.0)


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

    def update(self, dt, player=None):
        """player is the (x, z) of the player, which the creatures run from when it comes close. A creature that is about to
        bump another turns away from it, so the herd does not stack up in one place."""
        spots = [w.state()[0] for w in self.walkers]
        for i, walker in enumerate(self.walkers):
            walker.update(dt, player)
            for j, other in enumerate(spots):
                if i != j and math.hypot(spots[i][0] - other[0], spots[i][2] - other[2]) < BUMP_RANGE:
                    walker.turn_away(other[0], other[2])
                    break
