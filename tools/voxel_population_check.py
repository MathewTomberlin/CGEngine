"""
Checks the creature population (VoxelWalkers.Walkers.manage, and the herd's spawn search and despawn) from inside the running sample
game: spawn spots are on the ground between the spawn distances and out of the player's view, the search is cheap, the population
reaches its target, and a creature the player is dealing with is never taken out (kept, shot at, hit, or near). test_voxels.ps1 runs it
through the control channel (run_script) and reads its result from cg_control/voxel_population_check.json.
"""
import gc
import json
import math
import os
import time

import cg_engine_bindings as cge
import VoxelGame
import VoxelWalkers

RESULT = "cg_control/voxel_population_check.json"


def find_game():
    games = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"]
    if not games:
        raise RuntimeError("no VoxelGame in the engine (the scene must run it)")
    return games[0]


def main():
    checks = {}
    errors = []
    info = {}
    game = find_game()
    walkers = None
    try:
        walkers = game.walkers
        if walkers is None:
            raise RuntimeError("the creatures are not made yet")
        herd = walkers.herd
        world = game.world
        player = (game.pos[0], game.pos[2])
        feet = game.pos[1]
        look_3d = game.look_direction()
        horizontal = math.hypot(look_3d[0], look_3d[2])
        look = (look_3d[0] / horizontal, look_3d[2] / horizontal) if horizontal > 1e-6 else None
        tolerance = 0.75   # a spot is the centre of its block, up to this far from the point it was picked at

        # Spawn spots: found quickly, on the ground with room above, between the spawn distances, and out of view.
        spots = []
        began = time.perf_counter()
        for _ in range(200):
            spot = herd.find_spawn(player, feet, look, VoxelWalkers.SPAWN_ATTEMPTS, VoxelWalkers.SPAWN_MIN, VoxelWalkers.SPAWN_MAX)
            if spot is not None:
                spots.append(spot)
        info["spawn_search_ms"] = round((time.perf_counter() - began) * 1000.0 / 200, 4)
        info["spawn_found_of_200"] = len(spots)
        info["player"] = [round(game.pos[0], 2), round(game.pos[1], 2), round(game.pos[2], 2)]
        # With no view to avoid, a search finds a spot about half the time here: the spawn ring is mostly water on this shore, and the
        # searches that fail are retried by the game (SPAWN_SECONDS).
        anywhere = sum(1 for _ in range(200) if herd.find_spawn(player, feet, None, VoxelWalkers.SPAWN_ATTEMPTS,
                                                              VoxelWalkers.SPAWN_MIN, VoxelWalkers.SPAWN_MAX) is not None)
        info["spawn_found_anywhere_of_200"] = anywhere
        checks["spawn spots are found near the player"] = anywhere >= 100
        # Out of view, a third of the directions are not picked, so about two thirds of the searches find one.
        checks["spawn spots are found out of view"] = len(spots) >= 100
        checks["a spawn search takes under a millisecond"] = info["spawn_search_ms"] < 1.0
        checks["a spawn spot is between the spawn distances"] = all(
            VoxelWalkers.SPAWN_MIN - tolerance <= math.hypot(s[0] - player[0], s[2] - player[1]) <= VoxelWalkers.SPAWN_MAX + tolerance
            for s in spots)

        def out_of_view(s):
            if look is None:
                return True
            d = math.hypot(s[0] - player[0], s[2] - player[1])
            return d == 0 or ((s[0] - player[0]) * look[0] + (s[2] - player[1]) * look[1]) / d <= VoxelWalkers.SPAWN_LOOK_COS + 0.1
        checks["a spawn spot is out of the player's view"] = all(out_of_view(s) for s in spots)

        def on_ground(s):
            x, y, z = int(math.floor(s[0])), int(math.floor(s[1])), int(math.floor(s[2]))
            return world.is_solid(x, y - 1, z) and not world.is_solid(x, y, z)
        checks["a spawn spot stands on the ground, with air above"] = all(on_ground(s) for s in spots)

        # What a spawn and a frame of population management cost (a spawn loads the creature's body from a scene; a frame checks them all).
        began = time.perf_counter()
        probe = walkers.add_walker("pop_probe", herd.find_spawn(player, feet, None, 400, 2.0, 3.0) or (player[0], feet, player[1]))
        info["spawn_ms"] = round((time.perf_counter() - began) * 1000.0, 3)
        began = time.perf_counter()
        for _ in range(100):
            walkers.manage(0.0, player, feet, look, [], False, herd.live_count())
        info["manage_ms"] = round((time.perf_counter() - began) * 10.0, 4)
        walkers.remove(probe)
        cge.remove_body("pop_probe")

        # The population reaches its target from empty, one spawn every SPAWN_SECONDS, and does not go past it.
        for walker in list(walkers.walkers):
            walkers.remove(walker)
        target = 5
        for _ in range(200):
            walkers.manage(0.1, player, feet, look, [], False, target)
        checks["the population reaches its target and stops there"] = herd.live_count() == target

        # Protection: a creature far off that nothing holds is taken out; the same one is kept when it is held.
        held = herd.live_count()
        far_spot = herd.find_spawn(player, feet, None, 400, VoxelWalkers.DESPAWN_RANGE + 4.0, VoxelWalkers.DESPAWN_RANGE + 7.0)
        checks["a spot beyond the despawn range is found"] = far_spot is not None
        if far_spot is not None:
            far = walkers.add_walker("pop_far", far_spot)
            held = herd.live_count()
            walkers.manage(0.0, player, feet, look, ["pop_far"], False, held)
            checks["a creature the crosshair is on is kept, however far"] = herd.alive("pop_far") and cge.find_body("pop_far") is not None
            walkers.manage(0.0, player, feet, look, [], True, held)
            checks["no creature is taken out while an arrow flies"] = herd.alive("pop_far")
            walkers.manage(0.0, player, feet, look, [], False, held)
            checks["a far creature nothing holds is taken out, and its body with it"] = (
                not herd.alive("pop_far") and far not in walkers.walkers and cge.find_body("pop_far") is None)

        # A creature the player has just hit is held even when far (its hit is still in progress).
        hit_spot = herd.find_spawn(player, feet, None, 400, VoxelWalkers.DESPAWN_RANGE + 4.0, VoxelWalkers.DESPAWN_RANGE + 7.0)
        if hit_spot is not None:
            hit = walkers.add_walker("pop_hit", hit_spot)
            hit.hurt((hit.state()[0][0] - 1.0, hit.state()[0][2]), 0, False)   # no damage: only the flinch, which holds it
            held = herd.live_count()
            walkers.manage(0.0, player, feet, look, [], False, held)
            checks["a creature the player is hitting is not taken out"] = herd.alive("pop_hit")
            walkers.remove(hit)
            cge.remove_body("pop_hit")

        # A creature near the player is one the player is dealing with, so it stays while the herd is over its target and the
        # far ones go.
        near_spot = herd.find_spawn(player, feet, None, 400, 2.0, 3.0)
        if near_spot is not None:
            near = walkers.add_walker("pop_near", near_spot)
            walkers.manage(0.0, player, feet, look, [], False, 0)
            for _ in range(20):
                walkers.manage(0.0, player, feet, look, [], False, 0)
            checks["a creature beside the player stays while the far ones go"] = herd.alive("pop_near") and herd.live_count() == 1
            walkers.remove(near)
            cge.remove_body("pop_near")
    except Exception as error:   # the result must say what went wrong, not stop silently
        errors.append("%s: %s" % (type(error).__name__, error))
    finally:
        if walkers is not None:
            for walker in list(walkers.walkers):
                walkers.remove(walker)
                cge.remove_body(walker.name)

    os.makedirs("cg_control", exist_ok=True)
    with open(RESULT, "w") as f:
        json.dump({"check": checks, "errors": errors, "info": info}, f, indent=1)
    print("voxel population check: %d checks, %d failed, %d errors" % (
        len(checks), sum(1 for ok in checks.values() if not ok), len(errors)))


main()
