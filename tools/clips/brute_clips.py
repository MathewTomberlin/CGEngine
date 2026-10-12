"""
Writes the brute's clips (roadmap 6.5) to resources/clips/: brute_idle (a slow breath) and brute_walk (a heavy stride). Both
loop. They key the bones of resources/models/dungeon/brute_rig.fbx (see tools/blender/brute_rig.py): LegL, LegR, Body,
ArmL and ArmR. Angles are Euler degrees relative to each bone's rest pose, and positions are offsets in tiles. Edit the
numbers below and rerun:

    python -I tools/clips/brute_clips.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "clips")
MODEL = "models/dungeon/brute_rig.fbx"


def bone(name, position=None, rotation=None):
    """One channel per bone: its position keys, its rotation keys, or both."""
    channel = {"bone": name}
    if position:
        channel["position"] = [{"t": t, "v": list(v)} for t, v in position]
    if rotation:
        channel["rotation"] = [{"t": t, "euler": list(e)} for t, e in rotation]
    return channel


def clip(name, duration, channels):
    return {"version": 1, "name": name, "model": MODEL, "durationSeconds": duration, "channels": channels}


# Idle: the great body rises and falls, and the arms hang and sway a little. Loops.
idle = clip("brute_idle", 3.0, [
    bone("Body", position=[(0.0, (0, 0, 0)), (1.5, (0, 0, 0.02)), (3.0, (0, 0, 0))]),
    bone("ArmL", rotation=[(0.0, (0, 0, 0)), (1.5, (4, 0, 0)), (3.0, (0, 0, 0))]),
    bone("ArmR", rotation=[(0.0, (0, 0, 0)), (1.5, (-4, 0, 0)), (3.0, (0, 0, 0))]),
])

# Walk: the legs swing about the hips, the arms swing against them, and the body rocks. Loops.
walk = clip("brute_walk", 0.9, [
    bone("LegL", rotation=[(0.0, (25, 0, 0)), (0.45, (-25, 0, 0)), (0.9, (25, 0, 0))]),
    bone("LegR", rotation=[(0.0, (-25, 0, 0)), (0.45, (25, 0, 0)), (0.9, (-25, 0, 0))]),
    bone("ArmL", rotation=[(0.0, (-18, 0, 0)), (0.45, (18, 0, 0)), (0.9, (-18, 0, 0))]),
    bone("ArmR", rotation=[(0.0, (18, 0, 0)), (0.45, (-18, 0, 0)), (0.9, (18, 0, 0))]),
    bone("Body", position=[(0.0, (0, 0, 0)), (0.225, (0, 0, 0.04)), (0.45, (0, 0, 0)), (0.675, (0, 0, 0.04)), (0.9, (0, 0, 0))]),
])

os.makedirs(OUT, exist_ok=True)
for c in (idle, walk):
    path = os.path.join(OUT, c["name"] + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(c, f, indent=2)
        f.write("\n")
    print("wrote", os.path.relpath(path, ROOT))
