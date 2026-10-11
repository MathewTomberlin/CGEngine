"""
Rigged knight for the dungeon (roadmap 5.3). Same parts and proportions as dungeon_models.knight, each part rigidly
skinned (weight 1) to one bone of the armature "KnightRig":

  Root (hips)   belt
  Spine         torso
  Head          head, helmet, visor
  LegR, LegL    legs            (hip joints at the Root)
  ArmR          sword arm, sword        (-X side; the sword hand)
  ArmL          shield arm, shield      (+X side)

The armature faces -Y like the old model; feet are at z = 0. The action "Idle" holds the rest pose so the FBX carries
an animation (the engine's importer reads the skeleton from it). Walk and attack clips are authored as JSON in
tools/clips/knight_clips.py, because the engine plays clip files.

Run it inside Blender (Scripting tab, or the BlenderMCP addon's execute_code). Set OUT_DIR first. Only objects whose
names start with "KnightRig" are created or replaced.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models"))
FILE = "knight_rig.fbx"
BAKE_SPACE = globals().get("BAKE_SPACE", False)
ARM_NODETYPE = globals().get("ARM_NODETYPE", "NULL")
COLLECTION = bpy.context.scene.collection

PALETTE = {
    "dg_steel": (0.75, 0.78, 0.82), "dg_tunic": (0.15, 0.4, 1.0), "dg_skin": (1.0, 0.72, 0.5),
    "dg_gold": (1.0, 0.8, 0.15), "dg_shield": (0.95, 0.15, 0.15), "dg_slash": (1.0, 1.0, 0.7),
}

# Bone name -> (head, tail, parent) in Blender units (z up, feet at 0).
BONES = {
    "Root": ((0, 0, 0.26), (0, 0, 0.40), None),
    "Spine": ((0, 0, 0.40), (0, 0, 0.66), "Root"),
    "Head": ((0, 0, 0.66), (0, 0, 0.90), "Spine"),
    "LegR": ((-0.09, 0, 0.24), (-0.09, 0, 0.02), "Root"),
    "LegL": ((0.09, 0, 0.24), (0.09, 0, 0.02), "Root"),
    "ArmR": ((-0.23, 0, 0.55), (-0.23, 0, 0.29), "Spine"),
    "ArmL": ((0.23, 0, 0.55), (0.23, 0, 0.29), "Spine"),
}


def trs(loc, rot=(0, 0, 0), scale=(1, 1, 1)):
    r = Matrix.Rotation(math.radians(rot[2]), 4, "Z") @ Matrix.Rotation(math.radians(rot[1]), 4, "Y") @ \
        Matrix.Rotation(math.radians(rot[0]), 4, "X")
    return Matrix.Translation(loc) @ r @ Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))


def material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgb = PALETTE[name]
    mat.diffuse_color = (*rgb, 1.0)
    if hasattr(mat, "use_nodes"):
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.node_tree else None
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
            bsdf.inputs["Roughness"].default_value = 0.8
    return mat


def make_part(name, bone, mat_name, build, arm_obj):
    """build(bm) adds geometry to an empty bmesh; the part is then skinned to `bone` with weight 1."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    build(bm)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(material(mat_name))
    obj = bpy.data.objects.new(name, mesh)
    COLLECTION.objects.link(obj)
    group = obj.vertex_groups.new(name=bone)
    group.add(list(range(len(mesh.vertices))), 1.0, "REPLACE")
    obj.parent = arm_obj
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj
    return obj


def box(loc, size, rot=(0, 0, 0)):
    def build(bm):
        verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
        bmesh.ops.transform(bm, matrix=trs(loc, rot, size), verts=verts)
    return build


def sphere(loc, radius, scale=(1, 1, 1)):
    def build(bm):
        verts = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=1.0)["verts"]
        bmesh.ops.transform(bm, matrix=trs(loc, (0, 0, 0), [radius * s for s in scale]), verts=verts)
    return build


def cone(loc, r1, r2, depth, rot=(0, 0, 0), segments=8):
    def build(bm):
        verts = bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=r1, radius2=r2, depth=depth)["verts"]
        bmesh.ops.transform(bm, matrix=trs(loc, rot), verts=verts)
    return build


# ----- clear previous rig objects -----
for obj in list(bpy.data.objects):
    if obj.name.startswith("KnightRig"):
        bpy.data.objects.remove(obj, do_unlink=True)
for arm in list(bpy.data.armatures):
    if arm.name.startswith("KnightRig"):
        bpy.data.armatures.remove(arm)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith("KnightRig"):
        bpy.data.meshes.remove(mesh)
for act in list(bpy.data.actions):
    if act.name.startswith("KnightRig"):
        bpy.data.actions.remove(act)

# ----- armature -----
arm_data = bpy.data.armatures.new("KnightRig")
arm_obj = bpy.data.objects.new("KnightRig", arm_data)
COLLECTION.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode="EDIT")
for name, (head, tail, parent) in BONES.items():
    bone = arm_data.edit_bones.new(name)
    bone.head, bone.tail = Vector(head), Vector(tail)
    if parent:
        bone.parent = arm_data.edit_bones[parent]
bpy.ops.object.mode_set(mode="OBJECT")

# ----- skinned parts (one bone each) -----
make_part("KnightRigBelt", "Root", "dg_gold", box((0, 0, 0.27), (0.38, 0.24, 0.05)), arm_obj)
make_part("KnightRigTorso", "Spine", "dg_tunic", box((0, 0, 0.5), (0.36, 0.22, 0.30)), arm_obj)
make_part("KnightRigHead", "Head", "dg_skin", sphere((0, 0, 0.74), 0.14), arm_obj)
make_part("KnightRigHelmet", "Head", "dg_steel", sphere((0, 0.01, 0.78), 0.16, scale=(1, 1, 0.75)), arm_obj)
make_part("KnightRigVisor", "Head", "dg_steel", box((0, -0.12, 0.72), (0.2, 0.04, 0.03)), arm_obj)
make_part("KnightRigLegR", "LegR", "dg_steel", box((-0.09, 0, 0.12), (0.12, 0.14, 0.24)), arm_obj)
make_part("KnightRigLegL", "LegL", "dg_steel", box((0.09, 0, 0.12), (0.12, 0.14, 0.24)), arm_obj)
make_part("KnightRigArmR", "ArmR", "dg_tunic", box((-0.23, 0, 0.42), (0.1, 0.12, 0.26)), arm_obj)
make_part("KnightRigArmL", "ArmL", "dg_tunic", box((0.23, 0, 0.42), (0.1, 0.12, 0.26)), arm_obj)
make_part("KnightRigSwordBlade", "ArmR", "dg_steel", box((-0.26, -0.18, 0.62), (0.05, 0.03, 0.46)), arm_obj)
make_part("KnightRigSwordGuard", "ArmR", "dg_gold", box((-0.26, -0.18, 0.37), (0.16, 0.05, 0.04)), arm_obj)
make_part("KnightRigSwordGrip", "ArmR", "dg_gold", box((-0.26, -0.18, 0.31), (0.04, 0.04, 0.1)), arm_obj)
make_part("KnightRigShield", "ArmL", "dg_shield", cone((0.31, -0.03, 0.4), 0.17, 0.17, 0.04, rot=(0, 90, 0), segments=10), arm_obj)
make_part("KnightRigShieldBoss", "ArmL", "dg_gold", sphere((0.34, -0.03, 0.4), 0.05), arm_obj)

# ----- rest pose action (the FBX animation the importer needs) -----
arm_obj.animation_data_create()
action = bpy.data.actions.new("KnightRigIdle")
arm_obj.animation_data.action = action
for bone in arm_obj.pose.bones:
    bone.rotation_mode = "XYZ"
    bone.rotation_euler = (0, 0, 0)
    bone.keyframe_insert(data_path="rotation_euler", frame=1)
    bone.keyframe_insert(data_path="rotation_euler", frame=2)
bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, 2

# ----- export -----
os.makedirs(OUT_DIR, exist_ok=True)
for obj in bpy.context.view_layer.objects:
    obj.select_set(obj.name.startswith("KnightRig"))
bpy.context.view_layer.objects.active = arm_obj
path = os.path.join(OUT_DIR, FILE)
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True,
                         bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL",
                         bake_space_transform=BAKE_SPACE, armature_nodetype=ARM_NODETYPE)
print("exported", path, "bones", [b.name for b in arm_data.bones])
