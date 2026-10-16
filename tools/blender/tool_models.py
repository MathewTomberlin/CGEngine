"""
Makes the pickaxe models of the voxel sample (roadmap 34.1): a stone pickaxe and an iron pickaxe, each a turned and worn wooden handle
and a bevelled head of its own material. Run it inside Blender (the BlenderMCP socket runs it with exec of this file, or
`blender --background --python tools/blender/tool_models.py`). It builds in its own collection, "ToolModels", leaves the other
objects alone, and exports two FBX files to resources/models/viewmodel/:

  item_pickaxe.fbx       the stone pickaxe: head material "Stone", handle "Wood"
  item_pickaxe_iron.fbx  the iron pickaxe: head material "Iron", handle "Wood"

The materials are named in resources/scenes/voxels.json, so the head's look is data. Units are the engine's (one unit is one
block), so the pickaxe is about 0.74 long: the same size as the model it replaces. The handle is turned on its axis (a bulge and a
slight bend) and worn (its surface is roughened by a fixed noise), so it is not a plain cylinder. The head has bevelled edges. Each
run writes the same geometry (fixed seeds), so a change in the file is a change in the model.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "resources", "models", "viewmodel"))
COLLECTION = "ToolModels"
HANDLE_LENGTH = 0.66
HANDLE_RADIUS = 0.024
HEAD_SPAN = 0.46            # across the handle, the head's length
HEAD_DEPTH = 0.03           # the head's thickness at its middle
HEAD_HEIGHT = 0.075          # how far the head's curve rises above its ends
BEVEL = 0.006


def collection():
    coll = bpy.data.collections.get(COLLECTION)
    if coll is None:
        coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(coll)
    return coll


def link(obj):
    collection().objects.link(obj)
    return obj


def material(name, colour):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*colour, 1.0)
    return mat


def make_handle():
    """The handle: a cylinder along Z, bulged at the grip, bent a little, and roughened by a fixed noise."""
    rng = random.Random(34)
    mesh = bpy.data.meshes.new("handle")
    bm = bmesh.new()
    rings, sides = 14, 12
    for i in range(rings + 1):
        t = i / rings
        z = -HANDLE_LENGTH / 2 + t * HANDLE_LENGTH
        # The grip swells in the lower half and tapers to the top; the bend moves the top toward +X.
        radius = HANDLE_RADIUS * (1.0 + 0.25 * math.sin(math.pi * t) * (1.0 - t))
        bend = 0.012 * (t * t)
        for j in range(sides):
            a = 2 * math.pi * j / sides
            wobble = 1.0 + rng.uniform(-0.05, 0.05)       # the wear: a rough surface
            bm.verts.new((bend + math.cos(a) * radius * wobble, math.sin(a) * radius * wobble, z))
    bm.verts.ensure_lookup_table()
    for i in range(rings):
        for j in range(sides):
            a = i * sides + j
            b = i * sides + (j + 1) % sides
            c = (i + 1) * sides + (j + 1) % sides
            d = (i + 1) * sides + j
            bm.faces.new([bm.verts[a], bm.verts[b], bm.verts[c], bm.verts[d]])
    bm.faces.new([bm.verts[j] for j in range(sides)][::-1])
    bm.faces.new([bm.verts[rings * sides + j] for j in range(sides)])
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("handle", mesh)
    obj.data.materials.append(material("Wood", (0.45, 0.3, 0.18)))
    return link(obj)


def make_head(name, material_name, colour):
    """The head: a curved bar across the top of the handle, tapered to points at its ends, with its edges bevelled."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    segments = 14
    sections = []
    for i in range(segments + 1):
        t = i / segments                       # 0 at one tip, 1 at the other
        x = -HEAD_SPAN / 2 + t * HEAD_SPAN
        u = 2 * t - 1
        y = HEAD_HEIGHT * (1.0 - u * u)        # the curve: highest at the middle
        taper = max(0.12, 1.0 - u ** 6)        # narrow to the tips
        half = HEAD_DEPTH / 2 * taper
        z = HANDLE_LENGTH / 2                  # the head sits on the top of the handle
        sections.append([
            bm.verts.new((x, y, z + half)),
            bm.verts.new((x, y + half, z)),
            bm.verts.new((x, y, z - half)),
            bm.verts.new((x, y - half, z)),
        ])
    bm.verts.ensure_lookup_table()
    for i in range(segments):
        for k in range(4):
            a = sections[i][k]
            b = sections[i][(k + 1) % 4]
            c = sections[i + 1][(k + 1) % 4]
            d = sections[i + 1][k]
            bm.faces.new([a, b, c, d])
    bm.faces.new(sections[0][::-1])
    bm.faces.new(sections[-1])
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    obj = link(bpy.data.objects.new(name, mesh))
    bpy.context.view_layer.objects.active = obj
    bevel = obj.modifiers.new("Bevel", "BEVEL")
    bevel.width = BEVEL
    bevel.segments = 2
    bevel.limit_method = "ANGLE"
    for m in obj.modifiers:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=m.name)
    obj.data.materials.append(material(material_name, colour))
    return obj


def build(head_material, head_colour):
    """One pickaxe: its handle and its head, under one empty so the export takes them as a single model."""
    for obj in list(collection().objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    handle = make_handle()
    head = make_head("head", head_material, head_colour)
    return [handle, head]


def export(objects, path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                             global_scale=1.0, apply_scale_options="FBX_SCALE_NONE", apply_unit_scale=True,
                             bake_space_transform=True, add_leaf_bones=False, mesh_smooth_type="FACE")
    print("exported", path)


def main():
    export(build("Stone", (0.5, 0.5, 0.52)), os.path.join(OUT_DIR, "item_pickaxe.fbx"))
    export(build("Iron", (0.78, 0.8, 0.84)), os.path.join(OUT_DIR, "item_pickaxe_iron.fbx"))


if __name__ == "__main__" or __name__ == "<string>":
    main()
