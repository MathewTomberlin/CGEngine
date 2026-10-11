"""
Voxel sample: a procedural block world in chunks (docs/games/voxels.md).

Attached to the scene root's update domain by resources/scenes/voxels.json. The engine's cge.VoxelWorld generates the
terrain, meshes the chunks and uploads them on background threads, within a time budget each frame. This script asks for
the chunks near the player, unloads far ones, moves the player, and handles mouse look and breaking and placing.

Controls: W A S D walk, Space jumps, Enter toggles flying (then Space rises and R falls). Mouse: left-click captures
the mouse; move it to look, left-click breaks the block under the crosshair, right-click places stone against it, and
Escape releases the mouse. Without the mouse: arrow keys turn and look, J breaks, K places. VOXEL_SEED chooses a world.
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

VIEW_RADIUS = 6             # chunks requested around the player, in each direction
UNLOAD_RADIUS = 8           # chunks farther than this are unloaded
UPLOAD_BUDGET_MS = 2.0      # time per frame spent uploading finished meshes
WALK_SPEED = 5.0            # blocks per second
FLY_SPEED = 14.0
FLY_RISE = 10.0             # blocks per second when flying up or down
TURN_SPEED = 90.0           # degrees per second, for the arrow keys
MOUSE_SENSITIVITY = 0.12    # degrees per pixel
MAX_PITCH = 85.0            # the engine's camera stops at this angle
SPAWN_HEIGHT = 60.0         # above the tallest column, so the player drops onto the terrain
RESPAWN_DEPTH = -20.0       # below this the player is put back at the spawn height
PLACED_BLOCK = VoxelGen.STONE
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/voxel_state.json


class VoxelGame(PyScript):
    def __init__(self):
        super().__init__()
        self.seed = int(os.environ.get("VOXEL_SEED", str(int(time.time()))))
        self.world = None
        self.requested = set()          # (cx, cz) of the chunks asked for, loaded or not
        self.wanted_centre = None       # chunk the wanted set was last built for
        self.pos = [8.5, SPAWN_HEIGHT, 8.5]   # the player's feet, y up
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.flying = False
        self.mouse_captured = False
        self.yaw = 0.0                  # degrees; 0 looks down -z
        self.pitch = -25.0              # degrees; negative looks down
        self.target = None              # (block, normal) under the crosshair, or None
        self.keys_was = {}              # previous state of the one-shot keys and buttons
        self.last = None
        self.frames = 0
        self.max_dt = 0.0              # longest frame since the last state write, in seconds
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
        raw = 0.0 if self.last is None else now - self.last
        dt = min(0.05, raw)
        self.last = now
        self.frames += 1
        self.max_dt = max(self.max_dt, raw)
        if self.world is None:
            self.setup()

        centre = self.chunk_under_player()
        if centre != self.wanted_centre:
            self.wanted_centre = centre
            self.request_chunks(centre)
        self.world.update(UPLOAD_BUDGET_MS, centre[0], centre[1])

        self.update_mouse()
        self.update_player(dt)
        self.update_target()
        self.edit_blocks()
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
        self.world.set_terrain(self.seed, VoxelGen.SEA_LEVEL, 8, 44, VoxelGen.SNOW_LEVEL)

    def request_chunks(self, centre):
        """Ask for every chunk in the view that is not requested yet, nearest first. Unload the far ones."""
        cx, cz = centre
        wanted = [(cx + dx, cz + dz) for dz in range(-VIEW_RADIUS, VIEW_RADIUS + 1)
                  for dx in range(-VIEW_RADIUS, VIEW_RADIUS + 1)]
        wanted.sort(key=lambda c: (c[0] - cx) ** 2 + (c[1] - cz) ** 2)
        for chunk in wanted:
            if chunk not in self.requested:
                self.world.request_chunk(*chunk)
                self.requested.add(chunk)
        for chunk in list(self.requested):
            if max(abs(chunk[0] - cx), abs(chunk[1] - cz)) > UNLOAD_RADIUS:
                self.world.unload_chunk(*chunk)
                self.requested.discard(chunk)

    def chunk_under_player(self):
        size = VoxelGen.CHUNK
        return math.floor(self.pos[0] / size), math.floor(self.pos[2] / size)

    # ----- input --------------------------------------------------------------------------

    def key(self, name):
        return int(cge.key_down(name))

    def pressed(self, name, down):
        """True only on the frame the key or button goes down."""
        was = self.keys_was.get(name, False)
        self.keys_was[name] = down
        return down and not was

    def update_mouse(self):
        if not self.mouse_captured:
            if self.pressed("MouseLeft", cge.mouse_down("Left")):
                cge.set_mouse_captured(True)
                self.mouse_captured = True
            return
        if self.pressed("Escape", bool(cge.key_down("Escape"))):
            cge.set_mouse_captured(False)
            self.mouse_captured = False
            return
        dx, dy = cge.mouse_delta()
        self.turn(dx * MOUSE_SENSITIVITY, -dy * MOUSE_SENSITIVITY)

    def turn(self, yaw_delta, pitch_delta):
        self.yaw = (self.yaw + yaw_delta) % 360.0
        self.pitch = max(-MAX_PITCH, min(MAX_PITCH, self.pitch + pitch_delta))

    # ----- player -------------------------------------------------------------------------

    def update_player(self, dt):
        if self.pressed("Enter", bool(cge.key_down("Enter"))):
            self.flying = not self.flying
            self.vel[1] = 0.0

        self.turn((self.key("Right") - self.key("Left")) * TURN_SPEED * dt,
                  (self.key("Up") - self.key("Down")) * TURN_SPEED * dt)

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

        # Physics waits until the chunk under the player is in the world, so the player does not fall through air.
        if self.world.is_loaded(*self.chunk_under_player()):
            pos, vel, self.on_ground = physics.step(self.solid, tuple(self.pos), tuple(self.vel), dt, self.flying)
            self.pos = list(pos)
            self.vel = list(vel)
        if self.pos[1] < RESPAWN_DEPTH:
            self.pos = [self.pos[0], SPAWN_HEIGHT, self.pos[2]]
            self.vel = [0.0, 0.0, 0.0]

    def solid(self, x, y, z):
        return self.world.get_block(x, y, z) != 0

    def eye(self):
        return [self.pos[0], self.pos[1] + physics.EYE_HEIGHT, self.pos[2]]

    def look_direction(self):
        yaw = math.radians(self.yaw)
        pitch = math.radians(self.pitch)
        return (math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch))

    def update_target(self):
        self.target = physics.raycast(self.solid, tuple(self.eye()), self.look_direction())

    def edit_blocks(self):
        break_down = self.mouse_captured and bool(cge.mouse_down("Left"))
        place_down = self.mouse_captured and bool(cge.mouse_down("Right"))
        breaking = self.pressed("Break", break_down or bool(cge.key_down("J")))
        placing = self.pressed("Place", place_down or bool(cge.key_down("K")))
        if self.target is None or not (breaking or placing):
            return
        block, normal = self.target
        if breaking:
            self.world.set_block(block[0], block[1], block[2], VoxelGen.AIR)
        elif placing:
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
        """Write the player and the world's counts to cg_control/voxel_state.json, so an agent can check them without a screenshot."""
        stats = self.world.stats()
        state = {"chunks": stats["chunks"], "loaded": stats["loaded"], "pending": stats["pending"],
                 "dirty": stats["dirty"], "meshing": stats["meshing"], "quads": stats["quads"],
                 "upload_ms": round(stats["upload_ms"], 3), "seed": self.seed, "frames": self.frames,
                 "player": [round(v, 3) for v in self.pos], "on_ground": self.on_ground, "flying": self.flying,
                 "mouse_captured": self.mouse_captured, "yaw": round(self.yaw, 3), "pitch": round(self.pitch, 3),
                 "max_frame_ms": round(self.max_dt * 1000.0, 2),
                 "target": None if self.target is None else list(self.target[0])}
        self.max_dt = 0.0
        os.makedirs("cg_control", exist_ok=True)
        tmp = "cg_control/voxel_state.json.tmp"
        with open(tmp, "w") as f:
            json.dump(state, f)
        os.replace(tmp, "cg_control/voxel_state.json")


def create_instance():
    return VoxelGame()
