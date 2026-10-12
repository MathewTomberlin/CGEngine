"""
Rigged brute for the dungeon (roadmap 6.5). Five bones, each part skinned rigidly to one of them: "LegL" and "LegR" (the
legs, pivoting at the hips), "Body" (the hulking body, head, horns and eyes), and "ArmL" and "ArmR" (the arms; the club
is held by ArmR). The engine plays clips on these bones (tools/clips/brute_clips.py).

Same conventions as archer_rig.py and slime_rig.py: feet at z = 0 in Blender, the rig is turned into Y-up geometry, and
the armature object is turned +90 degrees about X so the exporter's -90 degree rotation cancels out. Front faces -Y in
Blender, which becomes +Z in the engine. The part shapes match the brute in dungeon_models.py.

Run it inside Blender (Scripting tab, or the BlenderMCP addon's execute_code). Set OUT_DIR first. Only objects whose
names start with "BruteRig" are created or replaced.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models"))
FILE = "brute_rig.fbx"
COLLECTION = bpy.context.scene.collection

PALETTE = {
    "dg_brute": (0.45, 0.7, 0.6),
    "dg_wood": (0.9, 0.5, 0.15),
    "dg_horn": (1.0, 0.95, 0.75),
    "dg_eye": (0.1, 0.1, 0.7),
}
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
            bsdf.inputs["Roughness"].default_value = 0.8
            bsdf.inputs["Specular IOR Level"].default_value = 0.1
    return mat


class Part:
    """Primitives collected into one bmesh, the same shapes as dungeon_models.Builder."""

    def __init__(self):
        self.bm = bmesh.new()

    def _finish(self, verts, matrix, smooth=False):
        bmesh.ops.transform(self.bm, matrix=matrix, verts=verts)
        for vert in verts:
            for face in vert.link_faces:
                face.smooth = smooth

    def box(self, loc, size, rot=(0, 0, 0)):
        verts = bmesh.ops.create_cube(self.bm, size=1.0)["verts"]
        self._finish(verts, trs(loc, rot, size))

    def sphere(self, loc, radius, scale=(1, 1, 1), segments=10, rings=6):
        verts = bmesh.ops.create_uvsphere(self.bm, u_segments=segments, v_segments=rings, radius=1.0)["verts"]
        self._finish(verts, trs(loc, (0, 0, 0), [radius * s for s in scale]), smooth=True)

    def cone(self, loc, r1, r2, depth, rot=(0, 0, 0), segments=8):
        verts = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=r1, radius2=r2, depth=depth)["verts"]
        self._finish(verts, trs(loc, rot, (1, 1, 1)))

    def torus(self, loc, major, minor, rot=(0, 0, 0), segments=10, sides=5, arc=150.0):
        """An open bend in the local XY plane (the bow)."""
        rings = []
        count = segments + 1
        for i in range(count):
            a = math.radians(arc) * i / segments - math.radians(arc) / 2
            centre = Vector((math.cos(a) * major, math.sin(a) * major, 0))
            out = Vector((math.cos(a), math.sin(a), 0))
            ring = []
            for j in range(sides):
                b = 2 * math.pi * j / sides
                ring.append(self.bm.verts.new(centre + out * math.cos(b) * minor + Vector((0, 0, math.sin(b) * minor))))
            rings.append(ring)
        for i in range(count - 1):
            r0, r1 = rings[i], rings[i + 1]
            for j in range(sides):
                self.bm.faces.new((r0[j], r1[j], r1[(j + 1) % sides], r0[(j + 1) % sides]))
        self.bm.faces.new(rings[0][::-1])
        self.bm.faces.new(rings[-1])
        self._finish([v for ring in rings for v in ring], trs(loc, rot, (1, 1, 1)), smooth=True)


def make_part(name, mat_name, bone, fill, arm_obj):
    part = Part()
    fill(part)
    mesh = bpy.data.meshes.new(name)
    part.bm.to_mesh(mesh)
    part.bm.free()
    mesh.materials.append(material(mat_name))
    obj = bpy.data.objects.new(name, mesh)
    COLLECTION.objects.link(obj)
    group = obj.vertex_groups.new(name=bone)
    group.add(list(range(len(mesh.vertices))), 1.0, "REPLACE")
    obj.parent = arm_obj
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj
    return obj


# The parts, with the bone each one follows. Positions are in Blender units (1 unit = 1 tile), before ORIENT.
PARTS = [
    ("BruteRigLegL", "dg_wood", "LegL", lambda p: p.box((-0.17, 0, 0.16), (0.2, 0.22, 0.32))),
    ("BruteRigLegR", "dg_wood", "LegR", lambda p: p.box((0.17, 0, 0.16), (0.2, 0.22, 0.32))),
    ("BruteRigBody", "dg_brute", "Body", lambda p: p.sphere((0, 0, 0.62), 0.42, scale=(1.1, 0.8, 0.9))),
    ("BruteRigHead", "dg_brute", "Body", lambda p: p.sphere((0, -0.12, 1.0), 0.18)),
    ("BruteRigHornL", "dg_horn", "Body", lambda p: p.cone((-0.14, -0.12, 1.15), 0.05, 0.0, 0.2, rot=(0, -35, 0), segments=6)),
    ("BruteRigHornR", "dg_horn", "Body", lambda p: p.cone((0.14, -0.12, 1.15), 0.05, 0.0, 0.2, rot=(0, 35, 0), segments=6)),
    ("BruteRigEyeL", "dg_eye", "Body", lambda p: p.sphere((-0.07, -0.29, 1.03), 0.035)),
    ("BruteRigEyeR", "dg_eye", "Body", lambda p: p.sphere((0.07, -0.29, 1.03), 0.035)),
    ("BruteRigArmL", "dg_brute", "ArmL", lambda p: p.box((-0.48, 0, 0.6), (0.16, 0.18, 0.44))),
    ("BruteRigArmR", "dg_brute", "ArmR", lambda p: p.box((0.48, 0, 0.6), (0.16, 0.18, 0.44))),
    ("BruteRigClub", "dg_wood", "ArmR", lambda p: p.cone((0.52, -0.25, 0.42), 0.04, 0.1, 0.6, rot=(-70, 0, 0), segments=7)),
]

for obj in list(bpy.data.objects):
    if obj.name.startswith("BruteRig"):
        bpy.data.objects.remove(obj, do_unlink=True)
for arm in list(bpy.data.armatures):
    if arm.name.startswith("BruteRig"):
        bpy.data.armatures.remove(arm)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith("BruteRig"):
        bpy.data.meshes.remove(mesh)
for act in list(bpy.data.actions):
    if act.name.startswith("BruteRig"):
        bpy.data.actions.remove(act)

arm_data = bpy.data.armatures.new("BruteRig")
arm_obj = bpy.data.objects.new("BruteRig", arm_data)
COLLECTION.objects.link(arm_obj)
bpy.context.view_layer.objects.active = arm_obj
bpy.ops.object.mode_set(mode="EDIT")
BONES = {
    "LegL": ((-0.17, 0, 0.32), (-0.17, 0, 0.0)),
    "LegR": ((0.17, 0, 0.32), (0.17, 0, 0.0)),
    "Body": ((0, 0, 0.4), (0, 0, 0.9)),
    "ArmL": ((-0.45, 0, 0.8), (-0.48, 0, 0.4)),
    "ArmR": ((0.45, 0, 0.8), (0.48, 0, 0.4)),
}
for name, (head, tail) in BONES.items():
    bone = arm_data.edit_bones.new(name)
    bone.head, bone.tail = Vector(head), Vector(tail)
    bone.parent = None
bpy.ops.object.mode_set(mode="OBJECT")

for name, mat_name, bone, fill in PARTS:
    make_part(name, mat_name, bone, fill, arm_obj)

# Orient for the engine (see slime_rig.py): geometry and bone rest turned Y-up, armature object turned back.
for obj in list(COLLECTION.objects):
    if obj.name.startswith("BruteRig") and obj.type == "MESH":
        obj.data.transform(ORIENT)
bpy.ops.object.mode_set(mode="EDIT")
for b in arm_data.edit_bones:
    b.head = ORIENT.to_3x3() @ b.head
    b.tail = ORIENT.to_3x3() @ b.tail
bpy.ops.object.mode_set(mode="OBJECT")
arm_obj.rotation_euler = (math.radians(90), 0, 0)

# One rest-pose action, so the exporter writes the bones with an animation (the clips take their hierarchy from it).
arm_obj.animation_data_create()
action = bpy.data.actions.new("BruteRigIdle")
arm_obj.animation_data.action = action
for pb in arm_obj.pose.bones:
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = (0, 0, 0)
    pb.keyframe_insert(data_path="rotation_euler", frame=1)
    pb.keyframe_insert(data_path="rotation_euler", frame=2)
bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, 2

os.makedirs(OUT_DIR, exist_ok=True)
for obj in bpy.context.view_layer.objects:
    obj.select_set(obj.name.startswith("BruteRig"))
bpy.context.view_layer.objects.active = arm_obj
path = os.path.join(OUT_DIR, FILE)
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True,
                         bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, apply_scale_options="FBX_SCALE_ALL")
print("exported", path, "bones", [b.name for b in arm_data.bones])
