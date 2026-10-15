import bpy, sys, math, os
argv = sys.argv[sys.argv.index("--") + 1:]
out_fbx, scale = argv[0], float(argv[1])
arm = bpy.data.objects["ARM-Drake"]
geo = bpy.data.objects["GEO-Drake"]
head = arm.data.bones["head"].head_local
tail = arm.data.bones["tail_1"].head_local
# Head toward -Y is the rigs' convention (as the horse's). A drake whose head is toward +Y is turned half a turn about Z.
facing = head.y - tail.y
print("FACING_Y", round(facing, 3))
# Only the drake's armature and mesh are exported; the selection and the rotation are put back afterwards.
selected = [o for o in bpy.data.objects if o.select_get()]
active = bpy.context.view_layer.objects.active
bpy.ops.object.select_all(action="DESELECT")
for o in (arm, geo):
    o.select_set(True)
bpy.context.view_layer.objects.active = arm
turn = math.pi if facing > 0.0 else 0.0
old_rotation = arm.rotation_euler.copy(), geo.rotation_euler.copy()
arm.rotation_euler[2] += turn
geo.rotation_euler[2] += turn
os.makedirs(os.path.dirname(out_fbx), exist_ok=True)
try:
    bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=True, object_types={"ARMATURE", "MESH"},
                             global_scale=scale, apply_scale_options="FBX_SCALE_NONE", apply_unit_scale=True,
                             bake_space_transform=True, add_leaf_bones=False, bake_anim=True,
                             bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                             bake_anim_force_startend_keying=True,
                             use_armature_deform_only=False, mesh_smooth_type="FACE")
finally:
    arm.rotation_euler, geo.rotation_euler = old_rotation
    bpy.ops.object.select_all(action="DESELECT")
    for o in selected:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active
print("EXPORTED", out_fbx, "turned" if turn else "as-is", "scale", scale)
