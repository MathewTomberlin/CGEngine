# Animation clip format (version 1)

A clip is a JSON file that keyframes bones of a skeletal model. Once loaded it is a normal animation:
it appears in `list_animations` and can be played with `play_animation` or by a scene body's `animation` key.

Load a clip with the control command `load_clip` (`{"path": "clips/wave.json"}`), or list clip files in a
scene with the top-level `"clips"` key. Paths are relative to the exe folder.

## Fields

```json
{
  "version": 1,
  "name": "wave",
  "model": "Caveman_Test2.fbx",
  "durationSeconds": 2.0,
  "channels": [
    { "bone": "UpperArm.R", "rotation": [
        { "t": 0.0, "euler": [0, 0, 0] },
        { "t": 1.0, "euler": [0, 0, -60] },
        { "t": 2.0, "euler": [0, 0, 0] } ] }
  ]
}
```

| field             | required | notes |
|-------------------|----------|-------|
| `version`         | yes      | Must be `1`. |
| `name`            | yes      | Unique across all animations. Reusing a name is an error. |
| `model`           | yes      | Skeletal model file in `resources/`. The model must have at least one imported clip (its node hierarchy is reused). |
| `durationSeconds` | yes      | Clip length, positive. |
| `channels`        | yes      | One entry per bone. Each bone may appear once. |

Each channel has `bone` (must be a bone of the model; `list_animations` returns the names in its `bones` field)
and any of `position`, `rotation`, `scale`:

Keys are relative to each bone's rest pose (its position and rotation in the model's hierarchy):

- `position`: keys `{ "t": seconds, "v": [x, y, z] }`, an offset added to the rest position, in the model's units.
- `rotation`: keys `{ "t": seconds, "euler": [x, y, z] }`, Euler angles in **degrees**, applied on top of the rest rotation. `[0, 0, 0]` is the rest pose.
- `scale`: keys `{ "t": seconds, "v": [x, y, z] }`, an absolute scale.

Rules:

- Key times are in seconds, strictly increasing within a property, and between 0 and `durationSeconds`.
- A property you leave out holds the bone's rest value.
- The clip loops by default when played with `looping: true`; scene and control settings control this.

## Errors

A clip is validated completely before anything is created. An invalid clip is rejected with a message that names
the bone or key at fault, and nothing is registered:

- unknown bone, or a bone listed twice
- key times out of order or outside the clip
- a name that already exists
- a model that is not skeletal or has no clip to take the hierarchy from

Invalid examples: `resources/clips/invalid/`.

## Limits

- Clips are evaluated per model, like imported clips: bodies built from the same model share playback.
- Time is in seconds; the engine's internal tick rate is not exposed.
