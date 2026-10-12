"""
Voxel sample: a procedural block world in chunks (docs/games/voxels.md).

Attached to the scene root's update domain by resources/scenes/voxels.json. The engine's cge.VoxelWorld generates the
terrain, meshes the chunks and uploads them on background threads, within a time budget each frame. This script asks for
the chunks near the player, unloads far ones, moves the player, and handles mouse look and breaking and placing.

Controls: W A S D walk, Space jumps, Enter toggles flying (then Space rises and R falls). Mouse: left-click captures
the mouse; move it to look, left-click breaks the block under the crosshair, right-click places stone against it, and
Escape releases the mouse. Without the mouse: arrow keys turn and look, J breaks, K places. VOXEL_SEED chooses a world.
"""
import math
import os
import time
import traceback
from array import array

import cg_engine_bindings as cge
from PyScript import PyScript
import StateFile
import VoxelGen
import VoxelPhysics as physics

VIEW_RADIUS = 14            # chunks requested around the player in each direction: (2 * 14 + 1)^2 = 841 chunks
UNLOAD_RADIUS = 16          # chunks farther than this are unloaded
UPLOAD_BUDGET_MS = 2.0      # time per frame spent uploading finished meshes
WALK_SPEED = 5.0            # blocks per second
FLY_SPEED = 14.0
FLY_RISE = 10.0             # blocks per second when flying up or down
TURN_SPEED = 90.0           # degrees per second, for the arrow keys
MOUSE_SENSITIVITY = float(os.environ.get("VOXEL_MOUSE_SENSITIVITY", "0.00625"))   # degrees per pixel
MAX_PITCH = 85.0            # the engine's camera stops at this angle
# VOXEL_FIXED_STEP (seconds) makes every frame advance the same time, so a scripted test holds a key for the same game time
# whatever the frame rate. Unset, a frame advances the real time since the last one.
FIXED_STEP = float(os.environ["VOXEL_FIXED_STEP"]) if os.environ.get("VOXEL_FIXED_STEP") else None
SPAWN_HEIGHT = 60.0         # above the tallest column, so the player drops onto the terrain
RESPAWN_DEPTH = -20.0       # below this the player is put back at the spawn height
# The blocks the number keys 1 to 7 choose from when placing (the hotbar), and the file the F5 and F9 keys save and load.
HOTBAR = [VoxelGen.STONE, VoxelGen.DIRT, VoxelGen.GRASS, VoxelGen.WOOD, VoxelGen.LEAVES, VoxelGen.SAND, VoxelGen.GRAVEL,
          VoxelGen.SNOW, VoxelGen.WATER]
SHADOWS = os.environ.get("VOXEL_SHADOWS", "1") != "0"   # the sun casts shadows (the map covers SHADOW_RADIUS around the camera)
SHADOW_RADIUS = float(os.environ.get("VOXEL_SHADOW_RADIUS", "40"))
HUD_FONT = "fonts/defaultFont.ttf"   # the font for the labels (copied with the resources)
SIM_BUDGET_MS = 1.5          # time per frame for the block physics (falling sand, flowing water)
# Block physics runs in ticks this far apart, and a falling block takes most of a tick to move one cell, so sand and water
# move at a pace the eye can follow: a block falls about four cells a second by default.
SIM_TICK = float(os.environ.get("VOXEL_SIM_TICK", "0.25"))
NEAR_RADIUS = int(os.environ.get("VOXEL_NEAR_RADIUS", "3"))     # chunks around the player that get the finer skin
NEAR_SAMPLES = int(os.environ.get("VOXEL_NEAR_SAMPLES", "2"))   # samples per block there (the far chunks use VOXEL_SMOOTH)
EDITS_FILE = "cg_control/voxel_edits.bin"
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/voxel_state.json
# The outline of the block the crosshair is on (yellow: it is broken), and of the cell a block would be placed in
# (its own colour, brightened). Both are thin bars around the cube's edges, made as runtime meshes.
WIRE_THICKNESS = 0.02
HELP_TEXT = "LMB break  RMB place  WASD move  Space jump  Enter fly  1-9 block  F5/F9 save/load  Esc release"
BREAK_COLOUR = (1.0, 0.85, 0.2)
HIDDEN_BOX = ((-1.0e6, -1.0e6, -1.0e6), (-1.0e6 + 1.0, -1.0e6 + 1.0, -1.0e6 + 1.0))   # where an outline is put when not shown
SMOOTH_RESOLUTION = int(os.environ.get("VOXEL_SMOOTH", "1"))   # samples per block for the smooth skin; 0 draws cubes
DAY_SECONDS = float(os.environ.get("VOXEL_DAY_SECONDS", "240"))   # real seconds for one day and night
START_TIME = float(os.environ.get("VOXEL_TIME", "0.3"))           # time of day at the start: 0 sunrise, 0.25 noon, 0.5 sunset
FOG_END_CHUNKS = VIEW_RADIUS + 0.5   # the fog is complete at the edge of the loaded chunks, so their edge never shows
FOG_START_FRACTION = 0.5             # the fog starts at this fraction of its end distance

# Colours of the sky, the fog and the light, blended by the sun's height.
DAY_SKY = (0.56, 0.76, 0.95)
NIGHT_SKY = (0.03, 0.04, 0.10)
DUSK_SKY = (0.95, 0.56, 0.36)
SUN_LIGHT = (1.0, 0.97, 0.90)
DUSK_LIGHT = (1.0, 0.60, 0.38)
DAY_ZENITH = (0.22, 0.48, 0.90)
NIGHT_ZENITH = (0.01, 0.015, 0.05)
DUSK_ZENITH = (0.30, 0.30, 0.52)
MOON_LIGHT = (0.45, 0.55, 0.85)


def box_faces(lo, hi):
    """The six faces of the box lo..hi, each as (corners, outward normal)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    return [
        ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
        ([(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)], (-1, 0, 0)),
        ([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], (0, 1, 0)),
        ([(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)], (0, -1, 0)),
        ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)),
    ]


def boxes_arrays(boxes):
    """Arrays for solid boxes: each (lo, hi, colour) is a closed box whose faces are wound to face outward."""
    positions, normals, colours, uvs, material_ids, indices = [], [], [], [], [], []
    face_uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    for lo, hi, colour in boxes:
        for corners, normal in box_faces(lo, hi):
            first = len(positions) // 3
            for corner, uv in zip(corners, face_uvs):
                positions.extend(corner)
                normals.extend(normal)
                colours.extend(colour)
                uvs.extend(uv)
                material_ids.append(0.0)
            # Two triangles, wound so their normal faces outward.
            a, b, c = corners[0], corners[1], corners[2]
            u = [b[i] - a[i] for i in range(3)]
            w = [c[i] - a[i] for i in range(3)]
            cross = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
            if sum(cross[i] * normal[i] for i in range(3)) >= 0:
                indices.extend((first, first + 1, first + 2, first, first + 2, first + 3))
            else:
                indices.extend((first, first + 2, first + 1, first, first + 3, first + 2))
    return (array("f", positions), array("f", normals), array("f", colours), array("f", uvs),
            array("f", material_ids), array("I", indices))


def outline_arrays(lo, hi, colour):
    """Arrays for the twelve edges of the box lo..hi, each edge a thin bar that sticks out a little from the box."""
    t = WIRE_THICKNESS
    boxes = []
    for axis in range(3):
        o1, o2 = [i for i in range(3) if i != axis]
        for v1 in (lo[o1], hi[o1]):
            for v2 in (lo[o2], hi[o2]):
                bmin, bmax = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
                bmin[axis], bmax[axis] = lo[axis] - t, hi[axis] + t
                bmin[o1], bmax[o1] = v1 - t, v1 + t
                bmin[o2], bmax[o2] = v2 - t, v2 + t
                boxes.append((bmin, bmax, colour))
    return boxes_arrays(boxes)


def mix(a, b, t):
    """Colour a moved t of the way from a to b (t in 0 to 1)."""
    return tuple(x + (y - x) * t for x, y in zip(a, b))


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
        self.pitch = float(os.environ.get("VOXEL_PITCH", "-25"))   # degrees; negative looks down
        self.target = None              # (block, normal) under the crosshair, or None
        self.keys_was = {}              # previous state of the one-shot keys and buttons
        self.last = None
        self.frames = 0
        self.max_dt = 0.0              # longest frame since the last state write, in seconds
        self.state_timer = 0.0
        self.broken = False
        self.hud_ready = False          # the label and the outline bodies exist (made on the second frame)
        self.paused = False             # the pause menu is open: the world stands still and the mouse is free
        self.outline_keys = {}          # the last outline shown under each name, so unchanged ones are not re-uploaded
        self.outline_bodies = set()     # the outlines already made as runtime meshes
        self.slot = 0                   # the hotbar slot placing uses
        self.clock = START_TIME % 1.0   # fraction of a day
        self.sun = (0.0, -1.0, 0.0)     # the direction the sun's light travels
        self.fog = (0.0, 0.0)           # fog start and end distances, in blocks

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
        if FIXED_STEP is not None:
            raw = 0.0 if self.last is None else FIXED_STEP
        dt = min(0.05, raw)
        raw_dt = min(0.05, raw)
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

        if not self.hud_ready and self.frames >= 2:
            # Made a frame after the world, not during the scene's start: the outlines first, then the labels, so the
            # labels are drawn over the bars.
            self.show_outline("target_break", None, None)
            self.show_outline("target_place", None, None)
            self.setup_hud()
            self.hud_ready = True
        self.update_pause_key()
        if self.paused:
            # The world stands still while the menu is open: no time passes, no block moves, the player does not move.
            self.update_pause_menu()
            dt = 0.0
        else:
            self.world.simulate(dt, SIM_BUDGET_MS, SIM_TICK)
            self.update_mouse()
            self.update_player(dt)
            self.update_target()
            self.edit_blocks()
        self.update_outlines()
        self.update_hud()
        self.update_sky(dt)
        self.place_camera()

        self.state_timer -= raw_dt   # the state keeps being written while paused, so tests can read the menu
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()

    # ----- world --------------------------------------------------------------------------

    def setup(self):
        self.world = cge.VoxelWorld(VoxelGen.CHUNK, VoxelGen.HEIGHT, "voxel")
        for block, rgb in VoxelGen.COLOURS.items():
            self.world.define_block(block, cge.Vector3f(*rgb), solid=block not in VoxelGen.LIQUIDS,
                                    falls=block in VoxelGen.FALLS, liquid=block in VoxelGen.LIQUIDS)
        self.world.set_smoothing(SMOOTH_RESOLUTION)
        self.world.set_move_seconds(SIM_TICK * 0.9)   # a falling block lands just before the next tick
        # Smooth skin nearest the camera is finer: NEAR_SAMPLES per block within NEAR_RADIUS chunks.
        self.world.set_detail(NEAR_RADIUS, NEAR_SAMPLES)
        cge.set_environment(shadows=SHADOWS, shadow_radius=SHADOW_RADIUS)
        self.world.set_terrain(self.seed, VoxelGen.SEA_LEVEL, 8, 44, VoxelGen.SNOW_LEVEL, VoxelGen.TREE_CHANCE,
                          VoxelGen.CAVE_WIDTH)

    def setup_hud(self):
        """Two labels: the top left holds what the crosshair is on and the controls; the hotbar sits along the bottom."""
        cge.load_font(HUD_FONT, "hud")
        # Each label is one text draw, which costs the renderer a state switch, so there are only two.
        cge.create_text_body("hud_target", "", "hud", cge.Vector2f(14, 10), 16)
        cge.create_text_body("hud_help", HELP_TEXT, "hud", cge.Vector2f(14, 34), 14, cge.Color(200, 200, 200))
        cge.create_text_body("hud_hotbar", "", "hud", cge.Vector2f(14, cge.window_size().y - 34), 16)
        self.target_text = ""
        self.hotbar_text = ""
        # The pause menu: a title and two items, drawn only while the game is paused (their text is empty otherwise).
        self.menu_text = {}
        self.menu_hover = None
        # Centred in the window. Each item is a rectangle (left, top, right, bottom) in window pixels for clicks, and its
        # text starts at the rectangle's left.
        size = cge.window_size()
        cx, cy = size.x / 2, size.y / 2
        self.menu_rects = {
            "resume": (cx - 100, cy - 4, cx + 100, cy + 30),
            "quit": (cx - 100, cy + 40, cx + 100, cy + 74),
        }
        cge.create_text_body("pause_title", "", "hud", cge.Vector2f(cx - 80, cy - 60), 28)
        cge.create_text_body("pause_resume", "", "hud", cge.Vector2f(cx - 62, cy), 22)
        cge.create_text_body("pause_quit", "", "hud", cge.Vector2f(cx - 40, cy + 44), 22)

    def set_menu_text(self, name, text):
        if self.menu_text.get(name) != text:
            self.menu_text[name] = text
            cge.set_text_body(name, text)

    def show_menu(self, shown):
        """The menu's labels are drawn while the game is paused, and are empty (so not drawn) otherwise."""
        self.set_menu_text("pause_title", "PAUSED" if shown else "")
        self.set_menu_text("pause_resume", self.menu_item("resume", "Resume") if shown else "")
        self.set_menu_text("pause_quit", self.menu_item("quit", "Quit") if shown else "")

    def menu_item(self, item, label):
        return "> %s <" % label if self.menu_hover == item else label

    def update_pause_key(self):
        if self.pressed("Pause", bool(cge.key_down("Escape"))):
            if self.paused:
                self.resume()
            else:
                self.pause()

    def pause(self):
        self.paused = True
        cge.set_mouse_captured(False)
        self.mouse_captured = False
        self.menu_hover = None
        self.show_menu(True)

    def resume(self):
        self.paused = False
        cge.set_mouse_captured(True)
        self.mouse_captured = True
        self.show_menu(False)

    def update_pause_menu(self):
        """Hover and click the menu: Resume locks the mouse again, Quit closes the window."""
        mouse_x, mouse_y = cge.mouse_position()
        self.menu_hover = None
        for item, (left, top, right, bottom) in self.menu_rects.items():
            if left <= mouse_x <= right and top <= mouse_y <= bottom:
                self.menu_hover = item
        clicked = self.pressed("MenuClick", bool(cge.mouse_down("Left")))
        if clicked and self.menu_hover == "resume":
            self.resume()
            return
        if clicked and self.menu_hover == "quit":
            cge.quit()
            return
        self.show_menu(True)

    def set_target_text(self, text):
        if self.target_text != text:
            self.target_text = text
            cge.set_text_body("hud_target", text)

    def set_hotbar(self, text):
        if self.hotbar_text != text:
            self.hotbar_text = text
            cge.set_text_body("hud_hotbar", text)

    def update_hud(self):
        if not self.hud_ready:
            return
        if self.paused:
            self.set_target_text("")
        elif not self.mouse_captured:
            self.set_target_text("Click the game to capture the mouse")
        elif self.target is None:
            self.set_target_text("Looking at: nothing in reach")
        else:
            block, normal = self.target
            text = "Looking at: %s" % VoxelGen.NAMES.get(self.world.get_block(*block), "?")
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if normal != (0, 0, 0) and not physics.cell_in_player(cell, self.pos):
                text += "   (right click: %s)" % VoxelGen.NAMES[HOTBAR[self.slot]]
            self.set_target_text(text)
        slots = []
        for slot, block in enumerate(HOTBAR):
            label = "%d %s" % (slot + 1, VoxelGen.NAMES[block])
            slots.append("[%s]" % label if slot == self.slot else label)
        self.set_hotbar("  ".join(slots))

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
            # The engine's mover is the same as physics.step (tests check they agree), and it is faster.
            pos, vel, self.on_ground = self.world.step_body(tuple(self.pos), tuple(self.vel), dt, self.flying)
            self.pos = list(pos)
            self.vel = list(vel)
        if self.pos[1] < RESPAWN_DEPTH:
            # Keep the state from the moment of the fall: where the player was, and whether the chunk under them was loaded.
            folder = os.path.abspath("cg_control/snapshots")
            os.makedirs(folder, exist_ok=True)
            stamp = time.strftime("%Y%m%d_%H%M%S")
            self.write_state(os.path.join(folder, "fell_%s.json" % stamp))
            print("[voxel] fell below the kill zone at %s (state saved)" % [round(v, 2) for v in self.pos])
            self.pos = [self.pos[0], SPAWN_HEIGHT, self.pos[2]]
            self.vel = [0.0, 0.0, 0.0]

    def solid(self, x, y, z):
        return self.world.is_solid(x, y, z)

    def eye(self):
        return [self.pos[0], self.pos[1] + physics.EYE_HEIGHT, self.pos[2]]

    def look_direction(self):
        yaw = math.radians(self.yaw)
        pitch = math.radians(self.pitch)
        return (math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch))

    def update_target(self):
        self.target = physics.raycast(self.solid, tuple(self.eye()), self.look_direction())

    def show_outline(self, name, box, colour):
        """Show the outline of box (lo, hi) in colour as the runtime mesh called name, or hide it when box is None."""
        key = (box, colour)
        if self.outline_keys.get(name) == key:
            return
        self.outline_keys[name] = key
        lo, hi = box if box is not None else HIDDEN_BOX
        arrays = outline_arrays(lo, hi, colour if box is not None else (0.0, 0.0, 0.0))
        if name in self.outline_bodies:
            cge.update_mesh_body(name, *arrays)
        else:
            cge.create_mesh_body(name, *arrays, materials=["voxel"], position=cge.Vector3f(0, 0, 0))
            self.outline_bodies.add(name)

    def update_outlines(self):
        """Outline the block the crosshair is on, and the cell a block would go into, in the colour it would have."""
        if self.target is None or self.paused:
            self.show_outline("target_break", None, None)
            self.show_outline("target_place", None, None)
            return
        block, normal = self.target
        self.show_outline("target_break", (tuple(float(v) for v in block),
                                           tuple(float(v + 1) for v in block)), BREAK_COLOUR)
        cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
        if normal == (0, 0, 0) or physics.cell_in_player(cell, self.pos):
            self.show_outline("target_place", None, None)
            return
        colour = mix(VoxelGen.COLOURS.get(HOTBAR[self.slot], (1.0, 1.0, 1.0)), (1.0, 1.0, 1.0), 0.4)
        self.show_outline("target_place", (tuple(float(v) for v in cell),
                                           tuple(float(v + 1) for v in cell)), colour)

    def choose_hotbar(self):
        for slot in range(len(HOTBAR)):
            if self.pressed("Slot%d" % slot, bool(cge.key_down(str(slot + 1)))):
                self.slot = slot

    def snapshot_key(self):
        """F12 saves a screenshot and the game state at that moment, for a bug that is hard to reproduce."""
        if self.pressed("Snapshot", bool(cge.key_down("F12"))):
            folder = os.path.abspath("cg_control/snapshots")
            os.makedirs(folder, exist_ok=True)
            stamp = time.strftime("%Y%m%d_%H%M%S")
            cge.screenshot(os.path.join(folder, "shot_%s.png" % stamp))
            self.write_state(os.path.join(folder, "state_%s.json" % stamp))

    def save_and_load(self):
        if self.pressed("Save", bool(cge.key_down("F5"))):
            os.makedirs("cg_control", exist_ok=True)
            self.world.settle_moves()   # blocks in flight land first, so none is missing from the file
            self.world.save_edits(EDITS_FILE)
        if self.pressed("Load", bool(cge.key_down("F9"))):
            self.world.load_edits(EDITS_FILE)

    def edit_blocks(self):
        self.choose_hotbar()
        self.save_and_load()
        self.snapshot_key()
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
                self.world.set_block(cell[0], cell[1], cell[2], HOTBAR[self.slot])

    # ----- sky ----------------------------------------------------------------------------

    def update_sky(self, dt):
        """The sun crosses the sky in DAY_SECONDS. The light, the sky and the fog follow it: at night the sky is dark and
        the moon gives a dim blue light; at sunrise and sunset the light and the sky turn orange."""
        self.clock = (self.clock + dt / DAY_SECONDS) % 1.0
        angle = 2.0 * math.pi * self.clock
        height = math.sin(angle)                  # -1 at midnight, 0 at sunrise and sunset, 1 at noon
        toward_sun = (0.45 * math.cos(angle), height, 0.3 * math.cos(angle))
        length = math.sqrt(sum(v * v for v in toward_sun))
        toward_sun = [v / length for v in toward_sun]
        travel = [-v for v in toward_sun]            # a directional light travels from the sun

        day = min(1.0, max(0.0, height * 3.0))     # 0 at night, full once the sun is 20 degrees up
        dusk = max(0.0, 1.0 - abs(height) * 6.0)   # strongest at the horizon
        light = mix(mix(MOON_LIGHT, SUN_LIGHT, day), DUSK_LIGHT, dusk * 0.7)
        sky = mix(mix(NIGHT_SKY, DAY_SKY, day), DUSK_SKY, dusk * 0.8)
        zenith = mix(mix(NIGHT_ZENITH, DAY_ZENITH, day), DUSK_ZENITH, dusk * 0.5)
        sun_disc = light if height > 0.0 else (0.0, 0.0, 0.0)   # the disc shows only above the horizon
        sun_disc = mix((0.0, 0.0, 0.0), sun_disc, min(1.0, max(0.0, height * 12.0)))

        fog_end = FOG_END_CHUNKS * VoxelGen.CHUNK
        cge.set_light("sun", direction=cge.Vector3f(*travel), colour=cge.Vector3f(*light),
                      brightness=0.45 + 0.55 * day)
        cge.set_environment(sky=cge.Vector3f(*sky), zenith=cge.Vector3f(*zenith), fog=cge.Vector3f(*sky),
                            fog_start=fog_end * FOG_START_FRACTION, fog_end=fog_end,
                            sun_direction=cge.Vector3f(*toward_sun), sun_colour=cge.Vector3f(*sun_disc),
                            sun_size=1.6)
        self.sun = travel
        self.fog = (fog_end * FOG_START_FRACTION, fog_end)

    def place_camera(self):
        eye = self.eye()
        look = self.look_direction()
        target = [eye[0] + look[0], eye[1] + look[1], eye[2] + look[2]]
        cge.set_camera(cge.Vector3f(*eye), cge.Vector3f(*target))

    # ----- state for tests and agents -----------------------------------------------------

    def write_state(self, path="cg_control/voxel_state.json"):
        """Write the player and the world's counts to cg_control/voxel_state.json, so an agent can check them without a screenshot."""
        stats = self.world.stats()
        basis = cge.camera_basis()
        state = {"chunks": stats["chunks"], "loaded": stats["loaded"], "pending": stats["pending"],
                 "dirty": stats["dirty"], "meshing": stats["meshing"], "quads": stats["quads"],
                 "upload_ms": round(stats["upload_ms"], 3), "edits": stats["edits"], "slot": self.slot,
                 "smoothing": self.world.smoothing(), "active": self.world.active_count(), "paused": self.paused,
                 "seed": self.seed, "frames": self.frames,
                 "player": [round(v, 3) for v in self.pos], "on_ground": self.on_ground, "flying": self.flying,
                 "mouse_captured": self.mouse_captured, "yaw": round(self.yaw, 3), "pitch": round(self.pitch, 3),
                 "max_frame_ms": round(self.max_dt * 1000.0, 2),
                 "time_of_day": round(self.clock, 5), "sun": [round(v, 5) for v in self.sun],
                 "fog": [round(v, 3) for v in self.fog],
                 "look": [round(v, 6) for v in self.look_direction()],
                 "camera_forward": [round(v, 6) for v in basis["forward"]],
                 "camera_right": [round(v, 6) for v in basis["right"]],
                 "camera_up": [round(v, 6) for v in basis["up"]],
                 "target": None if self.target is None else list(self.target[0])}
        self.max_dt = 0.0
        StateFile.write_json(path, state)


def create_instance():
    return VoxelGame()
