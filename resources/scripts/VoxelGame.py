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
import VoxelConsole
import VoxelDrops
import VoxelGen
import VoxelInventory
import VoxelNet
import VoxelParticles
import VoxelRecipes
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays, mix, outline_arrays
import VoxelWalkers

# The view distance, in chunks each way from the player (the settings page changes it). The default is the middle step, 14, which
# is (2 * 14 + 1)^2 = 841 chunks. The last step is "Unlimited", capped at UNLIMITED_VIEW: the chunks grow with the square of the
# distance and each one is held in memory. Without far skins (the default), 40 is about 6,600 chunks, 8.4 million quads and
# 2.2 GB private memory at about 96 frames a second; 60 measured 4.6 GB, so the cap stays at 40. With far skins on
# (VOXEL_FAR_RADIUS=6), 60 is about 14,600 chunks and 1.5 GB, but the far skins showed dark blotches (roadmap 15.8). The far
# skins are on from 12 chunks (roadmap 22.3): the launch to a settled view at 40 falls from 18 s to 12 s. Their vertex colour
# now takes the surface block (VoxelMesher exposedBlock), which removed the dirt showing through the grass.
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
TIME_SPEEDS = [("Paused", 0.0), ("Slow", 0.25), ("Normal", 1.0), ("Fast", 4.0)]   # the choices on the settings page
MAX_PITCH = 85.0            # the engine's camera stops at this angle
# VOXEL_FIXED_STEP (seconds) makes every frame advance the same time, so a scripted test holds a key for the same game time
# whatever the frame rate. Unset, a frame advances the real time since the last one.
FIXED_STEP = float(os.environ["VOXEL_FIXED_STEP"]) if os.environ.get("VOXEL_FIXED_STEP") else None
SPAWN_HEIGHT = 60.0         # above the tallest column, so the player drops onto the terrain
RESPAWN_DEPTH = -20.0       # below this the player is put back at the spawn height
# The player starts with nothing: the inventory (VoxelInventory) fills by breaking blocks. The files the F5 and F9 keys save and
# load are the edits and, beside them, the inventory and the known recipes.
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
RETICLE_PLAIN = (255, 255, 255, 255)   # the crosshair when it is not on a surface within reach
RETICLE_AIM = (240, 72, 36, 255)      # the crosshair when it is on a surface within reach (VoxelGame.update_reticle)
HIT_FLASH_SECONDS = 0.12    # the aimed block's outline is white this long after a hit
PLAYER_HEALTH = 10.0        # the player's health: ten segments on the bar above the hotbar
REGEN_DELAY = 6.0           # seconds without a hit before the player starts to heal
HURT_FLASH_SECONDS = 0.4    # the screen reddens this long after a bite or a fall
HURT_FLASH_ALPHA = 110      # the most red the screen gets (0 to 255), at the moment of the hurt, fading after
REGEN_RATE = 0.25           # health a second once it does
FALL_SAFE = 4.0             # blocks a player can fall without harm
FALL_DAMAGE = 1.0           # health lost for each block past FALL_SAFE
CRAFT_ROWS = 8             # recipes the crafting panel lists at once
CRAFT_ROW = 30             # pixels a recipe row takes
CRAFT_HEADER = 38          # pixels above the first row: the dropdown button, and its options when it is open
CRAFT_FOOTER = 24          # pixels below the last row: the hint
CRAFT_BUTTON_WIDTH = 220   # pixels, the dropdown button's width
HEALTH_SEGMENT = 16         # pixels, a side, of one segment of the health bar
INVENTORY_FILE = "cg_control/voxel_inventory.json"
EDITS_FILE = "cg_control/voxel_edits.bin"
NET_SPEC = os.environ.get("VOXEL_NET")   # listen:<port> or connect:<host>:<port>: two processes share their edits (VoxelNet)
POS_INTERVAL = 0.1         # seconds between the player's position messages


def inside(box, x, y):
    """True when a window point is inside a box (x, y, width, height)."""
    return box[0] <= x < box[0] + box[2] and box[1] <= y < box[1] + box[3]


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
FAR_RADIUS = int(os.environ.get("VOXEL_FAR_RADIUS", "12"))   # chunks more than this from the player get far skins (stride 2); 0 is off (roadmap 15.8, 22.3)
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
READY_RADII = (2, 6)          # the squares, in chunks around the player, whose ready counts the state file reports, besides the view (roadmap 22.1)
# The outline of the block the crosshair is on (yellow: it is broken), and of the cell a block would be placed in
# (its own colour, brightened). Both are thin bars around the cube's edges, made as runtime meshes.
HELP_TEXT = "LMB break  RMB place  WASD move  Space jump  Enter fly  1-0 - = slot  I inventory  C creative  F5/F9 save/load  Esc release"
BREAK_COLOUR = (1.0, 0.85, 0.2)
HIDDEN_BOX = ((-1.0e6, -1.0e6, -1.0e6), (-1.0e6 + 1.0, -1.0e6 + 1.0, -1.0e6 + 1.0))   # where an outline is put when not shown
SMOOTH_RESOLUTION = int(os.environ.get("VOXEL_SMOOTH", "1"))   # samples per block for the smooth skin; 0 draws cubes
DAY_SECONDS = float(os.environ.get("VOXEL_DAY_SECONDS", "240"))   # real seconds for one day and night
START_TIME = float(os.environ.get("VOXEL_TIME", "0.3"))           # time of day at the start: 0 sunrise, 0.25 noon, 0.5 sunset
FOG_START_FRACTION = 0.5             # the fog starts at this fraction of its end distance
CAMERA_NEAR = 0.1                    # blocks: the nearest surface the camera draws (the engine's default was 0.01)

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
    "settings": ("SETTINGS", ("sens_down", "sens_value", "sens_up", "view_down", "view_value", "view_up",
                              "window_down", "window_value", "window_up", "speed_down", "speed_value", "speed_up",
                              "time_value", "time_track", "back")),
}
MENU_TEXT = {"resume": "Resume", "settings": "Settings", "quit": "Quit", "back": "Back", "sens_down": "<", "sens_up": ">",
             "view_down": "<", "view_up": ">", "window_down": "<", "window_up": ">", "speed_down": "<", "speed_up": ">"}


def load_settings():
    """The saved settings: the mouse sensitivity (a percent, one of SENSITIVITY_STEPS) and the time of day (0 to 1, or None
    if none is saved). Missing or unreadable settings keep their defaults."""
    settings = {"mouse_sensitivity": 100, "time_of_day": None, "view_radius": DEFAULT_VIEW, "time_speed": 1.0,
                "fullscreen": False}
    try:
        with open(SETTINGS_FILE) as f:
            data = json.load(f)
        settings["mouse_sensitivity"] = int(data.get("mouse_sensitivity", 100))
        if "time_of_day" in data:
            settings["time_of_day"] = float(data["time_of_day"]) % 1.0
        settings["view_radius"] = int(data.get("view_radius", DEFAULT_VIEW))
        settings["time_speed"] = min(100.0, max(0.0, float(data.get("time_speed", 1.0))))
        settings["fullscreen"] = bool(data.get("fullscreen", False))
    except (OSError, ValueError, TypeError, AttributeError):
        pass
    settings["mouse_sensitivity"] = min(SENSITIVITY_STEPS, key=lambda step: abs(step - settings["mouse_sensitivity"]))
    settings["view_radius"] = min(VIEW_STEPS, key=lambda step: abs(step - settings["view_radius"]))
    return settings


def save_settings(sensitivity, time_of_day, view_radius, time_speed, fullscreen):
    StateFile.write_json(SETTINGS_FILE, {"mouse_sensitivity": sensitivity, "time_of_day": round(time_of_day, 5),
                                         "view_radius": view_radius, "time_speed": time_speed, "fullscreen": fullscreen})


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
        self.time_speed = saved["time_speed"]           # how fast the sun moves: 0 pauses it, 1 is a day every DAY_SECONDS
        self.fullscreen = saved["fullscreen"]           # full screen or windowed (applied once the window is up)
        self.console = VoxelConsole.Console(self)       # the developer console, opened with backtick
        self.title_page = None                          # the page whose title is placed (see show_menu)
        self.menu_titles = {}
        self.dragging = False           # the time-of-day slider is being dragged
        self.outline_keys = {}          # the last outline shown under each name, so unchanged ones are not re-uploaded
        self.outline_bodies = set()     # the outlines already made as runtime meshes
        self.slot = 0                   # the hotbar slot placing uses
        self.inventory = VoxelInventory.Inventory()   # what the player holds, by slot (the hotbar is the first row)
        self.creative = os.environ.get("VOXEL_CREATIVE", "0") == "1"   # placing takes nothing, breaking gives nothing
        self.inventory_open = False     # the inventory screen is up: the world stands still and the mouse is free
        self.inventory_hover = None     # the slot under the mouse on the inventory screen
        self.preview_key = None         # the placement the ghost shows (cell, item, dir), or None
        self.reticle_aiming = False     # the crosshair is coloured (on a surface within reach)
        self.crafting = VoxelRecipes.Crafting()   # the recipes the player knows, and the crafting panel's dropdown
        self.craft_hover = None         # the recipe row under the mouse on the crafting panel
        self.mouse_before_inventory = False   # the mouse was captured when the screen opened, so it is again when it closes
        self.notice = ""                # a short message on the HUD ("Inventory full"), shown until notice_until
        self.notice_until = 0.0
        self.flash_until = 0.0          # the aimed block's outline is white until this time, after a hit
        self.health = PLAYER_HEALTH     # the player's health (a bite, a fall and a death change it)
        self.deaths = 0                 # how many times the player has died this run (written to the state file for the tests)
        self.closest_creature = None    # the distance to the nearest creature, from the last frame's update (the state file)
        self.hurt_at = -1e9             # when the player was last hurt (the healing waits after it)
        self.hurt_flash_until = 0.0     # the screen is red until this time, after a hurt
        self.fell_from = None           # the height the player left the ground at, while in the air
        self.landed_once = False        # a fall counts only after the player has stood on the ground (the spawn drop does not)
        self.spawn_pos = list(self.pos)
        self.walker_target = None       # (distance, walker) of the creature under the crosshair, or None
        self.bursts = VoxelParticles.Bursts(random.Random(self.seed))   # the chips a hit or a break throws
        self.drops = VoxelDrops.Drops()  # the items lying on the ground, taken by walking up to them
        self.net = None                 # the link to the other process (VOXEL_NET), or None for a single player
        self.net_pending = {}           # the host's cell states for chunks not loaded yet: cell -> (block, damage, prop)
        self.net_touched = set()        # cells a client asked for this frame, sent back to it even if the host refused them
        self.net_snapshot = False       # send every edited cell next frame (a new client, or a load on the host)
        self.net_pos_timer = 0.0
        self.net_applied = 0            # cell states applied to this world (a client)
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
        self.clip_planes = None         # the camera's (near, far) clipping distances, as last set

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
            self.hide_placement_outlines()
            self.setup_hud()
            self.hud_ready = True
            self.apply_window_settings()
        self.update_pause_key()
        self.update_inventory_key()
        self.console.update()
        if self.paused:
            # The world stands still while the menu is open: no time passes, no block moves, the player does not move.
            self.update_pause_menu()
            dt = 0.0
        elif self.inventory_open:
            # The inventory screen stands the world still too, and frees the mouse for its slots.
            self.update_inventory()
            dt = 0.0
        elif self.console.open:
            # The console stands the world still while it is open, so typing a command moves nothing.
            dt = 0.0
        else:
            # A client does not run the block physics: the host does, and its changes arrive as cells (VoxelNet).
            if not self.net_client:
                self.world.simulate(dt, SIM_BUDGET_MS, SIM_TICK)
            self.update_mouse()
            self.update_player(dt)
            self.collect_drops()
            self.update_walkers(dt)
            self.update_target()
            self.edit_blocks()
            self.learn_recipes()
        self.update_outlines()
        self.update_preview()
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
        spec = VoxelNet.parse(NET_SPEC)
        if spec is not None:
            try:
                self.net = VoxelNet.Link(*spec)
            except OSError as error:
                print("[voxel] could not open the network link (%s): playing alone" % error)
        self.world.set_recording(self.net_host)   # the host sends the cells that change (VoxelNet)
        cge.add_command("voxel_edit", lambda text: json.dumps(self.control_edit(json.loads(text))))

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
        geometry = self.menu_geometry(size)
        self.menu_layout = geometry["items"]
        self.menu_titles = geometry["titles"]
        cge.create_text_body("menu_title", "", "hud", cge.Vector2f(*self.menu_titles["pause"]), 28, outline_thickness=HUD_OUTLINE)
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
        if shown and self.title_page != self.menu_page:
            # Each page has its own title place: the settings page is taller, so its title is higher.
            self.title_page = self.menu_page
            cge.set_text_body("menu_title", title, position=cge.Vector2f(*self.menu_titles[self.menu_page]))
        for item in self.menu_layout:
            self.set_menu_text("menu_" + item, self.menu_label(item) if item in items else "")

    def menu_label(self, item):
        if item == "sens_value":
            return "Mouse sensitivity: %d%%" % self.sensitivity
        if item == "view_value":
            if self.view_radius == UNLIMITED_VIEW:
                return "View distance: Unlimited (%d)" % self.view_radius
            return "View distance: %d chunks" % self.view_radius
        if item == "window_value":
            return "Window: %s" % ("full screen" if self.fullscreen else "windowed")
        if item == "speed_value":
            return "Time speed: %s" % self.time_speed_name()
        if item == "time_value":
            minutes = (round(self.clock * 1440) + 360) % 1440   # 0 is sunrise (06:00), 0.25 noon, 0.5 sunset, 0.75 midnight
            return "Time of day: %02d:%02d" % (minutes // 60, minutes % 60)
        if item == "time_track":
            knob = min(TRACK_CELLS - 1, int(self.clock * TRACK_CELLS))
            return "[" + "-" * knob + "O" + "-" * (TRACK_CELLS - 1 - knob) + "]"
        text = MENU_TEXT[item]
        if self.menu_hover == item:
            return "[%s]" % text if item.endswith(("_down", "_up")) else "> %s <" % text
        return text

    def update_pause_key(self):
        if self.console.open:
            # The console takes Escape (it closes), so the pause menu does not open over it.
            return
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

    def save_prefs(self):
        """The settings the game keeps between runs: the mouse sensitivity, the time of day and its speed, the view distance and
        the window's mode."""
        save_settings(self.sensitivity, self.clock, self.view_radius, self.time_speed, self.fullscreen)

    def set_clock(self, clock):
        self.clock = clock % 1.0
        self.save_prefs()

    def set_time_speed(self, speed):
        self.time_speed = speed
        self.save_prefs()

    def step_time_speed(self, step):
        """One step along TIME_SPEEDS (the settings page's arrows), from the choice nearest the current speed."""
        values = [value for _name, value in TIME_SPEEDS]
        index = min(range(len(values)), key=lambda i: abs(values[i] - self.time_speed))
        self.set_time_speed(values[max(0, min(len(values) - 1, index + step))])

    def time_speed_name(self):
        for name, value in TIME_SPEEDS:
            if value == self.time_speed:
                return name
        return "%gx" % self.time_speed

    def set_creative(self, on):
        self.creative = on
        self.notify("Creative mode on" if on else "Creative mode off")

    def set_view_distance(self, chunks):
        """The view distance nearest to chunks, from VIEW_STEPS. The chunks are asked for again at once (request_chunks)."""
        self.view_radius = min(VIEW_STEPS, key=lambda step: abs(step - chunks))
        self.wanted_centre = None
        self.save_prefs()
        return self.view_radius

    def teleport(self, x, y, z):
        self.pos = [x, y, z]
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.fell_from = None

    def heal(self, amount=None):
        """Health back to all of it, or to amount (at most all of it). Returns the health."""
        self.health = PLAYER_HEALTH if amount is None else max(0.0, min(PLAYER_HEALTH, amount))
        return self.health

    def set_fullscreen(self, on):
        """Full screen or windowed (cge.set_fullscreen). The HUD is laid out for the new size on the next frame. False if the
        window cannot switch here."""
        if not cge.set_fullscreen(on):
            return False
        self.fullscreen = cge.is_fullscreen()
        self.save_prefs()
        return True

    def toggle_fullscreen(self):
        if not self.set_fullscreen(not self.fullscreen):
            self.notify("Full screen is not available here")

    def apply_window_settings(self):
        """At the start: the engine's own console is hidden (the game has its own, on backtick), and the saved window mode is used."""
        console_body = cge.find_body("Console")
        if console_body is not None:
            console_body.set_rendering_enabled(False)
        if self.fullscreen != cge.is_fullscreen():
            self.set_fullscreen(self.fullscreen)

    def menu_geometry(self, size):
        """Where the pause menu's items and titles are for a window of this size. Each item has a click rectangle (left, top,
        right, bottom) or None, and a text position. The settings rows are stacked from the middle of the window."""
        cx, cy = size.x / 2, size.y / 2
        items = {
            "resume": ((cx - 100, cy - 4, cx + 100, cy + 30), (cx - 62, cy)),
            "settings": ((cx - 100, cy + 40, cx + 100, cy + 74), (cx - 62, cy + 44)),
            "quit": ((cx - 100, cy + 84, cx + 100, cy + 118), (cx - 40, cy + 88)),
            "sens_value": (None, (cx - 120, cy - 92)),
            "view_value": (None, (cx - 120, cy - 52)),
            "window_value": (None, (cx - 120, cy - 12)),
            "speed_value": (None, (cx - 120, cy + 28)),
            "time_value": (None, (cx - 120, cy + 64)),
            "time_track": ((cx - 150, cy + 84, cx + 150, cy + 110), (cx - 150, cy + 86)),
            "back": ((cx - 100, cy + 124, cx + 100, cy + 158), (cx - 30, cy + 128)),
        }
        for row, top in (("sens", cy - 96), ("view", cy - 56), ("window", cy - 16), ("speed", cy + 24)):
            items[row + "_down"] = ((cx - 215, top, cx - 150, top + 30), (cx - 200, top + 4))
            items[row + "_up"] = ((cx + 150, top, cx + 215, top + 30), (cx + 165, top + 4))
        return {"items": items, "titles": {"pause": (cx - 80, cy - 60), "settings": (cx - 90, cy - 130)},
                "cross": (cx - 6, cy - 13)}

    def layout_hud(self):
        """Places the crosshair and the pause menu for the window's current size."""
        geometry = self.menu_geometry(self.window)
        self.menu_layout = geometry["items"]
        self.menu_titles = geometry["titles"]
        cross_x, cross_y = geometry["cross"]
        cge.set_text_body("hud_cross", self.menu_text.get("hud_cross", ""), position=cge.Vector2f(cross_x, cross_y))
        for item, (_rect, (x, y)) in self.menu_layout.items():
            cge.set_text_body("menu_" + item, self.menu_text.get("menu_" + item, ""), position=cge.Vector2f(x, y))
        self.title_page = None   # the title goes to its place for the page on the next show_menu
        self.show_menu(self.paused)

    def draw_console(self):
        """The console's log and input line, under the help text, at the top of the window."""
        width = min(820, self.window.x - 28)
        self.console.draw((14, 62, width, (VoxelConsole.SHOWN_LINES + 1) * 18 + 12), layer=975)

    def change_sensitivity(self, step):
        index = SENSITIVITY_STEPS.index(self.sensitivity) + step
        self.sensitivity = SENSITIVITY_STEPS[max(0, min(len(SENSITIVITY_STEPS) - 1, index))]
        self.save_prefs()

    def change_view(self, step):
        """One step of view distance. The chunks are asked for again at once: a longer view requests the new ones, and a shorter
        one unloads the far ones (request_chunks)."""
        index = VIEW_STEPS.index(self.view_radius) + step
        self.view_radius = VIEW_STEPS[max(0, min(len(VIEW_STEPS) - 1, index))]
        self.wanted_centre = None
        self.save_prefs()

    def drag_time(self, mouse_x, pressing):
        """The time-of-day slider: while the button is held on the track, the time follows the mouse; the choice is saved
        when the button comes up."""
        left, right = self.menu_layout["time_track"][0][0], self.menu_layout["time_track"][0][2]
        if pressing and (self.dragging or self.menu_hover == "time_track"):
            self.dragging = True
            self.clock = min(0.999, max(0.0, (mouse_x - left) / (right - left)))
        elif self.dragging:
            self.dragging = False
            self.save_prefs()

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
        if clicked and self.menu_hover in ("window_down", "window_up"):
            self.toggle_fullscreen()
        if clicked and self.menu_hover == "speed_down":
            self.step_time_speed(-1)
        if clicked and self.menu_hover == "speed_up":
            self.step_time_speed(1)
        self.show_menu(True)

    def update_reticle(self):
        """The crosshair is coloured while it is on a surface within reach (self.target), and white otherwise."""
        aiming = self.target is not None and not (self.paused or self.inventory_open or self.console.open)
        if aiming != self.reticle_aiming:
            self.reticle_aiming = aiming
            cge.set_text_body("hud_cross", self.menu_text.get("hud_cross", ""),
                              colour=cge.Color(*(RETICLE_AIM if aiming else RETICLE_PLAIN)))

    def set_target_text(self, text):
        if self.target_text != text:
            self.target_text = text
            cge.set_text_body("hud_target", text)

    def update_hud(self):
        if not self.hud_ready:
            return
        size = cge.window_size()
        if (size.x, size.y) != (self.window.x, self.window.y):
            # The window changed size (full screen): the HUD is laid out for it again.
            self.window = size
            self.layout_hud()
        # The crosshair is for aiming, so it is hidden while a menu, the inventory or the console is open.
        self.set_menu_text("hud_cross", "" if self.paused or self.inventory_open or self.console.open else "+")
        self.update_reticle()
        biome = self.world.biome_at(math.floor(self.pos[0]), math.floor(self.pos[2])).capitalize()
        if self.paused or self.inventory_open or self.console.open:
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
        self.update_crafting_screen()
        self.update_notice()
        self.draw_console()

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

    # ----- the crafting panel: above the inventory screen, a category dropdown and the recipes ------------

    def craft_layout(self, count):
        """The crafting panel's boxes in window pixels, for count recipes: the panel, the dropdown button, its options (one a
        category), and a row per recipe shown. The panel sits above the inventory screen."""
        inv_left, inv_top = self.slot_origin(2 * COLUMNS)
        inv_width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        shown = min(max(count, 1), CRAFT_ROWS)
        height = CRAFT_HEADER + shown * CRAFT_ROW + CRAFT_FOOTER
        left = inv_left - 14
        top = inv_top - 46 - 12 - height
        panel = (left, top, inv_width + 28, height)
        button = (left + 12, top + 8, CRAFT_BUTTON_WIDTH, 24)
        options = [(left + 12, top + CRAFT_HEADER + k * 24, CRAFT_BUTTON_WIDTH, 22) for k in range(len(VoxelRecipes.CATEGORIES))]
        rows = [(left + 8, top + CRAFT_HEADER + i * CRAFT_ROW, inv_width + 12, CRAFT_ROW - 4) for i in range(shown)]
        return panel, button, options, rows

    def crafting_at(self, x, y):
        """What a window point is on the crafting panel, while the inventory screen is open: ("button", None), ("option", the
        category), ("row", the index of the recipe), or None. While the dropdown is open its list covers the rows, so only the
        list counts."""
        if not self.inventory_open:
            return None
        visible = self.crafting.visible(self.creative)
        _, button, options, rows = self.craft_layout(len(visible))
        if inside(button, x, y):
            return ("button", None)
        if self.crafting.dropdown_open:
            for name, box in zip(VoxelRecipes.CATEGORIES, options):
                if inside(box, x, y):
                    return ("option", name)
            return None
        for index, box in enumerate(rows):
            if inside(box, x, y):
                return ("row", index)
        return None

    def click_crafting(self, target):
        """A click on the crafting panel: the button opens or closes the dropdown, an option picks the category, and a recipe row
        makes that recipe. A recipe that cannot be made says what is missing."""
        kind, value = target
        if kind == "button":
            self.crafting.dropdown_open = not self.crafting.dropdown_open
        elif kind == "option":
            self.crafting.category = value
            self.crafting.dropdown_open = False
        elif kind == "row":
            recipes = self.crafting.visible(self.creative)
            if value >= len(recipes):
                return
            recipe = recipes[value]
            if self.crafting.craft(recipe, self.inventory, self.creative):
                self.notify("Made %s" % VoxelRecipes.describe(recipe["makes"]))
            else:
                lacking = self.crafting.missing(recipe, self.inventory, self.creative)
                if lacking:
                    self.notify("Missing: %s" % VoxelRecipes.describe(lacking))
                else:
                    self.notify("Inventory full: no room for the %s" % VoxelRecipes.describe(recipe["makes"]))

    def update_crafting_screen(self):
        """The crafting panel: the dropdown button with the chosen category, its options while it is open, and a row per recipe
        the player knows in that category. A recipe with an item missing is drawn dim, and says what it needs. Hidden while the
        inventory screen is closed."""
        visible = self.crafting.visible(self.creative) if self.inventory_open else []
        panel, button, options, rows = self.craft_layout(len(visible))
        if not self.inventory_open:
            self.hide_rect("craft_panel", (0, 0, 1, 1))
            self.hide_rect("craft_button", (0, 0, 1, 1))
            self.draw_text("craft_button_text", "", (0, 0), 14)
            self.draw_text("craft_title", "", (0, 0), 16)
            self.draw_text("craft_hint", "", (0, 0), 12)
            self.draw_text("craft_empty", "", (0, 0), 13)
            for k in range(len(VoxelRecipes.CATEGORIES)):
                self.hide_rect("craft_option_%d" % k, (0, 0, 1, 1))
                self.draw_text("craft_option_text_%d" % k, "", (0, 0), 13)
            for index in range(CRAFT_ROWS):
                self.hide_rect("craft_row_%d" % index, (0, 0, 1, 1))
                self.draw_text("craft_name_%d" % index, "", (0, 0), 15)
                self.draw_text("craft_detail_%d" % index, "", (0, 0), 13)
            return
        self.draw_rect("craft_panel", panel, (12, 12, 16, 225), (130, 130, 140, 255), 2, layer=980)
        self.draw_rect("craft_button", button, (32, 32, 40, 255), (150, 150, 160, 255), 1, layer=982)
        self.draw_text("craft_button_text", "Category: %s  v" % self.crafting.category, (button[0] + 8, button[1] + 4), 14)
        self.draw_text("craft_title", "Crafting", (button[0] + button[2] + 14, button[1] + 3), 16)
        self.draw_text("craft_hint", "Click a recipe to make it. I or Esc: close",
                       (panel[0] + 12, panel[1] + panel[3] - 20), 12, (200, 200, 200, 255))
        for k, name in enumerate(VoxelRecipes.CATEGORIES):
            if self.crafting.dropdown_open:
                box = options[k]
                edge = (255, 255, 255, 255) if name == self.crafting.category else (100, 100, 110, 255)
                self.draw_rect("craft_option_%d" % k, box, (24, 24, 30, 250), edge, 1, layer=996)
                self.draw_text("craft_option_text_%d" % k, name, (box[0] + 8, box[1] + 3), 13)
            else:
                self.hide_rect("craft_option_%d" % k, (0, 0, 1, 1))
                self.draw_text("craft_option_text_%d" % k, "", (0, 0), 13)
        if not visible:
            text = ("No recipes yet. Pick up an item to learn the recipes that use it."
                    if not self.crafting.known and not self.creative else "No recipes in this category.")
            self.draw_text("craft_empty", text, (rows[0][0] + 8, rows[0][1] + 7), 13, (200, 200, 200, 255))
        else:
            self.draw_text("craft_empty", "", (0, 0), 13)
        for index in range(CRAFT_ROWS):
            if index >= len(visible):
                self.hide_rect("craft_row_%d" % index, (0, 0, 1, 1))
                self.draw_text("craft_name_%d" % index, "", (0, 0), 15)
                self.draw_text("craft_detail_%d" % index, "", (0, 0), 13)
                continue
            recipe = visible[index]
            lacking = self.crafting.missing(recipe, self.inventory, self.creative)
            box = rows[index]
            edge = (255, 255, 255, 255) if index == self.craft_hover and not lacking else (70, 70, 80, 255)
            fill = (24, 24, 30, 220) if not lacking else (16, 16, 20, 200)
            self.draw_rect("craft_row_%d" % index, box, fill, edge, 1, layer=991)
            name_colour = (255, 255, 255, 255) if not lacking else (140, 140, 140, 255)
            self.draw_text("craft_name_%d" % index, recipe["name"], (box[0] + 8, box[1] + 5), 15, name_colour)
            if lacking:
                detail, colour = "needs %s" % VoxelRecipes.describe(lacking), (230, 150, 120, 255)
            else:
                detail, colour = "%s  ->  %s" % (VoxelRecipes.describe(recipe["takes"]), VoxelRecipes.describe(recipe["makes"])), (200, 200, 200, 255)
            self.draw_text("craft_detail_%d" % index, detail, (box[0] + 150, box[1] + 7), 13, colour)

    def learn_recipes(self):
        """A recipe is learned the first time the player holds one of its items, and the notice says so."""
        for name in self.crafting.learn(self.inventory):
            self.notify("New recipe: %s" % name)

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
        self.closest_creature = self.nearest_creature()

    def nearest_creature(self):
        """The horizontal distance from the player's centre to the nearest live creature, or None (for the state file)."""
        distances = [math.hypot(c[0] - self.pos[0], c[2] - self.pos[2])
                     for c in (w.state()[0] for w in self.walkers.walkers if not w.removed)]
        return round(min(distances), 3) if distances else None

    def creature_blocks(self, before, after):
        """True if the player's move from before to after (x, z) closes on a creature in contact (physics.blocks_move)."""
        if self.walkers is None:
            return False
        return any(physics.blocks_move(before, after, (c[0], c[2]))
                   for c in (w.state()[0] for w in self.walkers.walkers if not w.removed))

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
            if self.creature_blocks((self.pos[0], self.pos[2]), (pos[0], pos[2])):
                # A creature is in the way: the player stops at its side, as a creature stops at the player's.
                pos = (self.pos[0], pos[1], self.pos[2])
                vel = (0.0, vel[1], 0.0)
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
        # The ray starts half a block back from the surface. A start inside a block has no face to place against (its normal is
        # zero), so the start moves further back until the ray enters a block from outside.
        first = None
        for back in (0.5, 1.0, 1.5):
            start = tuple(surface[i] - look[i] * back for i in range(3))
            hit = physics.raycast(self.solid, start, look, max_distance=physics.REACH + back)
            if first is None:
                first = hit
            if hit is not None and hit[1] != (0, 0, 0):
                self.target = hit
                return
        self.target = first

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
        blocked = self.drops.blocked_in_reach(self.pos, self.inventory)
        if blocked is not None:
            self.notify("Inventory full: no room for the %s" % VoxelGen.NAMES[blocked])

    def spend(self):
        """Placing takes one from the selected slot. Creative mode takes nothing."""
        if not self.creative:
            self.inventory.take(self.slot, 1)

    def torches_on(self, block):
        """The cells of the torches leaning on a block: each one is in a cell beside it, pointing at it."""
        cells = []
        for d in VoxelGen.DIRECTIONS:
            cell = (block[0] + d[0], block[1] + d[1], block[2] + d[2])
            if self.world.prop_at(*cell) == DIRECTION_INDEX[(-d[0], -d[1], -d[2])]:
                cells.append(cell)
        return cells

    def drop_torches_on(self, block):
        """Torches leaning on a block that is broken are broken too, and go back into the inventory."""
        for cell in self.torches_on(block):
            self.world.remove_prop(*cell)
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
            cge.set_xray(name, True)
            self.outline_bodies.add(name)

    def update_outlines(self):
        """Outline what a break takes when it is a creature or a torch. A block is not outlined: its ghost and the reticle show it.
        A hit on a creature flashes its outline white for a moment."""
        if self.paused or self.inventory_open:
            self.show_outline("target_break", None, None)
            self.hide_placement_outlines()
            return
        if self.aims_at_walker():
            lo, hi = self.walker_target[1].box()
            flash = time.time() < self.flash_until
            self.show_outline("target_break", (lo, hi), WHITE if flash else BREAK_COLOUR)
            self.hide_placement_outlines()
            return
        if self.aims_at_torch():
            self.show_outline("target_break", (self.torch_target[2], self.torch_target[3]), BREAK_COLOUR)
            self.hide_placement_outlines()
            return
        if self.target is None:
            self.show_outline("target_break", None, None)
            self.hide_placement_outlines()
            return
        # A block is shown by the ghost of what it would become and by the reticle: no outline of the block, its face or the cell.
        self.show_outline("target_break", None, None)
        self.hide_placement_outlines()

    def hide_placement_outlines(self):
        self.show_outline("target_place", None, None)
        self.show_outline("target_face", None, None)

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
        """F5 saves the edits and the inventory, F9 loads them (see save_world and load_world)."""
        if self.pressed("Save", bool(cge.key_down("F5"))):
            self.save_world()
        if self.pressed("Load", bool(cge.key_down("F9"))):
            self.load_world()

    def save_world(self):
        """The edits and the inventory go to cg_control. The held stack goes back into the inventory first, so a save does not
        lose it."""
        os.makedirs("cg_control", exist_ok=True)
        self.world.settle_moves()   # blocks in flight land first, so none is missing from the file
        self.world.save_edits(EDITS_FILE)
        self.inventory.return_held()
        StateFile.write_json(INVENTORY_FILE, {"slots": self.inventory.to_list(), "slot": self.slot,
                                              "drops": self.drops.to_list(), "recipes": self.crafting.to_list()})
        self.notify("Saved")

    def load_world(self):
        if self.net_client:
            self.notify("The host owns the world: loading is off")
            return
        self.world.load_edits(EDITS_FILE)
        if self.net_host:
            self.net_snapshot = True   # the host sends the loaded edits to its client
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
            self.crafting.load_list(data.get("recipes", []))
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
            elif not self.paused and not self.console.open:
                self.open_inventory()

    def open_inventory(self):
        self.inventory_open = True
        self.inventory_hover = None
        self.mouse_before_inventory = self.mouse_captured
        cge.set_mouse_captured(False)
        self.mouse_captured = False

    def close_inventory(self):
        self.crafting.dropdown_open = False
        self.craft_hover = None
        self.inventory_open = False
        self.inventory_hover = None
        self.inventory.return_held()
        if self.mouse_before_inventory:
            cge.set_mouse_captured(True)
            self.mouse_captured = True

    def update_inventory(self):
        """The inventory screen: the left button takes or puts down a whole stack in the slot under the mouse, and the right takes
        half or puts one down (VoxelInventory.click). A click on the crafting panel is handled first. The buttons are read every
        frame, so a press is counted once."""
        mouse_x, mouse_y = cge.mouse_position()
        self.inventory_hover = self.slot_at(mouse_x, mouse_y)
        target = self.crafting_at(mouse_x, mouse_y)
        self.craft_hover = target[1] if target is not None and target[0] == "row" else None
        left = self.pressed("InventoryLeft", bool(cge.mouse_down("Left")))
        right = self.pressed("InventoryRight", bool(cge.mouse_down("Right")))
        if target is not None:
            if left:
                self.click_crafting(target)
            return
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
                self.send_request({"op": "unprop", "c": list(self.torch_target[1])})
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
                self.send_request({"op": "hit", "c": list(block), "swings": swings})
            if stage == -1:
                self.hit_times.pop(tuple(block), None)
                self.drop_torches_on(block)
                self.gain(kind)
                self.bursts.add(centre, colour, 10, 0.16, 0.6)
            elif stage > 0:
                self.hit_times[tuple(block)] = time.time()
                # The chips fly from the face that was hit, not from the middle of the block.
                face = [centre[i] + normal[i] * 0.5 for i in range(3)]
                self.bursts.add(face, tuple(c * 0.7 for c in colour), 3, 0.14, 0.35)
        elif placing:
            placement, reason = self.placement_check()
            if placement is None:
                if reason is not None:
                    self.notify(reason)
                return
            cell, item, direction = placement
            if item == VoxelGen.TORCH:
                # A torch leans on the block it is aimed at: the request names that block, so the host can check it.
                self.world.set_prop(cell[0], cell[1], cell[2], direction)
                self.send_request({"op": "prop", "c": list(cell), "s": list(block), "dir": direction})
            else:
                self.world.set_block(cell[0], cell[1], cell[2], item)
                self.send_request({"op": "place", "c": list(cell), "id": item})
            self.spend()

    def placement_check(self):
        """Whether the selected item can go where the crosshair is. Returns (placement, None), with placement (cell, item, dir) and
        dir the torch's direction (-1 for a block), or (None, reason) with the reason to tell the player (None says nothing)."""
        if self.target is None:
            return None, "Aim at a block to place against"
        block, normal = self.target
        if normal == (0, 0, 0):
            return None, "Aim at a face to place against"
        cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
        if physics.cell_in_player(cell, self.pos):
            return None, "You are standing there"
        item = self.selected()
        if item is None:
            return None, None if self.creative else "This slot is empty"
        if item not in VoxelGen.COLOURS:
            return None, "A %s cannot be placed" % VoxelGen.NAMES[item]
        if self.world.get_block(*cell) != VoxelGen.AIR or self.world.prop_at(*cell) != -1:
            return None, "That space is taken"
        if item == VoxelGen.TORCH:
            # A torch goes on a face: into the air cell in front of a solid block, leaning on it.
            if not self.world.is_solid(*block):
                return None, "A torch needs a block to lean on"
            return (cell, item, DIRECTION_INDEX[(-normal[0], -normal[1], -normal[2])]), None
        return (cell, item, -1), None

    def update_preview(self):
        """The ghost of the placement: the mesh the selected item would have where it goes, built by the world (VoxelWorld.set_preview).
        It is shown only while the placement would succeed, and rebuilt only when the cell or the item changes."""
        placement = None
        if not (self.paused or self.inventory_open or self.console.open or self.aims_at_walker() or self.aims_at_torch()):
            placement, _ = self.placement_check()
        if placement == self.preview_key:
            return
        self.preview_key = placement
        if placement is None:
            self.world.clear_preview()
            return
        cell, item, direction = placement
        if item == VoxelGen.TORCH:
            self.world.set_preview_prop(cell[0], cell[1], cell[2], direction)
        else:
            self.world.set_preview(cell[0], cell[1], cell[2], item)

    # ----- the other process (VoxelNet) ---------------------------------------------------

    @property
    def net_host(self):
        """The listening game: it owns the world. It applies the edits clients ask for and sends each changed cell back."""
        return self.net is not None and self.net.mode == "listen"

    @property
    def net_client(self):
        """The connecting game: it asks the host for its edits and draws the cells the host sends."""
        return self.net is not None and self.net.mode == "connect"

    def send_request(self, request):
        """A client's edit, which the host applies with its own rules. The host's own edits need no request."""
        if self.net_client:
            self.net.send(dict(request, t="req"))

    def update_net(self, dt):
        """Read what arrived and apply it, send the cells the host changed, retry the cells for chunks not loaded yet, and send
        the player's position."""
        if self.net is None:
            return
        connected = self.net.connected
        for message in self.net.poll():
            self.apply_remote(message)
        if self.net_host:
            if self.net.connected and not connected:
                self.net_snapshot = True   # a client has just joined: it is sent every edited cell
            self.send_cells()
        self.apply_pending()
        self.net_pos_timer -= dt
        if self.net_pos_timer <= 0.0 and not self.paused:
            self.net_pos_timer = POS_INTERVAL
            self.net.send({"t": "pos", "p": [round(v, 3) for v in self.pos], "yaw": round(self.yaw, 2)})

    def cell_state(self, cell):
        """A cell as the host sends it: x, y, z, its block, its damage stage and its torch (-1 for none)."""
        return [cell[0], cell[1], cell[2], self.world.get_block(*cell), self.world.damage_at(*cell), self.world.prop_at(*cell)]

    def loaded_at(self, cell):
        return self.world.is_loaded(math.floor(cell[0] / VoxelGen.CHUNK), math.floor(cell[2] / VoxelGen.CHUNK))

    def send_cells(self):
        """The host sends the state of each cell that changed this frame, or that a request touched, in one message. A cell in a
        chunk the host has not loaded is not sent: the host cannot tell what is there."""
        changed = {tuple(c) for c in self.world.take_changes()} | self.net_touched
        self.net_touched = set()
        if self.net_snapshot:
            changed |= {tuple(c) for c in self.world.edited_cells()}
            self.net_snapshot = False
        if not self.net.connected:
            return
        cells = [self.cell_state(cell) for cell in sorted(changed) if self.loaded_at(cell)]
        if cells:
            self.net.send({"t": "cells", "cells": cells})

    def apply_remote(self, message):
        kind = message.get("t")
        if kind == "pos":
            self.remote_pos = message
            self.show_remote(message["p"])
        elif kind == "req" and self.net_host:
            self.apply_request(message)
        elif kind == "cells" and self.net_client:
            for entry in message["cells"]:
                self.set_remote(tuple(entry[:3]), tuple(entry[3:]))

    def apply_request(self, request):
        """A client's edit, under the host's rules: a hit takes its swings (two for a pickaxe at the blocks it mines), a block
        goes only into air, and a torch leans on a solid block. A request for a cell the host has not loaded is ignored. The cell
        is sent back either way, so an edit the host refused is undone on the client."""
        try:
            op = request["op"]
            cell = tuple(int(v) for v in request["c"])
            item = int(request.get("id", -1))
            support = tuple(int(v) for v in request.get("s", (0, 0, 0)))
            direction = int(request.get("dir", -1))
            swings = int(request.get("swings", 1))
        except (KeyError, TypeError, ValueError):
            return
        if not self.loaded_at(cell):
            return
        if self.net_host:
            self.net_touched.add(cell)
        if op == "hit":
            kind = self.world.get_block(*cell)
            if kind == VoxelGen.AIR:
                return
            swings = 2 if swings >= 2 and kind in VoxelGen.PICKAXE_BLOCKS else 1
            stage = -2
            for _ in range(swings):
                stage = self.world.hit_block(*cell)
                if stage < 0:
                    break
            if stage == -1:
                self.hit_times.pop(cell, None)
                for torch in self.torches_on(cell):
                    self.world.remove_prop(*torch)   # the torches leaning on it break with it, and the client gains them
            elif stage > 0:
                self.hit_times[cell] = time.time()
        elif op == "place":
            if (item in VoxelGen.COLOURS and item != VoxelGen.TORCH and self.world.get_block(*cell) == VoxelGen.AIR
                    and not physics.cell_in_player(cell, self.pos)):
                self.world.set_block(cell[0], cell[1], cell[2], item)
        elif op == "prop":
            if (self.world.get_block(*cell) == VoxelGen.AIR and self.world.is_solid(*support)
                    and DIRECTION_INDEX.get(tuple(support[i] - cell[i] for i in range(3))) == direction):
                self.world.set_prop(cell[0], cell[1], cell[2], direction)
        elif op == "unprop":
            self.world.remove_prop(*cell)

    def control_edit(self, request):
        """The voxel_edit control command: an edit made as a player's click makes it, under the same rules (apply_request). A
        client sends it to the host, which answers with the cell, so the result is the cell as the client has it now; the host,
        or a game alone, applies it at once. An edit in a chunk that is not loaded is an error, since the cell is not known."""
        if request.get("op") not in ("hit", "place", "prop", "unprop"):
            raise ValueError("op must be hit, place, prop or unprop")
        cell = tuple(int(v) for v in request["c"])
        if not self.loaded_at(cell):
            raise ValueError("the cell's chunk is not loaded")
        if self.net_client:
            self.send_request(request)
        else:
            self.apply_request(request)
        block, damage, prop = self.cell_state(cell)[3:]
        return {"sent": self.net_client, "block": block, "damage": damage, "prop": prop}

    def set_remote(self, cell, state):
        """Make a cell match the host's now if its chunk is loaded, else keep the state until the chunk loads (the world ignores
        edits to chunks that are not loaded)."""
        if not self.apply_cell(cell, state):
            self.net_pending[cell] = state

    def apply_cell(self, cell, state):
        """Make a cell's block, damage stage and torch match the host's. False if its chunk is not loaded here."""
        if not self.loaded_at(cell):
            return False
        block, damage, prop = state
        if self.world.get_block(*cell) != block:
            self.world.set_block(cell[0], cell[1], cell[2], block)
        if self.world.damage_at(*cell) != damage:
            self.world.set_damage(cell[0], cell[1], cell[2], damage)
        if self.world.prop_at(*cell) != prop:
            if prop < 0:
                self.world.remove_prop(*cell)
            else:
                self.world.set_prop(cell[0], cell[1], cell[2], prop)
        self.net_applied += 1
        return True

    def apply_pending(self):
        for cell, state in list(self.net_pending.items()):
            if self.apply_cell(cell, state):
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
        if self.net_client:
            self.hit_times.clear()   # the host heals the damage it sends; a client heals nothing on its own
            return
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
        self.deaths += 1
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
        self.clock = (self.clock + dt * self.time_speed / DAY_SECONDS) % 1.0
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
        self.set_clip_planes((self.view_radius + 1) * VoxelGen.CHUNK)

    def set_clip_planes(self, far):
        """The camera draws out to the edge of the loaded chunks and no farther: the fog hides everything past its end, so the
        engine's default far plane (100 blocks, about six chunks) was cutting the view short at every setting."""
        planes = (CAMERA_NEAR, far)
        if planes != self.clip_planes:
            cge.set_camera_planes(*planes)
            self.clip_planes = planes

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
        centre = self.chunk_under_player()
        ready = {"r%d" % r: self.world.ready_within(centre[0], centre[1], r) for r in READY_RADII}
        ready["view"] = self.world.ready_within(centre[0], centre[1], self.view_radius)
        state = {"chunks": stats["chunks"], "loaded": stats["loaded"], "pending": stats["pending"], "ready": ready,
                 "timings": {"install": round(stats["install_ms"], 1), "start": round(stats["start_ms"], 1),
                             "terrain": round(stats["terrain_ms"], 1), "mesh": round(stats["mesh_ms"], 1),
                             "light": round(stats["light_ms"], 1), "skin": round(stats["skin_ms"], 1), "surface": round(stats["surface_ms"], 1),
                             "builds": round(stats["builds"])},
                 "dirty": stats["dirty"], "meshing": stats["meshing"], "quads": stats["quads"],
                 "upload_ms": round(stats["upload_ms"], 3), "uploads": stats["uploads"], "edits": stats["edits"], "slot": self.slot,
                 "smoothing": self.world.smoothing(), "active": self.world.active_count(), "paused": self.paused,
                 "menu": self.menu_page if self.paused else None, "sensitivity": self.sensitivity, "view_radius": self.view_radius,
                 "seed": self.seed, "frames": self.frames, "inventory": self.inventory.to_list(), "drops": len(self.drops.live), "creative": self.creative,
                 "known_recipes": self.crafting.to_list(), "recipes_shown": len(self.crafting.visible(self.creative)),
                 "inventory_open": self.inventory_open, "health": round(self.health, 3), "deaths": self.deaths,
                 "closest_creature": self.closest_creature,
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
