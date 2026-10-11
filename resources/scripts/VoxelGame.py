"""
Voxel sample: a procedural block world in chunks (docs/games/voxels.md).

Attached to the scene root's update domain by resources/scenes/voxels.json. VoxelGen makes the terrain, the engine's
cge.VoxelWorld meshes the chunks, and VoxelPhysics moves the player and picks blocks. Each frame this script loads the
chunks near the player, unloads far ones, re-meshes what changed, moves the player, and handles breaking and placing.

Controls: W A S D walk, Space jumps, arrow keys turn and look, Enter toggles flying (then Space rises and R falls),
J breaks the block under the crosshair, K places a stone block against it. Set VOXEL_SEED to choose a world.
"""
import json
import math
import os
import time
import traceback

import cg_engine_bindings as cge
from PyScript import PyScript
import VoxelGen
import VoxelPhysics as physics

VIEW_RADIUS = 4             # chunks loaded around the player, in each direction
UNLOAD_RADIUS = 6           # chunks farther than this are unloaded
CHUNKS_PER_FRAME = 2        # terrain generated per frame, so loading does not stall the game
REBUILD_PER_FRAME = 4       # chunks re-meshed per frame
WALK_SPEED = 5.0            # blocks per second
FLY_SPEED = 14.0
FLY_RISE = 10.0             # blocks per second when flying up or down
TURN_SPEED = 90.0           # degrees per second
RESPAWN_DEPTH = -20.0       # below this the player is put back on the surface
PLACED_BLOCK = VoxelGen.STONE
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/voxel_state.json


class VoxelGame(PyScript):
    def __init__(self):
        super().__init__()
        self.seed = int(os.environ.get("VOXEL_SEED", str(int(time.time()))))
        self.world = None
        self.loaded = set()             # (cx, cz) of the chunks in the world
        self.pos = [8.5, 40.0, 8.5]     # the player's feet, y up
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.flying = False
        self.yaw = 0.0                  # degrees; 0 looks down -z
        self.pitch = -25.0              # degrees; negative looks down
        self.target = None              # (block, normal) under the crosshair, or None
        self.keys_was = {}              # previous state of the one-shot keys (Enter, J, K)
        self.last = None
        self.frames = 0
        self.state_timer = 0.0
        self.broken = False

    # ----- engine entry point -------------------------------------------------------------

    def __call__(self, args):
        if self.broken:
            return
        try:
            self.frame()
        except Exception:
            # Stop quietly after the first failure, but say what went wrong.
            self.broken = True
            print("[voxel] stopped after an error:")
            traceback.print_exc()

    def frame(self):
        now = time.perf_counter()
        dt = 0.0 if self.last is None else min(0.05, now - self.last)
        self.last = now
        self.frames += 1
        if self.world is None:
            self.setup()

        self.stream_chunks()
        self.update_player(dt)
        self.update_target()
        self.edit_blocks()
        self.world.rebuild_dirty(REBUILD_PER_FRAME)
        self.place_camera()

        self.state_timer -= dt
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()

    # ----- world --------------------------------------------------------------------------

    def setup(self):
        self.world = cge.VoxelWorld(VoxelGen.CHUNK, VoxelGen.HEIGHT, "voxel")
        for block, rgb in VoxelGen.COLOURS.items():
            self.world.set_colour(block, cge.Vector3f(*rgb))
        # Start on the surface of the spawn column, not in the air above it.
        self.pos[1] = VoxelGen.column_height(int(self.pos[0]), int(self.pos[2]), self.seed) + 0.5

    def solid(self, x, y, z):
        return self.world.get_block(x, y, z) != 0

    def stream_chunks(self):
        cx, cz = self.chunk_under_player()
        wanted = []
        for dz in range(-VIEW_RADIUS, VIEW_RADIUS + 1):
            for dx in range(-VIEW_RADIUS, VIEW_RADIUS + 1):
                wanted.append((cx + dx, cz + dz))
        # Nearest first, so the ground under the player appears before the horizon.
        wanted.sort(key=lambda c: (c[0] - cx) ** 2 + (c[1] - cz) ** 2)
        made = 0
        for chunk in wanted:
            if made >= CHUNKS_PER_FRAME:
                break
            if chunk in self.loaded:
                continue
            self.load_chunk(*chunk)
            made += 1
        for chunk in list(self.loaded):
            if max(abs(chunk[0] - cx), abs(chunk[1] - cz)) > UNLOAD_RADIUS:
                self.world.unload_chunk(*chunk)
                self.loaded.discard(chunk)

    def load_chunk(self, cx, cz):
        self.world.load_chunk(cx, cz)
        for wx, wz, runs in VoxelGen.chunk_columns(cx, cz, self.seed):
            for y_from, y_to, block in runs:
                self.world.fill_column(wx, wz, y_from, y_to, block)
        self.loaded.add((cx, cz))

    def chunk_under_player(self):
        size = VoxelGen.CHUNK
        return math.floor(self.pos[0] / size), math.floor(self.pos[2] / size)

    # ----- player -------------------------------------------------------------------------

    def key(self, name):
        return int(cge.key_down(name))

    def key_pressed(self, name):
        """True only on the frame the key goes down."""
        down = bool(cge.key_down(name))
        pressed = down and not self.keys_was.get(name, False)
        self.keys_was[name] = down
        return pressed

    def update_player(self, dt):
        if self.key_pressed("Enter"):
            self.flying = not self.flying
            self.vel[1] = 0.0

        turn = (self.key("Right") - self.key("Left")) * TURN_SPEED * dt
        look = (self.key("Up") - self.key("Down")) * TURN_SPEED * dt
        self.yaw = (self.yaw + turn) % 360.0
        self.pitch = max(-89.0, min(89.0, self.pitch + look))

        # Walk along the ground in the direction the player faces (flat, so looking down does not sink the player).
        yaw = math.radians(self.yaw)
        forward = (math.sin(yaw), -math.cos(yaw))
        right = (math.cos(yaw), math.sin(yaw))
        across = self.key("W") - self.key("S")
        sideways = self.key("D") - self.key("A")
        dx = forward[0] * across + right[0] * sideways
        dz = forward[1] * across + right[1] * sideways
        length = math.hypot(dx, dz)
        speed = FLY_SPEED if self.flying else WALK_SPEED
        if length > 0.0:
            dx, dz = dx / length * speed, dz / length * speed
        self.vel[0], self.vel[2] = dx, dz

        if self.flying:
            self.vel[1] = (self.key("Space") - self.key("R")) * FLY_RISE
        elif self.key("Space") and self.on_ground:
            self.vel[1] = physics.JUMP_SPEED

        # Physics runs only once the chunk under the player is in the world; before that the player waits in the air.
        if self.world.is_loaded(*self.chunk_under_player()):
            pos, vel, self.on_ground = physics.step(self.solid, tuple(self.pos), tuple(self.vel), dt, self.flying)
            self.pos = list(pos)
            self.vel = list(vel)
        if self.pos[1] < RESPAWN_DEPTH:
            self.pos[1] = VoxelGen.column_height(int(self.pos[0]), int(self.pos[2]), self.seed) + 0.5
            self.vel = [0.0, 0.0, 0.0]

    def eye(self):
        return [self.pos[0], self.pos[1] + physics.EYE_HEIGHT, self.pos[2]]

    def look_direction(self):
        yaw = math.radians(self.yaw)
        pitch = math.radians(self.pitch)
        return (math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch))

    def update_target(self):
        self.target = physics.raycast(self.solid, tuple(self.eye()), self.look_direction())

    def edit_blocks(self):
        if self.target is None:
            self.key_pressed("J")
            self.key_pressed("K")
            return
        block, normal = self.target
        if self.key_pressed("J"):
            self.world.set_block(block[0], block[1], block[2], VoxelGen.AIR)
        if self.key_pressed("K"):
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if not physics.cell_in_player(cell, self.pos):
                self.world.set_block(cell[0], cell[1], cell[2], PLACED_BLOCK)

    def place_camera(self):
        eye = self.eye()
        look = self.look_direction()
        target = [eye[0] + look[0], eye[1] + look[1], eye[2] + look[2]]
        cge.set_camera(cge.Vector3f(*eye), cge.Vector3f(*target))

    # ----- state for tests and agents -----------------------------------------------------

    def write_state(self):
        """Write the player and the world to cg_control/voxel_state.json, so an agent can check them without a screenshot."""
        state = {"chunks": self.world.chunk_count(), "quads": self.world.quad_count(),
                 "dirty": self.world.dirty_count(), "seed": self.seed, "frames": self.frames,
                 "player": [round(v, 3) for v in self.pos], "on_ground": self.on_ground, "flying": self.flying,
                 "target": None if self.target is None else list(self.target[0])}
        os.makedirs("cg_control", exist_ok=True)
        tmp = "cg_control/voxel_state.json.tmp"
        with open(tmp, "w") as f:
            json.dump(state, f)
        os.replace(tmp, "cg_control/voxel_state.json")


def create_instance():
    return VoxelGame()
