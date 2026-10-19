"""Creatures that wander the voxel terrain. Each is a model (a horse, from its kind's data) in a herd (cge.VoxelHerd, C++: src/Core/Voxel/
VoxelHerd.h), which moves every creature once a frame: it falls onto the ground, stops against walls, and does not pass through terrain
that has not loaded. The herd also makes the decisions: it picks a heading and a pace each frame, turns when a wall stops a creature, and
jumps a block when it can. The kinds (their pace, health, colours, model and clips) are in resources/voxel_creatures.json, read by
VoxelCreatures. A creature is put back at its spawn if it falls out of the world.

Most creatures are timid: they run from the player when it comes close. Some bite: a biter chases the player when it is near and bites
when it is close, which is what lowers the player's health (VoxelGame). A biter stopped by a wall plans a route round it (VoxelWorld.find_route),
and walks it when there is one; with none it wanders. A creature can be hit: it has HEALTH hits, each pushes it away, and when it dies it is
taken out of the world and leaves a hide (VoxelGen.HIDE). A creature in lava loses health every LAVA_INTERVAL seconds.

This module keeps the numbers the herd uses (sent to it once), each creature's body, and the events the herd reports (clips to play, materials
to draw, bodies to take out). Walker is a proxy for one creature, by name: the game reads its feet, box, health and state from the herd."""
import json
import math
import random

import cg_engine_bindings as cge
import VoxelCreatures
import VoxelGen
import VoxelNight
import VoxelPhysics as physics

HALF_WIDTH = 0.3            # the creature is 0.6 wide and 1.2 tall
HEIGHT = 1.2
BUMP_RANGE = 0.8            # two creatures this close turn away from each other
RESPAWN_DEPTH = -10.0       # below this a creature is put back at its spawn point
LOOK_AHEAD = 1.0            # blocks ahead a creature checks for lava before it walks on
KNOCK_SECONDS = 0.25        # a hit pushes the creature away for this long
KNOCK_PACE = 4.5            # blocks per second while it is pushed back
FLASH_SECONDS = 0.15        # a hit tints the creature for this long
HURT_SECONDS = 0.37               # a hit plays the hurt clip (resources/clips/horse_hurt.json) for this long
DIE_SECONDS = 0.97                # a death plays the die clip (resources/clips/horse_die.json); the body goes when it has played
LAVA_INTERVAL = 0.5               # seconds between a creature's losses of health in lava
SHIELD_COS = 0.5                   # a raised shield takes the bites of a creature within 60 degrees of the way the player faces
FLANK_SECONDS = 4.0                # after a shield turns a bite, a biter circles the player for this long, to bite from the side
FLANK_TURN = math.radians(60.0)    # while it circles, a biter heads this far off the way to the player, round to its flank side
ATTACK_SECONDS = 0.8               # how long a bite's clip plays before the creature goes back to walking or running
BITE_WINDUP = 0.3                  # seconds from the start of a bite's clip to its strike: the damage lands then, if the player is still in reach
ROUTE_SECONDS = 1.0                # a biter works out a route round a wall at most this often
ROUTE_SEARCHES_PER_FRAME = 2       # route searches that may start in one frame, across all the creatures
ROUTE_REACH = 0.45                 # blocks: a route cell is reached when the creature is this close to its centre
CHUNK = VoxelGen.CHUNK
# The numbers of the two kinds the game starts with, for the checks and the console (the kinds themselves are in the data file).
TIMID = VoxelCreatures.by_name("timid")
BITER = VoxelCreatures.by_name("biter")
HEALTH = TIMID["health"]
BITE_DAMAGE = BITER["bite_damage"]
BITER_COLOUR = BITER["colours"][0]
FLASH_COLOUR = (1.0, 0.35, 0.25)
ROLES = ("walk", "bound", "attack", "hurt", "die")
SPAWN_MIN = 16.0                  # blocks from the player: the nearest a creature appears (out of its sight, see manage)
SPAWN_MAX = 32.0                  # the farthest a creature appears
DESPAWN_RANGE = 40.0              # beyond this, a creature nothing holds is taken out
INTERACT_RANGE = 4.0              # within this (and level with the player) a creature is one the player is dealing with: it is not taken out
SPAWN_SPACING = 2.0               # a spawn spot is this far from every creature
SPAWN_LOOK_COS = 0.5              # a spawn spot is never within 60 degrees of the way the player looks
SPAWN_SECONDS = 0.1               # seconds between spawns while the herd is under its target
SPAWN_ATTEMPTS = 48               # spots tried for one spawn


def herd_rules():
    """The numbers the herd's rules use, from this module's constants (sent once, when the herd is made)."""
    return {"respawn_depth": RESPAWN_DEPTH, "bump_range": BUMP_RANGE, "look_ahead": LOOK_AHEAD, "knock_seconds": KNOCK_SECONDS,
            "knock_pace": KNOCK_PACE, "flash_seconds": FLASH_SECONDS, "hurt_seconds": HURT_SECONDS, "die_seconds": DIE_SECONDS,
            "shield_cos": SHIELD_COS, "flank_seconds": FLANK_SECONDS, "flank_turn": FLANK_TURN, "attack_seconds": ATTACK_SECONDS,
            "bite_windup": BITE_WINDUP, "route_seconds": ROUTE_SECONDS, "route_searches_per_frame": ROUTE_SEARCHES_PER_FRAME,
            "route_reach": ROUTE_REACH, "contact": physics.CONTACT, "contact_slack": physics.CONTACT_SLACK,
            "jump_speed": physics.JUMP_SPEED, "player_height": physics.PLAYER_HEIGHT, "half_width": HALF_WIDTH,
            "creature_height": HEIGHT, "lava_interval": LAVA_INTERVAL, "chase_range": VoxelNight.CHASE_RANGE,
            "night_chase_share": VoxelNight.NIGHT_CHASE_SHARE, "lava": sorted(VoxelGen.LAVA_BLOCKS), "liquids": sorted(VoxelGen.LIQUIDS),
            "spawn_min": SPAWN_MIN, "spawn_max": SPAWN_MAX, "despawn_range": DESPAWN_RANGE, "interact_range": INTERACT_RANGE,
            "spacing": SPAWN_SPACING, "look_cos": SPAWN_LOOK_COS, "chunk": VoxelGen.CHUNK, "sea_level": VoxelGen.SEA_LEVEL,
            "world_height": VoxelGen.HEIGHT}


def herd_kind(kind):
    """A kind (from VoxelCreatures) as the herd takes it: its numbers, and its clip by role (only the roles it has a clip for)."""
    numbers = dict(kind)
    numbers["clips"] = {role: VoxelCreatures.clip_for(kind, role) for role in ROLES if VoxelCreatures.clip_for(kind, role) is not None}
    return numbers


def apply_events(herd):
    """Do what the herd reports the engine must do: play clips, draw materials, take bodies out (see VoxelHerd.take_events)."""
    for kind, name, value, looping in herd.take_events():
        if kind != "remove" and cge.find_body(name) is None:
            continue   # the body is gone already (taken out in the same frame): nothing to play or draw
        if kind == "clip":
            cge.play_clip(name, value, looping, 1.0)
        elif kind == "material":
            cge.set_body_material(name, value)
        elif kind == "remove":
            cge.remove_body(name)


class Walker:
    """One creature, by name: its body is a model the herd moves. The herd holds its state; this is the game's handle on it."""

    def __init__(self, herd, name, spawn, kind):
        self.herd = herd
        self.name = name
        self.kind = kind
        self.biter = bool(kind["bites"])
        cge.load_scene_json(json.dumps({"version": 1, "bodies": [{"name": name, "model": kind["model"], "position": list(spawn),
                                                                  "material": kind["material"], "rotation": [0, 0, 0]}]}))
        herd.add(name, tuple(spawn), herd_kind(kind))

    @property
    def removed(self):
        """True once the creature is dead (it is taken out of the herd's checks at once; its body may still play the die clip)."""
        return not self.herd.alive(self.name)

    @property
    def health(self):
        return self.herd.health(self.name)

    def hurt(self, from_xz, damage=1, provoked=True):
        """A hit from the point from_xz (x, z): returns True when it kills the creature (the caller then takes it out with remove)."""
        killed = self.herd.hurt(self.name, tuple(from_xz), damage, provoked)
        apply_events(self.herd)
        return killed

    def die(self):
        """The creature is dead: it stops, and its body plays the die clip, then goes. Does nothing once it is dead."""
        self.herd.die(self.name)
        apply_events(self.herd)

    @property
    def heading(self):
        """The way the creature is heading, in radians."""
        return self.herd.heading(self.name)

    def state(self):
        """The creature's feet, velocity and ground contact, as the herd has them now."""
        return self.herd.state(self.name)

    def box(self):
        """The creature's box as (lo, hi) corners, where it stands now."""
        pos = self.state()[0]
        return ((pos[0] - HALF_WIDTH, pos[1], pos[2] - HALF_WIDTH), (pos[0] + HALF_WIDTH, pos[1] + HEIGHT, pos[2] + HALF_WIDTH))


class Walkers:
    """The creatures near the player: a herd (C++) that the game keeps at its target. Each frame manage takes out the creatures that
    have gone far and that nothing holds, and spawns one when the herd is short, out of the player's view."""

    def __init__(self, world, seed, prefix="creature"):
        self.world = world          # the herd reads the terrain from it, so it must outlive the herd
        self.herd = cge.VoxelHerd(world, herd_rules(), seed & 0xFFFFFFFF)
        self.walkers = []
        self.killed = []            # creatures that died in the last update (by a bite or by lava): the game takes them out and leaves hides
        self.blocks = 0             # shield blocks in the last update
        self.rng = random.Random(seed)
        self.prefix = prefix
        self.next_id = 0
        self.spawn_timer = 0.0

    def add_walker(self, name, spot, kind=None, biter=False):
        """A creature of a kind (or a biter or a timid one) on a body called name, standing at spot. Returns it."""
        kind = kind if kind is not None else VoxelCreatures.by_name("biter" if biter else "timid")
        walker = Walker(self.herd, name, spot, kind)
        self.walkers.append(walker)
        apply_events(self.herd)
        return walker

    def manage(self, dt, player, feet, look=None, keep=(), arrows_flying=False, target=0):
        """Keep the creatures near the player. Those beyond DESPAWN_RANGE go, and so does the farthest free one while the herd is over
        target; nothing the player is dealing with goes (keep names the creature under the crosshair; see VoxelHerd.holdsPlayer), and none
        goes while an arrow flies. While the herd is under target, a creature spawns every SPAWN_SECONDS at a spot out of the player's view
        (look: the player's horizontal look, or None). Returns the names taken out."""
        gone = self.herd.despawn(player, feet, list(keep), arrows_flying, target)
        if gone:
            names = set(gone)
            self.walkers = [walker for walker in self.walkers if walker.name not in names]
        apply_events(self.herd)
        self.spawn_timer -= dt
        if self.spawn_timer <= 0.0 and self.herd.live_count() < target:
            self.spawn_timer = SPAWN_SECONDS
            spot = self.herd.find_spawn(player, feet, look, SPAWN_ATTEMPTS, SPAWN_MIN, SPAWN_MAX)
            if spot is not None:
                self.next_id += 1
                self.add_walker("%s_%d" % (self.prefix, self.next_id), spot, kind=VoxelCreatures.choose(self.rng))
        return gone

    def update(self, dt, player=None, player_feet=None, night=0.0, shield=None):
        """player is the (x, z) of the player, which the creatures run from or chase; player_feet its height. Moves the whole herd (in
        C++) and returns the health the biters took from the player. self.killed and self.blocks are set from the update."""
        result = self.herd.update(dt, player, player_feet, night, shield)
        apply_events(self.herd)
        self.blocks = result["blocks"]
        by_name = {walker.name: walker for walker in self.walkers}
        self.killed = [by_name[name] for name in result["killed"] if name in by_name]
        return result["bites"]

    def remove(self, walker):
        """Take a creature out of the herd's live list and kill it (its body plays its die clip, then goes)."""
        if walker in self.walkers:
            self.walkers.remove(walker)
        walker.die()
