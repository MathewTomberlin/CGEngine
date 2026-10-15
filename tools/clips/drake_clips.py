import bpy, sys, json, os, math
from mathutils import Quaternion
# Writes the drake's clips from its actions in Drake.blend: walk, bite (attack) and die. The drake has no hurt clip, so a hit only
# flashes it (its kind in resources/voxel_creatures.json names these clips).
# Usage: blender <Drake.blend> --background --python tools/clips/drake_clips.py -- <out_dir> <model_path>
argv = sys.argv[sys.argv.index("--") + 1:]
out_dir, model_path = argv[0], argv[1]
fps = bpy.context.scene.render.fps
ACTIONS = {"AN_Drake_Walk": "drake_walk", "AN_Drake_Attack": "drake_attack", "AN_Drake_Death": "drake_die"}
written = []


def fcurves_of(action):
    """The action's curves: a Blender 4.4 action keeps them directly; Blender 5 keeps them in layers and channel bags."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    out = []
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                out.extend(bag.fcurves)
    return out

for action_name, clip_name in ACTIONS.items():
    action = bpy.data.actions[action_name]
    start, end = action.frame_range
    frames = list(range(int(start), int(end) + 1))
    duration = (end - start) / fps
    bones = {}
    for fc in fcurves_of(action):
        if not fc.data_path.startswith('pose.bones["'):
            continue
        bone = fc.data_path.split('"')[1]
        bones.setdefault(bone, {})[(fc.data_path.split(".")[-1], fc.array_index)] = fc
    channels = []
    # Rotations are written in degrees (the clip format's unit); Blender's curves are radians.
    for bone, curves in sorted(bones.items()):
        channel = {"bone": bone}
        keys = []
        for f in frames:
            if any(k[0] == "rotation_euler" for k in curves):
                e = [curves[("rotation_euler", i)].evaluate(f) if ("rotation_euler", i) in curves else 0.0 for i in range(3)]
                keys.append({"t": round((f - start) / fps, 5), "euler": [round(math.degrees(v), 5) for v in e]})
            else:
                q = Quaternion([curves[("rotation_quaternion", i)].evaluate(f) if ("rotation_quaternion", i) in curves else (1.0 if i == 0 else 0.0) for i in range(4)])
                e = q.to_euler("XYZ")
                keys.append({"t": round((f - start) / fps, 5), "euler": [round(math.degrees(e.x), 5), round(math.degrees(e.y), 5), round(math.degrees(e.z), 5)]})
        channel["rotation"] = keys
        channels.append(channel)
    clip = {"version": 1, "name": clip_name, "model": model_path, "durationSeconds": round(duration, 5), "channels": channels}
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, clip_name + ".json"), "w", encoding="utf-8") as f:
        json.dump(clip, f, indent=1)
    written.append((clip_name, round(duration, 3), len(channels)))
print("CLIPS", written)
