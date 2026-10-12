"""
Writes the archer's clips (roadmap 6.5) to resources/clips/: archer_idle and archer_walk (both loop) and archer_draw (played
once when it shoots, and holds its last pose). Each keys the bones of resources/models/dungeon/archer_rig.fbx (see
tools/blender/archer_rig.py): Torso, Head and Arm. Angles are Euler degrees relative to each bone's rest pose, and
positions are offsets in tiles. Edit the numbers below and rerun:

    python -I tools/clips/archer_clips.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "clips")
MODEL = "models/dungeon/archer_rig.fbx"


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


# Idle: the body breathes and the head looks about a little. Loops, so the last key equals the first.
idle = clip("archer_idle", 3.0, [
    bone("Torso", position=[(0.0, (0, 0, 0)), (1.5, (0, 0.012, 0)), (3.0, (0, 0, 0))]),
    bone("Head", rotation=[(0.0, (0, 0, 0)), (1.0, (0, 14, 0)), (2.0, (0, -10, 0)), (3.0, (0, 0, 0))]),
])

# Walk: a bob on each step and a little sway of the bow arm. Loops.
walk = clip("archer_walk", 0.8, [
    bone("Torso", position=[(0.0, (0, 0, 0)), (0.2, (0, 0.04, 0)), (0.4, (0, 0, 0)), (0.6, (0, 0.04, 0)), (0.8, (0, 0, 0))],
         rotation=[(0.0, (-4, 0, 0)), (0.4, (4, 0, 0)), (0.8, (-4, 0, 0))]),
    bone("Arm", rotation=[(0.0, (12, 0, 0)), (0.4, (-12, 0, 0)), (0.8, (12, 0, 0))]),
    bone("Head", rotation=[(0.0, (3, 0, 0)), (0.4, (-3, 0, 0)), (0.8, (3, 0, 0))]),
])

# Draw: the bow arm swings back and the body leans into it, then the string is released. Played once; it holds the
# last pose, so the game plays idle or walk again once the shot is away.
draw = clip("archer_draw", 0.6, [
    bone("Arm", rotation=[(0.0, (0, 0, 0)), (0.35, (0, -40, 0)), (0.45, (0, -40, 0)), (0.6, (0, 0, 0))]),
    bone("Torso", rotation=[(0.0, (0, 0, 0)), (0.35, (0, -10, 0)), (0.6, (0, 0, 0))]),
])

os.makedirs(OUT, exist_ok=True)
for c in (idle, walk, draw):
    path = os.path.join(OUT, c["name"] + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(c, f, indent=2)
        f.write("\n")
    print("wrote", os.path.relpath(path, ROOT))
