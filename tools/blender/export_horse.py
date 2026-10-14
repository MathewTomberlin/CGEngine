import bpy, sys, math, os
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
out_fbx, scale = argv[0], float(argv[1])
arm = bpy.data.objects["HorseRig"]
root = arm.pose.bones["root"].head if False else arm.data.bones["root"].head_local
head = arm.data.bones["head"].head_local
tail = arm.data.bones["tail_1"].head_local
rear = arm.data.bones["hind_hoof_l"].head_local
print("HEAD_CHECK root", tuple(round(v, 3) for v in root), "head", tuple(round(v, 3) for v in head), "tail", tuple(round(v, 3) for v in tail))
print("FPS", bpy.context.scene.render.fps, "ACTIONS", [(a.name, tuple(int(v) for v in a.frame_range)) for a in bpy.data.actions])
# Head toward -Y is the rigs' convention. The horse's head should be away from its tail.
facing = head.y - tail.y
print("FACING_Y", round(facing, 3))
bpy.ops.object.select_all(action="DESELECT")
keep = ["HorseRig", "Horse"]
for o in bpy.data.objects:
    o.select_set(o.name in keep)
bpy.context.view_layer.objects.active = arm
rotate = facing > 0.0   # the head is toward +Y: turn the whole model half a turn about Z
if rotate:
    for o in (arm, bpy.data.objects["Horse"]):
        o.rotation_euler[2] += math.pi
os.makedirs(os.path.dirname(out_fbx), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=True, object_types={"ARMATURE", "MESH"},
                         global_scale=scale, apply_scale_options="FBX_SCALE_NONE", apply_unit_scale=True,
                         bake_space_transform=True, add_leaf_bones=False, bake_anim=False,
                         use_armature_deform_only=False, mesh_smooth_type="FACE")
print("EXPORTED", out_fbx, "rotated" if rotate else "as-is", "scale", scale)
