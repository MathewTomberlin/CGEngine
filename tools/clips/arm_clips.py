import bpy, sys, json, os, math
from mathutils import Quaternion
# Run in Blender: blender -b Arms.blend --python tools/clips/arm_clips.py -- resources/clips
# Writes arm_attack_L.json and arm_attack_R.json: each arm's rig, its attack action, as clip channels (degrees, seconds).
argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
MODELS = {"L": "models/viewmodel/viewarm_left.fbx", "R": "models/viewmodel/viewarm_right.fbx"}


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


fps = bpy.context.scene.render.fps
os.makedirs(out_dir, exist_ok=True)
for side in ("L", "R"):
    action = bpy.data.actions["arm_attack_" + side]
    start, end = action.frame_range
    frames = list(range(int(start), int(end) + 1))
    bones = {}
    for fc in fcurves_of(action):
        if not fc.data_path.startswith('pose.bones["'):
            continue
        bone = fc.data_path.split('"')[1]
        bones.setdefault(bone, {})[(fc.data_path.split(".")[-1], fc.array_index)] = fc
    channels = []
    for bone, curves in sorted(bones.items()):
        keys = []
        for f in frames:
            q = Quaternion([curves[("rotation_quaternion", i)].evaluate(f) if ("rotation_quaternion", i) in curves else (1.0 if i == 0 else 0.0) for i in range(4)])
            e = q.to_euler("XYZ")
            keys.append({"t": round((f - start) / fps, 5), "euler": [round(math.degrees(e.x), 5), round(math.degrees(e.y), 5), round(math.degrees(e.z), 5)]})
        channels.append({"bone": bone, "rotation": keys})
    clip = {"version": 1, "name": "arm_attack_" + side, "model": MODELS[side], "durationSeconds": round((end - start) / fps, 5), "channels": channels}
    with open(os.path.join(out_dir, "arm_attack_" + side + ".json"), "w", encoding="utf-8") as f:
        json.dump(clip, f, indent=1)
    print("CLIP", side, len(channels), "bones", round((end - start) / fps, 3), "s")
