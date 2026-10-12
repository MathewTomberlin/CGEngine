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
import StateFile
import VoxelGen
import VoxelPhysics as physics
from VoxelShapes import mix, outline_arrays
import VoxelWalkers

VIEW_RADIUS = 14            # chunks requested around the player in each direction: (2 * 14 + 1)^2 = 841 chunks
UNLOAD_RADIUS = 16          # chunks farther than this are unloaded
UPLOAD_BUDGET_MS = 2.0      # time per frame spent uploading finished meshes
WALK_SPEED = 5.0            # blocks per second
FLY_SPEED = 14.0
FLY_RISE = 10.0             # blocks per second when flying up or down
# In water (any liquid block at the feet or the chest) the player sinks slowly, swims up with Space and walks slower.
WATER_SPEED = 0.6           # the walking speed in water, as a share of the walk
WATER_SINK = 1.2            # blocks per second the player sinks in water
WATER_RISE = 3.0            # blocks per second the player swims up with Space
WATER_DRAG = 4.0            # how quickly the vertical speed moves to the swim speed (per second)
WATER_FOG = (0.30, 0.58, 0.70)   # the colour of the view under the water (lighter than the water itself, so the bed reads)
WATER_FOG_START = 4.0
WATER_FOG_END = 46.0
TURN_SPEED = 90.0           # degrees per second, for the arrow keys
MOUSE_SENSITIVITY = float(os.environ.get("VOXEL_MOUSE_SENSITIVITY", "0.00625"))   # degrees per pixel at 100%
VIEW_SMOOTH = 14.0          # how quickly the camera follows the eye's height (per second): a climbed block is seen as a step, not a jump
# The settings the player chose, kept between runs in the game's folder. VOXEL_SETTINGS moves the file (the tests use one of their own).
SETTINGS_FILE = os.environ.get("VOXEL_SETTINGS", "voxel_settings.json")
SENSITIVITY_STEPS = [25, 50, 75, 100, 125, 150, 200, 250, 300, 400, 500]   # mouse sensitivity, as a percent of MOUSE_SENSITIVITY
TRACK_CELLS = 39            # the characters of the time-of-day slider's track
MAX_PITCH = 85.0            # the engine's camera stops at this angle
# VOXEL_FIXED_STEP (seconds) makes every frame advance the same time, so a scripted test holds a key for the same game time
# whatever the frame rate. Unset, a frame advances the real time since the last one.
FIXED_STEP = float(os.environ["VOXEL_FIXED_STEP"]) if os.environ.get("VOXEL_FIXED_STEP") else None
SPAWN_HEIGHT = 60.0         # above the tallest column, so the player drops onto the terrain
RESPAWN_DEPTH = -20.0       # below this the player is put back at the spawn height
# The blocks the keys choose from when placing (the hotbar: 1 to 9, then 0, - and = for the tenth to twelfth), and the file the
# F5 and F9 keys save and load.
HOTBAR = [VoxelGen.STONE, VoxelGen.DIRT, VoxelGen.GRASS, VoxelGen.WOOD, VoxelGen.LEAVES, VoxelGen.SAND, VoxelGen.GRAVEL,
          VoxelGen.SNOW, VoxelGen.WATER, VoxelGen.LAMP, VoxelGen.BRICK, VoxelGen.TORCH]
DIRECTION_INDEX = {d: i for i, d in enumerate(VoxelGen.DIRECTIONS)}   # a direction to the dir that a prop uses for it
HOTBAR_KEYS = "1234567890-="


def ray_box(origin, direction, centre, half):
    """The distance along a ray (unit direction) where it enters a box, or None if it misses the box or the box is behind."""
    near, far = -math.inf, math.inf
    for axis in range(3):
        lo = centre[axis] - half[axis]
        hi = centre[axis] + half[axis]
        d = direction[axis]
        if abs(d) < 1e-12:
            if origin[axis] < lo or origin[axis] > hi:
                return None
            continue
        t1 = (lo - origin[axis]) / d
        t2 = (hi - origin[axis]) / d
        near = max(near, min(t1, t2))
        far = min(far, max(t1, t2))
    if near > far or far < 0.0:
        return None
    return max(near, 0.0)


def hotbar_key(slot):
    """The key for a hotbar slot: 1 to 9, then 0, - and =."""
    return HOTBAR_KEYS[slot]
HELD_TORCH_BRIGHTNESS = 2.5     # the light of the torch the player carries (L lights and puts it out)
WALKERS = int(os.environ.get("VOXEL_WALKERS", "12"))   # creatures that wander the terrain near the spawn
SHADOWS = os.environ.get("VOXEL_SHADOWS", "1") != "0"   # the sun casts shadows (the map covers SHADOW_RADIUS around the camera)
SHADOW_RADIUS = float(os.environ.get("VOXEL_SHADOW_RADIUS", "40"))
HUD_OUTLINE = 2.0                 # pixels of black outline round each label, so it reads over sand and grass
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
HELP_TEXT = "LMB break  RMB place  WASD move  Space jump  Enter fly  1-0 - = block  F5/F9 save/load  Esc release"
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


# The pause menu's pages: the title, and the items on each page. The settings page holds the mouse sensitivity.
MENU_PAGES = {
    "pause": ("PAUSED", ("resume", "settings", "quit")),
    "settings": ("SETTINGS", ("sens_down", "sens_value", "sens_up", "time_value", "time_track", "back")),
}
MENU_TEXT = {"resume": "Resume", "settings": "Settings", "quit": "Quit", "back": "Back", "sens_down": "<", "sens_up": ">"}


def load_settings():
    """The saved settings: the mouse sensitivity (a percent, one of SENSITIVITY_STEPS) and the time of day (0 to 1, or None
    if none is saved). Missing or unreadable settings keep their defaults."""
    settings = {"mouse_sensitivity": 100, "time_of_day": None}
    try:
        with open(SETTINGS_FILE) as f:
            data = json.load(f)
        settings["mouse_sensitivity"] = int(data.get("mouse_sensitivity", 100))
        if "time_of_day" in data:
            settings["time_of_day"] = float(data["time_of_day"]) % 1.0
    except (OSError, ValueError, TypeError, AttributeError):
        pass
    settings["mouse_sensitivity"] = min(SENSITIVITY_STEPS, key=lambda step: abs(step - settings["mouse_sensitivity"]))
    return settings


def save_settings(sensitivity, time_of_day):
    StateFile.write_json(SETTINGS_FILE, {"mouse_sensitivity": sensitivity, "time_of_day": round(time_of_day, 5)})


class VoxelGame(PyScript):
    def __init__(self):
        super().__init__()
        self.seed = int(os.environ.get("VOXEL_SEED", str(int(time.time()))))
        self.world = None
        self.requested = set()          # (cx, cz) of the chunks asked for, loaded or not
        self.wanted_centre = None       # chunk the wanted set was last built for
        self.pos = [8.5, SPAWN_HEIGHT, 8.5]   # the player's feet, y up
        if os.environ.get("VOXEL_SPAWN"):
            # VOXEL_SPAWN="x,z" starts the player over that column (for checks and for looking at a place)
            spawn_x, spawn_z = (float(v) for v in os.environ["VOXEL_SPAWN"].split(","))
            self.pos = [spawn_x, SPAWN_HEIGHT, spawn_z]
        self.swimming = False
        self.walkers = None             # the creatures, made once the spawn area has loaded
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.flying = False
        self.mouse_captured = False
        self.yaw = 0.0                  # degrees; 0 looks down -z
        self.pitch = float(os.environ.get("VOXEL_PITCH", "-25"))   # degrees; negative looks down
        self.target = None              # (block, normal) under the crosshair, or None
        self.torch_target = None        # (distance, cell) of the torch under the crosshair, or None
        self.held_lit = False           # the torch the player carries is lit (L toggles it)
        self.keys_was = {}              # previous state of the one-shot keys and buttons
        self.last = None
        self.frames = 0
        self.max_dt = 0.0              # longest frame since the last state write, in seconds
        self.state_timer = 0.0
        self.broken = False
        self.hud_ready = False          # the label and the outline bodies exist (made on the second frame)
        self.paused = False             # the pause menu is open: the world stands still and the mouse is free
        self.frame_dt = 0.0             # the time this frame advances the game (0 while paused)
        self.view_y = None              # the height the camera is at, which follows the eye's height smoothly
        self.menu_page = "pause"        # the page of the pause menu: "pause" or "settings"
        saved = load_settings()
        self.sensitivity = saved["mouse_sensitivity"]   # mouse sensitivity, a percent of MOUSE_SENSITIVITY
        self.dragging = False           # the time-of-day slider is being dragged
        self.outline_keys = {}          # the last outline shown under each name, so unchanged ones are not re-uploaded
        self.outline_bodies = set()     # the outlines already made as runtime meshes
        self.slot = 0                   # the hotbar slot placing uses
        # The time of day: VOXEL_TIME if it is set, else the time the player last chose on the settings page, else START_TIME.
        if "VOXEL_TIME" in os.environ or saved["time_of_day"] is None:
            self.clock = START_TIME % 1.0   # fraction of a day
        else:
            self.clock = saved["time_of_day"]
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
            self.update_walkers(dt)
            self.update_target()
            self.edit_blocks()
        self.update_outlines()
        self.update_hud()
        self.update_sky(dt)
        self.update_held_torch()
        self.frame_dt = dt
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
                                    falls=block in VoxelGen.FALLS, liquid=block in VoxelGen.LIQUIDS,
                                    emits=VoxelGen.EMITS.get(block, 0),
                                    shape="cube" if block in VoxelGen.CUBES else "smooth",
                                    group="own" if block in VoxelGen.OWN else "terrain",
                                    material=VoxelGen.MATERIALS.get(block),
                                    mesh=VoxelGen.WATER if block in VoxelGen.LIQUIDS else 0,
                                    join_above=VoxelGen.JOINS.get(block, 0),
                                    join_below=list(VoxelGen.JOINS_BELOW[block]) if block in VoxelGen.JOINS_BELOW else None,
                                    transparent=block in VoxelGen.TRANSPARENT)
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
        cge.create_text_body("hud_target", "", "hud", cge.Vector2f(14, 10), 16, outline_thickness=HUD_OUTLINE)
        cge.create_text_body("hud_help", HELP_TEXT, "hud", cge.Vector2f(14, 34), 14, cge.Color(200, 200, 200),
                             outline_thickness=HUD_OUTLINE)
        cge.create_text_body("hud_hotbar", "", "hud", cge.Vector2f(14, cge.window_size().y - 34), 16,
                             outline_thickness=HUD_OUTLINE)
        # A small cross at the middle of the window: the block the crosshair is on is outlined, and this shows where the aim is.
        size = cge.window_size()
        cge.create_text_body("hud_cross", "+", "hud", cge.Vector2f(size.x / 2 - 6, size.y / 2 - 13), 22,
                             outline_thickness=HUD_OUTLINE)
        self.target_text = ""
        self.hotbar_text = ""
        # The pause menu, drawn only while the game is paused (its labels are empty otherwise). Each item has a click
        # rectangle (left, top, right, bottom) in window pixels, or None if it is not clickable, and a text position.
        self.menu_text = {}
        self.menu_hover = None
        size = cge.window_size()
        cx, cy = size.x / 2, size.y / 2
        self.menu_layout = {
            "resume": ((cx - 100, cy - 4, cx + 100, cy + 30), (cx - 62, cy)),
            "settings": ((cx - 100, cy + 40, cx + 100, cy + 74), (cx - 62, cy + 44)),
            "quit": ((cx - 100, cy + 84, cx + 100, cy + 118), (cx - 40, cy + 88)),
            "sens_down": ((cx - 215, cy - 24, cx - 150, cy + 6), (cx - 200, cy - 20)),
            "sens_value": (None, (cx - 120, cy - 20)),
            "sens_up": ((cx + 150, cy - 24, cx + 215, cy + 6), (cx + 165, cy - 20)),
            "time_value": (None, (cx - 120, cy + 36)),
            "time_track": ((cx - 150, cy + 60, cx + 150, cy + 86), (cx - 150, cy + 62)),
            "back": ((cx - 100, cy + 110, cx + 100, cy + 144), (cx - 30, cy + 114)),
        }
        cge.create_text_body("menu_title", "", "hud", cge.Vector2f(cx - 80, cy - 60), 28, outline_thickness=HUD_OUTLINE)
        for item, (_rect, (x, y)) in self.menu_layout.items():
            cge.create_text_body("menu_" + item, "", "hud", cge.Vector2f(x, y), 22, outline_thickness=HUD_OUTLINE)

    def set_menu_text(self, name, text):
        if self.menu_text.get(name) != text:
            self.menu_text[name] = text
            cge.set_text_body(name, text)

    def show_menu(self, shown):
        """The labels of the current page are drawn while the game is paused, and are empty (so not drawn) otherwise."""
        title, items = MENU_PAGES[self.menu_page] if shown else ("", ())
        self.set_menu_text("menu_title", title)
        for item in self.menu_layout:
            self.set_menu_text("menu_" + item, self.menu_label(item) if item in items else "")

    def menu_label(self, item):
        if item == "sens_value":
            return "Mouse sensitivity: %d%%" % self.sensitivity
        if item == "time_value":
            minutes = (round(self.clock * 1440) + 360) % 1440   # 0 is sunrise (06:00), 0.25 noon, 0.5 sunset, 0.75 midnight
            return "Time of day: %02d:%02d" % (minutes // 60, minutes % 60)
        if item == "time_track":
            knob = min(TRACK_CELLS - 1, int(self.clock * TRACK_CELLS))
            return "[" + "-" * knob + "O" + "-" * (TRACK_CELLS - 1 - knob) + "]"
        text = MENU_TEXT[item]
        if self.menu_hover == item:
            return "[%s]" % text if item in ("sens_down", "sens_up") else "> %s <" % text
        return text

    def update_pause_key(self):
        if self.pressed("Pause", bool(cge.key_down("Escape"))):
            if not self.paused:
                self.pause()
            elif self.menu_page == "settings":
                self.open_page("pause")
            else:
                self.resume()

    def open_page(self, page):
        self.menu_page = page
        self.menu_hover = None
        self.show_menu(True)

    def change_sensitivity(self, step):
        index = SENSITIVITY_STEPS.index(self.sensitivity) + step
        self.sensitivity = SENSITIVITY_STEPS[max(0, min(len(SENSITIVITY_STEPS) - 1, index))]
        save_settings(self.sensitivity, self.clock)

    def drag_time(self, mouse_x, pressing):
        """The time-of-day slider: while the button is held on the track, the time follows the mouse; the choice is saved
        when the button comes up."""
        left, right = self.menu_layout["time_track"][0][0], self.menu_layout["time_track"][0][2]
        if pressing and (self.dragging or self.menu_hover == "time_track"):
            self.dragging = True
            self.clock = min(0.999, max(0.0, (mouse_x - left) / (right - left)))
        elif self.dragging:
            self.dragging = False
            save_settings(self.sensitivity, self.clock)

    def look_speed(self):
        """Degrees a pixel of mouse movement turns the view."""
        return MOUSE_SENSITIVITY * self.sensitivity / 100.0

    def pause(self):
        self.paused = True
        self.menu_page = "pause"
        self.dragging = False
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
        """Hover and click the menu of the current page. Resume locks the mouse again, Quit closes the window, Settings
        opens the settings page, the arrows change the mouse sensitivity, and Back returns to the pause page."""
        mouse_x, mouse_y = cge.mouse_position()
        self.menu_hover = None
        for item in MENU_PAGES[self.menu_page][1]:
            rect = self.menu_layout[item][0]
            if rect and rect[0] <= mouse_x <= rect[2] and rect[1] <= mouse_y <= rect[3]:
                self.menu_hover = item
        pressing = bool(cge.mouse_down("Left"))
        self.drag_time(mouse_x, pressing)
        clicked = self.pressed("MenuClick", pressing)
        if clicked and self.menu_hover == "resume":
            self.resume()
            return
        if clicked and self.menu_hover == "quit":
            cge.quit()
            return
        if clicked and self.menu_hover == "settings":
            self.open_page("settings")
            return
        if clicked and self.menu_hover == "back":
            self.open_page("pause")
            return
        if clicked and self.menu_hover == "sens_down":
            self.change_sensitivity(-1)
        if clicked and self.menu_hover == "sens_up":
            self.change_sensitivity(1)
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
        # The crosshair is for aiming, so it is hidden while the pause menu is open.
        self.set_menu_text("hud_cross", "" if self.paused else "+")
        biome = self.world.biome_at(math.floor(self.pos[0]), math.floor(self.pos[2])).capitalize()
        if self.paused:
            self.set_target_text("")
        elif not self.mouse_captured:
            self.set_target_text("%s.  Click the game to capture the mouse" % biome)
        elif self.target is None:
            self.set_target_text("%s.  Looking at: nothing in reach" % biome)
        else:
            block, normal = self.target
            text = "%s.  Looking at: %s" % (biome, VoxelGen.NAMES.get(self.world.get_block(*block), "?"))
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if normal != (0, 0, 0) and not physics.cell_in_player(cell, self.pos):
                text += "   (right click: %s)" % VoxelGen.NAMES[HOTBAR[self.slot]]
            self.set_target_text(text)
        slots = []
        for slot, block in enumerate(HOTBAR):
            label = "%s %s" % (hotbar_key(slot), VoxelGen.NAMES[block])
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

    def update_walkers(self, dt):
        """Creatures wander the terrain once the spawn area is loaded: made once, then moved each frame by the mover."""
        if self.walkers is None:
            if not self.world.is_loaded(0, 0):
                return
            self.walkers = VoxelWalkers.Walkers(self.world, (8.5, 8.5), WALKERS, self.seed)
        self.walkers.update(dt, (self.pos[0], self.pos[2]))

    def in_water(self):
        """True while a liquid block is at the player's feet or chest."""
        x, z = math.floor(self.pos[0]), math.floor(self.pos[2])
        feet = self.world.get_block(x, math.floor(self.pos[1] + 0.1), z)
        chest = self.world.get_block(x, math.floor(self.pos[1] + 1.0), z)
        return feet in VoxelGen.LIQUIDS or chest in VoxelGen.LIQUIDS

    def at_surface(self):
        """True when the feet are in water and the chest is above it: the player is floating at the surface."""
        x, z = math.floor(self.pos[0]), math.floor(self.pos[2])
        return self.world.get_block(x, math.floor(self.pos[1] + 1.0), z) not in VoxelGen.LIQUIDS

    def eye_in_water(self):
        eye = self.eye()
        return self.world.get_block(math.floor(eye[0]), math.floor(eye[1]), math.floor(eye[2])) in VoxelGen.LIQUIDS

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
        self.turn(dx * self.look_speed(), -dy * self.look_speed())

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
        self.swimming = not self.flying and self.in_water()

        # Walk along the ground in the direction the player faces (flat, so looking down does not sink the player).
        yaw = math.radians(self.yaw)
        forward = (math.sin(yaw), -math.cos(yaw))
        right = (math.cos(yaw), math.sin(yaw))
        across = self.key("W") - self.key("S")
        sideways = self.key("D") - self.key("A")
        dx = forward[0] * across + right[0] * sideways
        dz = forward[1] * across + right[1] * sideways
        length = math.hypot(dx, dz)
        speed = FLY_SPEED if self.flying else (WALK_SPEED * WATER_SPEED if self.swimming else WALK_SPEED)
        if length > 0.0:
            dx, dz = dx / length * speed, dz / length * speed
        self.vel[0], self.vel[2] = dx, dz

        if self.flying:
            self.vel[1] = (self.key("Space") - self.key("R")) * FLY_RISE
        elif self.swimming:
            if self.key("Space") and self.at_surface():
                # A jump from the surface lifts the player out onto a bank a block higher.
                self.vel[1] = physics.JUMP_SPEED
            else:
                # The vertical speed eases towards the swim speed: up while Space is held, down otherwise.
                target = WATER_RISE if self.key("Space") else -WATER_SINK
                self.vel[1] += (target - self.vel[1]) * min(1.0, dt * WATER_DRAG)
        elif self.key("Space") and self.on_ground:
            self.vel[1] = physics.JUMP_SPEED

        # Physics waits until the chunk under the player is in the world, so the player does not fall through air.
        if self.world.is_loaded(*self.chunk_under_player()):
            # The engine's mover is the same as physics.step (tests check they agree), and it is faster.
            # Swimming has no gravity of its own: the swim speed above takes its place, so the mover runs without it.
            pos, vel, self.on_ground = self.world.step_body(tuple(self.pos), tuple(self.vel), dt,
                                                           self.flying or self.swimming, on_ground=self.on_ground)
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
        return self.world.skin_at(x, y, z)

    def eye(self):
        return [self.pos[0], self.pos[1] + physics.EYE_HEIGHT, self.pos[2]]

    def look_direction(self):
        yaw = math.radians(self.yaw)
        pitch = math.radians(self.pitch)
        return (math.sin(yaw) * math.cos(pitch), math.sin(pitch), -math.cos(yaw) * math.cos(pitch))

    def update_target(self):
        """The block under the crosshair: the first block behind the drawn surface the view meets. The surface is found on the
        skin, then the blocks are walked from half a block in front of it, so the outline is on the ground that is drawn."""
        eye = tuple(self.eye())
        look = self.look_direction()
        self.torch_target = self.torch_under_aim(eye, look)
        surface = self.world.skin_surface(eye, look, physics.REACH)
        if surface is None:
            self.target = physics.raycast(self.solid, eye, look)
            return
        start = tuple(surface[i] - look[i] * 0.5 for i in range(3))
        self.target = physics.raycast(self.solid, start, look, max_distance=physics.REACH + 0.5)

    def torch_under_aim(self, eye, look):
        """The nearest torch the look ray meets within reach, as (distance, cell), or None. A torch is its stick and its flame,
        in the same place VoxelMesher.cpp (buildPropMesh) draws them."""
        best = None
        for x, y, z, dir in self.world.props():
            wall = VoxelGen.DIRECTIONS[dir]
            base = (x + 0.5 + 0.36 * wall[0], y + 0.5 + 0.36 * wall[1] - 0.1, z + 0.5 + 0.36 * wall[2])
            for centre, half in (((base[0], base[1] + 0.4, base[2]), (0.06, 0.25, 0.06)),
                                 ((base[0], base[1] + 0.78, base[2]), (0.1, 0.08, 0.1))):
                t = ray_box(eye, look, centre, half)
                if t is not None and t <= physics.REACH and (best is None or t < best[0]):
                    best = (t, (x, y, z))
        return best

    def drop_torches_on(self, block):
        """Torches leaning on a block that is broken are broken too: each one is in a cell beside it, pointing at it."""
        for dir, d in enumerate(VoxelGen.DIRECTIONS):
            cell = (block[0] + d[0], block[1] + d[1], block[2] + d[2])
            if self.world.prop_at(*cell) == DIRECTION_INDEX[(-d[0], -d[1], -d[2])]:
                self.world.remove_prop(*cell)

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
            if self.pressed("Slot%d" % slot, bool(cge.key_down(hotbar_key(slot)))):
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
        torch = self.torch_target
        if breaking and torch is not None:
            # A torch the crosshair is on, nearer than the block behind it, is the one broken.
            block_distance = None
            if self.target is not None:
                eye = self.eye()
                block = self.target[0]
                block_distance = math.dist(eye, (block[0] + 0.5, block[1] + 0.5, block[2] + 0.5))
            if block_distance is None or torch[0] < block_distance:
                self.world.remove_prop(*torch[1])
                return
        if self.target is None or not (breaking or placing):
            return
        block, normal = self.target
        if breaking:
            self.world.set_block(block[0], block[1], block[2], VoxelGen.AIR)
            self.drop_torches_on(block)
        elif placing:
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if physics.cell_in_player(cell, self.pos):
                return
            if HOTBAR[self.slot] == VoxelGen.TORCH:
                # A torch goes on a face: into the air cell in front of a solid block, leaning on it.
                if self.world.is_solid(block[0], block[1], block[2]) and self.world.get_block(cell[0], cell[1], cell[2]) == VoxelGen.AIR:
                    self.world.set_prop(cell[0], cell[1], cell[2], DIRECTION_INDEX[(-normal[0], -normal[1], -normal[2])])
            else:
                self.world.set_block(cell[0], cell[1], cell[2], HOTBAR[self.slot])

    # ----- sky ----------------------------------------------------------------------------

    def update_held_torch(self):
        """The carried torch: a point light at the eye, lit or out. It is a shader light, so it needs no re-mesh."""
        if self.pressed("HeldTorch", bool(cge.key_down("L"))):
            self.held_lit = not self.held_lit
        eye = self.eye()
        # A little flicker: two slow sines, so the light wavers without looking random.
        now = time.time()
        flicker = 0.92 + 0.08 * math.sin(now * 9.0) * math.sin(now * 3.1 + 1.0)
        cge.set_light("held_torch", position=cge.Vector3f(eye[0], eye[1] - 0.2, eye[2]),
                      brightness=HELD_TORCH_BRIGHTNESS * flicker if self.held_lit else 0.0)

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
        fog_colour, fog_start = sky, fog_end * FOG_START_FRACTION
        if self.world is not None and self.eye_in_water():
            # Under the water the view is short and blue-green, whatever the time of day. The sky is that colour too, since
            # the faces of the water a player is inside are back faces, and the sky shows through them.
            sky = zenith = fog_colour = WATER_FOG
            fog_start, fog_end = WATER_FOG_START, WATER_FOG_END
        cge.set_light("sun", direction=cge.Vector3f(*travel), colour=cge.Vector3f(*light),
                      brightness=0.45 + 0.55 * day)
        # The sun's shadows are off under the water: the water surface is a shadow over everything below it, which is dark.
        cge.set_environment(sky=cge.Vector3f(*sky), zenith=cge.Vector3f(*zenith), fog=cge.Vector3f(*fog_colour),
                            fog_start=fog_start, fog_end=fog_end,
                            sun_direction=cge.Vector3f(*toward_sun), sun_colour=cge.Vector3f(*sun_disc),
                            sun_size=1.6, shadows=SHADOWS and not (self.world is not None and self.eye_in_water()))
        self.sun = travel
        self.fog = (fog_end * FOG_START_FRACTION, fog_end)

    def place_camera(self):
        eye = self.eye()
        if self.view_y is None:
            self.view_y = eye[1]
        # The camera follows the eye's height over a few frames, so climbing a block is a step up, not a jump. Picking and the
        # water check use the eye itself.
        self.view_y += (eye[1] - self.view_y) * min(1.0, VIEW_SMOOTH * self.frame_dt)
        look = self.look_direction()
        origin = [eye[0], self.view_y, eye[2]]
        target = [origin[0] + look[0], origin[1] + look[1], origin[2] + look[2]]
        cge.set_camera(cge.Vector3f(*origin), cge.Vector3f(*target))

    # ----- state for tests and agents -----------------------------------------------------

    def write_state(self, path="cg_control/voxel_state.json"):
        """Write the player and the world's counts to cg_control/voxel_state.json, so an agent can check them without a screenshot."""
        stats = self.world.stats()
        basis = cge.camera_basis()
        state = {"chunks": stats["chunks"], "loaded": stats["loaded"], "pending": stats["pending"],
                 "dirty": stats["dirty"], "meshing": stats["meshing"], "quads": stats["quads"],
                 "upload_ms": round(stats["upload_ms"], 3), "edits": stats["edits"], "slot": self.slot,
                 "smoothing": self.world.smoothing(), "active": self.world.active_count(), "paused": self.paused,
                 "menu": self.menu_page if self.paused else None, "sensitivity": self.sensitivity,
                 "seed": self.seed, "frames": self.frames,
                 "player": [round(v, 3) for v in self.pos], "on_ground": self.on_ground, "flying": self.flying,
                 "swimming": self.swimming, "walkers": len(self.walkers.walkers) if self.walkers else 0,
                 "biome": self.world.biome_at(math.floor(self.pos[0]), math.floor(self.pos[2])),
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
