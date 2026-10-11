"""
Voxel sample: a procedural block world in chunks (docs/games/voxels.md).

Attached to the scene root's update domain by resources/scenes/voxels.json. VoxelGen makes the terrain, and the engine's
cge.VoxelWorld meshes the chunks. Each frame this script loads the chunks near the camera, unloads far ones, re-meshes
what changed, and moves a free camera.

Controls: W A S D move, arrow keys turn and look, Space rises, R falls. Set VOXEL_SEED to choose a world.
"""
import json
import math
import os
import time
import traceback

import cg_engine_bindings as cge
from PyScript import PyScript
import VoxelGen

VIEW_RADIUS = 4             # chunks loaded around the camera, in each direction
UNLOAD_RADIUS = 6           # chunks farther than this are unloaded
CHUNKS_PER_FRAME = 2        # terrain generated per frame, so loading does not stall the game
REBUILD_PER_FRAME = 4       # chunks re-meshed per frame
MOVE_SPEED = 14.0           # blocks per second
TURN_SPEED = 90.0           # degrees per second
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/voxel_state.json


class VoxelGame(PyScript):
    def __init__(self):
        super().__init__()
        self.seed = int(os.environ.get("VOXEL_SEED", str(int(time.time()))))
        self.world = None
        self.loaded = set()             # (cx, cz) of the chunks in the world
        self.pos = [8.0, 40.0, 8.0]     # camera position, y up
        self.yaw = 0.0                  # degrees; 0 looks down -z
        self.pitch = -25.0              # degrees; negative looks down
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

        self.move_camera(dt)
        self.stream_chunks()
        self.world.rebuild_dirty(REBUILD_PER_FRAME)

        self.state_timer -= dt
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()

    # ----- world --------------------------------------------------------------------------

    def setup(self):
        self.world = cge.VoxelWorld(VoxelGen.CHUNK, VoxelGen.HEIGHT, "voxel")
        for block, rgb in VoxelGen.COLOURS.items():
            self.world.set_colour(block, cge.Vector3f(*rgb))

    def stream_chunks(self):
        cx, cz = self.chunk_under_camera()
        wanted = []
        for dz in range(-VIEW_RADIUS, VIEW_RADIUS + 1):
            for dx in range(-VIEW_RADIUS, VIEW_RADIUS + 1):
                wanted.append((cx + dx, cz + dz))
        # Nearest first, so the ground under the camera appears before the horizon.
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

    def chunk_under_camera(self):
        size = VoxelGen.CHUNK
        return math.floor(self.pos[0] / size), math.floor(self.pos[2] / size)

    # ----- camera -------------------------------------------------------------------------

    def move_camera(self, dt):
        turn = (int(cge.key_down("Right")) - int(cge.key_down("Left"))) * TURN_SPEED * dt
        look = (int(cge.key_down("Up")) - int(cge.key_down("Down"))) * TURN_SPEED * dt
        self.yaw = (self.yaw + turn) % 360.0
        self.pitch = max(-89.0, min(89.0, self.pitch + look))

        yaw = math.radians(self.yaw)
        forward = (math.sin(yaw), 0.0, -math.cos(yaw))
        right = (math.cos(yaw), 0.0, math.sin(yaw))
        forwards = int(cge.key_down("W")) - int(cge.key_down("S"))
        sideways = int(cge.key_down("D")) - int(cge.key_down("A"))
        rises = int(cge.key_down("Space")) - int(cge.key_down("R"))
        step = MOVE_SPEED * dt
        for axis in range(3):
            self.pos[axis] += (forward[axis] * forwards + right[axis] * sideways) * step
        self.pos[1] += rises * step

        pitch = math.radians(self.pitch)
        look_at = [
            self.pos[0] + math.sin(yaw) * math.cos(pitch),
            self.pos[1] + math.sin(pitch),
            self.pos[2] - math.cos(yaw) * math.cos(pitch),
        ]
        cge.set_camera(cge.Vector3f(*self.pos), cge.Vector3f(*look_at))

    # ----- state for tests and agents -----------------------------------------------------

    def write_state(self):
        """Write the world's numbers to cg_control/voxel_state.json, so an agent can check it without a screenshot."""
        state = {"chunks": self.world.chunk_count(), "quads": self.world.quad_count(),
                 "dirty": self.world.dirty_count(), "seed": self.seed, "frames": self.frames,
                 "camera": [round(v, 3) for v in self.pos]}
        os.makedirs("cg_control", exist_ok=True)
        tmp = "cg_control/voxel_state.json.tmp"
        with open(tmp, "w") as f:
            json.dump(state, f)
        os.replace(tmp, "cg_control/voxel_state.json")


def create_instance():
    return VoxelGame()
