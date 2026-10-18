"""Creatures that wander the voxel terrain. Each is a model (a horse, from its kind's data) that the engine's voxel walker moves
once per update (cge.add_voxel_walker): it falls onto the ground, stops against walls, and does not pass through terrain that has not loaded.
The kinds (their pace, health, colours, model and clips) are in resources/voxel_creatures.json, read by VoxelCreatures.
This script is the brain: it picks a heading and a pace each frame, turns when a wall stops a creature, and jumps a block
when it can. A creature is put back at its spawn if it falls out of the world.

Most creatures are timid: they run from the player when it comes close. Some bite: a biter chases the player when it is near
and bites when it is close, which is what lowers the player's health (VoxelGame). A biter stopped by a wall plans a route round
it (VoxelPaths), and walks it when there is one; with none it wanders. A creature can be hit: it has HEALTH hits,
each pushes it away (the red flash is not drawn yet), and when it dies it is taken out of the world and leaves a hide (VoxelGen.HIDE)."""
import json
import math
import random

import cg_engine_bindings as cge
import VoxelCreatures
import VoxelGen
import VoxelNight
import VoxelPaths
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
CHASE_RANGE = VoxelNight.CHASE_RANGE   # a biter this close to the player comes after it by day; further at night (VoxelNight)
SHIELD_COS = 0.5                   # a raised shield takes the bites of a creature within 60 degrees of the way the player faces
FLANK_SECONDS = 4.0                # after a shield turns a bite, a biter circles the player for this long, to bite from the side
FLANK_TURN = math.radians(60.0)    # while it circles, a biter heads this far off the way to the player, round to its flank side
ATTACK_SECONDS = 0.8               # how long a bite's clip plays before the creature goes back to walking or running
BITE_WINDUP = 0.3                  # seconds from the start of a bite's clip to its strike: the damage lands then, if the player is still in reach
ROUTE_SECONDS = 1.0                # a biter works out a route round a wall at most this often
ROUTE_SEARCHES_PER_FRAME = 2       # route searches that may start in one frame, across all the creatures
_searches_left = [ROUTE_SEARCHES_PER_FRAME]   # searches left this frame (reset by the herd's update)
ROUTE_REACH = 0.45                 # blocks: a route cell is reached when the creature is this close to its centre
CHUNK = VoxelGen.CHUNK
# The numbers of the two kinds the game starts with, for the checks and the console (the kinds themselves are in the data file).
TIMID = VoxelCreatures.by_name("timid")
BITER = VoxelCreatures.by_name("biter")
HEALTH = TIMID["health"]
BITE_DAMAGE = BITER["bite_damage"]
PALETTE = TIMID["colours"]
BITER_COLOUR = BITER["colours"][0]
FLASH_COLOUR = (1.0, 0.35, 0.25)


class Walker:
    def __init__(self, world, name, spawn, colour, rng, biter=False, kind=None):
        self.name = name
        self.spawn = spawn
        self.rng = rng
        # The kind (resources/voxel_creatures.json) gives the creature its numbers, model and clips. Without one, biter picks it.
        self.kind = kind if kind is not None else VoxelCreatures.by_name("biter" if biter else "timid")
        self.biter = bool(self.kind["bites"])
        self.health = self.kind["health"]
        self.lava_timer = 0.0       # seconds the creature has been in lava (VoxelGame.burn_creatures)
        self.world = world          # the voxel world, which tells the creature where the lava is (lava_ahead)
        self.heading = rng.uniform(0.0, 2.0 * math.pi)
        self.timer = rng.uniform(1.0, 4.0)
        self.last = None            # the feet last frame, to see how far the creature got
        self.knock_timer = 0.0      # seconds left of being pushed back by a hit
        self.knock_heading = 0.0
        self.flank_timer = 0.0      # seconds left of circling the player to get round a raised shield
        self.flank_side = 1.0       # the way it circles: +1 or -1
        self.bite_timer = 0.0       # seconds until a biter may bite again
        self.fight_timer = 0.0      # seconds left of fighting back: a creature that was hit and lives chases and bites (hurt)
        self.windup = 0.0           # seconds left before a bite that has started lands (BITE_WINDUP); a hit or a lost chase cancels it
        self.flash_timer = 0.0      # seconds left of the red flash after a hit (FLASH_MATERIAL)
        self.hurt_timer = 0.0       # seconds left of the hurt clip (a flinch), played when the creature is hit
        self.die_timer = 0.0        # seconds left of the die clip, after a death: the body is taken out when it ends
        self.body_gone = False      # the body has been taken out of the world (remove)
        self.removed = False        # taken out of the world (it is dead): its walker and mesh are gone
        self.route = []             # the cells a biter walks round a wall to reach the player (VoxelPaths), next first
        self.route_timer = random.uniform(0.0, ROUTE_SECONDS)   # seconds until a biter may work out a route again (staggered)
        # The creature is a model body: a horse that the clips animate and the walker moves.
        cge.load_scene_json(json.dumps({"version": 1, "bodies": [{"name": name, "model": self.kind["model"], "position": list(spawn), "material": self.kind["material"],
                                                                  "rotation": [0, 0, 0]}]}))
        self.body = cge.find_body(name)
        self.attack_timer = 0.0     # seconds left of the bite clip
        self.clip = None            # the clip the model is playing
        cge.add_voxel_walker(name, world, HALF_WIDTH, HEIGHT)

    def faces(self, pos, player, shield):
        """Whether the creature at pos stands in front of the player, within the shield's arc: shield is the (x, z) the player faces.
        A creature on the player's own spot counts as in front."""
        dx, dz = pos[0] - player[0], pos[2] - player[1]
        distance = math.hypot(dx, dz)
        if distance < 1e-6:
            return True
        return (dx * shield[0] + dz * shield[1]) / distance > SHIELD_COS

    def chase_distance(self, night):
        """How close the player must be for this biter to chase it, for a night share of 0 to 1: its kind's sight (a day distance, see
        VoxelCreatures) scaled by the night, or the default chase range when the kind names no sight."""
        base = VoxelNight.chase_range(night)
        sight = self.kind.get("sight")
        return base if sight is None else base * sight / VoxelNight.CHASE_RANGE

    def update(self, dt, player=None, prey=None, player_feet=None, night=0.0, shield=None):
        """Move the creature one frame. Returns the health a biter takes from the player this frame (0 for most frames). prey is
        the (x, z) of a timid creature a biter hunts when the player is not near. player_feet is the height of the player's feet: a
        creature whose body does not reach the player's body (above or below it) neither bites it nor pushes against it. None
        means the player is level with the creature."""
        if self.removed:
            return 0.0
        self.flash(dt)
        self.hurt_timer = max(0.0, self.hurt_timer - dt)
        self.fight_timer = max(0.0, self.fight_timer - dt)
        fighting = self.fight_timer > 0.0
        pos, _vel, on_ground = cge.voxel_walker_state(self.name)
        if pos[1] < RESPAWN_DEPTH:
            cge.set_voxel_position(self.name, *self.spawn)
            self.last = None
            return 0.0
        self.bite_timer = max(0.0, self.bite_timer - dt)
        self.route_timer = max(0.0, self.route_timer - dt)
        if self.knock_timer > 0.0:
            # A hit pushes the creature away: it goes straight back, and its own choices wait until the push ends.
            self.knock_timer -= dt
            cge.set_voxel_velocity(self.name, math.cos(self.knock_heading) * KNOCK_PACE, math.sin(self.knock_heading) * KNOCK_PACE)
            self.last = pos
            return 0.0
        bite = 0.0
        pace = self.kind["pace"]
        self.timer -= dt
        distance = math.inf if player is None else math.hypot(pos[0] - player[0], pos[2] - player[1])
        level = player is not None and (player_feet is None
                                        or physics.overlaps_vertically(pos[1], HEIGHT, player_feet, physics.PLAYER_HEIGHT))
        if level and distance < physics.CONTACT:
            # A frame that ends with the creature inside contact (a long frame lets its move carry it past the limit the
            # velocity rule keeps): it is set back on the contact distance, beside the player, before it moves again.
            dx, dz = (pos[0] - player[0], pos[2] - player[1]) if distance > 0.0 else (math.cos(self.heading), math.sin(self.heading))
            gap = math.hypot(dx, dz) or 1.0
            cge.set_voxel_position(self.name, player[0] + dx / gap * physics.CONTACT, pos[1], player[1] + dz / gap * physics.CONTACT)
            pos = (player[0] + dx / gap * physics.CONTACT, pos[1], player[1] + dz / gap * physics.CONTACT)
            distance = physics.CONTACT
        chasing = (self.biter or fighting) and distance < self.chase_distance(night)
        if not chasing:
            self.route = []
            self.windup = 0.0
            self.flank_timer = 0.0
        biting = False              # a bite starts this frame: its clip plays now, and its damage lands BITE_WINDUP seconds later
        if chasing:
            # A biter heads for the player, and bites each time it is close and the last bite has had time to recover.
            self.heading = math.atan2(player[1] - pos[2], player[0] - pos[0])
            self.timer = 1.0
            pace = self.kind["chase_pace"]
            heading = self.follow_route(pos) if self.route else None
            if heading is not None:
                self.heading = heading
            if self.flank_timer > 0.0:
                # A creature a shield turned circles the player: it heads round to one side, until it is beside the shield
                # (out of its arc, where its bite lands) or the shield is lowered.
                self.flank_timer -= dt
                if shield is None or not self.faces(pos, player, shield):
                    self.flank_timer = 0.0
                else:
                    self.heading = math.atan2(player[1] - pos[2], player[0] - pos[0]) + self.flank_side * FLANK_TURN
            if self.windup > 0.0:
                # The lunge: the damage lands at the clip's strike, and only if the player is still in reach then.
                self.windup = max(0.0, self.windup - dt)
                if self.windup <= 0.0 and distance < self.kind["bite_range"] and level:
                    if shield is not None and self.faces(pos, player, shield):
                        # A shield held up in front of the player takes the bite, and the creature turns away from it.
                        self.blocked = True
                        self.turn_away(player[0], player[1])
                        # Pushed back from the shield, as a hit pushes a creature (hurt): the same knock, away from the player.
                        self.knock_heading = math.atan2(pos[2] - player[1], pos[0] - player[0])
                        self.knock_timer = KNOCK_SECONDS
                        # And it does not walk back into the shield: it circles round to bite from the side.
                        self.flank_side = self.rng.choice((-1.0, 1.0))
                        self.flank_timer = FLANK_SECONDS
                    else:
                        bite = self.kind["bite_damage"]
            elif distance < self.kind["bite_range"] and self.bite_timer <= 0.0 and level and self.flank_timer <= 0.0:
                self.windup = BITE_WINDUP
                self.bite_timer = self.kind["bite_interval"]
                biting = True
        elif self.biter and prey is not None:
            # With no player near, a biter hunts the nearest timid creature (Walkers.update does the biting).
            self.heading = math.atan2(prey[1] - pos[2], prey[0] - pos[0])
            self.timer = 1.0
            pace = self.kind["chase_pace"]
        elif not self.biter and distance < self.kind["flee_range"]:
            # Startled: it runs straight away from the player, and keeps running until it is out of range.
            self.heading = math.atan2(pos[2] - player[1], pos[0] - player[0])
            self.timer = 1.0
            pace = self.kind["flee_pace"]
        elif self.timer <= 0.0:
            self.heading += self.rng.uniform(-1.2, 1.2)
            self.timer = self.rng.uniform(1.0, 4.0)
        # A wall stops the creature short of the distance it meant to walk (seen in last frame's move): it turns, and
        # jumps a block if it stands on the ground.
        # A creature held at the player's side is not stuck on a wall: it stands there and bites.
        touching = level and distance < physics.CONTACT + physics.CONTACT_SLACK
        if self.last is not None and not touching:
            walked = math.hypot(pos[0] - self.last[0], pos[2] - self.last[2])
            if walked < 0.5 * pace * dt and on_ground:
                heading = self.follow_route(pos) if chasing and self.plan_route(pos, player, player_feet) else None
                if heading is not None:
                    self.heading = heading   # a way round the wall: no random turn
                else:
                    self.heading += self.rng.choice((-1.0, 1.0)) * self.rng.uniform(math.pi / 3, 2.0 * math.pi / 3)
                cge.voxel_jump(self.name, physics.JUMP_SPEED)
        self.last = pos
        if self.lava_ahead(pos):
            # Lava is a wall to a creature: it turns to one side of it, and keeps its other choices.
            self.heading += self.rng.choice((-1.0, 1.0)) * math.pi / 2.0
        vx, vz = math.cos(self.heading) * pace, math.sin(self.heading) * pace
        if player is not None:
            # The player is a body too: a creature does not walk into it (the same contact rule as the player's own moves).
            if level:
                vx, vz = physics.keep_out(vx, vz, player[0] - pos[0], player[1] - pos[2])
        cge.set_voxel_velocity(self.name, vx, vz)
        self.animate(dt, pace, biting)
        return bite

    def plan_route(self, pos, player, player_feet):
        """Work out a route from the creature's cell to a cell beside the player (VoxelPaths), at most once every ROUTE_SECONDS.
        Returns True when a route is there to walk. With no route found, the creature keeps to its own choices."""
        if self.route_timer > 0.0:
            return bool(self.route)
        if _searches_left[0] <= 0:
            return bool(self.route)   # the frame's searches are used up: this creature tries again next frame
        _searches_left[0] -= 1
        self.route_timer = ROUTE_SECONDS
        here = (math.floor(pos[0]), math.floor(pos[1] + 0.1), math.floor(pos[2]))
        feet = here[1] if player_feet is None else math.floor(player_feet + 0.1)
        px, pz = math.floor(player[0]), math.floor(player[1])

        # The search runs in C++ (VoxelWorld.find_route): the same route as VoxelPaths.find_path, which it replaces here.
        self.route = self.world.find_route(list(here), [px, feet, pz], VoxelGen.LIQUIDS) or []
        return bool(self.route)

    def follow_route(self, pos):
        """The heading toward the next cell of the route, which is dropped once the creature has reached it. None when no route is
        left to walk."""
        while self.route:
            x, _y, z = self.route[0]
            if math.hypot(x + 0.5 - pos[0], z + 0.5 - pos[2]) >= ROUTE_REACH:
                return math.atan2(z + 0.5 - pos[2], x + 0.5 - pos[0])
            self.route.pop(0)
        return None

    def hurt(self, from_xz, damage=1, provoked=True):
        """A hit from the point from_xz (x, z): the creature loses health, is pushed away from the hit. A kind that fights back
        (fight_back_seconds) and is still alive when a player's hit lands fights for that long; provoked is False for a hit from a
        biter, which does not turn the prey on the player. Returns True when the hit kills it. A creature that is already removed
        takes no more hits."""
        if self.removed:
            return False
        self.health -= damage
        if provoked and not self.biter and self.health > 0 and "fight_back_seconds" in self.kind:
            self.fight_timer = self.kind["fight_back_seconds"]
        self.windup = 0.0
        self.flash_timer = FLASH_SECONDS
        cge.set_body_material(self.name, self.kind["flash_material"])   # the kind's red-tinted material (voxels.json)
        hurt_clip = VoxelCreatures.clip_for(self.kind, "hurt")
        if hurt_clip is not None:
            self.hurt_timer = HURT_SECONDS
            self.clip = hurt_clip
            cge.play_clip(self.name, hurt_clip, False, 1.0)
        pos = self.state()[0]
        self.knock_heading = math.atan2(pos[2] - from_xz[1], pos[0] - from_xz[0])
        self.knock_timer = KNOCK_SECONDS
        return self.health <= 0

    def die(self):
        """The creature is dead: it stops being a walker at once, and its body plays the die clip. Walkers takes the body out
        when the clip has played (die_timer)."""
        if self.removed:
            return
        self.removed = True
        cge.remove_voxel_walker(self.name)
        die_clip = VoxelCreatures.clip_for(self.kind, "die")
        # A kind with no die clip is taken out at once; otherwise its body lies for the clip, then goes.
        self.die_timer = DIE_SECONDS if die_clip is not None else 0.0
        if die_clip is not None:
            self.clip = die_clip
            cge.play_clip(self.name, die_clip, False, 1.0)

    def flash(self, dt):
        """Count down the red flash after a hit, and give the creature its own material back when it ends."""
        if self.flash_timer <= 0.0:
            return
        self.flash_timer = max(0.0, self.flash_timer - dt)
        if self.flash_timer <= 0.0:
            cge.set_body_material(self.name, self.kind["material"])

    def bite_now(self):
        """A bite lands on a creature (prey), not on the player: the bite clip plays now, as it does for a bite on the player."""
        self.attack_timer = ATTACK_SECONDS
        clip = VoxelCreatures.clip_for(self.kind, "attack")
        if clip is not None and clip != self.clip:
            cge.play_clip(self.name, clip, False, 1.0)
            self.clip = clip

    def animate(self, dt, pace, bit):
        """Play the creature's clips: a hit's flinch, then a bite plays the attack once, a chase or a flight the bound, and a walk
        otherwise (VoxelCreatures.clip_for names each). The model turns to face its heading (the model faces +Z, as the dungeon's
        do, so the yaw is atan2(x, z))."""
        if bit:
            self.attack_timer = ATTACK_SECONDS
        self.attack_timer = max(0.0, self.attack_timer - dt)
        # A flinch comes first, then a bite, then the gait. A role the kind has no clip for falls back to its walk.
        if self.hurt_timer > 0.0:
            role = "hurt"
        elif self.attack_timer > 0.0:
            role = "attack"
        elif pace > self.kind["pace"] + 0.1:
            role = "bound"
        else:
            role = "walk"
        clip = VoxelCreatures.clip_for(self.kind, role) or VoxelCreatures.clip_for(self.kind, "walk")
        attack = VoxelCreatures.clip_for(self.kind, "attack")
        once = clip in (attack, VoxelCreatures.clip_for(self.kind, "hurt"))
        if clip is not None and (clip != self.clip or (clip == attack and bit)):
            cge.play_clip(self.name, clip, not once, 1.0)
            self.clip = clip
        yaw = math.degrees(math.atan2(math.cos(self.heading), math.sin(self.heading)))
        self.body.get_mesh().set_rotation(cge.Vector3f(0.0, yaw, 0.0))

    def remove(self):
        """Take the creature out of the world now: its walker and its mesh."""
        if self.body_gone:
            return
        self.body_gone = True
        if not self.removed:
            self.removed = True
            cge.remove_voxel_walker(self.name)
        cge.remove_body(self.name)

    def box(self):
        """The creature's box as (lo, hi) corners, where it stands now."""
        pos = self.state()[0]
        return ((pos[0] - HALF_WIDTH, pos[1], pos[2] - HALF_WIDTH), (pos[0] + HALF_WIDTH, pos[1] + HEIGHT, pos[2] + HALF_WIDTH))

    def state(self):
        """The creature's feet, velocity and ground contact, as the engine has them now."""
        return cge.voxel_walker_state(self.name)

    def lava_ahead(self, pos):
        """True when the cell a step ahead, at the creature's feet, is lava."""
        x = pos[0] + math.cos(self.heading) * LOOK_AHEAD
        z = pos[2] + math.sin(self.heading) * LOOK_AHEAD
        return self.world.get_block(math.floor(x), math.floor(pos[1] + 0.1), math.floor(z)) in VoxelGen.LAVA_BLOCKS

    def turn_away(self, x, z):
        """Head away from the point (x, z), a little off straight, so two creatures do not turn towards each other."""
        pos = self.state()[0]
        self.heading = math.atan2(pos[2] - z, pos[0] - x) + self.rng.uniform(-0.3, 0.3)
        self.timer = self.rng.uniform(1.0, 2.0)


class Walkers:
    """A herd of creatures spread over the loaded terrain near centre, each standing on the ground where it is put."""

    def __init__(self, world, centre, count, seed, prefix="creature"):
        self.walkers = []
        self.corpses = []           # creatures that died: their bodies play the die clip, then are taken out (see remove)
        self.killed = []            # creatures a biter killed in the last update: the game takes them out and leaves hides
        rng = random.Random(seed)
        for i in range(count):
            spot = self.find_ground(world, centre, rng)
            if spot is None:
                continue
            kind = VoxelCreatures.choose(rng)
            colour = rng.choice(kind["colours"])
            self.walkers.append(Walker(world, "%s_%d" % (prefix, i), spot, colour, rng, kind=kind))

    @staticmethod
    def find_ground(world, centre, rng, tries=40):
        """A point near centre standing on the top solid block of its column, on loaded terrain above the water."""
        for _ in range(tries):
            x = math.floor(centre[0] + rng.uniform(-12.0, 12.0))
            z = math.floor(centre[1] + rng.uniform(-12.0, 12.0))
            if not world.is_loaded(math.floor(x / CHUNK), math.floor(z / CHUNK)):
                continue
            for y in range(VoxelGen.HEIGHT - 2, 0, -1):
                if world.is_solid(x, y, z):
                    # The creature's box must have room above the ground: no leaves or walls within a block of it.
                    clear = all(not world.is_solid(x + dx, y + 1 + k, z + dz) for dx in (-1, 0, 1) for dz in (-1, 0, 1) for k in (0, 1))
                    if clear and y + 1 > VoxelGen.SEA_LEVEL:
                        return (x + 0.5, float(y + 1), z + 0.5)
                    break
        return None

    def update(self, dt, player=None, player_feet=None, night=0.0, shield=None):
        """player is the (x, z) of the player, which the creatures run from or chase. A creature that is about to bump another
        turns away from it, so the herd does not stack up in one place. Returns the health the biters took from the player.
        A biter with no player near hunts the nearest timid creature and bites it; a creature a bite kills is in self.killed."""
        _searches_left[0] = ROUTE_SEARCHES_PER_FRAME   # the frame's route searches, shared by the herd
        for corpse in list(self.corpses):
            corpse.die_timer -= dt
            if corpse.die_timer <= 0.0:
                corpse.remove()
                self.corpses.remove(corpse)
        spots = [w.state()[0] for w in self.walkers]
        bites = 0.0
        self.killed = []
        self.blocks = 0
        for i, walker in enumerate(self.walkers):
            prey = self.prey_for(i, spots) if walker.biter else None
            walker.blocked = False
            bites += walker.update(dt, player, None if prey is None else (spots[prey][0], spots[prey][2]), player_feet, night, shield)
            if walker.blocked:
                self.blocks += 1
            for j, other in enumerate(spots):
                if i != j and math.hypot(spots[i][0] - other[0], spots[i][2] - other[2]) < BUMP_RANGE:
                    walker.turn_away(other[0], other[2])
                    break
            # A prey already killed this frame (by another biter) is not hit again: it is in self.killed once, and is removed once.
            victim = self.walkers[prey] if prey is not None else None
            if victim is not None and walker.bite_timer <= 0.0 and not victim.removed and victim not in self.killed:
                if math.hypot(spots[i][0] - spots[prey][0], spots[i][2] - spots[prey][2]) < walker.kind["bite_range"]:
                    walker.bite_timer = walker.kind["bite_interval"]
                    walker.bite_now()
                    if victim.hurt((spots[i][0], spots[i][2]), provoked=False):
                        self.killed.append(victim)
        return bites

    def prey_for(self, index, spots):
        """The index of the prey of the biter at index, or None. The nearest timid creature within its prey_range comes first; with
        none, the nearest biter that is wounded (a healthy biter is not hunted). Each kind's nearest is kept apart, so a timid
        creature is taken over a wounded biter however near that biter is."""
        prey_range = self.walkers[index].kind["prey_range"]
        nearest = {}   # 0 for a timid creature, 1 for a wounded biter: the (distance, index) of the nearest of each
        for j, other in enumerate(self.walkers):
            if other.removed or j == index:
                continue
            if other.biter and other.health >= other.kind["health"]:
                continue
            distance = math.hypot(spots[index][0] - spots[j][0], spots[index][2] - spots[j][2])
            if distance >= prey_range:
                continue
            rank = 1 if other.biter else 0
            if rank not in nearest or distance < nearest[rank][0]:
                nearest[rank] = (distance, j)
        for rank in (0, 1):
            if rank in nearest:
                return nearest[rank][1]
        return None

    def remove(self, walker):
        """Take a creature out of the herd and out of the world (when it dies)."""
        if walker in self.walkers:
            self.walkers.remove(walker)
            walker.die()
            self.corpses.append(walker)
