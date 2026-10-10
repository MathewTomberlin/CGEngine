# Scene file format (version 1)

A scene is a JSON document that describes materials, lights and model bodies. Load one with:

- `CGENGINE_SCENE=scenes/example.json` (environment variable, read at startup; path is relative to the exe folder), or
- `SceneLoader::loadFile(path)` from C++.

Failures are reported with a message; a bad scene never silently produces a partial result in smoke mode (it exits 1).

## Top level

```json
{
  "version": 1,
  "replaceWorld": false,
  "clips": [ ... ],
  "materials": [ ... ],
  "lights": [ ... ],
  "bodies": [ ... ],
  "scripts": [ ... ]
}
```

`version` must be `1`. Every list is optional. Order matters only for references: clips are loaded first, then materials, lights, bodies, and finally scripts, so a body can name any material in the file, and a script can name any body in the file or one loaded earlier.

- `replaceWorld` (default `false`): when `true`, every body under the world root and every light is removed before the file loads, so the file brings its own lights. Use it when the file describes the whole level. Without it, the file is added to what is already in the world.
- `scripts`: attaches a Python script module to a named body. Each entry is `{ "body": name, "module": name, "domain": "update" }`. `domain` is `start`, `update` (default) or `delete`. The module must be in the `scripts` folder and define `create_instance()`. See `docs/ai/script-hooks.md`.

## materials

| field            | type    | required | default | notes                                        |
|------------------|---------|----------|---------|----------------------------------------------|
| `name`           | string  | yes      |         | Unique within materials. Referenced by bodies. |
| `diffuseTexture` | string  | yes      |         | File name in `resources/` (e.g. `brick_tile.png`). |
| `uvScale`        | number  | no       | `1`     | Texture repeat count across the surface.     |
| `specular`       | number  | no       | `1`     | Strength of the white highlight, 0 (matte) to 1. |

## lights

| field         | type         | required | default       | notes                                   |
|---------------|--------------|----------|---------------|-----------------------------------------|
| `name`        | string       | yes      |               | Unique within lights.                   |
| `position`    | [x, y, z]    | no       | `[0,0,0]`     | World units, y up. A light above the scene has a positive y. |
| `directional` | bool         | no       | `false`       | Directional lights ignore position.     |
| `brightness`  | number       | no       | `3.5`         | Scales the light's direct and ambient parts. |
| `color`       | [r, g, b]    | no       | `[1,1,1]`     | 0 to 1 per channel.                     |
| `attenuation` | number       | no       | `0.005`       | Point lights fade as 1 / (1 + attenuation * distance^2). |
| `ambiance`    | number       | no       | `0.1`         | Ambient fill is ambiance * brightness (0.35 by default). |
| `coneAngle`   | number       | no       | `180`         | Degrees.                                |
| `direction`   | [x, y, z]    | no       | `[0,0,-1]`    | Direction the light points. `[0,-1,0]` points straight down. |

A lit surface's colour is its texture times its material colour, times the light it receives: each light's
ambient fill plus its direct light (`brightness` times how squarely the surface faces it, times attenuation for
point lights). A surface facing away from every light shows only the ambient fill. Material colours are used as
given, so a dark colour stays dark.

For an outdoor or top-down scene, one directional light pointing down and slightly away from the camera, with
`brightness` near `1` and `ambiance` near `0.35`, lights every part of a large level evenly (see
`resources/scenes/dungeon.json`). Point lights fade with distance, so far parts of a big level get dark.

## bodies

| field      | type      | required | default     | notes                                                         |
|------------|-----------|----------|-------------|---------------------------------------------------------------|
| `name`     | string    | yes      |             | Name of the root Body of the instance.                        |
| `model`    | string    | yes      |             | Model file in `resources/` (`.obj`, `.fbx`).                  |
| `material` | string    | no       |             | Name of a material. Applied to **every** mesh of the model. Omit to keep the model's own materials. |
| `position` | [x, y, z] | no       | `[0,0,0]`   |                                                               |
| `rotation` | [x, y, z] | no       | `[0,0,0]`   | Euler angles in degrees.                                      |
| `scale`    | [x, y, z] | no       | `[1,1,1]`   |                                                               |

### Primitive bodies

A body can use `"primitive": "cube"` or `"primitive": "plane"` instead of `"model"`, with an optional `"size"`
(default `1`). `size` is the half extent: a cube of size `0.5` is one unit wide.

The plane is the cube's front face: a square in the local XY plane at local `z = size`, facing +Z. Rotated
`[-90, 0, 0]` to lie flat, its surface sits at `position.y + size`. To put a floor surface at `y = 0`, place the
plane at `y = -size`, as `resources/scripts/DungeonGame.py` does.

Optional top-level `"clips"`: a list of clip file paths (see `docs/ai/clip-format.md`), loaded before bodies.

Optional animation keys on a body:

| field             | type   | default | notes                                                      |
|-------------------|--------|---------|------------------------------------------------------------|
| `animation`       | string | none    | Clip name from the model (see `list_animations`). Starts it at load. |
| `animationSpeed`  | number | `1`     | Playback speed multiplier.                                 |
| `animationLooping`| bool   | `true`  | `false` holds the last pose.                               |

Animators belong to the model, so bodies that use the same model share its clip. The last body in the file wins.

## Example

See `resources/scenes/example.json`. A complete game scene that uses `replaceWorld` and `scripts` is `resources/scenes/dungeon.json`.

## Limits and notes

- Names are used as asset/Body names; keep them unique within their kind.
- A model that fails to load stops the load with an error naming the body.
- Units are engine units; the camera is at the origin looking down -Z by default.

## Testing

`python -I tools/test_scene_format.py` (no GPU) checks every scene in `resources/scenes` against the rules above and confirms each file in `resources/scenes/invalid` is rejected for its reason. It does not run the C++ loader; `tools/test_control_channel.ps1` does that.
