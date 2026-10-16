"""
Checks the runtime mesh bindings (create_mesh_body, update_mesh_body, remove_mesh_body) inside the running engine.
Run by tools/test_control_channel.ps1 through the control channel's run_script. Writes its results to the JSON file
named by the RUNTIME_MESH_RESULT environment variable, or cg_control/runtime_mesh_result.json.
"""
import json
import os
from array import array

import cg_engine_bindings as cge

result = {}


def expect_error(label, fn):
    try:
        fn()
    except ValueError as error:
        result[label] = str(error)
    else:
        result[label] = None


triangle = (array("f", [0, 0, 0, 1, 0, 0, 0, 1, 0]), array("f"), array("f", [1, 0, 0, 0, 1, 0, 0, 0, 1]),
            array("f"), array("f"), array("I", [0, 1, 2]))
cge.create_mesh_body("rt_triangle", *triangle, [], cge.Vector3f(0, 0, -5))
result["created"] = cge.find_body("rt_triangle") is not None

quad = (array("f", [0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0]), array("f", [0, 0, 1] * 4), array("f", [1, 1, 1] * 4),
        array("f", [0, 0, 1, 0, 1, 1, 0, 1]), array("f", [0, 0, 0, 0]), array("I", [0, 1, 2, 0, 2, 3]))
cge.update_mesh_body("rt_triangle", *quad)
result["updated"] = True

result["remove_existing"] = cge.remove_mesh_body("rt_triangle")
result["gone"] = cge.find_body("rt_triangle") is None
result["remove_missing"] = cge.remove_mesh_body("rt_triangle")

expect_error("bad_positions", lambda: cge.create_mesh_body(
    "rt_bad", array("f", [0, 0]), array("f"), array("f"), array("f"), array("f"), array("I"), [], cge.Vector3f(0, 0, 0)))
expect_error("bad_index", lambda: cge.create_mesh_body(
    "rt_bad", array("f", [0, 0, 0]), array("f"), array("f"), array("f"), array("f"), array("I", [0, 0, 5]), [], cge.Vector3f(0, 0, 0)))
expect_error("unknown_material", lambda: cge.create_mesh_body(
    "rt_bad", array("f", [0, 0, 0]), array("f"), array("f"), array("f"), array("f"), array("I", [0, 0, 0]),
    ["no_such_material"], cge.Vector3f(0, 0, 0)))
expect_error("update_scene_mesh", lambda: cge.update_mesh_body(
    "dg_nothing_here", array("f", [0, 0, 0]), array("f"), array("f"), array("f"), array("f"), array("I", [0, 0, 0])))

path = os.environ.get("RUNTIME_MESH_RESULT", os.path.join("cg_control", "runtime_mesh_result.json"))
os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
with open(path, "w") as f:
    json.dump(result, f)
