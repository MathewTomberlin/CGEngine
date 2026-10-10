"""
Builds the dungeon sample game's models in Blender and exports them as OBJ + MTL to resources/models/dungeon.

Run it inside Blender (Scripting tab, or through the BlenderMCP addon's execute_code). Set OUT_DIR below, or
define OUT_DIR before running. The models land in a collection named "CGEngine_Dungeon", laid out in a row so
they can be inspected; the user's own objects are left alone.

Conventions, so models drop into the engine without per-model fixes:
- 1 Blender unit = 1 tile. Origin at the feet (z = 0 in Blender, y = 0 in the engine).
- Front faces Blender -Y. The export maps Blender +Z to engine +Y and Blender -Y to engine +Z, so a model
  faces engine +Z, towards the camera. Turning it by atan2(fx, fz) degrees about Y faces direction (fx, fz).
- Colours only, no textures. The engine uses each material's diffuse colour (Kd) as given. Material names
  start with "dg_" because engine materials share one namespace.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT_DIR = globals().get("OUT_DIR", os.path.join(os.path.dirname(bpy.data.filepath or "."), "models"))
COLLECTION = "CGEngine_Dungeon"

PALETTE = {
    "dg_steel": (0.75, 0.78, 0.82),
    "dg_tunic": (0.15, 0.4, 1.0),
    "dg_skin": (1.0, 0.72, 0.5),
    "dg_gold": (1.0, 0.8, 0.15),
    "dg_shield": (0.95, 0.15, 0.15),
    "dg_slime": (0.25, 1.0, 0.35),
    "dg_eye": (0.1, 0.1, 0.7),
    "dg_robe": (0.55, 0.15, 1.0),
    "dg_wood": (0.9, 0.5, 0.15),
    "dg_brute": (0.45, 0.7, 0.6),
    "dg_horn": (1.0, 0.95, 0.75),
    "dg_heart": (1.0, 0.1, 0.25),
    "dg_crystal": (0.15, 0.9, 1.0),
    "dg_stone": (0.6, 0.6, 0.62),
    "dg_feather": (1.0, 0.2, 0.2),
    "dg_slash": (1.0, 1.0, 0.7),
}


def material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgb = PALETTE[name]
    mat.diffuse_color = (*rgb, 1.0)
    if mat.node_tree is None and hasattr(mat, "use_nodes"):
        mat.use_nodes = True
    if mat.node_tree:
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
            bsdf.inputs["Roughness"].default_value = 0.8
            # Exported as Ks. A weak highlight keeps top faces from washing out under the dungeon's overhead sun.
            bsdf.inputs["Specular IOR Level"].default_value = 0.1
    return mat


class Builder:
    """Collects parts into one bmesh. Each part gets a material slot; the model becomes one object."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.mats = []
        self.smooth_mats = set()

    def _slot(self, mat_name):
        if mat_name not in self.mats:
            self.mats.append(mat_name)
        return self.mats.index(mat_name)

    def _finish(self, verts, mat_name, matrix, smooth=False):
        bmesh.ops.transform(self.bm, matrix=matrix, verts=verts)
        slot = self._slot(mat_name)
        faces = {f for v in verts for f in v.link_faces}
        for f in faces:
            f.material_index = slot
            f.smooth = smooth

    def box(self, mat_name, loc, size, rot=(0, 0, 0)):
        verts = bmesh.ops.create_cube(self.bm, size=1.0)["verts"]
        self._finish(verts, mat_name, trs(loc, rot, size))

    def sphere(self, mat_name, loc, radius, scale=(1, 1, 1), rot=(0, 0, 0), segments=10, rings=6, smooth=False):
        verts = bmesh.ops.create_uvsphere(self.bm, u_segments=segments, v_segments=rings, radius=1.0)["verts"]
        self._finish(verts, mat_name, trs(loc, rot, [radius * s for s in scale]), smooth)

    def cone(self, mat_name, loc, r1, r2, depth, rot=(0, 0, 0), segments=8, scale=(1, 1, 1)):
        """Along local Z, centred on loc."""
        verts = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=r1, radius2=r2, depth=depth)["verts"]
        self._finish(verts, mat_name, trs(loc, rot, scale))

    def torus(self, mat_name, loc, major, minor, rot=(0, 0, 0), segments=16, sides=6, arc=360.0, scale=(1, 1, 1)):
        """Ring in the local XY plane. arc < 360 gives an open bend (used for the bow and the sword arc)."""
        rings = []
        closed = arc >= 360.0
        count = segments if closed else segments + 1
        for i in range(count):
            a = math.radians(arc) * i / segments - (0 if closed else math.radians(arc) / 2)
            centre = Vector((math.cos(a) * major, math.sin(a) * major, 0))
            out = Vector((math.cos(a), math.sin(a), 0))
            ring = []
            for j in range(sides):
                b = 2 * math.pi * j / sides
                ring.append(self.bm.verts.new(centre + out * math.cos(b) * minor + Vector((0, 0, math.sin(b) * minor))))
            rings.append(ring)
        pairs = range(count) if closed else range(count - 1)
        for i in pairs:
            r0, r1 = rings[i], rings[(i + 1) % count]
            for j in range(sides):
                self.bm.faces.new((r0[j], r1[j], r1[(j + 1) % sides], r0[(j + 1) % sides]))
        if not closed:
            self.bm.faces.new(rings[0][::-1])
            self.bm.faces.new(rings[-1])
        self._finish([v for ring in rings for v in ring], mat_name, trs(loc, rot, scale))

    def build(self, collection):
        old = bpy.data.objects.get(self.name)
        if old:
            bpy.data.objects.remove(old, do_unlink=True)
        mesh = bpy.data.meshes.get(self.name)
        if mesh:
            bpy.data.meshes.remove(mesh)
        mesh = bpy.data.meshes.new(self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for mat_name in self.mats:
            mesh.materials.append(material(mat_name))
        obj = bpy.data.objects.new(self.name, mesh)
        collection.objects.link(obj)
        return obj


def trs(loc, rot, scale):
    if not isinstance(scale, (list, tuple)):
        scale = (scale, scale, scale)
    r = Matrix.Rotation(math.radians(rot[2]), 4, "Z") @ Matrix.Rotation(math.radians(rot[1]), 4, "Y") @ \
        Matrix.Rotation(math.radians(rot[0]), 4, "X")
    s = Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
    return Matrix.Translation(loc) @ r @ s


# ----- models (front faces -Y, feet at z = 0) --------------------------------------------------

def knight():
    b = Builder("knight")
    b.box("dg_steel", (-0.09, 0, 0.12), (0.12, 0.14, 0.24))       # legs
    b.box("dg_steel", (0.09, 0, 0.12), (0.12, 0.14, 0.24))
    b.box("dg_tunic", (0, 0, 0.4), (0.36, 0.22, 0.34))            # body
    b.box("dg_gold", (0, 0, 0.27), (0.38, 0.24, 0.05))            # belt
    b.sphere("dg_skin", (0, 0, 0.68), 0.14)                       # head
    b.sphere("dg_steel", (0, 0.01, 0.72), 0.16, scale=(1, 1, 0.75))  # helmet
    b.box("dg_steel", (0, -0.12, 0.66), (0.2, 0.04, 0.03))        # visor band
    b.box("dg_tunic", (0.23, 0, 0.42), (0.1, 0.12, 0.26))         # arms
    b.box("dg_tunic", (-0.23, 0, 0.42), (0.1, 0.12, 0.26))
    b.box("dg_steel", (-0.26, -0.18, 0.62), (0.05, 0.03, 0.46))   # sword blade, upright in the right hand (-X when facing -Y)
    b.box("dg_gold", (-0.26, -0.18, 0.37), (0.16, 0.05, 0.04))     # crossguard
    b.box("dg_gold", (-0.26, -0.18, 0.31), (0.04, 0.04, 0.1))      # grip
    b.cone("dg_shield", (0.31, -0.03, 0.4), 0.17, 0.17, 0.04, rot=(0, 90, 0), segments=10)  # round shield, left arm
    b.sphere("dg_gold", (0.34, -0.03, 0.4), 0.05)                # shield boss
    return b


def slime():
    b = Builder("slime")
    b.sphere("dg_slime", (0, 0, 0.2), 0.32, scale=(1, 1, 0.68), segments=14, rings=8, smooth=True)
    b.sphere("dg_slime", (0, 0, 0.36), 0.14, scale=(1, 1, 0.8), segments=10, rings=6, smooth=True)
    b.sphere("dg_eye", (-0.1, -0.27, 0.27), 0.05)
    b.sphere("dg_eye", (0.1, -0.27, 0.27), 0.05)
    return b


def archer():
    b = Builder("archer")
    b.cone("dg_robe", (0, 0, 0.27), 0.22, 0.13, 0.54, segments=8)    # robe
    b.sphere("dg_skin", (0, -0.02, 0.62), 0.11)                        # face
    b.cone("dg_robe", (0, 0.02, 0.7), 0.16, 0.0, 0.34, segments=8)    # hood
    b.box("dg_robe", (0.18, -0.08, 0.42), (0.08, 0.2, 0.08))          # arm reaching for the bow
    # Bow held upright at the right side, bulging forward (-Y), with its string across the chord.
    b.torus("dg_wood", (0.22, 0.02, 0.42), 0.26, 0.02, rot=(90, 0, -90), arc=150, segments=10, sides=5)
    b.box("dg_slash", (0.22, -0.047, 0.42), (0.008, 0.008, 0.5))
    b.cone("dg_wood", (-0.05, 0.16, 0.5), 0.06, 0.06, 0.32, rot=(15, 0, 0), segments=6)  # quiver
    b.cone("dg_feather", (-0.05, 0.19, 0.7), 0.05, 0.0, 0.1, rot=(15, 0, 0), segments=4)
    return b


def brute():
    b = Builder("brute")
    b.box("dg_wood", (-0.17, 0, 0.16), (0.2, 0.22, 0.32))             # legs
    b.box("dg_wood", (0.17, 0, 0.16), (0.2, 0.22, 0.32))
    b.sphere("dg_brute", (0, 0, 0.62), 0.42, scale=(1.1, 0.8, 0.9))  # hulking body
    b.sphere("dg_brute", (0, -0.12, 1.0), 0.18)                       # head, low and forward
    b.cone("dg_horn", (-0.14, -0.12, 1.15), 0.05, 0.0, 0.2, rot=(0, -35, 0), segments=6)
    b.cone("dg_horn", (0.14, -0.12, 1.15), 0.05, 0.0, 0.2, rot=(0, 35, 0), segments=6)
    b.sphere("dg_eye", (-0.07, -0.29, 1.03), 0.035)
    b.sphere("dg_eye", (0.07, -0.29, 1.03), 0.035)
    b.box("dg_brute", (0.48, 0, 0.6), (0.16, 0.18, 0.44))            # arms
    b.box("dg_brute", (-0.48, 0, 0.6), (0.16, 0.18, 0.44))
    b.cone("dg_wood", (0.52, -0.25, 0.42), 0.04, 0.1, 0.6, rot=(-70, 0, 0), segments=7)  # club
    return b


def arrow():
    b = Builder("arrow")
    b.cone("dg_wood", (0, 0, 0), 0.015, 0.015, 0.44, rot=(90, 0, 0), segments=5)     # shaft along Y
    b.cone("dg_steel", (0, -0.25, 0), 0.04, 0.0, 0.08, rot=(90, 0, 0), segments=5)  # tip points -Y (front)
    b.box("dg_feather", (0, 0.19, 0), (0.1, 0.07, 0.01))
    b.box("dg_feather", (0, 0.19, 0), (0.01, 0.07, 0.1))
    return b


def heart():
    b = Builder("heart")
    b.sphere("dg_heart", (-0.07, 0, 0.21), 0.085, scale=(1, 0.6, 1), smooth=True)
    b.sphere("dg_heart", (0.07, 0, 0.21), 0.085, scale=(1, 0.6, 1), smooth=True)
    b.cone("dg_heart", (0, 0, 0.12), 0.0, 0.155, 0.18, segments=4, scale=(1, 0.35, 1))
    return b


def coin():
    b = Builder("coin")
    b.cone("dg_gold", (0, 0, 0.14), 0.12, 0.12, 0.03, rot=(90, 0, 0), segments=14)
    b.box("dg_horn", (0, -0.016, 0.14), (0.03, 0.005, 0.1))         # stamped mark
    return b


def goal():
    b = Builder("goal")
    b.cone("dg_stone", (0, 0, 0.06), 0.3, 0.26, 0.12, segments=8)   # pedestal
    b.cone("dg_crystal", (0, 0, 0.42), 0.13, 0.0, 0.36, segments=6)  # crystal top half
    b.cone("dg_crystal", (0, 0, 0.18), 0.0, 0.13, 0.12, segments=6)  # crystal bottom point
    b.torus("dg_gold", (0, 0, 0.3), 0.24, 0.015, segments=16, sides=4)
    return b


def slash():
    b = Builder("slash")
    # A flat crescent in front of the player, where the swing hits (DungeonGame.SLASH_REACH, about 0.9 ahead).
    # The ring is centred 0.35 ahead and bulges forward (-Y), so the arc runs from about 0.6 to 0.85 in front.
    b.torus("dg_slash", (0, -0.35, 0.3), 0.5, 0.04, rot=(0, 0, -90), arc=120, segments=10, sides=4, scale=(1, 1, 0.5))
    return b


MODELS = [knight, slime, archer, brute, arrow, heart, coin, goal, slash]


def stabilise_obj(path):
    """Rewrite an exported OBJ in a fixed order, so re-exporting an unchanged model gives the same file.

    Blender writes the vn list in a different order on each export; faces refer to normals by index, so the
    face lines are remapped to the sorted order. Everything else is kept as written.
    """
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    normals = [line for line in lines if line.startswith("vn ")]
    ordered = sorted(set(normals))
    position = {line: i + 1 for i, line in enumerate(ordered)}
    new_index = {old: position[line] for old, line in enumerate(normals, start=1)}
    out = []
    normals_written = False
    for line in lines:
        if line.startswith("vn "):
            if not normals_written:
                out.extend(ordered)
                normals_written = True
            continue
        if line.startswith("f "):
            corners = []
            for corner in line.split()[1:]:
                parts = corner.split("/")
                if len(parts) == 3 and parts[2]:
                    parts[2] = str(new_index[int(parts[2])])
                corners.append("/".join(parts))
            line = "f " + " ".join(corners)
        out.append(line)
    # Faces also come out in a varying order. Order does not matter for drawing, so each run of faces (one material
    # group) is sorted.
    stable, run = [], []
    for line in out + [""]:
        if line.startswith("f "):
            run.append(line)
            continue
        stable.extend(sorted(run))
        run = []
        stable.append(line)
    stable.pop()
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(stable) + "\n")


def run():
    os.makedirs(OUT_DIR, exist_ok=True)
    scene = bpy.context.scene
    collection = bpy.data.collections.get(COLLECTION)
    if collection is None:
        collection = bpy.data.collections.new(COLLECTION)
        scene.collection.children.link(collection)
    for obj in bpy.context.view_layer.objects:
        obj.select_set(False)
    exported = []
    for i, make in enumerate(MODELS):
        obj = make().build(collection)
        obj.location = (0, 0, 0)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = os.path.join(OUT_DIR, obj.name + ".obj")
        bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, forward_axis="NEGATIVE_Z",
                              up_axis="Y", export_materials=True, export_normals=True, export_uv=True,
                              export_triangulated_mesh=True, apply_modifiers=True, path_mode="STRIP")
        stabilise_obj(path)
        obj.select_set(False)
        obj.location = (3 + i * 1.5, 3, 0)  # a row for viewing, clear of the default cube; export used the origin
        exported.append((obj.name, len(obj.data.polygons)))
    return exported


print(run())
