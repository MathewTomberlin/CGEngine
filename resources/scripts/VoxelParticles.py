"""Bursts of small cubes for the voxel sample's hits and breaks (roadmap 16.3): a hit throws a few chips of the block's colour,
a break throws more, and a creature's death throws its own colour. Each burst is one runtime mesh body (cge.create_mesh_body)
that is rebuilt each frame with its cubes where they are now, and removed when its time is up. The cubes fall under gravity and
shrink as they fade. There are no engine particles yet: this is the small system the roadmap asked for, and it costs one mesh
upload a frame per live burst."""
import math

import cg_engine_bindings as cge
from VoxelShapes import boxes_arrays

GRAVITY = 14.0              # blocks per second squared
SPEED_MIN = 1.5             # blocks per second a chip leaves its burst at
SPEED_MAX = 4.0
MAX_BURSTS = 8              # live bursts at once; the oldest is removed to make room


class Burst:
    def __init__(self, name, centre, colour, count, size, life, rng, reuse=False):
        self.name = name
        self.colour = colour
        self.size = size
        self.life = life
        self.age = 0.0
        self.parts = []             # each chip: [position, velocity], in world blocks and blocks per second
        for _ in range(count):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            speed = rng.uniform(SPEED_MIN, SPEED_MAX)
            up = rng.uniform(0.5, 1.5)
            velocity = [math.cos(angle) * speed, up * speed * 0.6, math.sin(angle) * speed]
            self.parts.append([list(centre), velocity])
        # A burst takes over a body that a finished burst left behind (see Bursts), which costs far less than making one.
        if reuse:
            cge.update_mesh_body(name, *self.arrays(1.0))
        else:
            cge.create_mesh_body(name, *self.arrays(1.0), materials=["voxel"], position=cge.Vector3f(0.0, 0.0, 0.0))

    def boxes(self, scale):
        half = self.size * scale / 2.0
        return [((p[0] - half, p[1] - half, p[2] - half), (p[0] + half, p[1] + half, p[2] + half), self.colour)
                for p, _velocity in self.parts]

    def arrays(self, scale):
        return boxes_arrays(self.boxes(scale))

    def update(self, dt):
        """Move the chips one step and rebuild the burst. Returns False when the burst's time is up (the caller removes it)."""
        self.age += dt
        for part in self.parts:
            part[1][1] -= GRAVITY * dt
            for axis in range(3):
                part[0][axis] += part[1][axis] * dt
        if self.age >= self.life:
            return False
        scale = max(0.0, 1.0 - self.age / self.life)   # the chips shrink as they fade
        cge.update_mesh_body(self.name, *self.arrays(scale))
        return True

    def remove(self):
        # Hidden, not removed: the body stays for the next burst (Bursts keeps a few).
        cge.update_mesh_body(self.name, *boxes_arrays([((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0))]))


class Bursts:
    """The live bursts. A new burst is named in turn, so each one is its own mesh body."""

    def __init__(self, rng):
        self.rng = rng
        self.live = []
        self.made = 0
        self.spare = []             # the bodies of finished bursts, hidden, for the next burst to take over
        self.started = 0            # bursts started, reused bodies or not (counts the hits and the breaks)

    def add(self, centre, colour, count, size, life):
        if len(self.live) >= MAX_BURSTS:
            self.retire(self.live.pop(0))
        self.started += 1
        if self.spare:
            name, reuse = self.spare.pop(), True
        else:
            self.made += 1
            name, reuse = "burst_%d" % self.made, False
        self.live.append(Burst(name, centre, colour, count, size, life, self.rng, reuse=reuse))

    def retire(self, burst):
        """A finished burst is hidden, and its body kept for the next burst, up to MAX_BURSTS of them; the rest are removed."""
        burst.remove()
        if len(self.spare) < MAX_BURSTS:
            self.spare.append(burst.name)
        else:
            cge.remove_mesh_body(burst.name)

    def update(self, dt):
        """Step every burst by dt (a paused game passes 0, so nothing moves and nothing is rebuilt)."""
        if dt <= 0.0:
            return
        still = []
        for burst in self.live:
            if burst.update(dt):
                still.append(burst)
            else:
                self.retire(burst)
        self.live = still
