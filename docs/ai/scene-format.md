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
- `environment`: the sky and the distance fog. See below.
- `scripts`: attaches a Python script module to a named body. Each entry is `{ "body": name, "module": name, "domain": "update" }`. `domain` is `start`, `update` (default) or `delete`. The module must be in the `scripts` folder and define `create_instance()`. See `docs/ai/script-hooks.md`.

## materials

| field            | type    | required | default | notes                                        |
|------------------|---------|----------|---------|----------------------------------------------|
| `name`           | string  | yes      |         | Unique within materials. Referenced by bodies. |
| `diffuseTexture` | string  | yes      |         | File name in `resources/` (e.g. `brick_tile.png`). |
| `uvScale`        | number  | no       | `1`     | Texture repeat count across the surface.     |
| `specular`       | number  | no       | `1`     | Strength of the white highlight, 0 (matte) to 1. |
| `tint`           | [r, g, b] | no     | `[1, 1, 1]` | Multiplies the texture's colour, each 0 to 1. White is no tint. |
| `repeat`         | bool    | no       | `false` | The texture repeats past its edges, instead of stretching its edge pixels. Applies to the texture file, so every material that uses it repeats too. |
| `normalTexture`  | string  | no       |         | A tangent-space normal map, a file in `resources/`. Its UVs use `uvScale`. Lit surfaces only. Blue points out of the surface, as in Blender's and most engines' normal maps. |
| `normalStrength` | number  | no       | `1`     | How far the normal map tilts the surface: 1 as made, 0 for the flat surface (the same as no map). Not negative. |
| `metallic`       | number  | no       | `0`     | 0 for a dielectric (its highlight is the material's specular colour, white by default), 1 for a metal (the highlight takes the surface's colour). 0 to 1. |

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

## environment

| field       | type      | required | default     | notes                                                          |
|-------------|-----------|----------|-------------|----------------------------------------------------------------|
| `skyColour` | [r, g, b] | no       | `[0,0,0]`   | What the screen is cleared to, so it is the sky seen past the scenery. 0 to 1 per channel. |
| `fogColour` | [r, g, b] | no       | `[0,0,0]`   | The colour distant surfaces fade into. Use the sky colour, so the horizon blends into the sky. |
| `fogStart`  | number    | no       | `0`         | Distance from the camera, in units, where the fog begins.      |
| `fogEnd`    | number    | no       | `0`         | Distance where the fog is complete. The fog is off unless `fogEnd` is past `fogStart`. |
| `zenithColour` | [r, g, b] | no    | unset       | The sky straight up. Unset, the sky is one colour (`skyColour`). Otherwise a gradient from `skyColour` at the horizon. |
| `sunDirection` | [x, y, z] | no    | `[0,1,0]`   | Direction towards the sun. Used by the sun's disc.             |
| `sunColour` | [r, g, b] | no       | `[0,0,0]`   | The sun's disc and glow. Black (the default) hides it.         |
| `sunSize`   | number    | no       | `1.5`       | The sun's angular radius, in degrees.                          |
| `shadows`   | bool      | no       | `false`     | The sun casts shadows, from a map drawn around the camera. Costs a pass over the meshes when the sun is up. |
| `shadowRadius` | number | no      | `48`        | Half the width of the shadowed area, in blocks. Beyond it, surfaces are lit. |

Fog applies to every surface drawn with the standard shader, lit or not. With `replaceWorld`, the environment
returns to the defaults before the file's own values are read. Scripts change it with `cge.set_environment(...)`.

```json
"environment": { "skyColour": [0.56, 0.76, 0.95], "fogColour": [0.56, 0.76, 0.95], "fogStart": 84, "fogEnd": 168 }
```

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

A body can use `"primitive": "cube"`, `"primitive": "plane"` or `"primitive": "sphere"` instead of `"model"`, with an optional `"size"` (a sphere's size is its radius)
(default `1`). `size` is the half extent: a cube of size `0.5` is one unit wide.

The plane is the cube's front face: a square in the local XY plane at local `z = size`, facing +Z. Rotated
`[-90, 0, 0]` to lie flat, its surface sits at `position.y + size`. To put a floor surface at `y = 0`, place the
plane at `y = -size`, as `resources/scripts/DungeonGame.py` does.

Instancing: a primitive body can list `"instances"`, one entry per copy. Each entry has an optional `position`,
`rotation` (degrees) and `scale`, relative to the body. The body is drawn once for all of them, in one draw call,
and each instance is culled against the view on its own. The body's own transform still applies to every instance.
`"instances"` is only accepted on primitive bodies.

```json
{ "name": "floor", "primitive": "plane", "size": 0.5, "material": "floor",
  "instances": [ { "position": [0, -0.5, 0], "rotation": [-90, 0, 0] }, { "position": [1, -0.5, 0], "rotation": [-90, 0, 0] } ] }
```

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
