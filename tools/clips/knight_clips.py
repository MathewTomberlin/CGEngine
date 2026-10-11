"""
Writes the knight's animation clips (roadmap 5.3) to resources/clips/: knight_idle, knight_walk, knight_attack.

Each clip keyframes the bones of resources/models/dungeon/knight_rig.fbx (see tools/blender/knight_rig.py). Angles are
Euler degrees relative to each bone's rest pose. Edit the numbers below and rerun:

    python -I tools/clips/knight_clips.py

Walk and idle loop, so their last key equals their first. Attack is played once.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "clips")
MODEL = "models/dungeon/knight_rig.fbx"


def rot(bone, keys):
    """keys: list of (seconds, (x, y, z) degrees)."""
    return {"bone": bone, "rotation": [{"t": t, "euler": list(e)} for t, e in keys]}


def clip(name, duration, channels):
    return {"version": 1, "name": name, "model": MODEL, "durationSeconds": duration, "channels": channels}


# Legs swing about the hips; the arms swing against the legs; the torso rises and falls a little.
WALK_STRIDE = 25.0
WALK_ARM = 20.0
walk = clip("knight_walk", 1.0, [
    rot("LegR", [(0.0, (0, 0, 0)), (0.5, (WALK_STRIDE, 0, 0)), (1.0, (0, 0, 0))]),
    rot("LegL", [(0.0, (0, 0, 0)), (0.5, (-WALK_STRIDE, 0, 0)), (1.0, (0, 0, 0))]),
    rot("ArmR", [(0.0, (0, 0, 0)), (0.5, (-WALK_ARM, 0, 0)), (1.0, (0, 0, 0))]),
    rot("ArmL", [(0.0, (0, 0, 0)), (0.5, (WALK_ARM, 0, 0)), (1.0, (0, 0, 0))]),
    rot("Spine", [(0.0, (0, 0, 0)), (0.25, (2, 0, 0)), (0.5, (0, 0, 0)), (0.75, (2, 0, 0)), (1.0, (0, 0, 0))]),
])

# The sword arm raises and comes down; the torso leans into the blow.
attack = clip("knight_attack", 0.45, [
    rot("ArmR", [(0.0, (0, 0, 0)), (0.15, (95, 0, 0)), (0.45, (0, 0, 0))]),
    rot("Spine", [(0.0, (0, 0, 0)), (0.15, (12, 0, 0)), (0.45, (0, 0, 0))]),
])

# Breathing: the torso and head rise slightly.
idle = clip("knight_idle", 2.0, [
    rot("Spine", [(0.0, (0, 0, 0)), (1.0, (2, 0, 0)), (2.0, (0, 0, 0))]),
    rot("Head", [(0.0, (0, 0, 0)), (1.0, (-2, 0, 0)), (2.0, (0, 0, 0))]),
])

os.makedirs(OUT, exist_ok=True)
for c in (idle, walk, attack):
    path = os.path.join(OUT, c["name"] + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(c, f, indent=2)
        f.write("\n")
    print("wrote", os.path.relpath(path, ROOT))
