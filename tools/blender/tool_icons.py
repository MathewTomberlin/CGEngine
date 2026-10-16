"""
Renders the hotbar icons of the voxel sample's tools (roadmap 34.3): a picture of each tool's model, with a transparent background,
saved to resources/icons/ as PNG. Run inside Blender after tools/blender/tool_models.py (the pickaxes must be in the "ToolModels"
collection). The renders use a temporary camera and light in their own collection, which is removed afterwards, and the scene's
camera, render engine and output settings are restored, so the user's objects and settings are left as they were.

  icons/pickaxe.png       the stone pickaxe (the "handle" object and a copy of "head.001" made of Stone)
  icons/iron_pickaxe.png  the iron pickaxe (the same handle, the copy made of Iron)

Usage: exec this file inside Blender (the BlenderMCP socket does), or `blender --background <file> --python tools/blender/tool_icons.py`
after the tool models exist in the open file.
"""
import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "resources", "icons"))
ICON_PIXELS = 128
COLLECTION = "ToolIcons"
# The camera looks at the middle of the tool from the front-right and a little above, so the head and the handle both show.
VIEW_DIRECTION = Vector((1.0, -1.0, 0.45)).normalized()
ORTHO_SCALE = 0.95


def temp_collection():
    coll = bpy.data.collections.new(COLLECTION)
    bpy.context.scene.collection.children.link(coll)
    return coll


def add_camera(coll, centre):
    cam_data = bpy.data.cameras.new("IconCamera")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ORTHO_SCALE
    cam = bpy.data.objects.new("IconCamera", cam_data)
    coll.objects.link(cam)
    cam.location = centre + VIEW_DIRECTION * 2.0
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    return cam


def add_light(coll, centre):
    light_data = bpy.data.lights.new("IconLight", "SUN")
    light_data.energy = 3.0
    light = bpy.data.objects.new("IconLight", light_data)
    coll.objects.link(light)
    light.location = centre + Vector((0.5, -1.0, 1.5))
    light.rotation_euler = (centre - light.location).to_track_quat("-Z", "Y").to_euler()
    return light


# The colours the icons show the heads in: stone is dark and matte, iron is bright (the viewport colours of the models' materials).
ICON_COLOURS = {"Stone": (0.42, 0.42, 0.44), "Iron": (0.9, 0.92, 0.95)}


def icon_material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.diffuse_color = (*ICON_COLOURS[name], 1.0)
    return mat


def render_tool(name, head_name, head_material, out_path):
    """Renders the handle and a copy of the head named head_name, made of head_material, as one tool, into out_path. The copy is
    removed afterwards, so the head in the scene keeps its own material."""
    handle = bpy.data.objects["handle"]
    source = bpy.data.objects[head_name]
    centre = Vector((0.0, 0.0, 0.0))
    for obj in (handle, source):
        centre += obj.matrix_world.translation
    centre /= 2.0
    scene = bpy.context.scene
    saved = {
        "camera": scene.camera,
        "engine": scene.render.engine,
        "transparent": scene.render.film_transparent,
        "x": scene.render.resolution_x,
        "y": scene.render.resolution_y,
        "percent": scene.render.resolution_percentage,
        "filepath": scene.render.filepath,
        "format": scene.render.image_settings.file_format,
        "colour": scene.render.image_settings.color_mode,
    }
    coll = temp_collection()
    head = source.copy()
    head.data = source.data.copy()
    head_mesh = head.data
    head.data.materials.clear()
    head.data.materials.append(icon_material(head_material))
    coll.objects.link(head)
    coll.objects.link(handle)
    try:
        scene.camera = add_camera(coll, centre)
        add_light(coll, centre)
        scene.render.engine = "BLENDER_WORKBENCH"
        scene.render.film_transparent = True
        scene.render.resolution_x = ICON_PIXELS
        scene.render.resolution_y = ICON_PIXELS
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.image_settings.color_mode = "RGBA"
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        scene.render.filepath = out_path
        bpy.ops.render.render(write_still=True)
        print("rendered", name, out_path)
    finally:
        for obj in list(coll.objects):
            if obj is handle:
                coll.objects.unlink(obj)     # the handle is the scene's own; only its link to the render collection is removed
                continue
            bpy.data.objects.remove(obj, do_unlink=True)
        if head_mesh.users == 0:
            bpy.data.meshes.remove(head_mesh)
        bpy.data.collections.remove(coll)
        for block in list(bpy.data.cameras):
            if block.name.startswith("IconCamera") and block.users == 0:
                bpy.data.cameras.remove(block)
        for block in list(bpy.data.lights):
            if block.name.startswith("IconLight") and block.users == 0:
                bpy.data.lights.remove(block)
        scene.camera = saved["camera"]
        scene.render.engine = saved["engine"]
        scene.render.film_transparent = saved["transparent"]
        scene.render.resolution_x = saved["x"]
        scene.render.resolution_y = saved["y"]
        scene.render.resolution_percentage = saved["percent"]
        scene.render.filepath = saved["filepath"]
        scene.render.image_settings.file_format = saved["format"]
        scene.render.image_settings.color_mode = saved["colour"]


def main():
    render_tool("pickaxe", "head.001", "Stone", os.path.join(OUT_DIR, "pickaxe.png"))
    render_tool("iron pickaxe", "head.001", "Iron", os.path.join(OUT_DIR, "iron_pickaxe.png"))


if __name__ == "__main__" or __name__ == "<string>":
    main()
