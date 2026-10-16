"""
Rigged slime for the dungeon (roadmap 5.3). The body and eyes are skinned rigidly to one bone, "Body", whose head is
at the slime's feet. The engine plays clips on the bone (tools/clips/slime_clips.py).

Same conventions as knight_rig.py: feet at z = 0 in Blender, the rig is turned into Y-up geometry, and the armature
object is turned +90 degrees about X so the exporter's -90 degree rotation cancels out. Front faces -Y in Blender,
which becomes +Z in the engine.

Run it inside Blender (Scripting tab, or the BlenderMCP addon's execute_code). Set OUT_DIR first. Only objects whose
names start with "SlimeRig" are created or replaced.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models"))
FILE = "slime_rig.fbx"
COLLECTION = bpy.context.scene.collection

PALETTE = {"dg_slime": (0.25, 1.0, 0.35), "dg_eye": (0.1, 0.1, 0.7)}
ORIENT = Matrix.Rotation(math.radians(-90), 4, "X")


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
            bsdf.inputs["Roughness"].default_value = 0.6
    return mat


def make_part(name, mat_name, build, arm_obj):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    build(bm)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(material(mat_name))
    obj = bpy.data.objects.new(name, mesh)
    COLLECTION.objects.link(obj)
    group = obj.vertex_groups.new(name="Body")
    group.add(list(range(len(mesh.vertices))), 1.0, "REPLACE")
    obj.parent = arm_obj
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj
    return obj


def sphere(loc, radius, scale=(1, 1, 1), segments=14, rings=8):
    def build(bm):
        verts = bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)["verts"]
        bmesh.ops.transform(bm, matrix=trs(loc, (0, 0, 0), [radius * s for s in scale]), verts=verts)
        for face in bm.faces:
            face.smooth = True
    return build


for obj in list(bpy.data.objects):
    if obj.name.startswith("SlimeRig"):
        bpy.data.objects.remove(obj, do_unlink=True)
for arm in list(bpy.data.armatures):
    if arm.name.startswith("SlimeRig"):
        bpy.data.armatures.remove(arm)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith("SlimeRig"):
        bpy.data.meshes.remove(mesh)
for act in list(bpy.data.actions):
    if act.name.startswith("SlimeRig"):
        bpy.data.actions.remove(act)

arm_data = bpy.data.armatures.new("SlimeRig")
arm_obj = bpy.data.objects.new("SlimeRig", arm_data)
COLLECTION.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode="EDIT")
bone = arm_data.edit_bones.new("Body")
bone.head, bone.tail = Vector((0, 0, 0)), Vector((0, 0, 0.4))
bpy.ops.object.mode_set(mode="OBJECT")

make_part("SlimeRigBody", "dg_slime", sphere((0, 0, 0.2), 0.32, scale=(1, 1, 0.68)), arm_obj)
make_part("SlimeRigEyeR", "dg_eye", sphere((0.1, -0.27, 0.27), 0.05), arm_obj)
make_part("SlimeRigEyeL", "dg_eye", sphere((-0.1, -0.27, 0.27), 0.05), arm_obj)

# Orient for the engine (see knight_rig.py): geometry and bone rest turned Y-up, armature object turned back.
for obj in list(COLLECTION.objects):
    if obj.name.startswith("SlimeRig") and obj.type == "MESH":
        obj.data.transform(ORIENT)
bpy.ops.object.mode_set(mode="EDIT")
for b in arm_data.edit_bones:
    b.head = ORIENT.to_3x3() @ b.head
    b.tail = ORIENT.to_3x3() @ b.tail
bpy.ops.object.mode_set(mode="OBJECT")
arm_obj.rotation_euler = (math.radians(90), 0, 0)

arm_obj.animation_data_create()
action = bpy.data.actions.new("SlimeRigIdle")
arm_obj.animation_data.action = action
for pb in arm_obj.pose.bones:
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = (0, 0, 0)
    pb.keyframe_insert(data_path="rotation_euler", frame=1)
    pb.keyframe_insert(data_path="rotation_euler", frame=2)
bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, 2

os.makedirs(OUT_DIR, exist_ok=True)
for obj in bpy.context.view_layer.objects:
    obj.select_set(obj.name.startswith("SlimeRig"))
bpy.context.view_layer.objects.active = arm_obj
path = os.path.join(OUT_DIR, FILE)
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True,
                         bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL")
print("exported", path, "bones", [b.name for b in arm_data.bones])
