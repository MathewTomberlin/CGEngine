"""
Writes the slime's clips (roadmap 5.3) to resources/clips/: slime_hop (looping, used while the slime moves) and
slime_idle (looping breathing). Both key the single bone "Body" of resources/models/dungeon/slime_rig.fbx.
Scale keys are absolute; position keys are offsets from the rest pose, in tiles.

    python -I tools/clips/slime_clips.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "resources", "clips")
MODEL = "models/dungeon/slime_rig.fbx"


def clip(name, duration, channels):
    return {"version": 1, "name": name, "model": MODEL, "durationSeconds": duration, "channels": channels}


hop = clip("slime_hop", 1.0, [
    {"bone": "Body", "scale": [
        {"t": 0.0, "v": [1, 1, 1]}, {"t": 0.25, "v": [1.08, 1.08, 0.9]}, {"t": 0.5, "v": [0.94, 0.94, 1.12]},
        {"t": 0.75, "v": [1.08, 1.08, 0.9]}, {"t": 1.0, "v": [1, 1, 1]}],
     "position": [{"t": 0.0, "v": [0, 0, 0]}, {"t": 0.5, "v": [0, 0, 0.2]}, {"t": 1.0, "v": [0, 0, 0]}]},
])
idle = clip("slime_idle", 2.0, [
    {"bone": "Body", "scale": [
        {"t": 0.0, "v": [1, 1, 1]}, {"t": 1.0, "v": [1.03, 1.03, 0.97]}, {"t": 2.0, "v": [1, 1, 1]}]},
])

os.makedirs(OUT, exist_ok=True)
for c in (hop, idle):
    path = os.path.join(OUT, c["name"] + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(c, f, indent=2)
        f.write("\n")
    print("wrote", os.path.relpath(path, ROOT))
