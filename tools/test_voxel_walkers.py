"""
Checks the biters' reaction to a raised shield (resources/scripts/VoxelWalkers.py) without the engine: a bite that the shield turns
knocks the biter back, then starts a circling of the player (FLANK_SECONDS), heading round to one side of the way to the player; a
biter that is beside the shield (out of its arc) bites there, and the bite lands; lowering the shield ends the circling. The
engine's walker calls are stand-ins: a creature moves by its velocity each frame, and its pose (animation) is not drawn.
Usage (from the repo root): python -I tools/test_voxel_walkers.py   Exits 0 on success, 1 on any failed check.
"""
import math
import os
import random
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

# A stand-in engine: a walker's feet are kept where the test puts them, and its velocity is the one the walker set.
state = {"pos": (0.0, 0.0, 0.0), "vel": (0.0, 0.0)}


def walker_state(name):
    return list(state["pos"]), (0.0, 0.0, 0.0), True


def set_position(name, x, y, z):
    state["pos"] = (x, y, z)


def set_velocity(name, vx, vz):
    state["vel"] = (vx, vz)


fake = types.ModuleType("cg_engine_bindings")
fake.voxel_walker_state = walker_state
fake.set_voxel_position = set_position
fake.set_voxel_velocity = set_velocity
fake.__getattr__ = lambda name: (lambda *args, **kwargs: None)
sys.modules["cg_engine_bindings"] = fake

import VoxelWalkers  # noqa: E402

# The walker's pose (its model's clip and turn) is the engine's; the checks here are on the choices.
VoxelWalkers.Walker.animate = lambda self, *args, **kwargs: None

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


class OpenGround:
    """A world with no lava and no walls: every block is air."""
    def get_block(self, x, y, z):
        return 0


DT = 1.0 / 60.0
PLAYER = (0.0, 0.0)          # the player's (x, z)
FEET = 0.0
SHIELD = (1.0, 0.0)          # the way the player faces: +x
FRONT = (1.0, 0.2)           # in front of the shield, and within reach of a bite


def step(walker, seconds, shield):
    """Run the walker for seconds, moving it by its velocity each frame. Returns the bite damage that landed and whether a bite
    was blocked on the way."""
    bite = 0.0
    blocked = False
    for _ in range(int(round(seconds / DT))):
        walker.blocked = False      # Walkers.update clears this each frame before the walker moves
        bite += walker.update(DT, PLAYER, None, FEET, 0.0, shield)
        blocked = blocked or walker.blocked
        x, y, z = state["pos"]
        state["pos"] = (x + state["vel"][0] * DT, y, z + state["vel"][1] * DT)
    return bite, blocked


def angle_off_player(pos, heading):
    """The turn, in radians, from the way to the player to heading (the short way round)."""
    to_player = math.atan2(PLAYER[1] - pos[2], PLAYER[0] - pos[0])
    return (heading - to_player + math.pi) % (2.0 * math.pi) - math.pi


kind = VoxelWalkers.VoxelCreatures.by_name("biter")
# A biter's sight is how far it notices the player by day, scaled by the night; a kind with no sight keeps the default range.
short = dict(kind, sight=3.0)
seen = VoxelWalkers.Walker(OpenGround(), "biter_sight", (1.0, FEET, 0.0), (0.4, 0.1, 0.1), random.Random(5), kind=short)
check("a biter with sight 3 chases from 3 blocks by day, not from 5", abs(seen.chase_distance(0.0) - 3.0) < 1e-9)
check("a biter with no sight chases from the default 7 blocks by day",
      abs(VoxelWalkers.Walker(OpenGround(), "biter_default", (1.0, FEET, 0.0), (0.4, 0.1, 0.1), random.Random(6),
                              kind=kind).chase_distance(0.0) - VoxelWalkers.VoxelNight.CHASE_RANGE) < 1e-9)
check("the night extends a biter's sight the same way as the default",
      abs(seen.chase_distance(1.0) - 3.0 * (VoxelWalkers.VoxelNight.chase_range(1.0) / VoxelWalkers.VoxelNight.CHASE_RANGE)) < 1e-9)
knock = VoxelWalkers.KNOCK_SECONDS

walker = VoxelWalkers.Walker(OpenGround(), "biter_test", (FRONT[0], FEET, FRONT[1]), (0.4, 0.1, 0.1), random.Random(3), kind=kind)
state["pos"] = (FRONT[0], FEET, FRONT[1])
bite, blocked = step(walker, VoxelWalkers.BITE_WINDUP + 0.1, SHIELD)
check("a biter in front of a raised shield has its bite turned", blocked and bite == 0.0)
check("the turned bite sets the circling for FLANK_SECONDS, to one side",
      walker.flank_timer > 0.0 and walker.flank_side in (1.0, -1.0))

# The knock pushes it back (it stays in front of the shield), then it circles: one frame later it heads a turn off the way to the
# player, round to its flank side.
step(walker, knock + 0.05, SHIELD)
check("the knock keeps it in front, and the circling goes on", walker.flank_timer > 0.0)
walker.blocked = False
walker.update(DT, PLAYER, None, FEET, 0.0, SHIELD)
heading = math.atan2(state["vel"][1], state["vel"][0])
turn = angle_off_player(state["pos"], heading)
check("while in front, it heads FLANK_TURN off the way to the player, round to its flank side",
      abs(abs(turn) - VoxelWalkers.FLANK_TURN) < 1e-6 and (turn > 0) == (walker.flank_side > 0))

# Beside the shield (out of its arc): the circling ends, and its next bite lands.
state["pos"] = (0.3, FEET, 1.0)
step(walker, 0.02, SHIELD)
check("beside the shield, the circling ends", walker.flank_timer == 0.0)
bite, blocked = step(walker, kind["bite_interval"] + VoxelWalkers.BITE_WINDUP + 0.2, SHIELD)
check("beside the shield, its next bite lands (it is not blocked)", bite > 0.0 and not blocked)

# Lowering the shield while it circles ends the circling at once.
other = VoxelWalkers.Walker(OpenGround(), "biter_test2", (FRONT[0], FEET, FRONT[1]), (0.4, 0.1, 0.1), random.Random(4), kind=kind)
state["pos"] = (FRONT[0], FEET, FRONT[1])
step(other, VoxelWalkers.BITE_WINDUP + knock + 0.1, SHIELD)
check("a biter circling a raised shield keeps circling", other.flank_timer > 0.0)
step(other, 0.02, None)
check("with the shield lowered, the circling ends", other.flank_timer == 0.0)

# A timid creature runs from the player until it is hit; a hit that leaves it alive makes it fight back for a while (16.5).
timid = VoxelWalkers.VoxelCreatures.by_name("timid")
state["pos"] = (3.0, FEET, 0.0)
runner = VoxelWalkers.Walker(OpenGround(), "timid_test", (3.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(7), kind=timid)
step(runner, 0.5, None)
check("an unhurt timid creature runs away from the player", state["vel"][0] > 0.0 and runner.fight_timer == 0.0)
runner.hurt((2.0, 0.0), 1)
check("a hit that leaves it alive makes it fight back", runner.fight_timer == timid["fight_back_seconds"] and runner.health == 2)
bite, _ = step(runner, 3.0, None)
check("while it fights, it chases the player and bites", bite > 0.0)
step(runner, timid["fight_back_seconds"], None)
check("the fight ends after its seconds, and the creature goes back to running", runner.fight_timer == 0.0)
dying = VoxelWalkers.Walker(OpenGround(), "timid_dying", (3.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(8), kind=timid)
dying.health = 1
check("a killing hit does not start a fight", dying.hurt((2.0, 0.0), 1) and dying.fight_timer == 0.0)
prey = VoxelWalkers.Walker(OpenGround(), "timid_prey", (3.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(9), kind=timid)
prey.hurt((2.0, 0.0), 1, provoked=False)
check("a biter's hit on a timid creature does not turn it on the player", prey.fight_timer == 0.0)
check("a creature that bites takes no fight_back_seconds (it bites already)", "fight_back_seconds" not in kind)

# Biters hunt a wounded biter, and a timid creature first.
herd_world = OpenGround()
hunter = VoxelWalkers.Walker(herd_world, "biter_hunter", (0.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(10), kind=kind)
hurt_biter = VoxelWalkers.Walker(herd_world, "biter_hurt", (4.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(11), kind=kind)
healthy_biter = VoxelWalkers.Walker(herd_world, "biter_healthy", (3.0, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(12), kind=kind)
hurt_biter.health = 1
herd = VoxelWalkers.Walkers.__new__(VoxelWalkers.Walkers)
herd.walkers = [hunter, healthy_biter, hurt_biter]
spots = [w.state()[0] for w in herd.walkers]
check("a biter hunts a wounded biter when no timid creature is near", herd.prey_for(0, spots) == 2)
check("a healthy biter is not hunted by another biter", herd.prey_for(2, spots) is None)
herd.walkers.append(VoxelWalkers.Walker(herd_world, "timid_near", (5.5, FEET, 0.0), (0.9, 0.9, 0.9), random.Random(13), kind=timid))
spots = [w.state()[0] for w in herd.walkers]
check("a timid creature is taken over a wounded biter, however near that biter is", herd.prey_for(0, spots) == 3)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel walker checks passed.")
