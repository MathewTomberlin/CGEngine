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
import VoxelArrows
import VoxelConsole
import VoxelDrops
import VoxelGen
import VoxelInventory
import VoxelEquipment
import VoxelNet
import VoxelParticles
import VoxelNight
import VoxelRecipes
import VoxelRules
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays, mix, outline_arrays
import VoxelWalkers
from VoxelConfig import *
SPIKE_MS = 12.0              # a frame longer than this (in python, or in the gap before it) is recorded with its phases
from VoxelHud import HudMixin
from VoxelInventoryScreen import InventoryMixin
from VoxelHands import HandsMixin
from VoxelNetSync import NetMixin
from VoxelSettingsMenu import SettingsMixin


LAVA_DAMAGE = 2.0           # health lost a second in lava (ten points last five seconds)
LAVA_FOG = (1.00, 0.45, 0.12)   # the view inside lava: orange and short
LAVA_FOG_START = 1.0
LAVA_FOG_END = 9.0
LAVA_SOUND_SECONDS = 0.9    # seconds between the hisses while the player stands in lava (the lava sound)
REPEAT_DELAY = 0.35         # seconds a held mouse button waits before it repeats its break or place
REPEAT_RATE = 0.25          # seconds between the repeats while it is held
BREAK_REPEAT_RATE = 1.0     # seconds between the breaks (attacks and block breaks) while the break button is held: half as fast as before
TILES_FILE = "voxel_tiles.png"   # the block textures, relative to the game's folder (tools/make_voxel_tiles.py)
RULES_FILE = os.environ.get("VOXEL_RULES")   # a rule file (VoxelRules.py): the reactions the world runs, in place of none


class VoxelGame(PyScript, HudMixin, InventoryMixin, HandsMixin, NetMixin, SettingsMixin):
    def __init__(self):
        super().__init__()
        self.seed = int(os.environ.get("VOXEL_SEED", str(int(time.time()))))
        self.world = None
        self.requested = set()          # (cx, cz) of the chunks asked for, loaded or not
        self.wanted_centre = None       # chunk the wanted set was last built for
        self.walker_ms = []             # creature update times, ms, for the state (median and max)
        self.frame_end = None           # when the last frame ended, for the engine's gap (roadmap 45.3)
        self.gap_ms = 0.0
        self.spikes = []                # the frames that took long, with their phases (see record_spike)
        self.frame_sums = {}            # the phases summed over the frames since the state file was last written (see record_spike)
        self.frame_count = 0
        self.cloud_time = 0.0           # seconds the clouds have drifted for (roadmap 45.2)
        self.pos = [8.5, SPAWN_HEIGHT, 8.5]   # the player's feet, y up
        # A headless host (CGENGINE_HEADLESS=1) has no window: it runs the world, the physics and the network, and draws nothing.
        self.headless = cge.is_headless()
        if os.environ.get("VOXEL_SPAWN"):
            # VOXEL_SPAWN="x,z" starts the player over that column (for checks and for looking at a place)
            spawn_x, spawn_z = (float(v) for v in os.environ["VOXEL_SPAWN"].split(","))
            self.pos = [spawn_x, SPAWN_HEIGHT, spawn_z]
        self.swimming = False
        self.was_swimming = False       # for the splash when the player goes under
        self.step_timer = 0.0           # seconds until the next footstep sound
        self.lava_sound_timer = 0.0     # seconds until the next hiss while the player is in lava
        self.world_io = ""              # the last save or load from the pause menu or F5 and F9 ("saved" or "loaded"), for the state file
        self.music_on = False           # the night loop is playing
        self.night = 0.0                # how dark it is, 0 to 1 (update_sky)
        self.sound_rng = random.Random(self.seed + 7)   # the pitch of each footstep and placement: its own sequence, not the global one
        self.walkers = None             # the creatures, made once the spawn area has loaded
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.flying = False
        self.move_speed = 1.0          # the console's speed multiplier, for walking and flying (speed)
        self.mouse_captured = False
        self.yaw = 0.0                  # degrees; 0 looks down -z
        self.pitch = float(os.environ.get("VOXEL_PITCH", "-25"))   # degrees; negative looks down
        self.target = None              # (block, normal) under the crosshair, or None
        self.torch_target = None        # (distance, cell) of the torch under the crosshair, or None
        self.held_lit = False           # the torch the player carries is lit (L toggles it)
        self.cam_origin = [0.0, 0.0, 0.0]   # the eye the camera sits at this frame (place_camera), where the hands are placed from
        self.hands_ready = False        # the arm bodies are made (VoxelHands.hands_setup)
        self.held_model = None          # the model of the held item, or None for an empty hand
        self.swing_time = None          # seconds into the swing, or None when the hand is not swinging
        self.swing_length = 0.0         # how long the swing lasts
        self.swing_turn = 0.0           # how far the swing turns the arm, in degrees
        self.reach_time = None          # seconds into the left arm's reach after a placement, or None
        self.hit_times = {}             # the damaged blocks, by cell: the time each was last hit
        self.hit_carry = {}             # the damage a block has from strikes that did not make a whole hit yet, by cell (edit_blocks)
        self.keys_was = {}              # previous state of the one-shot keys and buttons
        self.last = None
        self.frames = 0
        self.max_dt = 0.0              # longest frame since the last state write, in seconds
        self.state_timer = 0.0
        self.broken = False
        self.hud_ready = False          # the label and the outline bodies exist (made on the second frame)
        self.paused = False             # the pause menu is open: the world stands still and the mouse is free
        self.frame_dt = 0.0             # the time this frame advances the game (0 while paused)
        self.repeat_wait = {}           # per button, the seconds until a held button repeats its action (see repeating)
        self.view_y = None              # the height the camera is at, which follows the eye's height smoothly
        self.menu_page = "pause"        # the page of the pause menu: "pause" or "settings"
        saved = load_settings()
        self.sensitivity = saved["mouse_sensitivity"]   # mouse sensitivity, a percent of MOUSE_SENSITIVITY
        self.view_radius = saved["view_radius"]         # the view distance in chunks each way (VIEW_STEPS)
        self.time_speed = saved["time_speed"]           # how fast the sun moves: 0 pauses it, 1 is a day every DAY_SECONDS
        self.auto_jump = saved["auto_jump"]             # jump when a walk is stopped by a wall of up to 4 blocks (see physics.wall_ahead)
        self.fullscreen = saved["fullscreen"]           # full screen or windowed (applied once the window is up)
        self.console = VoxelConsole.Console(self)       # the developer console, opened with backtick
        self.title_page = None                          # the page whose title is placed (see show_menu)
        self.menu_titles = {}
        self.dragging = False           # the time-of-day slider is being dragged
        self.outline_keys = {}          # the last outline shown under each name, so unchanged ones are not re-uploaded
        self.outline_bodies = set()     # the outlines already made as runtime meshes
        self.slot = 0                   # the hotbar slot placing uses
        self.inventory = VoxelInventory.Inventory()   # what the player holds, by slot (the hotbar is the first row)
        self.offhand = None             # the item in the left hand (a shield, or None): F moves it, Q raises a shield
        self.equipment = VoxelEquipment.Equipment(VoxelGen.EQUIPMENT)   # the armour worn (roadmap 39.3, 39.4)
        self.offhand_uses = VoxelGen.SHIELD_USES   # the blocked bites the shield in the left hand has left (wear_shield)
        self.creative = os.environ.get("VOXEL_CREATIVE", "0") == "1"   # placing takes nothing, breaking gives nothing
        self.inventory_open = False     # the inventory screen is up: the world stands still and the mouse is free
        self.inventory_hover = None     # the slot under the mouse on the inventory screen
        self.preview_key = None         # the placement the ghost shows (cell, item, dir), or None
        self.preview_stats = (0, 0.0, 0.0)   # the ghost's builds, the last build's milliseconds, and the longest (roadmap 24.5)
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
        self.bow_draw = None            # seconds the bow has been drawn, or None while it is not (see update_bow)
        self.arrows = []                # the arrows in flight: dicts with the body's name, position, velocity, damage and age
        self.arrow_count = 0            # counts every arrow fired, so each body has a name no other has
        self.net = None                 # the link to the other process (VOXEL_NET), or None for a single player
        self.net_pending = {}           # the host's cell states for chunks not loaded yet: cell -> (block, damage, prop)
        self.net_touched = set()        # cells a client asked for this frame, sent back to it even if the host refused them
        self.net_snapshot = False       # send every edited cell next frame (a new client, or a load on the host)
        self.net_reverts = set()        # cells a host's load undid: sent to the client once their chunk is loaded
        self.net_requests = {}          # the client's requests the host has not answered: id -> what this client did at once (settle_request)
        self.net_request_id = 0         # the id of the last request this client sent
        self.net_moves_shown = 0        # falling blocks the host started that this client has shown (VoxelNetSync)
        self.net_pos_timer = 0.0
        self.net_applied = 0            # cell states applied to this world (a client)
        self.remote_pos = None          # the other player's feet and yaw, from their last position message
        self.window = None              # the window's size, set when the HUD is made
        self.bodies = set()             # the HUD's rectangle and text bodies, made once each
        self.drawn = {}                 # the last value set on each HUD body, so unchanged ones are not set again
        self.text_colour = {}   # the colour each label was made with (a label's colour is set only when it is made)
        # The time of day: VOXEL_TIME if it is set, else the time the player last chose on the settings page, else START_TIME.
        if "VOXEL_TIME" in os.environ or saved["time_of_day"] is None:
            self.clock = START_TIME % 1.0   # fraction of a day
        else:
            self.clock = saved["time_of_day"]
        self.moon_days = MOON_START_PHASE * MOON_MONTH_DAYS   # the game days the moon's cycle has run: its phase is this over a month
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
        # Frame timing for the spikes (roadmap 45.3): each phase's time, and the gap since the last frame ended (the engine's render).
        marks = [("start", now)]
        if self.frame_end is not None:
            self.gap_ms = (now - self.frame_end) * 1000.0
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
        # The vertical view is the same distance as the horizontal one, in sections up and down from the player's (VoxelWorld.set_band).
        self.world.set_band(self.view_radius)
        # The clouds drift with the game's clock (roadmap 45.2): the sheet is rebuilt about twice a second inside the world.
        self.world.update_clouds(self.cloud_time, centre[0] * VoxelGen.CHUNK, centre[1] * VoxelGen.CHUNK)
        self.cloud_time += raw_dt
        marks.append(("requests", time.perf_counter()))
        self.world.update(UPLOAD_BUDGET_MS, centre[0], centre[1], math.floor(self.pos[1]))
        marks.append(("world", time.perf_counter()))
        self.update_net(raw_dt)
        marks.append(("net", time.perf_counter()))

        if not self.hud_ready and self.frames >= 2:
            # Made a frame after the world, not during the scene's start: the outlines first, then the labels, so the
            # labels are drawn over the bars. A headless host has no window to lay them out in, so it makes none.
            if not self.headless:
                self.show_outline("target_break", None, None)
                self.hide_placement_outlines()
                self.setup_hud()
                self.apply_window_settings()
            self.hud_ready = True
        self.update_pause_key()
        self.update_inventory_key()
        self.console.update()
        # The frame time stops while the world stands still (below): the clips of the creatures and the hands, and the walkers.
        cge.set_time_frozen(self.paused or self.inventory_open or self.console.open)
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
            else:
                self.world.advance_moves(dt)   # the host's falling blocks, shown as the host started them (VoxelNetSync)
            self.update_mouse()
            self.update_player(dt)
            self.collect_drops()
            self.update_walkers(dt)
            self.update_target()
            self.edit_blocks(dt)
            self.learn_recipes()
        marks.append(("interaction", time.perf_counter()))
        if not self.headless:
            self.update_outlines()
            self.update_preview()
            self.update_hud()
        marks.append(("hud", time.perf_counter()))
        self.update_sky(dt)
        self.update_held_torch()
        self.update_healing()
        self.update_health(dt)
        marks.append(("sky_health", time.perf_counter()))
        self.bursts.update(dt)
        self.drops.update(dt)
        self.update_arrows(dt)
        marks.append(("bursts_arrows", time.perf_counter()))
        self.frame_dt = dt
        self.place_camera()
        if not self.headless:
            self.update_hands(dt)
        marks.append(("camera_hands", time.perf_counter()))

        self.state_timer -= raw_dt   # the state keeps being written while paused, so tests can read the menu
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()
        marks.append(("state", time.perf_counter()))
        self.record_spike(marks)
        self.frame_end = time.perf_counter()

    def record_spike(self, marks):
        """Keeps the last few frames that took long: the python time of each phase, and the engine's gap before the frame."""
        total = (marks[-1][1] - marks[0][1]) * 1000.0
        # Every frame's phases are summed (the state file reports the averages), not only the long ones.
        parts = {}
        for (name, at), (_next, later) in zip(marks, marks[1:]):
            parts[_next] = round((later - at) * 1000.0, 2)
        phases = self.world.phases()
        parts["world_phases"] = [round(v, 2) for v in phases]
        stats = cge.render_stats()
        parts["render_ms"] = round(stats["render_ms"], 2)
        parts["poll_ms"] = round(stats["poll_ms"], 2)
        self.frame_sums["gap"] = self.frame_sums.get("gap", 0.0) + self.gap_ms
        self.frame_sums["total"] = self.frame_sums.get("total", 0.0) + total
        for key, value in parts.items():
            if key == "world_phases":
                sums = self.frame_sums.get(key, [0.0] * len(value))
                self.frame_sums[key] = [a + b for a, b in zip(sums, value)]
            else:
                self.frame_sums[key] = self.frame_sums.get(key, 0.0) + value
        self.frame_count += 1
        if total < SPIKE_MS and self.gap_ms < SPIKE_MS:
            return
        self.spikes.append({"total": round(total, 1), "gap": round(self.gap_ms, 1), "parts": parts})
        del self.spikes[:-6]

    # ----- world --------------------------------------------------------------------------

    def setup(self):
        # The block textures (VoxelGen.TILES). If the file cannot be read the blocks keep their colours, so the game still looks right.
        try:
            cge.set_voxel_tiles(TILES_FILE)
            cge.set_voxel_splat_tiles([VoxelGen.TILES[block] for block in VoxelGen.SPLAT])
            textured = True
        except Exception as error:
            textured = False
            print("VoxelTiles: %s was not loaded, the blocks use their colours: %s" % (TILES_FILE, error))
        self.world = cge.VoxelWorld(VoxelGen.CHUNK, VoxelGen.HEIGHT, "voxel")
        for block, rgb in VoxelGen.COLOURS.items():
            self.world.define_block(block, cge.Vector3f(*rgb), solid=block not in VoxelGen.LIQUIDS,
                                    falls=block in VoxelGen.FALLS, liquid=block in VoxelGen.LIQUIDS,
                                    emits=VoxelGen.EMITS.get(block, 0), tiles=VoxelGen.TILES.get(block) if textured else None,
                                    splat=VoxelGen.splat_channel(block, textured),
                                    shape="cube" if block in VoxelGen.CUBES else "smooth",
                                    group="own" if block in VoxelGen.OWN else "terrain",
                                    material=VoxelGen.MATERIALS.get(block),
                                    mesh=VoxelGen.WATER if block in VoxelGen.WATER_BLOCKS else (VoxelGen.LAVA if block in VoxelGen.LAVA_BLOCKS else 0),
                                    join_above=VoxelGen.JOINS.get(block, 0),
                                    join_below=list(VoxelGen.JOINS_BELOW[block]) if block in VoxelGen.JOINS_BELOW else None,
                                    hits=VoxelGen.HITS.get(block, 1), flows=block not in VoxelGen.STILL_LIQUIDS,
                                    transparent=block in VoxelGen.TRANSPARENT)
        count = VoxelRules.install(self.world, VoxelGen.REACTIONS)
        if RULES_FILE:
            try:
                count += VoxelRules.add_file(self.world, RULES_FILE)
            except (OSError, ValueError) as error:
                print("VoxelRules: %s was not loaded: %s" % (RULES_FILE, error))
        print("VoxelRules: %d reactions" % count)
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
            self.walkers = VoxelWalkers.Walkers(self.world, self.seed)
        # The shield faces the way the player looks, flat on the ground (the same forward as the walk, see update_player).
        shield = None
        if self.shield_up():
            yaw = math.radians(self.yaw)
            shield = (math.sin(yaw), -math.cos(yaw))
        began = time.perf_counter()
        bites = self.walkers.update(dt, (self.pos[0], self.pos[2]), self.pos[1], self.night, shield)
        self.walker_ms.append((time.perf_counter() - began) * 1000.0)
        del self.walker_ms[:-300]   # the last 300 frames, for the state
        if self.walkers.blocks:
            cge.play_sound("hurt", 0.3)
            self.wear_shield(self.walkers.blocks)
        if bites > 0.0:
            cge.play_sound("bite", 0.8)
        self.hurt_player(bites)
        for walker in self.walkers.killed:   # killed by a biter or by lava (the herd): the same hide as a hit leaves
            self.creature_died(walker)
        self.walkers.killed = []
        self.closest_creature = self.nearest_creature()
        self.manage_creatures(dt)

    def manage_creatures(self, dt):
        """Keep the creatures near the player at WALKERS: spawn out of view when short, take out the far ones nothing holds
        (VoxelWalkers.Walkers.manage). The creature under the crosshair is kept, and none goes while an arrow flies."""
        look = self.look_direction()
        horizontal = math.hypot(look[0], look[2])
        look_xz = (look[0] / horizontal, look[2] / horizontal) if horizontal > 1e-6 else None
        keep = [self.walker_target[1].name] if self.walker_target is not None else []
        self.walkers.manage(dt, (self.pos[0], self.pos[2]), self.pos[1], look_xz, keep, bool(self.arrows), WALKERS)

    def wear_weapon(self):
        """A strike on a creature uses one of the weapon's uses (an item that does not wear is left as it is). It breaks when they
        are gone; creative mode never wears it."""
        if self.creative or VoxelGen.durability_of(self.selected()) is None:
            return
        share_before = self.inventory.durability(self.slot)
        name = VoxelGen.NAMES.get(self.selected(), "weapon")
        if self.inventory.wear(self.slot):
            self.notify("Your %s broke" % name)
            cge.play_sound("tool_break", 0.8)
        elif share_before is not None and share_before > 0.1 and self.inventory.durability(self.slot) <= 0.1:
            self.notify("Your %s is nearly broken" % name)

    def update_bow(self, dt, drawing):
        """The bow in the selected slot: drawn while the place button is held (the draw grows with game time, so a paused game does
        not draw), and fired when it is let go. Creative mode fires without arrows; otherwise an arrow is taken from the inventory."""
        if drawing:
            self.bow_draw = min(VoxelArrows.DRAW_SECONDS, (self.bow_draw or 0.0) + dt)
            return
        if self.bow_draw is None:
            return
        charge = VoxelArrows.charge_of(self.bow_draw)
        self.bow_draw = None
        if not self.creative:
            if self.inventory.take_item(VoxelGen.ARROW, 1) == 0:
                self.notify("You have no arrows")
                return
        self.fire_arrow(charge)

    def fire_arrow(self, charge):
        """Make an arrow at the eye, flying along the look direction at the draw's speed, and draw it as a small body."""
        look = self.look_direction()
        eye = self.eye()
        origin = [eye[i] + look[i] * 0.6 for i in range(3)]
        pos, velocity = VoxelArrows.launch(origin, look, charge)
        self.arrow_count += 1
        name = "arrow_%d" % self.arrow_count
        lo = (-0.02, -0.02, -0.3)
        hi = (0.02, 0.02, 0.3)
        arrays = boxes_arrays([(lo, hi, VoxelGen.ITEM_COLOURS[VoxelGen.ARROW])])
        cge.create_mesh_body(name, *arrays, materials=["voxel"], position=cge.Vector3f(*pos))
        cge.set_mesh_body_rotation(name, cge.Vector3f(*VoxelArrows.facing(velocity)))
        self.arrows.append({"name": name, "pos": pos, "vel": velocity, "damage": VoxelArrows.damage_of_charge(charge) * self.equipment.shot_factor(), "age": 0.0})
        cge.play_sound("place", 0.5, 1.3)

    def update_arrows(self, dt):
        """Fly each arrow for this frame. It stops when it reaches a block (it lies where it landed, for the player to take) or a
        creature (the creature takes its damage, and the arrow is spent), or when its life is up."""
        if dt <= 0.0:
            return
        for arrow in list(self.arrows):
            arrow["age"] += dt
            pos, vel, landed = VoxelArrows.step(arrow["pos"], arrow["vel"], dt, self.world.is_solid)
            hit = None
            if landed is None and self.walkers is not None:
                for walker in self.walkers.walkers:
                    if walker.removed:
                        continue
                    lo, hi = walker.box()
                    if VoxelArrows.hits_box(pos, lo, hi):
                        hit = walker
                        break
            if hit is not None:
                if hit.hurt((pos[0], pos[2]), arrow["damage"]):
                    self.creature_died(hit)
                self.bursts.add(pos, (0.85, 0.25, 0.2), 4, 0.12, 0.4)
                self.end_arrow(arrow)
                continue
            if landed is not None:
                if not self.creative:
                    self.drops.add(VoxelGen.ARROW, VoxelGen.ITEM_COLOURS[VoxelGen.ARROW],
                                   (landed[0], self.ground_under(landed[0], landed[1], landed[2]), landed[2]))
                self.end_arrow(arrow)
                continue
            if arrow["age"] >= VoxelArrows.LIFE_SECONDS:
                self.end_arrow(arrow)
                continue
            arrow["pos"], arrow["vel"] = pos, vel
            cge.set_mesh_body_position(arrow["name"], cge.Vector3f(*pos))
            cge.set_mesh_body_rotation(arrow["name"], cge.Vector3f(*VoxelArrows.facing(vel)))

    def end_arrow(self, arrow):
        """Take an arrow out of the world: its body goes, and it is no longer in flight."""
        cge.remove_mesh_body(arrow["name"])
        self.arrows.remove(arrow)

    def creature_died(self, walker):
        """A creature's death: a burst of its colour, it leaves the world, and (unless creative) a hide where it fell."""
        lo, hi = walker.box()
        centre = [(lo[i] + hi[i]) / 2 for i in range(3)]
        self.bursts.add(centre, (0.55, 0.45, 0.40), 12, 0.14, 0.7)
        self.walkers.remove(walker)
        if self.walker_target is not None and self.walker_target[1] is walker:
            self.walker_target = None
        if not self.creative:
            # The hide lies where the creature fell, on the ground under its middle, for the player to walk up to. A biter also
            # leaves a tooth beside it: the crafting item for the bone blade.
            rest = self.ground_under(centre[0], lo[1], centre[2])
            self.drops.add(VoxelGen.HIDE, VoxelGen.ITEM_COLOURS[VoxelGen.HIDE], (centre[0], rest, centre[2]))
            if walker.biter:
                self.drops.add(VoxelGen.TOOTH, VoxelGen.ITEM_COLOURS[VoxelGen.TOOTH], (centre[0] + 0.35, rest, centre[2]))

    def nearest_creature(self):
        """The horizontal distance from the player's centre to the nearest live creature, or None (for the state file)."""
        distances = [math.hypot(c[0] - self.pos[0], c[2] - self.pos[2])
                     for c in (w.state()[0] for w in self.walkers.walkers if not w.removed)]
        return round(min(distances), 3) if distances else None

    def creature_blocks(self, before, after, feet):
        """True if the player's move from before to after (x, z) closes on a creature in contact (physics.blocks_move), and the
        creature is at the player's height: one above or below the player's body is not in the way. feet is the player's feet."""
        if self.walkers is None:
            return False
        return any(physics.blocks_move(before, after, (c[0], c[2]))
                   and physics.overlaps_vertically(feet, physics.PLAYER_HEIGHT, c[1], VoxelWalkers.HEIGHT)
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

    def in_lava(self):
        """True while a lava block is at the player's feet or chest. Lava hurts; it does not hold the player up."""
        x, z = math.floor(self.pos[0]), math.floor(self.pos[2])
        return (self.world.get_block(x, math.floor(self.pos[1] + 0.1), z) in VoxelGen.LAVA_BLOCKS
                or self.world.get_block(x, math.floor(self.pos[1] + 1.0), z) in VoxelGen.LAVA_BLOCKS)

    def eye_in_water(self):
        eye = self.eye()
        return self.world.get_block(math.floor(eye[0]), math.floor(eye[1]), math.floor(eye[2])) in VoxelGen.LIQUIDS

    def chunk_under_player(self):
        size = VoxelGen.CHUNK
        return math.floor(self.pos[0] / size), math.floor(self.pos[2] / size)

    # ----- input --------------------------------------------------------------------------

    def hotkey_down(self, name):
        """A key of the game itself (movement, the hotbar, placing, the lantern, save and load). It reads as released while the
        pause menu, the inventory screen or the console is open, so their keys do not act behind them."""
        if self.paused or self.inventory_open or self.console.open:
            return False
        return bool(cge.key_down(name))

    def key(self, name):
        return int(self.hotkey_down(name))

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
        if self.pressed("Enter", self.hotkey_down("Enter")):
            self.flying = not self.flying
            self.vel[1] = 0.0

        self.turn((self.key("Right") - self.key("Left")) * TURN_SPEED * dt,
                  (self.key("Up") - self.key("Down")) * TURN_SPEED * dt)
        self.swimming = not self.flying and self.in_water()
        if self.swimming and not self.was_swimming:
            cge.play_sound("splash", 0.7)
        self.was_swimming = self.swimming

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
        speed *= self.move_speed
        speed *= self.equipment.speed_factor()
        if length > 0.0:
            dx, dz = dx / length * speed, dz / length * speed
        self.vel[0], self.vel[2] = dx, dz

        if self.flying:
            self.vel[1] = (self.key("Space") - self.key("R")) * FLY_RISE
        elif self.swimming:
            if self.key("Space") and self.at_surface():
                # A jump from the surface lifts the player out onto a bank a block higher.
                self.vel[1] = physics.JUMP_SPEED * self.equipment.jump_factor()
            else:
                # The vertical speed eases towards the swim speed: up while Space is held, down otherwise.
                target = WATER_RISE if self.key("Space") else -WATER_SINK
                self.vel[1] += (target - self.vel[1]) * min(1.0, dt * WATER_DRAG)
        elif self.key("Space") and self.on_ground:
            self.vel[1] = physics.JUMP_SPEED * self.equipment.jump_factor()
        elif (self.auto_jump and self.on_ground and length > 0.0
              and physics.auto_jump_wanted(self.world.is_solid, self.pos, (dx / speed, dz / speed) if speed else (0.0, 0.0))):
            # Auto jump: the walk is stopped by a wall the player can climb over (a phone cannot walk and jump at once).
            self.vel[1] = physics.JUMP_SPEED * self.equipment.jump_factor()

        # Physics waits until the chunk under the player is in the world, so the player does not fall through air.
        if self.world.is_loaded(*self.chunk_under_player()):
            # The engine's mover is the same as physics.step (tests check they agree), and it is faster.
            # Swimming has no gravity of its own: the swim speed above takes its place, so the mover runs without it.
            pos, vel, self.on_ground = self.world.step_body(tuple(self.pos), tuple(self.vel), dt,
                                                           self.flying or self.swimming, on_ground=self.on_ground)
            if self.creature_blocks((self.pos[0], self.pos[2]), (pos[0], pos[2]), pos[1]):
                # A creature is in the way: the player stops at its side, as a creature stops at the player's.
                pos = (self.pos[0], pos[1], self.pos[2])
                vel = (0.0, vel[1], 0.0)
            self.pos = list(pos)
            self.vel = list(vel)
        self.track_fall()
        # Footsteps: while the player walks on the ground, a step every STEP_SECONDS, each a little different in pitch.
        walking = self.on_ground and not self.flying and not self.swimming and math.hypot(self.vel[0], self.vel[2]) > 0.5
        if walking:
            self.step_timer -= dt
            if self.step_timer <= 0.0:
                cge.play_sound(self.footstep_sound(), 0.35, 0.9 + 0.2 * self.sound_rng.random())
                self.step_timer = STEP_SECONDS
        else:
            self.step_timer = 0.0
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

    def fluid(self, x, y, z):
        """A block a bucket aims at: solid, or a liquid (the water and lava a bucket fills from or places against)."""
        return self.solid(x, y, z) or self.world.get_block(x, y, z) in VoxelGen.LIQUIDS

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
        if self.selected() in VoxelGen.BUCKETS:
            # A bucket aims at the liquids as well as the solid blocks: a strike fills it from a liquid, and it places against one.
            self.target = physics.raycast(self.fluid, eye, look)
            return
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
        if item is None:
            return "empty slot"
        name = VoxelGen.NAMES.get(item, "unknown block %d" % item)
        if item in VoxelGen.FILLED_VESSELS:
            name += " (%d of %d)" % (self.inventory.slots[self.slot][2], VoxelGen.vessel_capacity(item))
        return name

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
        taken = False
        for item in self.drops.take_in_reach(self.pos, self.inventory):
            taken = True
            self.notify("Picked up a %s" % VoxelGen.NAMES[item])
        if taken:
            cge.play_sound("pickup", 0.6)
        blocked = self.drops.blocked_in_reach(self.pos, self.inventory)
        if blocked is not None:
            self.notify("Inventory full: no room for the %s" % VoxelGen.NAMES[blocked])

    def fill_bucket(self, block, kind):
        """A strike on a liquid with a bucket. An empty bucket takes what the volume gives up to its capacity (VoxelGen.VESSEL_CAPACITY),
        from the cell hit and the volume around it, so the water goes down by that much; a filled bucket of the same liquid takes more
        until it is full. A spring's block goes to air and fills an empty bucket. The host takes the same units (a "scoop" request)."""
        x, y, z = block
        liquid = VoxelGen.WATER if kind in VoxelGen.WATER_BLOCKS else VoxelGen.LAVA
        tool = self.selected()
        filled = tool in VoxelGen.FILLED_VESSELS
        if filled and VoxelGen.FILLED_BUCKET[liquid] != tool:
            self.notify("Your bucket holds %s: empty it first" % VoxelGen.NAMES[tool].split()[0])
            return
        capacity = VoxelGen.vessel_capacity(tool)
        held = self.inventory.slots[self.slot][2] if filled else 0
        if held >= capacity:
            self.notify("Your bucket is full")
            return
        if kind in VoxelGen.VOLUME_BLOCKS:
            taken, _ = self.world.drain_volume(x, y, z, capacity - held, True)
            if taken == 0:
                self.notify("There is no water here to fill a bucket")
                return
        else:
            if filled:
                self.notify("Empty the bucket before you take from a spring")
                return
            taken = capacity
            self.world.set_block(x, y, z, VoxelGen.AIR)
        self.inventory.slots[self.slot] = [VoxelGen.FILLED_BUCKET[liquid], 1, held + taken]
        self.send_request({"op": "scoop", "c": list(block), "want": capacity - held})
        cge.play_sound("place", 0.6, 0.7)
        self.hands_hit("block", [block[i] + 0.5 for i in range(3)])

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
            if self.pressed("Slot%d" % slot, self.hotkey_down(hotbar_key(slot))):
                self.slot = slot

    def snapshot_key(self):
        """F12 saves a screenshot and the game state at that moment, for a bug that is hard to reproduce."""
        if self.pressed("Snapshot", self.hotkey_down("F12")):
            folder = os.path.abspath("cg_control/snapshots")
            os.makedirs(folder, exist_ok=True)
            stamp = time.strftime("%Y%m%d_%H%M%S")
            cge.screenshot(os.path.join(folder, "shot_%s.png" % stamp))
            self.write_state(os.path.join(folder, "state_%s.json" % stamp))

    def footstep_sound(self):
        """The footstep for the block the player stands on (VoxelGen.STEPS): the sound of its ground, or the default step."""
        below = self.world.get_block(math.floor(self.pos[0]), math.floor(self.pos[1] - 0.25), math.floor(self.pos[2]))
        return VoxelGen.STEPS.get(below, "step")

    def save_and_load(self):
        """F5 saves the edits and the inventory, F9 loads them (see save_world and load_world)."""
        if self.pressed("Save", self.hotkey_down("F5")):
            self.save_world()
        if self.pressed("Load", self.hotkey_down("F9")):
            self.load_world()

    def save_world(self):
        """The edits, the inventory and the time of day go to the world's folder (VoxelConfig.WORLD_FOLDER). The held stack and the
        crafting grid go back into the inventory first, so a save does not lose them."""
        os.makedirs(WORLD_FOLDER, exist_ok=True)
        self.world.settle_moves()   # blocks in flight land first, so none is missing from the file
        self.world.save_edits(EDITS_FILE)
        self.crafting.return_grid(self.inventory)
        self.inventory.return_held()
        StateFile.write_json(INVENTORY_FILE, {"slots": self.inventory.to_list(), "slot": self.slot, "offhand": self.offhand,
                                              "offhand_uses": self.offhand_uses,
                                              "drops": self.drops.to_list(), "recipes": self.crafting.to_list(),
                                              "equipment": self.equipment.to_list(),
                                              "clock": round(self.clock, 5), "moon_days": round(self.moon_days, 5)})
        self.world_io = "saved"
        self.notify("Saved")

    def load_world(self):
        if self.net_client:
            self.notify("The host owns the world: loading is off")
            return
        before = [tuple(c) for c in self.world.edited_cells()]
        self.world.load_edits(EDITS_FILE)
        if self.net_host:
            self.net_snapshot = True   # the host sends the loaded edits to its client
            self.net_reverts |= set(before)   # the edits the file lacks: the client is told their cells again
        self.load_inventory()
        self.world_io = "loaded"
        self.notify("Loaded")

    def load_inventory(self):
        """The inventory saved with the edits. A missing or unreadable file keeps the inventory the player has."""
        try:
            with open(INVENTORY_FILE) as f:
                data = json.load(f)
            self.inventory.load_list(data.get("slots", []))
            self.slot = int(data.get("slot", self.slot)) % COLUMNS
            self.offhand = data.get("offhand") if data.get("offhand") == VoxelGen.SHIELD else None
            self.offhand_uses = min(max(int(data.get("offhand_uses", VoxelGen.SHIELD_USES)), 1), VoxelGen.SHIELD_USES)
            self.drops.load_list(data.get("drops", []), lambda item: VoxelGen.ITEM_COLOURS.get(item, (0.8, 0.8, 0.8)))
            self.crafting.load_list(data.get("recipes", []))
            self.equipment.load_list(data.get("equipment"))
            if "clock" in data:   # the time of day and the moon are the world's, so a load brings them back
                self.clock = float(data["clock"]) % 1.0
                self.moon_days = float(data.get("moon_days", self.moon_days))
        except (OSError, ValueError, TypeError, AttributeError):
            pass

    def update_offhand(self):
        """F moves a shield between the selected slot and the left hand, and back."""
        if not self.pressed("Offhand", self.hotkey_down("F")):
            return
        if self.offhand is not None:
            if self.creative or self.inventory.fits(self.offhand):
                self.return_shield()
            else:
                self.notify("Inventory full")
        elif self.selected() == VoxelGen.SHIELD:
            stack = self.inventory.slots[self.slot]
            self.offhand_uses = stack[2] if len(stack) > 2 else VoxelGen.SHIELD_USES
            self.inventory.take(self.slot, 1)
            self.offhand = VoxelGen.SHIELD
        else:
            self.notify("Select a shield to hold it in the left hand")

    def return_shield(self):
        """The shield in the left hand goes back into the inventory with the uses it has left. Creative mode keeps no stock."""
        if self.creative:
            self.offhand = None
            return
        self.inventory.add(VoxelGen.SHIELD)
        for index, stack in enumerate(self.inventory.slots):
            if stack is not None and stack[0] == VoxelGen.SHIELD and stack[1] == 1 and stack[2] == VoxelGen.SHIELD_USES:
                self.inventory.slots[index] = [VoxelGen.SHIELD, 1, self.offhand_uses]
                break
        self.offhand = None
        self.offhand_uses = VoxelGen.SHIELD_USES

    def wear_shield(self, blocks):
        """A shield in the left hand takes one use for each bite it blocks. It breaks when its uses are gone; creative mode never
        wears it."""
        if self.creative or self.offhand != VoxelGen.SHIELD:
            return
        share_before = self.offhand_uses / VoxelGen.SHIELD_USES
        self.offhand_uses -= blocks
        if self.offhand_uses <= 0:
            self.offhand = None
            self.offhand_uses = VoxelGen.SHIELD_USES
            self.notify("Your shield broke")
            cge.play_sound("tool_break", 0.8)
        elif share_before > 0.1 and self.offhand_uses / VoxelGen.SHIELD_USES <= 0.1:
            self.notify("Your shield is nearly broken")

    def shield_up(self):
        """Q holds a shield up while one is in the left hand (it takes the bites of creatures in front)."""
        return self.offhand == VoxelGen.SHIELD and self.hotkey_down("Q")

    def update_modes(self):
        """C toggles creative mode: placing takes nothing from the slots, and breaking adds nothing to them."""
        if self.pressed("Creative", self.hotkey_down("C")):
            self.creative = not self.creative
            self.notify("Creative mode on" if self.creative else "Creative mode off")

    def repeating(self, name, down, dt, rate=REPEAT_RATE):
        """True on the frame a button goes down, and again every rate seconds while it stays down after REPEAT_DELAY, so
        holding a mouse button keeps breaking or placing. The wait runs in game time (dt), so a paused game does not repeat."""
        if self.pressed(name, down):
            self.repeat_wait[name] = REPEAT_DELAY
            return True
        if not down:
            self.repeat_wait.pop(name, None)
            return False
        if name not in self.repeat_wait:
            return False
        self.repeat_wait[name] -= dt
        if self.repeat_wait[name] <= 0.0:
            self.repeat_wait[name] += rate
            return True
        return False

    def edit_blocks(self, dt=0.0):
        self.choose_hotbar()
        self.update_modes()
        self.update_offhand()
        self.save_and_load()
        self.snapshot_key()
        break_down = self.mouse_captured and bool(cge.mouse_down("Left"))
        place_down = self.mouse_captured and bool(cge.mouse_down("Right"))
        # A held break repeats at the empty hand's rate, times the held item's attack speed (the shovel strikes three times as fast).
        rate = BREAK_REPEAT_RATE / VoxelGen.attack_speed_of(self.selected())
        breaking = self.repeating("Break", break_down or self.hotkey_down("J"), dt, rate)
        if self.selected() == VoxelGen.BOW:
            # A bow is drawn with the place button, so placing waits: letting go fires.
            self.update_bow(dt, place_down or self.hotkey_down("K"))
            placing = False
        else:
            self.bow_draw = None
            placing = self.repeating("Place", place_down or self.hotkey_down("K"), dt)
        if breaking and self.aims_at_walker():
            # A creature the crosshair is on, nearer than the block behind it, is hit: each hit takes one of its HEALTH. The
            # one that kills it leaves a hide for the inventory.
            walker = self.walker_target[1]
            self.flash_until = time.time() + HIT_FLASH_SECONDS
            lo, hi = walker.box()
            centre = [(lo[i] + hi[i]) / 2 for i in range(3)]
            if walker.hurt((self.pos[0], self.pos[2]), VoxelGen.damage_of(self.selected()) * self.equipment.damage_factor()):
                self.creature_died(walker)
            else:
                self.bursts.add(centre, (0.85, 0.25, 0.2), 4, 0.12, 0.4)
            self.wear_weapon()
            self.hands_hit("creature", centre)
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
            # A block takes its hits (VoxelGen.HITS) to break. A strike makes the whole hits its tool's damage on the block gives
            # (VoxelGen.block_damage: a shovel's half hit makes one hit every second strike). The hit that would break it needs room
            # for it in the inventory; a hit that leaves it standing does not, so a full inventory can still damage a block.
            kind = self.world.get_block(*block)
            tool = self.selected()
            if tool in VoxelGen.BUCKETS and kind in VoxelGen.LIQUIDS:
                self.fill_bucket(block, kind)
                return
            if kind in VoxelGen.LAVA_BLOCKS:
                self.notify("Lava cannot be broken")
                return
            # A tool in the selected slot must be able to mine the block (VoxelGen.BLOCK_TIER). Creative mode has no tools to wear
            # out and no tiers to meet.
            if not self.creative and not VoxelGen.can_mine(tool, kind):
                self.notify("You need an iron pickaxe to mine %s" % VoxelGen.NAMES.get(kind, "that"))
                return
            key = tuple(block)
            carry = self.hit_carry.get(key, 0.0) + VoxelGen.block_damage(tool, kind) * self.equipment.break_factor()
            swings = int(carry + 1e-9)
            breaks = swings > 0 and self.world.damage_at(*block) + swings >= VoxelGen.HITS.get(kind, 1)
            if breaks and not self.creative and not self.inventory.fits(kind):
                self.notify("Inventory full")
                return
            self.hit_carry[key] = carry - swings
            stage = -2
            hits = 0
            for _ in range(swings):
                stage = self.world.hit_block(block[0], block[1], block[2])
                if stage == -2:
                    break
                hits += 1
                if stage < 0:
                    break
            if tool in VoxelGen.TOOLS and not self.creative:
                # A strike uses one of the tool's uses, or one for each hit it makes when it makes more than one.
                share_before = self.inventory.durability(self.slot)
                if self.inventory.wear(self.slot, max(1, swings)):
                    self.notify("Your %s broke" % VoxelGen.NAMES.get(tool, "tool"))
                    cge.play_sound("tool_break", 0.8)
                elif share_before is not None and share_before > 0.1 and self.inventory.durability(self.slot) <= 0.1:
                    # Told once, as it crosses a tenth of its uses left, so the player can choose to finish the job or not.
                    self.notify("Your %s is nearly broken" % VoxelGen.NAMES.get(tool, "tool"))
            colour = VoxelGen.COLOURS.get(kind, (0.5, 0.5, 0.5))
            centre = [block[i] + 0.5 for i in range(3)]
            if stage == -1:
                cge.play_sound("break", 0.8)
            elif stage > 0:
                cge.play_sound("break", 0.35, 1.4)
            if stage != -2:
                self.flash_until = time.time() + HIT_FLASH_SECONDS
                self.send_request({"op": "hit", "c": list(block), "swings": swings}, {"kind": kind, "gained": stage == -1})
                self.hands_hit("block", centre)
            elif swings == 0:
                # The strike lands on the block, but its hit is still carried to the next strike.
                self.hands_hit("block", centre)
            if stage == -1:
                self.hit_times.pop(tuple(block), None)
                self.hit_carry.pop(key, None)
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
                self.send_request({"op": "prop", "c": list(cell), "s": list(block), "dir": direction},
                                  {"item": item, "spent": not self.creative})
            elif item in VoxelGen.VOLUME_BLOCKS:
                units = self.inventory.slots[self.slot][2]
                self.world.pour_volume(cell[0], cell[1], cell[2], units, item, True)
                self.send_request({"op": "pour", "c": list(cell), "id": item, "units": units})
            else:
                self.world.set_block(cell[0], cell[1], cell[2], item)
                self.send_request({"op": "place", "c": list(cell), "id": item}, {"item": item, "spent": not self.creative})
            cge.play_sound("place", 0.7, 0.95 + 0.1 * self.sound_rng.random())
            if item in VoxelGen.VOLUME_BLOCKS:
                # A filled bucket pours its units and is empty again (creative mode keeps it full).
                if not self.creative:
                    self.inventory.slots[self.slot] = [VoxelGen.BUCKET, 1]
            else:
                self.spend()
            self.reach()

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
        liquid = VoxelGen.BUCKET_VOLUME.get(item)
        if liquid is None and item not in VoxelGen.COLOURS:
            return None, "A %s cannot be placed" % VoxelGen.NAMES.get(item, "unknown block %d" % item)
        if self.world.get_block(*cell) != VoxelGen.AIR or self.world.prop_at(*cell) != -1:
            return None, "That space is taken"
        if liquid is not None:
            # A filled bucket pours its units into the air cell in front of the face it is aimed at, and all of them must fit.
            units = self.inventory.slots[self.slot][2]
            if self.world.pour_volume(cell[0], cell[1], cell[2], units, liquid, False)[0] < units:
                return None, "There is no room here for the water"
            return (cell, liquid, -1), None
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
        began = time.perf_counter()
        if item == VoxelGen.TORCH:
            self.world.set_preview_prop(cell[0], cell[1], cell[2], direction)
        else:
            self.world.set_preview(cell[0], cell[1], cell[2], item)
        # How long the ghost took to build, for the state file: the ghost is rebuilt each time the aimed cell changes (roadmap 24.5).
        built = (time.perf_counter() - began) * 1000.0
        self.preview_stats = (self.preview_stats[0] + 1, built, max(self.preview_stats[2], built))

    # ----- the other process (VoxelNet) ---------------------------------------------------

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
        cge.play_sound("hurt", 0.8)
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
        if self.in_lava():
            self.hurt_player(LAVA_DAMAGE * dt)
            self.lava_sound_timer -= dt
            if self.lava_sound_timer <= 0.0:
                cge.play_sound("lava", 0.4, 0.9 + 0.2 * self.sound_rng.random())
                self.lava_sound_timer = LAVA_SOUND_SECONDS
        else:
            self.lava_sound_timer = 0.0
        if self.health < PLAYER_HEALTH and time.time() - self.hurt_at > REGEN_DELAY:
            self.health = min(PLAYER_HEALTH, self.health + REGEN_RATE * dt)

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
        if self.pressed("HeldTorch", self.hotkey_down("L")):
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
        self.moon_days += dt * self.time_speed / DAY_SECONDS
        moon_phase = (self.moon_days / MOON_MONTH_DAYS) % 1.0
        moon = moon_share(moon_phase)   # the share of the moon's light this phase gives
        angle = 2.0 * math.pi * self.clock
        height = math.sin(angle)                  # -1 at midnight, 0 at sunrise and sunset, 1 at noon
        toward_sun = (0.45 * math.cos(angle), height, 0.3 * math.cos(angle))
        length = math.sqrt(sum(v * v for v in toward_sun))
        toward_sun = [v / length for v in toward_sun]
        travel = [-v for v in toward_sun]            # a directional light travels from the sun

        day = min(1.0, max(0.0, height * 3.0))     # 0 at night, full once the sun is 20 degrees up
        dusk = max(0.0, 1.0 - abs(height) * 6.0)   # strongest at the horizon
        light = mix(SUN_LIGHT, DUSK_LIGHT, dusk * 0.7)
        sky = mix(mix(NIGHT_SKY, DAY_SKY, day), DUSK_SKY, dusk * 0.8)
        zenith = mix(mix(NIGHT_ZENITH, DAY_ZENITH, day), DUSK_ZENITH, dusk * 0.5)
        sun_disc = light if height > 0.0 else (0.0, 0.0, 0.0)   # the disc shows only above the horizon
        sun_disc = mix((0.0, 0.0, 0.0), sun_disc, min(1.0, max(0.0, height * 12.0)))

        fog_end = (self.view_radius + 0.5) * VoxelGen.CHUNK   # the fog is complete at the edge of the loaded chunks
        fog_colour, fog_start = sky, fog_end * FOG_START_FRACTION
        if self.world is not None and self.eye_in_water():
            # Under the water the view is short and blue-green, whatever the time of day. The sky is that colour too, since
            # the faces of the water a player is inside are back faces, and the sky shows through them.
            eye = self.eye()
            in_lava = self.world.get_block(math.floor(eye[0]), math.floor(eye[1]), math.floor(eye[2])) in VoxelGen.LAVA_BLOCKS
            sky = zenith = fog_colour = LAVA_FOG if in_lava else WATER_FOG
            fog_start, fog_end = (LAVA_FOG_START, LAVA_FOG_END) if in_lava else (WATER_FOG_START, WATER_FOG_END)
        # The sun gives light only above the horizon: below it, its direction points up and would light the undersides of everything.
        sun_up = min(1.0, max(0.0, (height + 0.02) / 0.07))
        # The moon is opposite the sun, so at night its light comes down from above: faint, and white.
        night = VoxelNight.night_share(height)
        self.night = night   # the creatures' chase range grows with it (VoxelNight)
        cge.set_light("sun", direction=cge.Vector3f(*travel), colour=cge.Vector3f(*light),
                      brightness=(0.45 + 0.55 * day) * sun_up)
        cge.set_light("moon", direction=cge.Vector3f(*toward_sun), colour=cge.Vector3f(*MOON_LIGHT),
                      brightness=MOON_BRIGHTNESS * night * moon)
        # The night loop plays while it is dark, and stops once it is day again (the hysteresis keeps it from flickering at dusk).
        if night > 0.5 and not self.music_on:
            self.music_on = cge.play_music("ambient", 0.3)
        elif night < 0.2 and self.music_on:
            cge.stop_music()
            self.music_on = False
        # The sun's shadows are off under the water: the water surface is a shadow over everything below it, which is dark.
        cge.set_environment(sky=cge.Vector3f(*sky), zenith=cge.Vector3f(*zenith), fog=cge.Vector3f(*fog_colour),
                            fog_start=fog_start, fog_end=fog_end,
                            sun_direction=cge.Vector3f(*toward_sun), sun_colour=cge.Vector3f(*sun_disc),
                            sun_size=1.6, shadows=SHADOWS and not (self.world is not None and self.eye_in_water()),
                            moon_direction=cge.Vector3f(*[-v for v in toward_sun]),
                            moon_colour=cge.Vector3f(*MOON_DISC), moon_size=MOON_SIZE, moon_lit=moon,
                            moon_lit_axis=cge.Vector3f(1.0 if moon_phase < 0.5 else -1.0, 0.0, 0.0))   # waxing: lit on one side, waning the other
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
        self.cam_origin = origin

    # ----- state for tests and agents -----------------------------------------------------

    def write_state(self, path="cg_control/voxel_state.json"):
        """Write the player and the world's counts to cg_control/voxel_state.json, so an agent can check them without a screenshot."""
        stats = self.world.stats()
        basis = cge.camera_basis()
        centre = self.chunk_under_player()
        radii = list(READY_RADII) + [self.view_radius]
        counts = self.world.ready_counts(centre[0], centre[1], radii)
        ready = {"r%d" % r: counts[i] for i, r in enumerate(READY_RADII)}
        ready["view"] = counts[-1]
        state = {"chunks": stats["chunks"], "loaded": stats["loaded"], "pending": stats["pending"], "ready": ready,
                 "timings": {"install": round(stats["install_ms"], 1), "start": round(stats["start_ms"], 1),
                             "terrain": round(stats["terrain_ms"], 1), "mesh": round(stats["mesh_ms"], 1),
                             "light": round(stats["light_ms"], 1), "skin": round(stats["skin_ms"], 1), "surface": round(stats["surface_ms"], 1),
                             "builds": round(stats["builds"])},
                 "preview": {"builds": self.preview_stats[0], "last_ms": round(self.preview_stats[1], 3),
                             "max_ms": round(self.preview_stats[2], 3)},
                 "dirty": stats["dirty"], "meshing": stats["meshing"], "quads": stats["quads"],
                 "upload_ms": round(stats["upload_ms"], 3), "uploads": stats["uploads"], "edits": stats["edits"], "slot": self.slot,
                 "smoothing": self.world.smoothing(), "active": self.world.active_count(), "paused": self.paused, "headless": self.headless,
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
                 "spikes": self.spikes,
                 "render": cge.render_stats(),
                 "frame_avg": {k: ([round(a / max(1, self.frame_count), 2) for a in val] if isinstance(val, list) else round(val / max(1, self.frame_count), 2)) for k, val in self.frame_sums.items()},
                 "walkers_ms": [round(sorted(self.walker_ms)[len(self.walker_ms) // 2], 3) if self.walker_ms else 0.0, round(max(self.walker_ms), 3) if self.walker_ms else 0.0],
                 "sounds": cge.sound_count(), "music": self.music_on,
                 "time_of_day": round(self.clock, 5), "world_io": self.world_io, "moon_phase": round((self.moon_days / MOON_MONTH_DAYS) % 1.0, 4), "moon_share": round(moon_share((self.moon_days / MOON_MONTH_DAYS) % 1.0), 4), "sun": [round(v, 5) for v in self.sun],
                 "fog": [round(v, 3) for v in self.fog],
                 "look": [round(v, 6) for v in self.look_direction()],
                 "camera_forward": [round(v, 6) for v in basis["forward"]],
                 "camera_right": [round(v, 6) for v in basis["right"]],
                 "camera_up": [round(v, 6) for v in basis["up"]],
                 "target": None if self.target is None else list(self.target[0])}
        self.max_dt = 0.0
        self.frame_sums = {}
        self.frame_count = 0
        StateFile.write_json_async(path, state)


def create_instance():
    return VoxelGame()
