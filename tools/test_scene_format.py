"""
Offline checks for the scene file format and the control channel command list. No GPU or engine needed.

Checks that:
- every scene in resources/scenes follows docs/ai/scene-format.md (types, required fields, references, files exist);
- every file in resources/scenes/invalid is rejected for its documented reason;
- the commands the engine accepts (src/Core/Control/ControlChannel.cpp) match docs/ai/control-api.md.

This checks the documented format, not the C++ parser. tools/test_control_channel.ps1 runs the real
loader and needs a GPU session.

Usage (from the repo root):
    python -I tools/test_scene_format.py
Exits 0 when every check passes, 1 otherwise.
"""
import json
import os
import re
import sys

sys.dont_write_bytecode = True  # keep __pycache__ out of resources/scripts
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESOURCES = os.path.join(ROOT, "resources")
SCENES = os.path.join(RESOURCES, "scenes")
INVALID = os.path.join(SCENES, "invalid")
SCRIPTS = os.path.join(RESOURCES, "scripts")
CONTROL_CPP = os.path.join(ROOT, "src", "Core", "Control", "ControlChannel.cpp")
CONTROL_DOC = os.path.join(ROOT, "docs", "ai", "control-api.md")

WORLD_ROOT = "Root"  # created by the engine before any scene loads, so scripts may attach to it
LIST_KEYS = ("clips", "materials", "lights", "bodies", "scripts")
SCRIPT_DOMAINS = ("start", "update", "delete")
PRIMITIVES = ("cube", "plane")

# Each invalid fixture and a fragment of the error it must produce. None means the static checks
# pass and the failure only shows up in the engine (the clip name is read from the model).
EXPECTED_INVALID = {
    "syntax_error.json": "JSON",
    "bad_version.json": "version",
    "bad_vector.json": "3 numbers",
    "missing_model.json": "failed to load model",
    "unknown_material.json": "unknown material",
    "unknown_primitive.json": "unknown primitive",
    "unknown_animation.json": None,
}


class SceneError(Exception):
    pass


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def require_str(obj, key, context):
    if not isinstance(obj.get(key), str):
        raise SceneError(f"{context}: '{key}' is required and must be a string")
    return obj[key]


def check_vec3(obj, key, context):
    if key not in obj:
        return
    value = obj[key]
    if not (isinstance(value, list) and len(value) == 3 and all(is_number(v) for v in value)):
        raise SceneError(f"{context}: '{key}' must be an array of 3 numbers")


def check_unique_names(items, kind):
    seen = set()
    for item in items:
        if item["name"] in seen:
            raise SceneError(f"duplicate {kind} name '{item['name']}'")
        seen.add(item["name"])


def check_scene(scene):
    """Raises SceneError with the first problem found."""
    version = scene.get("version") if isinstance(scene, dict) else None
    if type(version) is not int or version != 1:
        raise SceneError("scene must be an object with \"version\": 1")
    if "replaceWorld" in scene and not isinstance(scene["replaceWorld"], bool):
        raise SceneError("'replaceWorld' must be true or false")

    lists = {}
    for key in LIST_KEYS:
        if key not in scene:
            continue
        if not isinstance(scene[key], list):
            raise SceneError(f"'{key}' must be a list")
        lists[key] = scene[key]
        if key != "clips" and not all(isinstance(entry, dict) for entry in scene[key]):
            raise SceneError(f"every entry in '{key}' must be an object")

    for path in lists.get("clips", []):
        if not isinstance(path, str):
            raise SceneError("'clips' must be a list of file paths")
        if not os.path.isfile(os.path.join(RESOURCES, path)):
            raise SceneError(f"clip file '{path}' does not exist")

    materials = lists.get("materials", [])
    check_unique_names(materials, "material")
    for m in materials:
        name = require_str(m, "name", "material")
        require_str(m, "diffuseTexture", f"material '{name}'")
        if not os.path.isfile(os.path.join(RESOURCES, m["diffuseTexture"])):
            raise SceneError(f"material '{name}': texture '{m['diffuseTexture']}' does not exist")
        if "uvScale" in m and not is_number(m["uvScale"]):
            raise SceneError(f"material '{name}': 'uvScale' must be a number")
    material_names = {m["name"] for m in materials}

    lights = lists.get("lights", [])
    check_unique_names(lights, "light")
    for light in lights:
        context = f"light '{require_str(light, 'name', 'light')}'"
        for key in ("position", "color", "direction"):
            check_vec3(light, key, context)

    bodies = lists.get("bodies", [])
    check_unique_names(bodies, "body")
    for b in bodies:
        context = f"body '{require_str(b, 'name', 'body')}'"
        for key in ("position", "rotation", "scale"):
            check_vec3(b, key, context)
        if "material" in b:
            material = require_str(b, "material", context)
            if material not in material_names:
                raise SceneError(f"{context} references unknown material '{material}'")
        if "primitive" in b:
            primitive = require_str(b, "primitive", context)
            if primitive not in PRIMITIVES:
                raise SceneError(f"{context} has unknown primitive '{primitive}' (use \"cube\" or \"plane\")")
            if "size" in b and not is_number(b["size"]):
                raise SceneError(f"{context}: 'size' must be a number")
        else:
            model = require_str(b, "model", context)
            if not os.path.isfile(os.path.join(RESOURCES, model)):
                raise SceneError(f"failed to load model '{model}' for {context}")
        if "animation" in b:
            require_str(b, "animation", context)
        if "animationSpeed" in b and not is_number(b["animationSpeed"]):
            raise SceneError(f"{context}: 'animationSpeed' must be a number")
        if "animationLooping" in b and not isinstance(b["animationLooping"], bool):
            raise SceneError(f"{context}: 'animationLooping' must be true or false")

    body_names = {b["name"] for b in bodies} | {WORLD_ROOT}
    for s in lists.get("scripts", []):
        body = require_str(s, "body", "script entry")
        module = require_str(s, "module", f"script for body '{body}'")
        if s.get("domain", "update") not in SCRIPT_DOMAINS:
            raise SceneError(f"script for body '{body}': domain must be \"start\", \"update\" or \"delete\"")
        if body not in body_names:
            raise SceneError(f"script for unknown body '{body}' (this checker only sees bodies in the same file)")
        if not os.path.isfile(os.path.join(SCRIPTS, module + ".py")):
            raise SceneError(f"script module '{module}' is not in resources/scripts")


def load(path):
    """Returns (scene, None), or (None, message) when the file is not valid JSON."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    try:
        return json.loads(text), None
    except json.JSONDecodeError as e:
        return None, f"invalid JSON: {e}"


def check_command_list():
    """Returns the number of engine commands, after comparing them with the documented list."""
    with open(CONTROL_CPP, encoding="utf-8") as f:
        engine = set(re.findall(r'command == "(\w+)"', f.read()))
    with open(CONTROL_DOC, encoding="utf-8") as f:
        documented = set(re.findall(r"^### `(\w+)`", f.read(), re.MULTILINE))
    for name in sorted(engine - documented):
        failures.append(f"command '{name}' is in ControlChannel.cpp but not in docs/ai/control-api.md")
    for name in sorted(documented - engine):
        failures.append(f"command '{name}' is in docs/ai/control-api.md but not in ControlChannel.cpp")
    return len(engine)


failures = []

scene_count = 0
for name in sorted(os.listdir(SCENES)):
    path = os.path.join(SCENES, name)
    if not name.endswith(".json") or os.path.isdir(path):
        continue
    scene_count += 1
    scene, error = load(path)
    if error:
        failures.append(f"{name}: {error}")
        continue
    try:
        check_scene(scene)
    except SceneError as e:
        failures.append(f"{name}: {e}")

if set(os.listdir(INVALID)) != set(EXPECTED_INVALID):
    failures.append("resources/scenes/invalid does not match EXPECTED_INVALID in tools/test_scene_format.py")
for name, expected in EXPECTED_INVALID.items():
    scene, error = load(os.path.join(INVALID, name))
    reason = error
    if error is None:
        try:
            check_scene(scene)
        except SceneError as e:
            reason = str(e)
    if expected is None:
        if reason:
            failures.append(f"invalid/{name}: should pass the static checks, got {reason!r}")
    elif reason is None or expected not in reason:
        failures.append(f"invalid/{name}: expected an error containing '{expected}', got {reason!r}")

command_count = check_command_list()

if failures:
    print(f"{len(failures)} check(s) failed:")
    for line in failures:
        print("  - " + line)
    sys.exit(1)

print(f"Scene format checks passed: {scene_count} scenes, {len(EXPECTED_INVALID)} invalid fixtures, {command_count} commands.")
