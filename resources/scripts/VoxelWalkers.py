"""Creatures that wander the voxel terrain. Each is a box mesh that the engine's voxel walker moves once per update
(cge.add_voxel_walker): it falls onto the ground, stops against walls, and does not pass through terrain that has not loaded.
This script is the brain: it picks a heading and a pace each frame, turns when a wall stops a creature, and jumps a block
when it can. A creature is put back at its spawn if it falls out of the world.

Most creatures are timid: they run from the player when it comes close. Some bite: a biter chases the player when it is near
and bites when it is close, which is what lowers the player's health (VoxelGame). A creature can be hit: it has HEALTH hits,
each pushes it away and flashes it red, and when it dies it is taken out of the world and leaves a hide (VoxelGen.HIDE)."""
import math
import random

import cg_engine_bindings as cge
import VoxelGen
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays

HALF_WIDTH = 0.3            # the creature is 0.6 wide and 1.2 tall
HEIGHT = 1.2
PACE = 1.4                  # blocks per second
FLEE_RANGE = 4.5            # a timid creature this close to the player runs away from it
FLEE_PACE = 3.0             # blocks per second while it runs
BUMP_RANGE = 0.8            # two creatures this close turn away from each other
RESPAWN_DEPTH = -10.0       # below this a creature is put back at its spawn point
HEALTH = 3                  # hits a creature takes to die (each hit takes one)
KNOCK_SECONDS = 0.25        # a hit pushes the creature away for this long
KNOCK_PACE = 4.5            # blocks per second while it is pushed back
FLASH_SECONDS = 0.15        # a hit tints the creature for this long
BITER_SHARE = 0.3           # the share of creatures that bite
CHASE_RANGE = 7.0           # a biter this close to the player comes after it
CHASE_PACE = 2.0            # blocks per second while it chases
BITE_RANGE = 1.3            # a biter this close to the player bites
BITE_DAMAGE = 1.0           # health the player loses to one bite
BITE_INTERVAL = 1.2         # seconds between a biter's bites
CHUNK = VoxelGen.CHUNK
PALETTE = [(0.92, 0.90, 0.85), (0.55, 0.36, 0.22), (0.20, 0.20, 0.24), (0.85, 0.62, 0.30)]
BITER_COLOUR = (0.42, 0.10, 0.08)
FLASH_COLOUR = (1.0, 0.35, 0.25)


class Walker:
    def __init__(self, world, name, spawn, colour, rng, biter=False):
        self.name = name
        self.spawn = spawn
        self.rng = rng
        self.biter = biter
        self.health = HEALTH
        self.heading = rng.uniform(0.0, 2.0 * math.pi)
        self.timer = rng.uniform(1.0, 4.0)
        self.last = None            # the feet last frame, to see how far the creature got
        self.knock_timer = 0.0      # seconds left of being pushed back by a hit
        self.knock_heading = 0.0
        self.flash_timer = 0.0      # seconds left of the hit's red flash
        self.bite_timer = 0.0       # seconds until a biter may bite again
        self.removed = False        # taken out of the world (it is dead): its walker and mesh are gone
        self.arrays = boxes_arrays([((-HALF_WIDTH, 0.0, -HALF_WIDTH), (HALF_WIDTH, HEIGHT, HALF_WIDTH), colour)])
        self.flash_arrays = boxes_arrays([((-HALF_WIDTH, 0.0, -HALF_WIDTH), (HALF_WIDTH, HEIGHT, HALF_WIDTH), FLASH_COLOUR)])
        cge.create_mesh_body(name, *self.arrays, materials=["voxel"], position=cge.Vector3f(0.0, 0.0, 0.0))
        cge.find_body(name).get_mesh().set_position(cge.Vector3f(*spawn))
        cge.add_voxel_walker(name, world, HALF_WIDTH, HEIGHT)

    def update(self, dt, player=None):
        """Move the creature one frame. Returns the health a biter takes from the player this frame (0 for most frames)."""
        if self.removed:
            return 0.0
        pos, _vel, on_ground = cge.voxel_walker_state(self.name)
        if pos[1] < RESPAWN_DEPTH:
            cge.set_voxel_position(self.name, *self.spawn)
            self.last = None
            return 0.0
        self.update_flash(dt)
        self.bite_timer = max(0.0, self.bite_timer - dt)
        if self.knock_timer > 0.0:
            # A hit pushes the creature away: it goes straight back, and its own choices wait until the push ends.
            self.knock_timer -= dt
            cge.set_voxel_velocity(self.name, math.cos(self.knock_heading) * KNOCK_PACE, math.sin(self.knock_heading) * KNOCK_PACE)
            self.last = pos
            return 0.0
        bite = 0.0
        pace = PACE
        self.timer -= dt
        distance = math.inf if player is None else math.hypot(pos[0] - player[0], pos[2] - player[1])
        if self.biter and distance < CHASE_RANGE:
            # A biter heads for the player, and bites each time it is close and the last bite has had time to recover.
            self.heading = math.atan2(player[1] - pos[2], player[0] - pos[0])
            self.timer = 1.0
            pace = CHASE_PACE
            if distance < BITE_RANGE and self.bite_timer <= 0.0:
                bite = BITE_DAMAGE
                self.bite_timer = BITE_INTERVAL
        elif not self.biter and distance < FLEE_RANGE:
            # Startled: it runs straight away from the player, and keeps running until it is out of range.
            self.heading = math.atan2(pos[2] - player[1], pos[0] - player[0])
            self.timer = 1.0
            pace = FLEE_PACE
        elif self.timer <= 0.0:
            self.heading += self.rng.uniform(-1.2, 1.2)
            self.timer = self.rng.uniform(1.0, 4.0)
        # A wall stops the creature short of the distance it meant to walk (seen in last frame's move): it turns, and
        # jumps a block if it stands on the ground.
        # A creature held at the player's side is not stuck on a wall: it stands there and bites.
        touching = distance < physics.CONTACT + physics.CONTACT_SLACK
        if self.last is not None and not touching:
            walked = math.hypot(pos[0] - self.last[0], pos[2] - self.last[2])
            if walked < 0.5 * pace * dt and on_ground:
                self.heading += self.rng.choice((-1.0, 1.0)) * self.rng.uniform(math.pi / 3, 2.0 * math.pi / 3)
                cge.voxel_jump(self.name, physics.JUMP_SPEED)
        self.last = pos
        vx, vz = math.cos(self.heading) * pace, math.sin(self.heading) * pace
        if player is not None:
            # The player is a body too: a creature does not walk into it (the same contact rule as the player's own moves).
            vx, vz = physics.keep_out(vx, vz, player[0] - pos[0], player[1] - pos[2])
        cge.set_voxel_velocity(self.name, vx, vz)
        return bite

    def hurt(self, from_xz, damage=1):
        """A hit from the point from_xz (x, z): the creature loses health, is pushed away from the hit and flashes red.
        Returns True when the hit kills it. A creature that is already removed takes no more hits."""
        if self.removed:
            return False
        self.health -= damage
        pos = self.state()[0]
        self.knock_heading = math.atan2(pos[2] - from_xz[1], pos[0] - from_xz[0])
        self.knock_timer = KNOCK_SECONDS
        self.flash_timer = FLASH_SECONDS
        cge.update_mesh_body(self.name, *self.flash_arrays)
        return self.health <= 0

    def update_flash(self, dt):
        """The red flash lasts FLASH_SECONDS: the creature's own colour comes back when it ends."""
        if self.flash_timer > 0.0:
            self.flash_timer -= dt
            if self.flash_timer <= 0.0:
                cge.update_mesh_body(self.name, *self.arrays)

    def remove(self):
        """Take the creature out of the world: its walker and its mesh."""
        if self.removed:
            return
        self.removed = True
        cge.remove_voxel_walker(self.name)
        cge.remove_mesh_body(self.name)

    def box(self):
        """The creature's box as (lo, hi) corners, where it stands now."""
        pos = self.state()[0]
        return ((pos[0] - HALF_WIDTH, pos[1], pos[2] - HALF_WIDTH), (pos[0] + HALF_WIDTH, pos[1] + HEIGHT, pos[2] + HALF_WIDTH))

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
            biter = rng.random() < BITER_SHARE
            colour = BITER_COLOUR if biter else rng.choice(PALETTE)
            self.walkers.append(Walker(world, "%s_%d" % (prefix, i), spot, colour, rng, biter=biter))

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
                    # The creature's box must have room above the ground: no leaves or walls within a block of it.
                    clear = all(not world.is_solid(x + dx, y + 1 + k, z + dz) for dx in (-1, 0, 1) for dz in (-1, 0, 1) for k in (0, 1))
                    if clear and y + 1 > VoxelGen.SEA_LEVEL:
                        return (x + 0.5, float(y + 1), z + 0.5)
                    break
        return None

    def update(self, dt, player=None):
        """player is the (x, z) of the player, which the creatures run from or chase. A creature that is about to bump another
        turns away from it, so the herd does not stack up in one place. Returns the health the biters took from the player."""
        spots = [w.state()[0] for w in self.walkers]
        bites = 0.0
        for i, walker in enumerate(self.walkers):
            bites += walker.update(dt, player)
            for j, other in enumerate(spots):
                if i != j and math.hypot(spots[i][0] - other[0], spots[i][2] - other[2]) < BUMP_RANGE:
                    walker.turn_away(other[0], other[2])
                    break
        return bites

    def remove(self, walker):
        """Take a creature out of the herd and out of the world (when it dies)."""
        if walker in self.walkers:
            self.walkers.remove(walker)
            walker.remove()
