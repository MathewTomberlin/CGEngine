"""
Two-bone test rig for the animation pipeline (roadmap 5.3). Builds an armature "RigTest" with bones Root and Arm,
two boxes skinned to them, and an action "Idle" holding the rest pose. Exports resources/models/dungeon/rig_test.fbx.

Run it inside Blender (Scripting tab, or send to the BlenderMCP addon's execute_code). Set OUT_DIR before running.
Only objects named "RigTest*" are created or replaced; the rest of the scene is left alone.
"""
import math
import os

import bpy
from mathutils import Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models"))
FILE = "rig_test.fbx"

for obj in list(bpy.data.objects):
    if obj.name.startswith("RigTest"):
        bpy.data.objects.remove(obj, do_unlink=True)
for arm in list(bpy.data.armatures):
    if arm.name.startswith("RigTest"):
        bpy.data.armatures.remove(arm)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith("RigTest"):
        bpy.data.meshes.remove(mesh)
for act in list(bpy.data.actions):
    if act.name.startswith("RigTest"):
        bpy.data.actions.remove(act)

collection = bpy.context.scene.collection
arm_data = bpy.data.armatures.new("RigTest")
arm_obj = bpy.data.objects.new("RigTest", arm_data)
collection.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
arm_obj.select_set(True)

bpy.ops.object.mode_set(mode="EDIT")
root = arm_data.edit_bones.new("Root")
root.head, root.tail = Vector((0, 0, 0)), Vector((0, 0, 0.5))
arm = arm_data.edit_bones.new("Arm")
arm.head, arm.tail = Vector((0, 0, 0.5)), Vector((0, 0, 1.0))
arm.parent = root
bpy.ops.object.mode_set(mode="OBJECT")


def box(name, z0, z1, group):
    mesh = bpy.data.meshes.new(name)
    half = 0.15
    verts = [(-half, -half, z0), (half, -half, z0), (half, half, z0), (-half, half, z0),
             (-half, -half, z1), (half, -half, z1), (half, half, z1), (-half, half, z1)]
    faces = [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    group_obj = obj.vertex_groups.new(name=group)
    group_obj.add(list(range(8)), 1.0, "REPLACE")
    obj.parent = arm_obj
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj
    return obj


box("RigTestBody", 0.0, 0.5, "Root")
box("RigTestArm", 0.5, 1.0, "Arm")

# Rest-pose action, so the exported FBX carries an animation (the engine's importer reads the skeleton from it).
arm_obj.animation_data_create()
action = bpy.data.actions.new("RigTestIdle")
arm_obj.animation_data.action = action
pose = arm_obj.pose.bones["Arm"]
pose.rotation_mode = "XYZ"
for frame in (1, 2):
    pose.rotation_euler = (0.0, 0.0, 0.0)
    pose.keyframe_insert(data_path="rotation_euler", frame=frame)
bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, 2

os.makedirs(OUT_DIR, exist_ok=True)
for obj in bpy.context.view_layer.objects:
    obj.select_set(obj.name.startswith("RigTest"))
bpy.context.view_layer.objects.active = arm_obj
path = os.path.join(OUT_DIR, FILE)
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True,
                         bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL")
print("exported", path, "bones", [b.name for b in arm_data.bones])
