import bpy, sys, json, os, math
from mathutils import Quaternion
# Writes the horse's hit clips (hurt, die) from the actions of the same names in the open horse rig. The walk, idle, attack and
# bound clips are written by horse_clips.py; this one leaves them alone.
# Usage: blender <file.blend> --background --python tools/clips/horse_hit_clips.py -- <out_dir> <scale> <model_path>
argv = sys.argv[sys.argv.index("--") + 1:]
out_dir, scale, model_path = argv[0], float(argv[1]), argv[2]
arm = bpy.data.objects["HorseRig"]
fps = bpy.context.scene.render.fps
ACTIONS = {"hurt": "horse_hurt", "die": "horse_die"}
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
            q = Quaternion([curves[("rotation_quaternion", i)].evaluate(f) if ("rotation_quaternion", i) in curves else (1.0 if i == 0 else 0.0) for i in range(4)])
            e = q.to_euler("XYZ")
            keys.append({"t": round((f - start) / fps, 5), "euler": [round(math.degrees(e.x), 5), round(math.degrees(e.y), 5), round(math.degrees(e.z), 5)]})
        channel["rotation"] = keys
        channels.append(channel)
    clip = {"version": 1, "name": clip_name, "model": model_path, "durationSeconds": round(duration, 5), "channels": channels}
    path = os.path.join(out_dir, clip_name + ".json")
    os.makedirs(out_dir, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(clip, f, indent=1)
    written.append((clip_name, round(duration, 3), len(channels)))
print("CLIPS", written)
