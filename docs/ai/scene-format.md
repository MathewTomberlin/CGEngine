# Scene file format (version 1)

A scene is a JSON document that describes materials, lights and model bodies. Load one with:

- `CGENGINE_SCENE=scenes/example.json` (environment variable, read at startup; path is relative to the exe folder), or
- `SceneLoader::loadFile(path)` from C++.

Failures are reported with a message; a bad scene never silently produces a partial result in smoke mode (it exits 1).

## Top level

```json
{
  "version": 1,
  "materials": [ ... ],
  "lights": [ ... ],
  "bodies": [ ... ]
}
```

`version` must be `1`. The three lists are optional. Order matters only for references: materials are created before bodies, so a body can name any material in the file.

## materials

| field            | type    | required | default | notes                                        |
|------------------|---------|----------|---------|----------------------------------------------|
| `name`           | string  | yes      |         | Unique within materials. Referenced by bodies. |
| `diffuseTexture` | string  | yes      |         | File name in `resources/` (e.g. `brick_tile.png`). |
| `uvScale`        | number  | no       | `1`     | Texture repeat count across the surface.     |

## lights

| field         | type         | required | default       | notes                                   |
|---------------|--------------|----------|---------------|-----------------------------------------|
| `name`        | string       | yes      |               | Unique within lights.                   |
| `position`    | [x, y, z]    | no       | `[0,0,0]`     |                                         |
| `directional` | bool         | no       | `false`       | Directional lights ignore position.     |
| `brightness`  | number       | no       | `5`           |                                         |
| `color`       | [r, g, b]    | no       | `[1,1,1]`     | 0 to 1 per channel.                     |
| `attenuation` | number       | no       | `0.005`       |                                         |
| `ambiance`    | number       | no       | `0.001`       |                                         |
| `coneAngle`   | number       | no       | `180`         | Degrees.                                |
| `direction`   | [x, y, z]    | no       | `[0,0,-1]`    | Direction the light points.             |

## bodies

| field      | type      | required | default     | notes                                                         |
|------------|-----------|----------|-------------|---------------------------------------------------------------|
| `name`     | string    | yes      |             | Name of the root Body of the instance.                        |
| `model`    | string    | yes      |             | Model file in `resources/` (`.obj`, `.fbx`).                  |
| `material` | string    | no       |             | Name of a material. Applied to **every** mesh of the model. Omit to keep the model's own materials. |
| `position` | [x, y, z] | no       | `[0,0,0]`   |                                                               |
| `rotation` | [x, y, z] | no       | `[0,0,0]`   | Euler angles in degrees.                                      |
| `scale`    | [x, y, z] | no       | `[1,1,1]`   |                                                               |

Optional animation keys on a body:

| field             | type   | default | notes                                                      |
|-------------------|--------|---------|------------------------------------------------------------|
| `animation`       | string | none    | Clip name from the model (see `list_animations`). Starts it at load. |
| `animationSpeed`  | number | `1`     | Playback speed multiplier.                                 |
| `animationLooping`| bool   | `true`  | `false` holds the last pose.                               |

Animators belong to the model, so bodies that use the same model share its clip. The last body in the file wins.

## Example

See `resources/scenes/example.json`.

## Limits and notes

- Names are used as asset/Body names; keep them unique within their kind.
- A model that fails to load stops the load with an error naming the body.
- Units are engine units; the camera is at the origin looking down -Z by default.
