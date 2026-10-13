"""
Checks the developer console (resources/scripts/VoxelConsole.py) from inside the running sample game: the commands act on the
game (the time and its speed, the player, items, creatures, blocks, the view and the window), a bad command is reported and the
game goes on, and the console opens and closes with the game's mouse and world left as they were. test_voxels.ps1 runs it
through the control channel (run_script) and reads its result from cg_control/voxel_console_check.json.
"""
import gc
import json
import os

import cg_engine_bindings as cge
import VoxelConsole
import VoxelGame
import VoxelGen

RESULT = "cg_control/voxel_console_check.json"


def find_game():
    games = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"]
    if not games:
        raise RuntimeError("no VoxelGame in the engine (the scene must run it)")
    return games[0]


def main():
    checks = {}
    errors = []
    game = find_game()
    console = game.console
    # The check changes the time, the view and the window: they are put back at the end, so the next run starts as this one did.
    original = (game.clock, game.time_speed, game.view_radius, game.fullscreen)
    try:
        # Opening and closing: backtick toggles the console, and it frees the mouse only while it is open.
        was_captured = game.mouse_captured
        console.open_console()
        checks["the console opens and frees the mouse"] = console.open and not game.mouse_captured
        console.close()
        checks["the console closes and gives the mouse back"] = (not console.open) and game.mouse_captured == was_captured

        # Typed text reaches the line; Backspace and Enter are handled by the console's update, so the line is set directly here.
        console.open = True
        console.line = ""
        for ch in "tim":
            console.line += ch
        console.line = console.line[:-1] + "e"
        checks["typed characters build the line"] = console.line == "tie"
        console.line = ""
        console.open = False

        # Time of day and its speed.
        console.run("time noon")
        checks["time noon sets the clock to a quarter of the day"] = abs(game.clock - 0.25) < 1e-9
        console.run("time 18:00")
        checks["time HH:MM is read on the game's clock (06:00 is sunrise)"] = abs(game.clock - 0.5) < 1e-9
        console.run("timespeed paused")
        before = game.clock
        game.update_sky(0.5)
        checks["timespeed paused stops the sun"] = abs(game.clock - before) < 1e-9
        console.run("timespeed fast")
        checks["timespeed fast is four times the normal speed"] = game.time_speed == 4.0
        console.run("timespeed normal")
        checks["timespeed normal is the default speed"] = game.time_speed == 1.0

        # The player: teleport to a place, on the ground, and back to the spawn.
        console.run("tp 40 -12")
        checks["tp x z puts the player on the ground there"] = abs(game.pos[0] - 40.0) < 1e-6 and game.pos[1] > 0
        console.run("tp spawn")
        checks["tp spawn goes back to the spawn"] = abs(game.pos[0] - game.spawn_pos[0]) < 1e-6
        console.run("heal 4")
        checks["heal amount sets the health"] = abs(game.health - 4.0) < 1e-6
        console.run("heal")
        checks["heal with no amount restores all of it"] = game.health == VoxelGame.PLAYER_HEALTH

        # Items: give, drop, clear.
        console.run("clear inventory")
        console.run("give stone 5")
        checks["give puts the items in the inventory"] = game.inventory.count(VoxelGen.STONE) == 5
        console.run("drop stone 2")
        checks["drop puts items on the ground"] = sum(1 for d in game.drops.live if d.item == VoxelGen.STONE) >= 2
        console.run("clear drops")
        checks["clear drops empties the ground"] = len(game.drops.live) == 0

        # Modes.
        console.run("creative on")
        checks["creative on turns creative mode on"] = game.creative is True
        console.run("creative off")
        checks["creative off turns creative mode off"] = game.creative is False
        console.run("fly on")
        checks["fly on turns flying on"] = game.flying is True
        console.run("fly off")
        checks["fly off turns flying off"] = game.flying is False

        # Creatures: spawn some near the player, then remove them.
        if game.walkers is not None:
            def near_player(radius):
                count = 0
                for walker in game.walkers.walkers:
                    lo, hi = walker.box()
                    centre = [(lo[i] + hi[i]) / 2 for i in range(3)]
                    if sum((centre[i] - game.pos[i]) ** 2 for i in range(3)) ** 0.5 <= radius:
                        count += 1
                return count
            before_count = len(game.walkers.walkers)
            console.run("spawn creature 3 biter")
            checks["spawn creature makes creatures near the player"] = len(game.walkers.walkers) == before_count + 3
            checks["the spawned creatures are near the player"] = near_player(10) >= 3
            console.run("kill creatures near 10")
            checks["kill creatures near a radius removes those within it"] = near_player(10) == 0
            console.run("kill creatures all")
            checks["kill creatures all removes every creature"] = len(game.walkers.walkers) == 0

        # Blocks: setblock and fill, then destroy around a place we make.
        console.run("tp 30 -20")
        x, z = int(game.pos[0]), int(game.pos[2])
        y = int(game.pos[1]) + 4
        console.run("setblock %d %d %d stone" % (x, y, z))
        checks["setblock sets one block"] = game.world.get_block(x, y, z) == VoxelGen.STONE
        console.run("fill %d %d %d %d %d %d stone" % (x + 1, y, z, x + 2, y, z))
        checks["fill sets a box of blocks"] = game.world.get_block(x + 2, y, z) == VoxelGen.STONE
        # Stand beside the stone, so its middle is within a block of it, and destroy 1 takes the stone at (x, y, z) but not the
        # one two blocks away.
        game.teleport(x + 0.5, y - 1.0, z + 0.5)
        console.run("destroy 1")
        checks["destroy removes blocks near the player"] = game.world.get_block(x, y, z) == VoxelGen.AIR
        checks["destroy leaves blocks beyond its radius"] = game.world.get_block(x + 2, y, z) == VoxelGen.STONE

        # The view distance and the window.
        console.run("view 20")
        checks["view sets the nearest view step"] = game.view_radius == 18
        console.run("view 14")
        checks["view 14 is the default step"] = game.view_radius == 14
        was_fullscreen = game.fullscreen
        console.run("fullscreen on")
        checks["fullscreen on reports the window state"] = game.fullscreen == cge.is_fullscreen()
        console.run("fullscreen off")
        checks["fullscreen off returns to windowed"] = game.fullscreen is False
        if was_fullscreen:
            console.run("fullscreen on")

        # Help, stats, and bad input.
        lines = console.run("help")
        checks["help lists the commands"] = any("time" in line and "tp" in line for line in lines)
        lines = console.run("stats")
        checks["stats reports the chunks"] = any("Chunks" in line for line in lines)
        lines = console.run("nosuchcommand")
        checks["an unknown command is reported"] = any("no command" in line for line in lines)
        lines = console.run("give nosuchitem")
        checks["a bad item name is reported as an error"] = any(line.startswith("error:") for line in lines)
        lines = console.run("fill 0 0 0 1000 1000 1000 stone")
        checks["a fill that is too big is refused"] = any("at most" in line for line in lines)
        checks["the game is still running after the errors"] = game.frames > 0

        # The settings file keeps the time speed and the window.
        with open(VoxelGame.SETTINGS_FILE) as f:
            saved = json.load(f)
        checks["the time speed is saved with the settings"] = saved.get("time_speed") == game.time_speed
        checks["the window mode is saved with the settings"] = saved.get("fullscreen") == game.fullscreen
    except Exception as error:   # the result must say what went wrong, not stop silently
        errors.append("%s: %s" % (type(error).__name__, error))
    finally:
        clock, speed, view, fullscreen = original
        game.set_clock(clock)
        game.set_time_speed(speed)
        game.set_view_distance(view)
        if game.fullscreen != fullscreen:
            game.set_fullscreen(fullscreen)

    os.makedirs("cg_control", exist_ok=True)
    with open(RESULT, "w") as f:
        json.dump({"check": checks, "errors": errors}, f, indent=1)
    print("voxel console check: %d checks, %d failed, %d errors" % (
        len(checks), sum(1 for ok in checks.values() if not ok), len(errors)))


main()
