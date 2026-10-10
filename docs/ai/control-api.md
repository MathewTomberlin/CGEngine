# AI control channel (file-based)

The engine can be driven by writing JSON command files. It needs no sockets or extra
dependencies, so any agent or tool that can write files can use it.

## Folders

The engine runs from its exe folder (for example `build/bin/Debug`). It creates:

```
cg_control/
  inbox/    <- you write commands here
  outbox/   <- the engine writes results here
```

## Sending a command

1. Write the command to `inbox/<id>.json.tmp`.
2. Rename it to `inbox/<id>.json`. Only files ending in `.json` are read, so the rename makes the
   file complete before the engine sees it.
3. Read `outbox/<id>.result.json` when it appears.

Commands are polled every 10 frames, at most 16 per poll, in file-name order. Use names such as
`0001_describe.json` to control ordering.

Each command file:

```json
{ "command": "describe_scene", "params": { } }
```

`params` is optional. Each result file:

```json
{ "ok": true,  "result": { } }
{ "ok": false, "error": "human-readable reason" }
```

## Commands

### `load_scene`
Loads a scene file (format: `docs/ai/scene-format.md`). Paths are relative to the exe folder.

- params: `path` (string, required)
- result: `{ "bodies": n, "lights": n, "materials": n }` (counts created)
- errors: missing file, invalid JSON, unknown material or model, duplicate names

### `describe_scene`
Lists what is in the world. Unnamed model sub-parts are left out; each named body reports its child count.

- params: none
- result:
  ```json
  {
    "bodies":    [ { "id": 4, "name": "cube_right", "parent": 0, "children": 3,
                     "position": [3, 0, -5], "rotation": [0, 45, 0], "scale": [1, 1, 1] } ],
    "lights":    [ { "name": "sun", "id": 1, "position": [0, -20, 5], "brightness": 5 } ],
    "materials": [ { "name": "brick", "id": 2 } ]
  }
  ```
  Position, rotation and scale are only reported for bodies that have a mesh.

### `set_transform`
Changes a named body's mesh transform. Only the fields you send are changed.

- params: `name` (required), `position`, `rotation`, `scale` (each `[x, y, z]`, optional)
- rotation is in degrees (Euler angles)
- result: the body's transform after the change
- errors: no body with that name, body has no mesh. If several bodies share a name, the first is used.

### `set_material`
Assigns a material (by name, from a scene's `materials` or any loaded material) to a body's mesh.

- params: `name` (body, required), `material` (required), `recursive` (bool, default `true`: also applies to the sub-parts of a model instance)
- result: `{ "name": ..., "material": ..., "meshesUpdated": n }`
- errors: no body with that name, no material with that name

### `remove_body`
Removes a body and destroys it. The world root cannot be removed.

- params: `name` (required), `children` (`"terminate"` default: remove the subtree; `"orphan"`: move children to the world root; `"inherit"`: move children to the parent)
- result: `{ "removed": name, "children": mode }`
- errors: no body with that name, root removal, unknown `children` mode

### `load_clip`
Loads an animation clip from a file (format: `docs/ai/clip-format.md`).

- params: `path` (required; relative to the exe folder or absolute)
- result: `{ "name": clip name }`
- errors: invalid clip, unknown bone, duplicate name, model not skeletal

### `list_animations`
Lists the animation clips of the model a body belongs to, and the playback state.

- params: `name` (required; a body inside an imported model)
- result: `{ "bones": [skeleton bone names], "current", "timeSeconds", "durationSeconds", "paused", "speed", "looping", "animations": [ { "name", "durationSeconds" } ] }`
- errors: no body, body not part of a model, model without clips

### `play_animation`
Starts a clip on the body's model.

- params: `name`, `animation` (required), `speed` (default 1), `looping` (default true; false holds the last pose)
- result: the playback state after the change
- errors: as above, or the clip is not in the model

### `pause_animation`
- params: `name` (required), `paused` (default true; false resumes)
- result: `{ "name", "paused" }`

Note: animators belong to the model, so every body built from the same model shares its playback state.

### `run_script`
Runs a Python file once (format and examples: `docs/ai/script-hooks.md`).

- params: `path` (required; relative to the exe folder or absolute)
- result: `{ "ran": path }`
- errors: the Python error text

### `attach_script`
Attaches a Python module from the `scripts` folder to a named body, for the `start`, `update` or `delete` domain.

- params: `name` (body, required), `module` (required), `domain` (default `"update"`)
- result: `{ "name", "domain", "scriptId" }`
- errors: no body, bad domain, module that cannot be loaded

### `screenshot`
Saves the next rendered frame as a PNG.

- params: `path` (required; absolute, or relative to the exe folder)
- result: `{ "queued": true, "path": "..." }` immediately. The file appears after the next frame
  is rendered, so poll for the file if you need the image.
- The saved image is opaque (alpha 255).
- Use a Windows path (for example `C:/work/shot.png`). Git Bash paths such as `/c/work/shot.png` are read
  as `C:\c\work\shot.png`, the save fails, and the only trace is a "Failed to save image" line in the
  engine's console output. The result file still reports `ok: true`.

### `get_stats`
- params: none
- result: `{ "frames": n, "frameSeconds": s, "bodies": n, "lights": n, "materials": n }`
- Counts include engine default assets (for example two lights and six materials in an empty scene).

## Example

```json
{ "command": "set_transform", "params": { "name": "cube", "position": [0, 2, -5], "rotation": [0, 90, 0] } }
```

## Testing

`tools/test_control_channel.ps1` starts the engine with `scenes/example.json`, runs every command
above, and checks the results. Run it from the repo root after a Debug build.

## Security

The channel reads and writes local files and loads any path it is given. It is intended for a
developer's own machine and CI. Do not expose the exe folder to untrusted writers.
