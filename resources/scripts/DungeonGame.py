"""
Dungeon sample game: a top-down action adventure with procedural rooms (docs/games/dungeon.md).

Attached to the scene root's update domain by resources/scenes/dungeon.json. Each frame it reads the
keyboard, moves the player and enemies on the level grid, handles the sword, and moves rooms when the
player crosses a door. Game state lives here in Python. Bodies are only used to show that state.

Controls: WASD or arrow keys to move, J or Space to swing the sword, the goal is the glowing tile
in the far room. Set DUNGEON_SEED to choose a layout (default: the current time).
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
ENEMY_SPEED = 1.6
PLAYER_RADIUS = 0.3
ENEMY_RADIUS = 0.35
CONTACT_DISTANCE = 0.7      # an enemy this close hurts the player
INVULNERABLE_TIME = 1.0     # seconds of protection after a hit
MAX_HP = 3
ATTACK_COOLDOWN = 0.35
SLASH_TIME = 0.18
SLASH_REACH = 0.9           # how far in front of the player the swing lands
SLASH_RADIUS = 0.85
GOAL_RADIUS = 0.6
CAMERA_HEIGHT = 12.0
CAMERA_BACK = 5.0           # the camera sits this far behind the player, for a three-quarter view
STATE_INTERVAL = 0.25       # seconds between writes to cg_control/dungeon_state.json


def cube(name, size, pos, material):
    return {"name": name, "primitive": "cube", "size": size, "position": list(pos), "material": material}


def floor_tile(name, gx, gy):
    # A plane is built in the XY plane, so it is turned to lie flat on the XZ floor.
    return {"name": name, "primitive": "plane", "size": 0.5, "position": [gx, 0.0, gy],
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
        self.enemies = {}           # room -> list of {"id", "x", "z", "hp"}
        self.next_enemy_id = 0
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
        if not self.setup_done:
            self.setup()
            self.setup_done = True
            self.start_level(self.level_seed)

        self.cooldown = max(0.0, self.cooldown - dt)
        self.invulnerable = max(0.0, self.invulnerable - dt)
        self.move_player(dt)
        self.enter_room_under_player()
        if self.reached_goal():
            return
        self.update_enemies(dt)
        self.update_sword(dt)
        self.update_hud()
        self.update_camera()
        self.state_timer -= dt
        if self.state_timer <= 0.0:
            self.state_timer = STATE_INTERVAL
            self.write_state()

    def write_state(self):
        """Write the game state to cg_control/dungeon_state.json, so an agent can read it without a screenshot."""
        enemies = sum(len(v) for v in self.enemies.values())
        state = {"level": self.level_no, "seed": self.level_seed, "hp": self.hp,
                 "room": list(self.room) if self.room else None, "goal_room": list(self.level.goal_room),
                 "player": [round(self.px, 3), round(self.pz, 3)], "enemies": enemies}
        os.makedirs("cg_control", exist_ok=True)
        tmp = "cg_control/dungeon_state.json.tmp"
        with open(tmp, "w") as f:
            json.dump(state, f)
        os.replace(tmp, "cg_control/dungeon_state.json")

    # ----- setup and levels ---------------------------------------------------------------

    def setup(self):
        """Create the bodies that live for the whole game: the player, the sword and the hearts."""
        bodies = [
            cube("dg_player", 0.3, (0, 0.3, 0), "player"),
            cube("dg_slash", 0.3, (0, 0.2, 0), "slash"),
        ]
        bodies += [cube(f"dg_heart_{i}", 0.1, (0, 1.0, 0), "player") for i in range(MAX_HP)]
        cge.load_scene_json(json.dumps({"version": 1, "bodies": bodies}))
        for body in bodies:
            self.fixed[body["name"]] = cge.find_body(body["name"])
        self.fixed["dg_slash"].set_rendering_enabled(False)

    def start_level(self, seed):
        self.unload_room()
        self.level = DungeonGen.generate(seed)
        self.level_seed = seed
        self.enemies = {}
        for room, spots in self.level.enemy_spawns.items():
            self.enemies[room] = []
            for gx, gy in spots:
                self.enemies[room].append({"id": self.next_enemy_id, "x": float(gx), "z": float(gy), "hp": 2})
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
            bodies.append(cube("dg_goal", 0.3, (gx, 0.4, gy), "goal"))
        for enemy in self.enemies.get(room, []):
            bodies.append(cube(self.enemy_name(enemy), 0.35, (enemy["x"], 0.35, enemy["z"]), "enemy"))
        cge.load_scene_json(json.dumps({"version": 1, "bodies": bodies}))
        self.room_bodies = [body["name"] for body in bodies]
        for name in self.room_bodies:
            if name == "dg_goal" or name.startswith("dg_enemy_"):
                self.room_handles[name] = cge.find_body(name)

    def unload_room(self):
        for name in self.room_bodies:
            cge.remove_body(name)
        self.room_bodies = []
        self.room_handles = {}
        self.room = None

    def enemy_name(self, enemy):
        return f"dg_enemy_{enemy['id']}"

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
        self.fixed["dg_player"].get_mesh().set_position(cge.Vector3f(self.px, 0.3, self.pz))

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
        for enemy in list(self.enemies.get(self.room, [])):
            dx, dz = self.px - enemy["x"], self.pz - enemy["z"]
            distance = math.hypot(dx, dz)
            if distance > 0.01:
                step = ENEMY_SPEED * dt
                nx = enemy["x"] + dx / distance * step
                nz = enemy["z"] + dz / distance * step
                if self.free_for_enemy(nx, enemy["z"]):
                    enemy["x"] = nx
                if self.free_for_enemy(enemy["x"], nz):
                    enemy["z"] = nz
            body = self.room_handles.get(self.enemy_name(enemy))
            if body is not None:
                body.get_mesh().set_position(cge.Vector3f(enemy["x"], 0.35, enemy["z"]))
            if distance < CONTACT_DISTANCE and self.invulnerable <= 0.0:
                if self.hurt():
                    return  # the level restarted, so this room's enemy list is stale

    def free_for_enemy(self, x, z):
        return self.free(x, z, ENEMY_RADIUS)

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
        slash.get_mesh().set_position(cge.Vector3f(self.slash_x, 0.2, self.slash_z))
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
        name = self.enemy_name(enemy)
        cge.remove_body(name)
        self.room_bodies.remove(name)
        self.room_handles.pop(name, None)

    # ----- presentation -------------------------------------------------------------------

    def update_hud(self):
        for i in range(MAX_HP):
            heart = self.fixed[f"dg_heart_{i}"]
            heart.set_rendering_enabled(i < self.hp)
            heart.get_mesh().set_position(cge.Vector3f(self.px - 0.25 + 0.25 * i, 1.0, self.pz - 0.6))

    def update_camera(self):
        cge.set_camera(cge.Vector3f(self.px, CAMERA_HEIGHT, self.pz + CAMERA_BACK),
                       cge.Vector3f(self.px, 0.0, self.pz))


def create_instance():
    return DungeonGame()
