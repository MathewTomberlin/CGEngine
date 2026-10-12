"""
Voxel sample: a procedural block world in chunks (docs/games/voxels.md).

Attached to the scene root's update domain by resources/scenes/voxels.json. The engine's cge.VoxelWorld generates the
terrain, meshes the chunks and uploads them on background threads, within a time budget each frame. This script asks for
the chunks near the player, unloads far ones, moves the player, and handles mouse look and breaking and placing.

Controls: W A S D walk, Space jumps, Enter toggles flying (then Space rises and R falls). Mouse: left-click captures
the mouse; move it to look, left-click breaks the block under the crosshair, right-click places the selected item against
it, and Escape releases the mouse. Without the mouse: arrow keys turn and look, J breaks, K places. I opens the inventory,
C toggles creative mode (placing takes nothing from the slots, and the player cannot be hurt). VOXEL_SEED chooses a world.
"""
import json
import math
import os
import random
import time
import traceback

import cg_engine_bindings as cge
from PyScript import PyScript
import StateFile
import VoxelDrops
import VoxelGen
import VoxelInventory
import VoxelNet
import VoxelParticles
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays, mix, outline_arrays
import VoxelWalkers

# The view distance, in chunks each way from the player (the settings page changes it). The default is the middle step, 14, which
# is (2 * 14 + 1)^2 = 841 chunks. The last step is "Unlimited", capped at UNLIMITED_VIEW: the chunks grow with the square of the
# distance and each one is held in memory (40 is about 6,500 chunks, roughly 2 GB).
VIEW_STEPS = [6, 8, 10, 12, 14, 18, 24, 32, 40]
DEFAULT_VIEW = 14
UNLIMITED_VIEW = 40
UNLOAD_MARGIN = 2           # chunks farther than the view distance plus this are unloaded
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
# The items the player starts with, in the hotbar (keys 1 to 9, then 0, - and = for the tenth to twelfth slot), and how many
# of each. The inventory (VoxelInventory) holds them from there. The files the F5 and F9 keys save and load are the edits and,
# beside them, the inventory.
START_ITEMS = [VoxelGen.STONE, VoxelGen.DIRT, VoxelGen.GRASS, VoxelGen.WOOD, VoxelGen.LEAVES, VoxelGen.SAND, VoxelGen.GRAVEL,
               VoxelGen.SNOW, VoxelGen.WATER, VoxelGen.LAMP, VoxelGen.BRICK, VoxelGen.TORCH]
START_COUNT = 16
DIRECTION_INDEX = {d: i for i, d in enumerate(VoxelGen.DIRECTIONS)}   # a direction to the dir that a prop uses for it
HOTBAR_KEYS = "1234567890-="
COLUMNS = VoxelInventory.COLUMNS
ROWS = VoxelInventory.ROWS
SLOTS = VoxelInventory.SLOTS
SLOT_SIZE = 52              # pixels, a side, of a slot on the hotbar and the inventory screen
SLOT_GAP = 6                # pixels between slots
HUD_BOTTOM = 14             # pixels between the hotbar and the bottom of the window
NOTICE_SECONDS = 1.5        # how long a notice ("Inventory full") stays on screen
WHITE = (1.0, 1.0, 1.0)
HIT_FLASH_SECONDS = 0.12    # the aimed block's outline is white this long after a hit
PLAYER_HEALTH = 10.0        # the player's health: ten segments on the bar above the hotbar
REGEN_DELAY = 6.0           # seconds without a hit before the player starts to heal
HURT_FLASH_SECONDS = 0.4    # the screen reddens this long after a bite or a fall
HURT_FLASH_ALPHA = 110      # the most red the screen gets (0 to 255), at the moment of the hurt, fading after
REGEN_RATE = 0.25           # health a second once it does
FALL_SAFE = 4.0             # blocks a player can fall without harm
FALL_DAMAGE = 1.0           # health lost for each block past FALL_SAFE
HEALTH_SEGMENT = 16         # pixels, a side, of one segment of the health bar
INVENTORY_FILE = "cg_control/voxel_inventory.json"
EDITS_FILE = "cg_control/voxel_edits.bin"
NET_SPEC = os.environ.get("VOXEL_NET")   # listen:<port> or connect:<host>:<port>: two processes share their edits (VoxelNet)
POS_INTERVAL = 0.1         # seconds between the player's position messages


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
HEAL_AFTER = 4.0                 # seconds a damaged block is left alone before it starts to heal
HEAL_STEP = 1.0                  # seconds per damage stage it heals once it does
HELD_TORCH_BRIGHTNESS = 2.5     # the light of the torch the player carries (L lights and puts it out)
WALKERS = int(os.environ.get("VOXEL_WALKERS", "12"))   # creatures that wander the terrain near the spawn
FAR_RADIUS = int(os.environ.get("VOXEL_FAR_RADIUS", "6"))   # chunks more than this from the player get far skins
FAR_STRIDE = 2             # the far skins sample every second block: a quarter of the quads
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
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/voxel_state.json
# The outline of the block the crosshair is on (yellow: it is broken), and of the cell a block would be placed in
# (its own colour, brightened). Both are thin bars around the cube's edges, made as runtime meshes.
HELP_TEXT = "LMB break  RMB place  WASD move  Space jump  Enter fly  1-0 - = slot  I inventory  C creative  F5/F9 save/load  Esc release"
BREAK_COLOUR = (1.0, 0.85, 0.2)
HIDDEN_BOX = ((-1.0e6, -1.0e6, -1.0e6), (-1.0e6 + 1.0, -1.0e6 + 1.0, -1.0e6 + 1.0))   # where an outline is put when not shown
SMOOTH_RESOLUTION = int(os.environ.get("VOXEL_SMOOTH", "1"))   # samples per block for the smooth skin; 0 draws cubes
DAY_SECONDS = float(os.environ.get("VOXEL_DAY_SECONDS", "240"))   # real seconds for one day and night
START_TIME = float(os.environ.get("VOXEL_TIME", "0.3"))           # time of day at the start: 0 sunrise, 0.25 noon, 0.5 sunset
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
    "settings": ("SETTINGS", ("sens_down", "sens_value", "sens_up", "view_down", "view_value", "view_up", "time_value",
                              "time_track", "back")),
}
MENU_TEXT = {"resume": "Resume", "settings": "Settings", "quit": "Quit", "back": "Back", "sens_down": "<", "sens_up": ">",
             "view_down": "<", "view_up": ">"}


def load_settings():
    """The saved settings: the mouse sensitivity (a percent, one of SENSITIVITY_STEPS) and the time of day (0 to 1, or None
    if none is saved). Missing or unreadable settings keep their defaults."""
    settings = {"mouse_sensitivity": 100, "time_of_day": None, "view_radius": DEFAULT_VIEW}
    try:
        with open(SETTINGS_FILE) as f:
            data = json.load(f)
        settings["mouse_sensitivity"] = int(data.get("mouse_sensitivity", 100))
        if "time_of_day" in data:
            settings["time_of_day"] = float(data["time_of_day"]) % 1.0
        settings["view_radius"] = int(data.get("view_radius", DEFAULT_VIEW))
    except (OSError, ValueError, TypeError, AttributeError):
        pass
    settings["mouse_sensitivity"] = min(SENSITIVITY_STEPS, key=lambda step: abs(step - settings["mouse_sensitivity"]))
    settings["view_radius"] = min(VIEW_STEPS, key=lambda step: abs(step - settings["view_radius"]))
    return settings


def save_settings(sensitivity, time_of_day, view_radius):
    StateFile.write_json(SETTINGS_FILE, {"mouse_sensitivity": sensitivity, "time_of_day": round(time_of_day, 5),
                                         "view_radius": view_radius})


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
        self.hit_times = {}             # the damaged blocks, by cell: the time each was last hit
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
        self.view_radius = saved["view_radius"]         # the view distance in chunks each way (VIEW_STEPS)
        self.dragging = False           # the time-of-day slider is being dragged
        self.outline_keys = {}          # the last outline shown under each name, so unchanged ones are not re-uploaded
        self.outline_bodies = set()     # the outlines already made as runtime meshes
        self.slot = 0                   # the hotbar slot placing uses
        self.inventory = VoxelInventory.Inventory()   # what the player holds, by slot (the hotbar is the first row)
        self.creative = os.environ.get("VOXEL_CREATIVE", "0") == "1"   # placing takes nothing, breaking gives nothing
        self.inventory_open = False     # the inventory screen is up: the world stands still and the mouse is free
        self.inventory_hover = None     # the slot under the mouse on the inventory screen
        self.mouse_before_inventory = False   # the mouse was captured when the screen opened, so it is again when it closes
        self.notice = ""                # a short message on the HUD ("Inventory full"), shown until notice_until
        self.notice_until = 0.0
        self.flash_until = 0.0          # the aimed block's outline is white until this time, after a hit
        self.health = PLAYER_HEALTH     # the player's health (a bite, a fall and a death change it)
        self.hurt_at = -1e9             # when the player was last hurt (the healing waits after it)
        self.hurt_flash_until = 0.0     # the screen is red until this time, after a hurt
        self.fell_from = None           # the height the player left the ground at, while in the air
        self.landed_once = False        # a fall counts only after the player has stood on the ground (the spawn drop does not)
        self.spawn_pos = list(self.pos)
        self.walker_target = None       # (distance, walker) of the creature under the crosshair, or None
        self.bursts = VoxelParticles.Bursts(random.Random(self.seed))   # the chips a hit or a break throws
        self.drops = VoxelDrops.Drops()  # the items lying on the ground, taken by walking up to them
        self.net = None                 # the link to the other process (VOXEL_NET), or None for a single player
        self.net_pending = {}           # remote edits for chunks not loaded yet: cell -> (kind, value), applied when they load
        self.net_pos_timer = 0.0
        self.net_applied = 0            # remote edits applied to this world
        self.remote_pos = None          # the other player's feet and yaw, from their last position message
        self.window = None              # the window's size, set when the HUD is made
        self.bodies = set()             # the HUD's rectangle and text bodies, made once each
        self.drawn = {}                 # the last value set on each HUD body, so unchanged ones are not set again
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
        self.update_net(raw_dt)

        if not self.hud_ready and self.frames >= 2:
            # Made a frame after the world, not during the scene's start: the outlines first, then the labels, so the
            # labels are drawn over the bars.
            self.show_outline("target_break", None, None)
            self.show_outline("target_place", None, None)
            self.setup_hud()
            self.hud_ready = True
        self.update_pause_key()
        self.update_inventory_key()
        if self.paused:
            # The world stands still while the menu is open: no time passes, no block moves, the player does not move.
            self.update_pause_menu()
            dt = 0.0
        elif self.inventory_open:
            # The inventory screen stands the world still too, and frees the mouse for its slots.
            self.update_inventory()
            dt = 0.0
        else:
            self.world.simulate(dt, SIM_BUDGET_MS, SIM_TICK)
            self.update_mouse()
            self.update_player(dt)
            self.collect_drops()
            self.update_walkers(dt)
            self.update_target()
            self.edit_blocks()
        self.update_outlines()
        self.update_hud()
        self.update_sky(dt)
        self.update_held_torch()
        self.update_healing()
        self.update_health(dt)
        self.bursts.update(dt)
        self.drops.update(dt)
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
                                    hits=VoxelGen.HITS.get(block, 1),
                                    transparent=block in VoxelGen.TRANSPARENT)
        self.world.set_smoothing(SMOOTH_RESOLUTION)
        self.world.set_move_seconds(SIM_TICK * 0.9)   # a falling block lands just before the next tick
        # Smooth skin nearest the camera is finer: NEAR_SAMPLES per block within NEAR_RADIUS chunks.
        self.world.set_detail(NEAR_RADIUS, NEAR_SAMPLES)
        self.world.set_far(FAR_RADIUS, FAR_STRIDE)
        cge.set_environment(shadows=SHADOWS, shadow_radius=SHADOW_RADIUS)
        self.world.set_terrain(self.seed, VoxelGen.SEA_LEVEL, 8, 44, VoxelGen.SNOW_LEVEL, VoxelGen.TREE_CHANCE,
                          VoxelGen.CAVE_WIDTH)
        self.inventory.start(START_ITEMS, START_COUNT)
        spec = VoxelNet.parse(NET_SPEC)
        if spec is not None:
            try:
                self.net = VoxelNet.Link(*spec)
            except OSError as error:
                print("[voxel] could not open the network link (%s): playing alone" % error)
        self.inventory.slots[COLUMNS] = [VoxelGen.PICKAXE, 1]   # the first row below the hotbar: drag it into the hotbar to use it

    def setup_hud(self):
        """Two labels: the top left holds what the crosshair is on and the controls; the hotbar sits along the bottom."""
        cge.load_font(HUD_FONT, "hud")
        # Each label is one text draw, which costs the renderer a state switch, so there are only two.
        cge.create_text_body("hud_target", "", "hud", cge.Vector2f(14, 10), 16, outline_thickness=HUD_OUTLINE)
        cge.create_text_body("hud_help", HELP_TEXT, "hud", cge.Vector2f(14, 34), 14, cge.Color(200, 200, 200),
                             outline_thickness=HUD_OUTLINE)
        # A small cross at the middle of the window: the block the crosshair is on is outlined, and this shows where the aim is.
        size = cge.window_size()
        self.window = size
        cge.create_text_body("hud_cross", "+", "hud", cge.Vector2f(size.x / 2 - 6, size.y / 2 - 13), 22,
                             outline_thickness=HUD_OUTLINE)
        self.target_text = ""
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
            "view_down": ((cx - 215, cy + 8, cx - 150, cy + 38), (cx - 200, cy + 12)),
            "view_value": (None, (cx - 120, cy + 12)),
            "view_up": ((cx + 150, cy + 8, cx + 215, cy + 38), (cx + 165, cy + 12)),
            "time_value": (None, (cx - 120, cy + 44)),
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
        if item == "view_value":
            if self.view_radius == UNLIMITED_VIEW:
                return "View distance: Unlimited (%d)" % self.view_radius
            return "View distance: %d chunks" % self.view_radius
        if item == "time_value":
            minutes = (round(self.clock * 1440) + 360) % 1440   # 0 is sunrise (06:00), 0.25 noon, 0.5 sunset, 0.75 midnight
            return "Time of day: %02d:%02d" % (minutes // 60, minutes % 60)
        if item == "time_track":
            knob = min(TRACK_CELLS - 1, int(self.clock * TRACK_CELLS))
            return "[" + "-" * knob + "O" + "-" * (TRACK_CELLS - 1 - knob) + "]"
        text = MENU_TEXT[item]
        if self.menu_hover == item:
            return "[%s]" % text if item in ("sens_down", "sens_up", "view_down", "view_up") else "> %s <" % text
        return text

    def update_pause_key(self):
        if self.inventory_open:
            # Escape closes the inventory screen first; the menu opens on the next press.
            if self.pressed("Pause", bool(cge.key_down("Escape"))):
                self.close_inventory()
            return
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
        save_settings(self.sensitivity, self.clock, self.view_radius)

    def change_view(self, step):
        """One step of view distance. The chunks are asked for again at once: a longer view requests the new ones, and a shorter
        one unloads the far ones (request_chunks)."""
        index = VIEW_STEPS.index(self.view_radius) + step
        self.view_radius = VIEW_STEPS[max(0, min(len(VIEW_STEPS) - 1, index))]
        self.wanted_centre = None
        save_settings(self.sensitivity, self.clock, self.view_radius)

    def drag_time(self, mouse_x, pressing):
        """The time-of-day slider: while the button is held on the track, the time follows the mouse; the choice is saved
        when the button comes up."""
        left, right = self.menu_layout["time_track"][0][0], self.menu_layout["time_track"][0][2]
        if pressing and (self.dragging or self.menu_hover == "time_track"):
            self.dragging = True
            self.clock = min(0.999, max(0.0, (mouse_x - left) / (right - left)))
        elif self.dragging:
            self.dragging = False
            save_settings(self.sensitivity, self.clock, self.view_radius)

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
        if clicked and self.menu_hover == "view_down":
            self.change_view(-1)
        if clicked and self.menu_hover == "view_up":
            self.change_view(1)
        self.show_menu(True)

    def set_target_text(self, text):
        if self.target_text != text:
            self.target_text = text
            cge.set_text_body("hud_target", text)

    def update_hud(self):
        if not self.hud_ready:
            return
        # The crosshair is for aiming, so it is hidden while a menu or the inventory is open.
        self.set_menu_text("hud_cross", "" if self.paused or self.inventory_open else "+")
        biome = self.world.biome_at(math.floor(self.pos[0]), math.floor(self.pos[2])).capitalize()
        if self.paused or self.inventory_open:
            self.set_target_text("")
        elif not self.mouse_captured:
            self.set_target_text("%s.  Click the game to capture the mouse" % biome)
        elif self.aims_at_walker():
            hits = self.walker_target[1].health
            self.set_target_text("%s.  Looking at: creature   (left click: %d hit%s to go)" % (biome, hits, "" if hits == 1 else "s"))
        elif self.aims_at_torch():
            self.set_target_text("%s.  Looking at: torch   (left click: take it back)" % biome)
        elif self.target is None:
            self.set_target_text("%s.  Looking at: nothing in reach" % biome)
        else:
            block, normal = self.target
            text = "%s.  Looking at: %s" % (biome, VoxelGen.NAMES.get(self.world.get_block(*block), "?"))
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if normal != (0, 0, 0) and not physics.cell_in_player(cell, self.pos):
                text += "   (right click: %s)" % self.selected_name()
            self.set_target_text(text)
        self.update_slots()
        self.update_health_bar()
        self.update_hurt_flash()
        self.update_inventory_screen()
        self.update_notice()

    # ----- the HUD's boxes: the hotbar, the inventory screen, the held stack and notices -----------

    def draw_rect(self, name, box, fill, outline=(0, 0, 0, 0), thickness=0, layer=990):
        """A rectangle on the HUD, made the first time at the layer (its z order). box is (x, y, width, height) in window pixels;
        fill and outline are (r, g, b, a) with 0 to 255 channels. It is only moved or recoloured when its value changes."""
        key = (box, fill, outline, thickness)
        if self.drawn.get(name) == key:
            return
        self.drawn[name] = key
        position, size = cge.Vector2f(box[0], box[1]), cge.Vector2f(box[2], box[3])
        if name in self.bodies:
            cge.set_rect_body(name, position, size, cge.Color(*fill), cge.Color(*outline), float(thickness))
        else:
            self.bodies.add(name)
            cge.create_rect_body(name, position, size, cge.Color(*fill), cge.Color(*outline), float(thickness), z_order=layer)

    def draw_text(self, name, text, position, size, colour=(255, 255, 255, 255)):
        """A label on the HUD at position (window pixels). Its text, and its position, change only when they differ from last time."""
        key = (text, position)
        if self.drawn.get(name) == key:
            return
        self.drawn[name] = key
        if name in self.bodies:
            cge.set_text_body(name, text, position=cge.Vector2f(position[0], position[1]))
        else:
            self.bodies.add(name)
            cge.create_text_body(name, text, "hud", cge.Vector2f(position[0], position[1]), size, cge.Color(*colour),
                                 outline_thickness=HUD_OUTLINE)

    def hide_rect(self, name, box):
        self.draw_rect(name, box, (0, 0, 0, 0))

    def item_colour(self, item):
        """An item's colour as (r, g, b, 255), for its swatch."""
        r, g, b = VoxelGen.COLOURS.get(item) or VoxelGen.ITEM_COLOURS.get(item, WHITE)
        return (int(r * 255), int(g * 255), int(b * 255), 255)

    def slot_origin(self, index):
        """The top left corner of a slot, in window pixels. Row 0, the hotbar, is at the bottom of the window; rows 1 and 2
        (slots 12 to 35) stand above it when the inventory is open."""
        row, column = divmod(index, COLUMNS)
        row_width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        left = (self.window.x - row_width) / 2 + column * (SLOT_SIZE + SLOT_GAP)
        top = self.window.y - HUD_BOTTOM - SLOT_SIZE - row * (SLOT_SIZE + SLOT_GAP)
        return left, top

    def slot_at(self, x, y):
        """The slot under a window point, or None. Only the rows on screen count: the hotbar, or all three rows with the inventory open."""
        rows = ROWS if self.inventory_open else 1
        for index in range(COLUMNS * rows):
            left, top = self.slot_origin(index)
            if left <= x < left + SLOT_SIZE and top <= y < top + SLOT_SIZE:
                return index
        return None

    def update_slots(self):
        """The hotbar, and with the inventory open its two other rows: a box per slot, a swatch of the item's colour and the count.
        The selected slot is outlined in yellow and the slot under the mouse in white. Under the pause menu they are not drawn."""
        rows = ROWS if self.inventory_open else 1
        for index in range(SLOTS):
            row = index // COLUMNS
            left, top = self.slot_origin(index)
            box = (left, top, SLOT_SIZE, SLOT_SIZE)
            shown = row < rows and not self.paused
            stack = self.inventory.slots[index] if shown else None
            if not shown:
                self.hide_rect("slot_%d" % index, box)
                self.hide_rect("swatch_%d" % index, box)
                self.draw_text("count_%d" % index, "", (left, top), 14)
                self.draw_text("key_%d" % index, "", (left, top), 12)
                continue
            if index == self.slot and row == 0:
                edge, width = (255, 217, 51, 255), 3
            elif index == self.inventory_hover:
                edge, width = (255, 255, 255, 255), 2
            else:
                edge, width = (90, 90, 100, 255), 2
            self.draw_rect("slot_%d" % index, box, (20, 20, 26, 190), edge, width, layer=991)
            inset = 7
            swatch = (left + inset, top + inset, SLOT_SIZE - 2 * inset, SLOT_SIZE - 2 * inset)
            if stack:
                self.draw_rect("swatch_%d" % index, swatch, self.item_colour(stack[0]), (0, 0, 0, 170), 1, layer=992)
            else:
                self.hide_rect("swatch_%d" % index, swatch)
            count = str(stack[1]) if stack and stack[1] > 1 else ""
            self.draw_text("count_%d" % index, count, (left + 5, top + SLOT_SIZE - 21), 14)
            key = hotbar_key(index) if row == 0 else ""
            self.draw_text("key_%d" % index, key, (left + 4, top + 1), 12, (200, 200, 200, 255))

    def update_inventory_screen(self):
        """The inventory screen: a panel over the two rows above the hotbar, its title and how to use it, and the held stack, which
        follows the mouse. All of it is hidden while the screen is closed."""
        width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        left, top = self.slot_origin(2 * COLUMNS)
        bottom = self.slot_origin(0)[1] + SLOT_SIZE
        panel = (left - 14, top - 46, width + 28, bottom + 8 - (top - 46))
        if self.inventory_open:
            self.draw_rect("inv_panel", panel, (12, 12, 16, 225), (130, 130, 140, 255), 2, layer=980)
            self.draw_text("inv_title", "Inventory", (panel[0] + 14, panel[1] + 4), 22)
            self.draw_text("inv_hint", "Left click: take or put down a stack    Right click: half, or one    I or Esc: close",
                           (panel[0] + 14, panel[1] + 28), 13, (200, 200, 200, 255))
        else:
            self.hide_rect("inv_panel", panel)
            self.draw_text("inv_title", "", (0, 0), 22)
            self.draw_text("inv_hint", "", (0, 0), 13)
        held = self.inventory.held if self.inventory_open else None
        if held is None:
            self.hide_rect("held", (0, 0, 1, 1))
            self.draw_text("held_count", "", (0, 0), 14)
            return
        mouse_x, mouse_y = cge.mouse_position()
        size = SLOT_SIZE - 14
        self.draw_rect("held", (mouse_x - size / 2, mouse_y - size / 2, size, size), self.item_colour(held[0]),
                       (0, 0, 0, 200), 1, layer=995)
        count = str(held[1]) if held[1] > 1 else ""
        self.draw_text("held_count", count, (mouse_x - size / 2 + 4, mouse_y + size / 2 - 16), 14)

    def notify(self, text):
        """Show a short message on the HUD, under the crosshair, for NOTICE_SECONDS."""
        self.notice = text
        self.notice_until = time.time() + NOTICE_SECONDS

    def update_notice(self):
        text = self.notice if time.time() < self.notice_until else ""
        self.draw_text("hud_notice", text, (self.window.x / 2 - 90, self.window.y / 2 + 26), 18, (255, 220, 120, 255))

    def request_chunks(self, centre):
        """Ask for every chunk in the view that is not requested yet, nearest first. Unload the far ones."""
        cx, cz = centre
        view = self.view_radius
        wanted = [(cx + dx, cz + dz) for dz in range(-view, view + 1) for dx in range(-view, view + 1)]
        wanted.sort(key=lambda c: (c[0] - cx) ** 2 + (c[1] - cz) ** 2)
        for chunk in wanted:
            if chunk not in self.requested:
                self.world.request_chunk(*chunk)
                self.requested.add(chunk)
        for chunk in list(self.requested):
            if max(abs(chunk[0] - cx), abs(chunk[1] - cz)) > view + UNLOAD_MARGIN:
                self.world.unload_chunk(*chunk)
                self.requested.discard(chunk)

    def update_walkers(self, dt):
        """Creatures wander the terrain once the spawn area is loaded: made once, then moved each frame by the mover."""
        if self.walkers is None:
            if not self.world.is_loaded(0, 0):
                return
            self.walkers = VoxelWalkers.Walkers(self.world, (8.5, 8.5), WALKERS, self.seed)
        bites = self.walkers.update(dt, (self.pos[0], self.pos[2]))
        self.hurt_player(bites)

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
        self.track_fall()
        if self.pos[1] < RESPAWN_DEPTH:
            # Keep the state from the moment of the fall: where the player was, and whether the chunk under them was loaded.
            folder = os.path.abspath("cg_control/snapshots")
            os.makedirs(folder, exist_ok=True)
            stamp = time.strftime("%Y%m%d_%H%M%S")
            self.write_state(os.path.join(folder, "fell_%s.json" % stamp))
            print("[voxel] fell below the kill zone at %s (state saved)" % [round(v, 2) for v in self.pos])
            self.pos = [self.pos[0], SPAWN_HEIGHT, self.pos[2]]
            self.vel = [0.0, 0.0, 0.0]
            self.fell_from = None
            self.landed_once = False

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
        self.walker_target = self.walker_under_aim(eye, look)
        surface = self.world.skin_surface(eye, look, physics.REACH)
        if surface is None:
            self.target = physics.raycast(self.solid, eye, look)
            return
        start = tuple(surface[i] - look[i] * 0.5 for i in range(3))
        self.target = physics.raycast(self.solid, start, look, max_distance=physics.REACH + 0.5)

    def torch_under_aim(self, eye, look):
        """The nearest torch the look ray meets within reach, as (distance, cell, lo, hi), or None: lo and hi are the corners of
        the box round the whole torch (its stick and its flame). The boxes are the ones the torch is drawn with
        (VoxelWorld.prop_boxes), so the player aims at what they see."""
        best = None
        for x, y, z, dir, stick, stick_half, flame, flame_half in self.world.prop_boxes():
            for centre, half in ((stick, stick_half), (flame, flame_half)):
                t = ray_box(eye, look, centre, half)
                if t is not None and t <= physics.REACH and (best is None or t < best[0]):
                    lo = tuple(min(stick[i] - stick_half[i], flame[i] - flame_half[i]) for i in range(3))
                    hi = tuple(max(stick[i] + stick_half[i], flame[i] + flame_half[i]) for i in range(3))
                    best = (t, (x, y, z), lo, hi)
        return best

    def aims_at_torch(self):
        """True when the crosshair is on a torch that is nearer than the block behind it: a break takes the torch then."""
        if self.torch_target is None:
            return False
        if self.target is None:
            return True
        block = self.target[0]
        eye = self.eye()
        block_distance = math.dist(eye, (block[0] + 0.5, block[1] + 0.5, block[2] + 0.5))
        return self.torch_target[0] < block_distance

    def selected(self):
        """The item in the hotbar slot placing uses, or None when that slot is empty."""
        return self.inventory.item(self.slot)

    def selected_name(self):
        item = self.selected()
        return "empty slot" if item is None else VoxelGen.NAMES[item]

    def gain(self, item, amount=1):
        """Put broken blocks (or torches) into the inventory. Creative mode keeps no stock, so nothing is added."""
        if not self.creative:
            self.inventory.add(item, amount)

    def ground_under(self, x, y, z):
        """The height a dropped item at (x, y, z) comes to rest at: the top of the first solid block below it, so an item dropped in
        the air falls to the ground. Water does not hold it up."""
        column = (math.floor(x), math.floor(z))
        cell = math.floor(y)
        while cell > 0:
            below = self.world.get_block(column[0], cell - 1, column[1])
            if below != VoxelGen.AIR and below not in VoxelGen.LIQUIDS:
                break
            cell -= 1
        return float(cell)

    def collect_drops(self):
        """Walking up to an item on the ground takes it, when the inventory has room. Creative mode leaves the drops where they are,
        since it keeps no stock: an item taken there would be lost."""
        if self.creative:
            return
        for item in self.drops.take_in_reach(self.pos, self.inventory):
            self.notify("Picked up a %s" % VoxelGen.NAMES[item])

    def spend(self):
        """Placing takes one from the selected slot. Creative mode takes nothing."""
        if not self.creative:
            self.inventory.take(self.slot, 1)

    def drop_torches_on(self, block):
        """Torches leaning on a block that is broken are broken too, and go back into the inventory: each one is in a cell beside
        it, pointing at it."""
        for dir, d in enumerate(VoxelGen.DIRECTIONS):
            cell = (block[0] + d[0], block[1] + d[1], block[2] + d[2])
            if self.world.prop_at(*cell) == DIRECTION_INDEX[(-d[0], -d[1], -d[2])]:
                self.world.remove_prop(*cell)
                self.net_send({"t": "prop", "c": list(cell), "dir": -1})
                self.gain(VoxelGen.TORCH)

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
        """Outline what a break takes (the block the crosshair is on, or the torch it is on), and the cell the selected item
        would go into, in the colour it would have. A hit flashes the break outline white for a moment."""
        if self.paused or self.inventory_open:
            self.show_outline("target_break", None, None)
            self.show_outline("target_place", None, None)
            return
        if self.aims_at_walker():
            lo, hi = self.walker_target[1].box()
            flash = time.time() < self.flash_until
            self.show_outline("target_break", (lo, hi), WHITE if flash else BREAK_COLOUR)
            self.show_outline("target_place", None, None)
            return
        if self.aims_at_torch():
            self.show_outline("target_break", (self.torch_target[2], self.torch_target[3]), BREAK_COLOUR)
            self.show_outline("target_place", None, None)
            return
        if self.target is None:
            self.show_outline("target_break", None, None)
            self.show_outline("target_place", None, None)
            return
        block, normal = self.target
        flash = time.time() < self.flash_until
        self.show_outline("target_break", (tuple(float(v) for v in block),
                                           tuple(float(v + 1) for v in block)), WHITE if flash else BREAK_COLOUR)
        cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
        item = self.selected()
        if normal == (0, 0, 0) or physics.cell_in_player(cell, self.pos) or item is None:
            self.show_outline("target_place", None, None)
            return
        colour = mix(VoxelGen.COLOURS.get(item, (1.0, 1.0, 1.0)), (1.0, 1.0, 1.0), 0.4)
        self.show_outline("target_place", (tuple(float(v) for v in cell),
                                           tuple(float(v + 1) for v in cell)), colour)

    def choose_hotbar(self):
        for slot in range(COLUMNS):
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
        """F5 saves the edits and the inventory, F9 loads them. The held stack goes back into the inventory first, so a save
        does not lose it."""
        if self.pressed("Save", bool(cge.key_down("F5"))):
            os.makedirs("cg_control", exist_ok=True)
            self.world.settle_moves()   # blocks in flight land first, so none is missing from the file
            self.world.save_edits(EDITS_FILE)
            self.inventory.return_held()
            StateFile.write_json(INVENTORY_FILE, {"slots": self.inventory.to_list(), "slot": self.slot,
                                                  "drops": self.drops.to_list()})
            self.notify("Saved")
        if self.pressed("Load", bool(cge.key_down("F9"))):
            self.world.load_edits(EDITS_FILE)
            self.load_inventory()
            self.notify("Loaded")

    def load_inventory(self):
        """The inventory saved with the edits. A missing or unreadable file keeps the inventory the player has."""
        try:
            with open(INVENTORY_FILE) as f:
                data = json.load(f)
            self.inventory.load_list(data.get("slots", []))
            self.slot = int(data.get("slot", self.slot)) % COLUMNS
            self.drops.load_list(data.get("drops", []), lambda item: VoxelGen.ITEM_COLOURS.get(item, (0.8, 0.8, 0.8)))
        except (OSError, ValueError, TypeError, AttributeError):
            pass

    def update_modes(self):
        """C toggles creative mode: placing takes nothing from the slots, and breaking adds nothing to them."""
        if self.pressed("Creative", bool(cge.key_down("C"))):
            self.creative = not self.creative
            self.notify("Creative mode on" if self.creative else "Creative mode off")

    def update_inventory_key(self):
        """I opens the inventory screen, or closes it. It does not open over the pause menu."""
        if self.pressed("Inventory", bool(cge.key_down("I"))):
            if self.inventory_open:
                self.close_inventory()
            elif not self.paused:
                self.open_inventory()

    def open_inventory(self):
        self.inventory_open = True
        self.inventory_hover = None
        self.mouse_before_inventory = self.mouse_captured
        cge.set_mouse_captured(False)
        self.mouse_captured = False

    def close_inventory(self):
        self.inventory_open = False
        self.inventory_hover = None
        self.inventory.return_held()
        if self.mouse_before_inventory:
            cge.set_mouse_captured(True)
            self.mouse_captured = True

    def update_inventory(self):
        """The inventory screen: the left button takes or puts down a whole stack in the slot under the mouse, and the right takes
        half or puts one down (VoxelInventory.click). The buttons are read every frame, so a press is counted once."""
        mouse_x, mouse_y = cge.mouse_position()
        self.inventory_hover = self.slot_at(mouse_x, mouse_y)
        left = self.pressed("InventoryLeft", bool(cge.mouse_down("Left")))
        right = self.pressed("InventoryRight", bool(cge.mouse_down("Right")))
        if self.inventory_hover is None:
            return
        if left:
            self.inventory.click(self.inventory_hover)
        if right:
            self.inventory.click(self.inventory_hover, secondary=True)

    def edit_blocks(self):
        self.choose_hotbar()
        self.update_modes()
        self.save_and_load()
        self.snapshot_key()
        break_down = self.mouse_captured and bool(cge.mouse_down("Left"))
        place_down = self.mouse_captured and bool(cge.mouse_down("Right"))
        breaking = self.pressed("Break", break_down or bool(cge.key_down("J")))
        placing = self.pressed("Place", place_down or bool(cge.key_down("K")))
        if breaking and self.aims_at_walker():
            # A creature the crosshair is on, nearer than the block behind it, is hit: each hit takes one of its HEALTH. The
            # one that kills it leaves a hide for the inventory.
            walker = self.walker_target[1]
            self.flash_until = time.time() + HIT_FLASH_SECONDS
            lo, hi = walker.box()
            centre = [(lo[i] + hi[i]) / 2 for i in range(3)]
            if walker.hurt((self.pos[0], self.pos[2])):
                self.bursts.add(centre, (0.55, 0.45, 0.40), 12, 0.14, 0.7)
                self.walkers.remove(walker)
                self.walker_target = None
                if not self.creative:
                    # The hide lies where the creature fell, on the ground under its middle, for the player to walk up to.
                    rest = self.ground_under(centre[0], lo[1], centre[2])
                    self.drops.add(VoxelGen.HIDE, VoxelGen.ITEM_COLOURS[VoxelGen.HIDE], (centre[0], rest, centre[2]))
                self.notify("A creature died: a hide on the ground" if not self.creative else "A creature died")
            else:
                self.bursts.add(centre, (0.85, 0.25, 0.2), 4, 0.12, 0.4)
            return
        if breaking and self.aims_at_torch():
            # A torch the crosshair is on, nearer than the block behind it, is the one broken, and it goes into the inventory.
            if self.creative or self.inventory.fits(VoxelGen.TORCH):
                self.world.remove_prop(*self.torch_target[1])
                self.net_send({"t": "prop", "c": list(self.torch_target[1]), "dir": -1})
                self.gain(VoxelGen.TORCH)
            else:
                self.notify("Inventory full")
            return
        if self.target is None or not (breaking or placing):
            return
        block, normal = self.target
        if breaking:
            # A block takes its hits (VoxelGen.HITS) to break. The hit that would break it needs room for it in the inventory;
            # a hit that leaves it standing does not, so a full inventory can still damage a block.
            kind = self.world.get_block(*block)
            # A pickaxe in the selected slot takes two swings at the blocks it mines (VoxelGen.PICKAXE_BLOCKS).
            swings = 2 if self.selected() == VoxelGen.PICKAXE and kind in VoxelGen.PICKAXE_BLOCKS else 1
            breaks = self.world.damage_at(*block) + swings >= VoxelGen.HITS.get(kind, 1)
            if breaks and not self.creative and not self.inventory.fits(kind):
                self.notify("Inventory full")
                return
            stage = -2
            for _ in range(swings):
                stage = self.world.hit_block(block[0], block[1], block[2])
                if stage < 0:
                    break
            colour = VoxelGen.COLOURS.get(kind, (0.5, 0.5, 0.5))
            centre = [block[i] + 0.5 for i in range(3)]
            if stage != -2:
                self.flash_until = time.time() + HIT_FLASH_SECONDS
            if stage == -1:
                self.hit_times.pop(tuple(block), None)
                self.drop_torches_on(block)
                self.gain(kind)
                self.net_send({"t": "block", "c": list(block), "id": 0})
                self.bursts.add(centre, colour, 10, 0.16, 0.6)
            elif stage > 0:
                self.hit_times[tuple(block)] = time.time()
                self.net_send({"t": "hit", "c": list(block), "stage": stage})
                # The chips fly from the face that was hit, not from the middle of the block.
                face = [centre[i] + normal[i] * 0.5 for i in range(3)]
                self.bursts.add(face, tuple(c * 0.7 for c in colour), 3, 0.14, 0.35)
        elif placing:
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if physics.cell_in_player(cell, self.pos):
                return
            item = self.selected()
            if item is None:
                if not self.creative:
                    self.notify("This slot is empty")
                return
            if item not in VoxelGen.COLOURS:
                self.notify("A %s cannot be placed" % VoxelGen.NAMES[item])
                return
            if item == VoxelGen.TORCH:
                # A torch goes on a face: into the air cell in front of a solid block, leaning on it.
                if self.world.is_solid(block[0], block[1], block[2]) and self.world.get_block(cell[0], cell[1], cell[2]) == VoxelGen.AIR:
                    direction = DIRECTION_INDEX[(-normal[0], -normal[1], -normal[2])]
                    self.world.set_prop(cell[0], cell[1], cell[2], direction)
                    self.net_send({"t": "prop", "c": list(cell), "dir": direction})
                    self.spend()
            else:
                self.world.set_block(cell[0], cell[1], cell[2], item)
                self.net_send({"t": "block", "c": list(cell), "id": item})
                self.spend()

    # ----- the other process (VoxelNet) ---------------------------------------------------

    def net_send(self, message):
        if self.net is not None:
            self.net.send(message)

    def update_net(self, dt):
        """Read the messages that arrived, apply them, retry the ones for chunks not loaded yet, and send the player's position."""
        if self.net is None:
            return
        for message in self.net.poll():
            self.apply_remote(message)
        self.apply_pending()
        self.net_pos_timer -= dt
        if self.net_pos_timer <= 0.0 and not self.paused:
            self.net_pos_timer = POS_INTERVAL
            self.net.send({"t": "pos", "p": [round(v, 3) for v in self.pos], "yaw": round(self.yaw, 2)})

    def apply_remote(self, message):
        kind = message.get("t")
        if kind == "pos":
            self.remote_pos = message
            self.show_remote(message["p"])
        elif kind == "block":
            self.set_remote(tuple(message["c"]), "block", int(message["id"]))
        elif kind == "prop":
            self.set_remote(tuple(message["c"]), "prop", int(message["dir"]))
        elif kind == "hit":
            self.set_remote(tuple(message["c"]), "hit", int(message["stage"]))

    def set_remote(self, cell, kind, value):
        """Apply a remote edit now if its chunk is loaded, else keep it until the chunk loads (the world ignores edits to
        chunks that are not loaded)."""
        if not self.apply_remote_edit(cell, kind, value):
            self.net_pending[cell] = (kind, value)

    def apply_remote_edit(self, cell, kind, value):
        if not self.world.is_loaded(math.floor(cell[0] / VoxelGen.CHUNK), math.floor(cell[2] / VoxelGen.CHUNK)):
            return False
        if kind == "block":
            self.world.set_block(cell[0], cell[1], cell[2], value)
        elif kind == "prop":
            if value < 0:
                self.world.remove_prop(*cell)
            else:
                self.world.set_prop(cell[0], cell[1], cell[2], value)
        elif kind == "hit":
            # The other process's stage: hit up to it, never past it (a break comes as a block message).
            hits = VoxelGen.HITS.get(self.world.get_block(*cell), 1)
            while self.world.damage_at(*cell) < min(value, hits - 1):
                self.world.hit_block(cell[0], cell[1], cell[2])
        self.net_applied += 1
        return True

    def apply_pending(self):
        for cell, (kind, value) in list(self.net_pending.items()):
            if self.apply_remote_edit(cell, kind, value):
                del self.net_pending[cell]

    def show_remote(self, feet):
        """The other player, as a box where their feet are: made the first time, then moved."""
        if "remote_player" not in self.bodies:
            self.bodies.add("remote_player")
            cge.create_mesh_body("remote_player", *boxes_arrays([((-0.3, 0.0, -0.3), (0.3, 1.8, 0.3), (0.9, 0.5, 0.2))]),
                                 materials=["voxel"], position=cge.Vector3f(*feet))
        else:
            cge.find_body("remote_player").get_mesh().set_position(cge.Vector3f(*feet))

    # ----- sky ----------------------------------------------------------------------------

    def update_healing(self):
        """A damaged block that is left alone for HEAL_AFTER seconds heals a stage every HEAL_STEP seconds, so a half-broken
        wall is a choice rather than a trap."""
        now = time.time()
        for cell, last in list(self.hit_times.items()):
            if now - last < HEAL_AFTER:
                continue
            if self.world.heal_block(*cell) <= 0:
                del self.hit_times[cell]
            else:
                self.hit_times[cell] = now - HEAL_AFTER + HEAL_STEP

    def track_fall(self):
        """The fall: the highest point the player leaves the ground from, to the height they land at. A fall past FALL_SAFE
        blocks hurts. Flying and swimming are not falls, and the spawn drop is not counted (landed_once)."""
        if self.flying or self.swimming:
            self.fell_from = None
        elif not self.on_ground:
            if self.landed_once:
                self.fell_from = self.pos[1] if self.fell_from is None else max(self.fell_from, self.pos[1])
        else:
            self.landed_once = True
            if self.fell_from is not None:
                fall = self.fell_from - self.pos[1]
                self.fell_from = None
                if fall > FALL_SAFE:
                    self.hurt_player((fall - FALL_SAFE) * FALL_DAMAGE)

    def hurt_player(self, amount):
        """The player loses health (a bite, a fall). At zero the player is back at the spawn, healed, with the inventory kept.
        Creative mode is invincible: nothing hurts the player."""
        if amount <= 0.0 or self.creative:
            return
        self.health -= amount
        self.hurt_at = time.time()
        self.hurt_flash_until = self.hurt_at + HURT_FLASH_SECONDS
        if self.health > 0.0:
            return
        self.pos = [self.spawn_pos[0], SPAWN_HEIGHT, self.spawn_pos[2]]
        self.vel = [0.0, 0.0, 0.0]
        self.health = PLAYER_HEALTH
        self.fell_from = None
        self.landed_once = False
        self.notify("You died. Back at the spawn, with the inventory kept")

    def update_health(self, dt):
        """The player heals REGEN_RATE a second once REGEN_DELAY seconds have passed without a hit. The world's time is dt, so
        nothing heals while the game is paused."""
        if self.health < PLAYER_HEALTH and time.time() - self.hurt_at > REGEN_DELAY:
            self.health = min(PLAYER_HEALTH, self.health + REGEN_RATE * dt)

    def update_health_bar(self):
        """Ten segments above the hotbar: red for the health left, dark for the rest. Not drawn under the pause menu."""
        segments = int(PLAYER_HEALTH)
        row_width = segments * HEALTH_SEGMENT + (segments - 1) * SLOT_GAP
        left = (self.window.x - row_width) / 2
        top = self.slot_origin(0)[1] - HEALTH_SEGMENT - 6
        for i in range(segments):
            x = left + i * (HEALTH_SEGMENT + SLOT_GAP)
            box = (x, top, HEALTH_SEGMENT, HEALTH_SEGMENT)
            if self.paused:
                self.hide_rect("hp_%d" % i, box)
                self.hide_rect("hp_fill_%d" % i, box)
                continue
            fill = max(0.0, min(1.0, self.health - i))
            self.draw_rect("hp_%d" % i, box, (50, 14, 14, 200), (0, 0, 0, 220), 1, layer=985)
            inner = HEALTH_SEGMENT - 4
            self.draw_rect("hp_fill_%d" % i, (x + 2, top + 2, inner * fill, inner), (210, 45, 45, 255), layer=986)

    def update_hurt_flash(self):
        """A red wash over the whole window at the moment of a hurt, fading to nothing over HURT_FLASH_SECONDS. It sits under
        the hotbar and the health bar. Clear (alpha 0) when there is no hurt or the pause menu is up."""
        left = self.hurt_flash_until - time.time()
        alpha = 0
        if left > 0.0 and not self.paused:
            alpha = int(HURT_FLASH_ALPHA * min(1.0, left / HURT_FLASH_SECONDS))
        self.draw_rect("hurt_flash", (0, 0, self.window.x, self.window.y), (200, 20, 20, alpha), layer=970)

    def walker_under_aim(self, eye, look):
        """The nearest creature the look ray meets within reach, as (distance, walker), or None. The boxes are the creatures' own."""
        if self.walkers is None:
            return None
        best = None
        for walker in self.walkers.walkers:
            lo, hi = walker.box()
            centre = tuple((lo[i] + hi[i]) / 2 for i in range(3))
            half = tuple((hi[i] - lo[i]) / 2 for i in range(3))
            t = ray_box(eye, look, centre, half)
            if t is not None and t <= physics.REACH and (best is None or t < best[0]):
                best = (t, walker)
        return best

    def aims_at_walker(self):
        """True when the crosshair is on a creature that is nearer than the block behind it: a hit takes the creature then."""
        if self.walker_target is None:
            return False
        if self.target is None:
            return True
        block = self.target[0]
        block_distance = math.dist(self.eye(), (block[0] + 0.5, block[1] + 0.5, block[2] + 0.5))
        return self.walker_target[0] < block_distance

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

        fog_end = (self.view_radius + 0.5) * VoxelGen.CHUNK   # the fog is complete at the edge of the loaded chunks
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
                 "menu": self.menu_page if self.paused else None, "sensitivity": self.sensitivity, "view_radius": self.view_radius,
                 "seed": self.seed, "frames": self.frames, "inventory": self.inventory.to_list(), "drops": len(self.drops.live), "creative": self.creative,
                 "inventory_open": self.inventory_open, "health": round(self.health, 3),
                 "net": None if self.net is None else {"mode": self.net.mode, "connected": self.net.connected,
                                                       "sent": self.net.sent, "received": self.net.received,
                                                       "applied": self.net_applied, "pending": len(self.net_pending),
                                                       "remote": self.remote_pos},
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
