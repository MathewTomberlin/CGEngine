"""
A realistic quadruped for the voxel sample's creatures (roadmap 33.1): a smooth, subdivided body with a head, muzzle, ears, a
tail, four legs and hooves, rigged to an armature "CreatureRig" with automatic weights. The clips (walk, idle, bite, hurt, die) are
JSON files written by tools/clips/creature_clips.py, because the engine plays clip files; the FBX carries the rest pose.

Conventions match the dungeon's rigs (tools/blender/knight_rig.py): the front faces -Y, the feet are at z = 0, one unit is one
block. Only objects whose names start with "CreatureRig" are created or replaced, so the rest of the scene is left alone.

Run it inside Blender through the BlenderMCP addon's execute_code, with OUT_DIR set first:
    OUT_DIR = "C:/Users/admin/Documents/CGEngine/resources/models/voxel"
"""
import math
import os

import bpy
import bmesh
from mathutils import Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models", "voxel"))
FILE = "creature.fbx"
PREFIX = "CreatureRig"

# Remove only what an earlier run of this script made.
for obj in list(bpy.data.objects):
    if obj.name.startswith(PREFIX):
        bpy.data.objects.remove(obj, do_unlink=True)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith(PREFIX) and mesh.users == 0:
        bpy.data.meshes.remove(mesh)


def material(name, colour, roughness):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
    return mat


def ellipsoid(bm, centre, scale, segments=16, rings=10):
    """A sphere of radius 1 moved to centre and stretched by scale: the building block of the body."""
    result = bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)
    verts = result["verts"]
    bmesh.ops.scale(bm, vec=scale, verts=verts)
    bmesh.ops.translate(bm, vec=centre, verts=verts)


bm = bmesh.new()
# Body: a barrel of a torso, a chest that narrows into the neck.
ellipsoid(bm, (0.0, 0.10, 0.75), (0.28, 0.45, 0.25))
ellipsoid(bm, (0.0, -0.40, 0.90), (0.19, 0.22, 0.22))
# Head and muzzle.
ellipsoid(bm, (0.0, -0.62, 1.00), (0.13, 0.19, 0.14))
ellipsoid(bm, (0.0, -0.80, 0.95), (0.075, 0.10, 0.07))
# Ears, set back from the crown.
for side in (-1, 1):
    ellipsoid(bm, (side * 0.09, -0.52, 1.16), (0.028, 0.07, 0.12), segments=10, rings=6)
# Tail.
ellipsoid(bm, (0.0, 0.58, 0.86), (0.05, 0.10, 0.05), segments=10, rings=6)
ellipsoid(bm, (0.0, 0.68, 0.90), (0.035, 0.07, 0.035), segments=8, rings=5)
# Legs: a thigh and a shin each, the hoof at the foot.
for x in (-0.17, 0.17):
    for y in (-0.30, 0.30):
        ellipsoid(bm, (x, y, 0.62), (0.075, 0.075, 0.22), segments=12, rings=8)
        ellipsoid(bm, (x, y, 0.26), (0.055, 0.055, 0.22), segments=12, rings=8)
        ellipsoid(bm, (x, y, 0.035), (0.065, 0.075, 0.04), segments=12, rings=6)

mesh = bpy.data.meshes.new(PREFIX + "Mesh")
bm.to_mesh(mesh)
bm.free()
body = bpy.data.objects.new(PREFIX + "Body", mesh)
bpy.context.collection.objects.link(body)
body.data.materials.append(material(PREFIX + "Skin", (0.42, 0.31, 0.22), 0.7))
for poly in body.data.polygons:
    poly.use_smooth = True

# Smooth the silhouette: two levels of subdivision, applied so the skin is real geometry before it is weighted.
bpy.context.view_layer.objects.active = body
body.select_set(True)
subsurf = body.modifiers.new("Smooth", "SUBSURF")
subsurf.levels = 1
subsurf.render_levels = 2
bpy.ops.object.modifier_apply(modifier=subsurf.name)

# The armature: the spine from the pelvis to the head, the tail, and a leg from hip to hoof on each corner.
arm_data = bpy.data.armatures.new(PREFIX + "Armature")
arm = bpy.data.objects.new(PREFIX, arm_data)
bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
edit = arm_data.edit_bones


def bone(name, head, tail, parent=None):
    b = edit.new(name)
    b.head, b.tail = Vector(head), Vector(tail)
    if parent:
        b.parent = edit[parent]
    return b


bone("Root", (0.0, 0.0, 0.78), (0.0, 0.02, 0.80))
bone("Spine", (0.0, 0.02, 0.80), (0.0, -0.40, 0.92), "Root")
bone("Head", (0.0, -0.40, 0.92), (0.0, -0.70, 1.00), "Spine")
bone("Tail", (0.0, 0.50, 0.84), (0.0, 0.70, 0.90), "Root")
for x, side in ((-0.17, "R"), (0.17, "L")):
    for y, end in ((-0.30, "Front"), (0.30, "Back")):
        bone("Leg%s%s" % (end, side), (x, y, 0.74), (x, y, 0.02), "Root")
bpy.ops.object.mode_set(mode="OBJECT")

# Bind the skin to the bones with automatic weights, then place both under the one armature object.
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
arm.name = PREFIX
body.name = PREFIX + "Body"

os.makedirs(OUT_DIR, exist_ok=True)
for obj in bpy.context.view_layer.objects:
    obj.select_set(obj.name.startswith(PREFIX))
bpy.context.view_layer.objects.active = arm
path = os.path.join(OUT_DIR, FILE)
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True,
                         bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL")
print("exported", path, "bones", [b.name for b in arm_data.bones], "vertices", len(body.data.vertices))
