"""
Dungeon sample game: a top-down action adventure with procedural rooms (docs/games/dungeon.md).

Attached to the scene root's update domain by resources/scenes/dungeon.json. Each frame it reads the
keyboard, moves the player and enemies on the level grid, handles the sword, and moves rooms when the
player crosses a door. Game state lives here in Python. Bodies are only used to show that state.

Controls: WASD or arrow keys to move, J or Space to swing the sword, the goal is the glowing tile
in the far room. Enemies come in three kinds (slime, archer, brute) and may drop a heart or a coin. Set DUNGEON_SEED to choose a layout (default: the current time).
"""
import json
import math
import os
import time
import traceback

import cg_engine_bindings as cge
from PyScript import PyScript
import DungeonGen

PLAYER_SPEED = 4.0          # tiles per second
PLAYER_RADIUS = 0.3
CONTACT_DISTANCE = 0.7      # an enemy this close (plus its extra size) hurts the player
INVULNERABLE_TIME = 1.0     # seconds of protection after a hit
MAX_HP = 3
ATTACK_COOLDOWN = 0.35
ATTACK_ANIM_TIME = 0.45     # length of the knight_attack clip
PLAYER_MODEL = "models/dungeon/knight_rig.fbx"   # rigged knight; clips come from dungeon.json
SLASH_TIME = 0.18
SLASH_REACH = 0.9           # how far in front of the player the swing lands
SLASH_RADIUS = 0.85
GOAL_RADIUS = 0.6
CAMERA_HEIGHT = 7.5
CAMERA_BACK = 4.5           # the camera sits this far behind the player, for a three-quarter view
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/dungeon_state.json

# Models, built in Blender by tools/blender/dungeon_models.py. Each faces +Z with its origin at its feet.
MODEL_DIR = "models/dungeon/"

# Enemy kinds (DungeonGen.ENEMY_KINDS). size is how far the body reaches (contact range grows with it);
# radius is used for wall collision.
ENEMY_STATS = {
    "slime":  {"speed": 1.6, "hp": 2, "size": 0.35, "radius": 0.35, "model": "slime"},
    "archer": {"speed": 1.3, "hp": 1, "size": 0.3,  "radius": 0.3,  "model": "archer"},
    "brute":  {"speed": 0.9, "hp": 4, "size": 0.5,  "radius": 0.45, "model": "brute"},
}
ARCHER_RANGE = (3.0, 5.0)   # an archer backs off inside the first distance and closes in beyond the second
ARCHER_SHOT_TIME = 2.0      # seconds between shots
ARCHER_SIGHT = 7.0          # an archer only shoots at a player this close
BOLT_SPEED = 5.0
BOLT_HIT_DISTANCE = 0.4
ITEM_PICKUP_DISTANCE = 0.55
BOLT_HEIGHT = 0.4
SPIN_SPEED = 120.0          # degrees per second for coins and the goal crystal


def cube(name, size, pos, material):
    return {"name": name, "primitive": "cube", "size": size, "position": list(pos), "material": material}


def model(name, file, pos, yaw=0.0, scale=1.0):
    return {"name": name, "model": MODEL_DIR + file + ".obj", "position": list(pos),
            "rotation": [0, yaw, 0], "scale": [scale, scale, scale]}


def yaw_towards(dx, dz):
    """Degrees about Y that turn a model facing +Z towards (dx, dz)."""
    return math.degrees(math.atan2(dx, dz))


def place(body, x, y, z, yaw=None):
    mesh = body.get_mesh()
    mesh.set_position(cge.Vector3f(x, y, z))
    if yaw is not None:
        mesh.set_rotation(cge.Vector3f(0.0, yaw, 0.0))


def floor_tile(name, gx, gy):
    # A plane primitive is a square at local z = size, facing +Z (docs/ai/scene-format.md). Turned to lie flat,
    # its surface is at position.y + size, so it is placed at -size to put the floor surface at y = 0.
    return {"name": name, "primitive": "plane", "size": 0.5, "position": [gx, -0.5, gy],
            "rotation": [-90, 0, 0], "material": "floor"}


def near(ax, az, bx, bz, distance):
    return math.hypot(ax - bx, az - bz) < distance


class DungeonGame(PyScript):
    def __init__(self):
        super().__init__()
        self.base_seed = int(os.environ.get("DUNGEON_SEED", str(int(time.time()))))
        self.level_no = 1
        self.level = None
        self.level_seed = self.base_seed
        self.room = None            # room whose bodies are loaded
        self.room_bodies = []       # names of every body loaded for self.room
        self.room_handles = {}      # name -> Body, for enemies and the goal in self.room
        self.fixed = {}             # name -> Body, for the player, sword and hearts
        self.enemies = {}           # room -> list of {"id", "kind", "x", "z", "hp", "shot"}
        self.next_enemy_id = 0
        self.items = {}             # room -> list of {"id", "kind", "x", "z"}, left by defeated enemies
        self.bolts = []             # archer shots in self.room: {"id", "x", "z", "dx", "dz"}
        self.next_object_id = 0     # names for items and bolts are never reused, even across rooms
        self.coins = 0
        self.clock = 0.0            # seconds of play, for idle motion (bobbing, spinning)
        self.px = self.pz = 0.0
        self.facing = (0.0, 1.0)
        self.hp = MAX_HP
        self.invulnerable = 0.0
        self.cooldown = 0.0
        self.slash_time = 0.0
        self.slash_x = self.slash_z = 0.0
        self.slash_hits = set()
        self.attack_was_down = False
        self.last = None
        self.setup_done = False
        self.broken = False
        self.state_timer = 0.0
        self.anim = None            # clip the player is playing; None forces the next choice to start
        self.attack_time = 0.0      # seconds left of the attack clip

    # ----- engine entry point -------------------------------------------------------------

    def __call__(self, args):
        if self.broken:
            return
        try:
            self.frame()
        except Exception:
            # Stop quietly after the first failure, but say what went wrong.
            self.broken = True
            print("[dungeon] stopped after an error:")
            traceback.print_exc()

    def frame(self):
        now = time.perf_counter()
        dt = 0.0 if self.last is None else min(0.05, now - self.last)
        self.last = now
        self.clock += dt
        if not self.setup_done:
            self.setup()
            self.setup_done = True
            self.start_level(self.level_seed)

        self.cooldown = max(0.0, self.cooldown - dt)
        self.invulnerable = max(0.0, self.invulnerable - dt)
        moved = self.move_player(dt)
        self.enter_room_under_player()
        if self.reached_goal():
            return
        self.pick_up_items()
        if self.update_bolts(dt):
            return
        if self.update_enemies(dt):
            return
        self.update_sword(dt)
        self.update_player_animation(dt, moved)
        self.update_hud()
        self.update_props()
        self.update_camera()
        self.state_timer -= dt
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()

    def write_state(self):
        """Write the game state to cg_control/dungeon_state.json, so an agent can read it without a screenshot."""
        enemies = sum(len(v) for v in self.enemies.values())
        kinds = {kind: sum(1 for v in self.enemies.values() for e in v if e["kind"] == kind)
                 for kind in DungeonGen.ENEMY_KINDS}
        state = {"level": self.level_no, "seed": self.level_seed, "hp": self.hp,
                 "room": list(self.room) if self.room else None, "goal_room": list(self.level.goal_room),
                 "player": [round(self.px, 3), round(self.pz, 3)], "enemies": enemies, "enemy_kinds": kinds,
                 "items": sum(len(v) for v in self.items.values()), "bolts": len(self.bolts), "coins": self.coins}
        os.makedirs("cg_control", exist_ok=True)
        tmp = "cg_control/dungeon_state.json.tmp"
        with open(tmp, "w") as f:
            json.dump(state, f)
        os.replace(tmp, "cg_control/dungeon_state.json")

    # ----- setup and levels ---------------------------------------------------------------

    def setup(self):
        """Create the bodies that live for the whole game: the player, the sword and the hearts."""
        bodies = [
            {"name": "dg_player", "model": PLAYER_MODEL, "position": [0, 0, 0]},
            model("dg_slash", "slash", (0, 0, 0)),
        ]
        bodies += [model(f"dg_heart_{i}", "heart", (0, 1.0, 0), scale=0.6) for i in range(MAX_HP)]
        cge.load_scene_json(json.dumps({"version": 1, "bodies": bodies}))
        for body in bodies:
            self.fixed[body["name"]] = cge.find_body(body["name"])
        self.fixed["dg_slash"].set_rendering_enabled(False)

    def start_level(self, seed):
        self.unload_room()
        self.level = DungeonGen.generate(seed)
        self.level_seed = seed
        self.enemies = {}
        self.items = {}
        for room, spots in self.level.enemy_spawns.items():
            self.enemies[room] = []
            for (gx, gy), kind in zip(spots, self.level.enemy_kinds[room]):
                self.enemies[room].append({"id": self.next_enemy_id, "kind": kind, "x": float(gx), "z": float(gy),
                                           "hp": ENEMY_STATS[kind]["hp"], "shot": ARCHER_SHOT_TIME})
                self.next_enemy_id += 1
        self.px, self.pz = float(self.level.start[0]), float(self.level.start[1])
        self.hp = MAX_HP
        self.invulnerable = 0.0
        self.cooldown = 0.0
        self.slash_time = 0.0
        self.fixed["dg_slash"].set_rendering_enabled(False)
        self.load_room(self.level.room_of(*self.level.start))
        print(f"[dungeon] level {self.level_no} (seed {seed}): goal in room {self.level.goal_room}, "
              f"{sum(len(s) for s in self.enemies.values())} enemies")

    def load_room(self, room):
        self.unload_room()
        self.room = room
        floors, walls = self.level.room_tiles(room)
        bodies = [floor_tile(f"dg_f_{gx}_{gy}", gx, gy) for gx, gy in floors]
        bodies += [cube(f"dg_w_{gx}_{gy}", 0.5, (gx, 0.5, gy), "wall") for gx, gy in walls]
        if room == self.level.goal_room:
            gx, gy = self.level.goal
            bodies.append(model("dg_goal", "goal", (gx, 0.0, gy)))
        for enemy in self.enemies.get(room, []):
            bodies.append(self.enemy_body(enemy))
        for item in self.items.get(room, []):
            bodies.append(self.item_body(item))
        cge.load_scene_json(json.dumps({"version": 1, "bodies": bodies}))
        self.room_bodies = [body["name"] for body in bodies]
        for name in self.room_bodies:
            if name == "dg_goal" or name.startswith("dg_enemy_") or name.startswith("dg_item_"):
                self.room_handles[name] = cge.find_body(name)

    def unload_room(self):
        for name in self.room_bodies:
            cge.remove_body(name)
        self.room_bodies = []
        self.room_handles = {}
        self.bolts = []             # shots do not outlive the room they were fired in
        self.room = None

    def add_room_body(self, body):
        """Add one body to the loaded room, so unload_room removes it with the rest."""
        cge.load_scene_json(json.dumps({"version": 1, "bodies": [body]}))
        self.room_bodies.append(body["name"])
        self.room_handles[body["name"]] = cge.find_body(body["name"])

    def remove_room_body(self, name):
        cge.remove_body(name)
        if name in self.room_bodies:
            self.room_bodies.remove(name)
        self.room_handles.pop(name, None)

    def new_object_id(self):
        self.next_object_id += 1
        return self.next_object_id

    def enemy_name(self, enemy):
        return f"dg_enemy_{enemy['id']}"

    def enemy_body(self, enemy):
        yaw = yaw_towards(self.px - enemy["x"], self.pz - enemy["z"])
        return model(self.enemy_name(enemy), ENEMY_STATS[enemy["kind"]]["model"], (enemy["x"], 0.0, enemy["z"]), yaw)

    def item_body(self, item):
        return model(f"dg_item_{item['id']}", item["kind"], (item["x"], 0.0, item["z"]))

    def reached_goal(self):
        if self.room != self.level.goal_room:
            return False
        gx, gy = self.level.goal
        if not near(self.px, self.pz, gx, gy, GOAL_RADIUS):
            return False
        self.level_no += 1
        print(f"[dungeon] level {self.level_no - 1} complete")
        self.start_level(self.base_seed + self.level_no)
        return True

    # ----- player -------------------------------------------------------------------------

    def move_player(self, dt):
        dx = int(cge.key_down("D") or cge.key_down("Right")) - int(cge.key_down("A") or cge.key_down("Left"))
        dz = int(cge.key_down("S") or cge.key_down("Down")) - int(cge.key_down("W") or cge.key_down("Up"))
        if dx or dz:
            length = math.hypot(dx, dz)
            dx, dz = dx / length, dz / length
            self.facing = (dx, dz)
            step = PLAYER_SPEED * dt
            if self.free(self.px + dx * step, self.pz, PLAYER_RADIUS):
                self.px += dx * step
            if self.free(self.px, self.pz + dz * step, PLAYER_RADIUS):
                self.pz += dz * step
        place(self.fixed["dg_player"], self.px, 0.0, self.pz, yaw_towards(*self.facing))
        return bool(dx or dz)

    def update_player_animation(self, dt, moved):
        """Choose the clip: attack while swinging, walk while moving, otherwise idle. Only changes are sent to the engine."""
        self.attack_time = max(0.0, self.attack_time - dt)
        if self.attack_time > 0.0:
            want = "knight_attack"
        elif moved:
            want = "knight_walk"
        else:
            want = "knight_idle"
        if want != self.anim:
            cge.play_clip("dg_player", want, want != "knight_attack")
            self.anim = want

    def free(self, x, z, radius):
        """True if a box of this radius around (x, z) lies only on passable tiles."""
        for sx in (-1, 1):
            for sz in (-1, 1):
                if not self.level.passable(math.floor(x + sx * radius + 0.5), math.floor(z + sz * radius + 0.5)):
                    return False
        return True

    def enter_room_under_player(self):
        gx, gy = math.floor(self.px + 0.5), math.floor(self.pz + 0.5)
        room = self.level.room_of(gx, gy)
        if room != self.room:
            self.load_room(room)

    # ----- enemies and combat -------------------------------------------------------------

    def update_enemies(self, dt):
        """Move and attack with every enemy in the room. Returns True if the player was defeated."""
        for enemy in list(self.enemies.get(self.room, [])):
            stats = ENEMY_STATS[enemy["kind"]]
            dx, dz = self.px - enemy["x"], self.pz - enemy["z"]
            distance = math.hypot(dx, dz)
            # Slimes and brutes walk straight at the player. Archers keep to a band of distances and shoot.
            direction = 1.0
            if enemy["kind"] == "archer":
                if distance < ARCHER_RANGE[0]:
                    direction = -1.0
                elif distance <= ARCHER_RANGE[1]:
                    direction = 0.0
                enemy["shot"] -= dt
                if enemy["shot"] <= 0.0 and 0.01 < distance < ARCHER_SIGHT:
                    enemy["shot"] = ARCHER_SHOT_TIME
                    self.fire_bolt(enemy["x"], enemy["z"], dx / distance, dz / distance)
            if distance > 0.01 and direction:
                step = stats["speed"] * dt * direction
                nx = enemy["x"] + dx / distance * step
                nz = enemy["z"] + dz / distance * step
                if self.free(nx, enemy["z"], stats["radius"]):
                    enemy["x"] = nx
                if self.free(enemy["x"], nz, stats["radius"]):
                    enemy["z"] = nz
            body = self.room_handles.get(self.enemy_name(enemy))
            if body is not None:
                place(body, enemy["x"], 0.0, enemy["z"], yaw_towards(dx, dz) if distance > 0.01 else None)
                if enemy["kind"] == "slime":
                    # Squash and stretch while it hops along. Each slime gets its own phase.
                    s = math.sin(self.clock * 9.0 + enemy["id"])
                    body.get_mesh().set_scale(cge.Vector3f(1.0 + 0.08 * s, 1.0 - 0.12 * s, 1.0 + 0.08 * s))
            reach = CONTACT_DISTANCE + stats["size"] - ENEMY_STATS["slime"]["size"]
            if distance < reach and self.invulnerable <= 0.0:
                if self.hurt():
                    return True  # the level restarted, so this room's enemy list is stale
        return False

    def fire_bolt(self, x, z, dx, dz):
        bolt = {"id": self.new_object_id(), "x": x, "z": z, "dx": dx, "dz": dz}
        self.bolts.append(bolt)
        self.add_room_body(model(f"dg_bolt_{bolt['id']}", "arrow", (x, BOLT_HEIGHT, z), yaw_towards(dx, dz)))

    def update_bolts(self, dt):
        """Fly archer shots. A shot stops at a wall or on the player. Returns True if the player was defeated."""
        for bolt in list(self.bolts):
            bolt["x"] += bolt["dx"] * BOLT_SPEED * dt
            bolt["z"] += bolt["dz"] * BOLT_SPEED * dt
            name = f"dg_bolt_{bolt['id']}"
            hit_player = near(bolt["x"], bolt["z"], self.px, self.pz, BOLT_HIT_DISTANCE)
            if hit_player or not self.level.passable(math.floor(bolt["x"] + 0.5), math.floor(bolt["z"] + 0.5)):
                self.bolts.remove(bolt)
                self.remove_room_body(name)
                if hit_player and self.invulnerable <= 0.0 and self.hurt():
                    return True
                continue
            body = self.room_handles.get(name)
            if body is not None:
                place(body, bolt["x"], BOLT_HEIGHT, bolt["z"])
        return False

    def pick_up_items(self):
        """Collect items the player stands on. A heart is left on the floor while health is full."""
        for item in list(self.items.get(self.room, [])):
            if not near(item["x"], item["z"], self.px, self.pz, ITEM_PICKUP_DISTANCE):
                continue
            if item["kind"] == "heart":
                if self.hp >= MAX_HP:
                    continue  # leave it for later
                self.hp += 1
            else:
                self.coins += 1
            self.items[self.room].remove(item)
            self.remove_room_body(f"dg_item_{item['id']}")

    def hurt(self):
        """Take one point of damage. Returns True if the player was defeated and the level restarted."""
        self.hp -= 1
        self.invulnerable = INVULNERABLE_TIME
        if self.hp <= 0:
            print("[dungeon] you were defeated; the level restarts")
            self.start_level(self.level_seed)
            return True
        return False

    def update_sword(self, dt):
        attack = bool(cge.key_down("J") or cge.key_down("Space"))
        pressed = attack and not self.attack_was_down
        self.attack_was_down = attack
        if pressed and self.cooldown <= 0.0:
            self.cooldown = ATTACK_COOLDOWN
            self.attack_time = ATTACK_ANIM_TIME
            self.anim = None            # replay the attack clip from its start
            self.slash_time = SLASH_TIME
            self.slash_hits = set()
            fx, fz = self.facing
            self.slash_x = self.px + fx * SLASH_REACH
            self.slash_z = self.pz + fz * SLASH_REACH
            self.fixed["dg_slash"].set_rendering_enabled(True)

        if self.slash_time <= 0.0:
            return
        self.slash_time -= dt
        slash = self.fixed["dg_slash"]
        # The crescent is modelled in front of its origin, so it sits on the player and turns with the swing.
        place(slash, self.px, 0.0, self.pz, yaw_towards(*self.facing))
        for enemy in list(self.enemies.get(self.room, [])):
            if enemy["id"] in self.slash_hits or not near(enemy["x"], enemy["z"], self.slash_x, self.slash_z, SLASH_RADIUS):
                continue
            self.slash_hits.add(enemy["id"])
            enemy["hp"] -= 1
            if enemy["hp"] <= 0:
                self.kill_enemy(enemy)
        if self.slash_time <= 0.0:
            slash.set_rendering_enabled(False)

    def kill_enemy(self, enemy):
        self.enemies[self.room].remove(enemy)
        self.remove_room_body(self.enemy_name(enemy))
        kind = DungeonGen.drop_for(self.level_seed, enemy["id"], enemy["kind"])
        if kind is not None:
            item = {"id": self.new_object_id(), "kind": kind, "x": enemy["x"], "z": enemy["z"]}
            self.items.setdefault(self.room, []).append(item)
            self.add_room_body(self.item_body(item))

    # ----- presentation -------------------------------------------------------------------

    def update_hud(self):
        for i in range(MAX_HP):
            heart = self.fixed[f"dg_heart_{i}"]
            heart.set_rendering_enabled(i < self.hp)
            place(heart, self.px - 0.25 + 0.25 * i, 1.05, self.pz - 0.1)

    def update_props(self):
        """Idle motion: coins and the goal crystal spin, hearts on the floor bob."""
        spin = (self.clock * SPIN_SPEED) % 360.0
        goal = self.room_handles.get("dg_goal")
        if goal is not None:
            place(goal, self.level.goal[0], 0.0, self.level.goal[1], spin)
        for item in self.items.get(self.room, []):
            body = self.room_handles.get(f"dg_item_{item['id']}")
            if body is None:
                continue
            if item["kind"] == "coin":
                place(body, item["x"], 0.0, item["z"], spin)
            else:
                place(body, item["x"], 0.06 + 0.06 * math.sin(self.clock * 4.0 + item["id"]), item["z"])

    def update_camera(self):
        cge.set_camera(cge.Vector3f(self.px, CAMERA_HEIGHT, self.pz + CAMERA_BACK),
                       cge.Vector3f(self.px, 0.0, self.pz))


def create_instance():
    return DungeonGame()
