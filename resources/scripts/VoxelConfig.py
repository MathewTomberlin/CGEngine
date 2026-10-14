"""
Constants and small helpers the voxel game's modules share: the view, the player, the HUD, the files, and the timing
of the world. Behaviour is in VoxelGame and its mixins (VoxelHud, VoxelInventoryScreen, VoxelNetSync, VoxelSettingsMenu).
"""
import json
import math
import os
import random
import time
import traceback

import StateFile
import VoxelGen
import VoxelInventory
import VoxelRecipes
import VoxelPhysics as physics

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
MOON_LIGHT = (0.86, 0.92, 1.0)      # the moon's light: white, like the sun's but fainter
MOON_BRIGHTNESS = 0.6              # the moon's brightness at full night (it is off by day)
MOON_DISC = (0.92, 0.95, 1.0)      # the colour of the moon's disc in the sky (white, a little blue)
MOON_SIZE = 2.0                    # the moon's angular radius in degrees, a little larger than the sun's (1.6)
MOON_MONTH_DAYS = float(os.environ.get("VOXEL_MOON_MONTH_DAYS", "30"))   # in-game days for one full cycle of the moon's phases
MOON_START_PHASE = 0.5             # where a new game starts in the cycle: 0 new moon, 0.25 first quarter, 0.5 full, 0.75 last quarter
# The share of the moon's light at each phase of the cycle (phase 0 to 1): new 0%, crescent 25%, half 50%, gibbous 75%, full
# 100%, then the same back down through the waning phases. The share between two keys is lerped.
MOON_LIGHT_KEYS = ((0.0, 0.0), (0.125, 0.25), (0.25, 0.5), (0.375, 0.75), (0.5, 1.0), (0.625, 0.75), (0.75, 0.5), (0.875, 0.25),
                   (1.0, 0.0))


def moon_share(phase):
    """The share of the moon's light at a phase of its cycle (0 to 1, wrapping), lerped between MOON_LIGHT_KEYS."""
    p = phase % 1.0
    for (p0, s0), (p1, s1) in zip(MOON_LIGHT_KEYS, MOON_LIGHT_KEYS[1:]):
        if p0 <= p <= p1:
            return s0 + (s1 - s0) * (p - p0) / (p1 - p0)
    return 0.0


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
