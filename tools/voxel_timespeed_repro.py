"""
Runs the console's "timespeed 0.5" from inside the running voxel game (roadmap 23.4): the command's own result is written to
cg_control/timespeed_repro.json, and the game runs on, so tools/voxel_timespeed_repro.ps1 can see whether the process survives.
Run through the control channel (run_script) in a Debug build.
"""
import gc
import json
import os

COMMAND = os.environ.get("VOXEL_REPRO_COMMAND", "timespeed 0.5")
if os.path.exists("cg_control/repro_command.txt"):
    with open("cg_control/repro_command.txt") as f:
        COMMAND = f.read().strip()

import VoxelGame

games = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"]
result = {"found": bool(games)}
if games:
    game = games[0]
    # The same path as typing it: the console is open (so its log is drawn), the line is typed, and Enter submits it.
    game.console.open_console()
    result["opened"] = game.console.open
    result["command"] = COMMAND
    result["before"] = game.time_speed
    game.console.line = COMMAND
    game.console.submit()
    # the console stays open after the command, as it does when a player keeps typing
    result["output"] = game.console.log[-2:]
    result["after"] = game.time_speed
os.makedirs("cg_control", exist_ok=True)
with open("cg_control/timespeed_repro.json", "w") as f:
    json.dump(result, f, indent=2)
print("timespeed repro:", result)
