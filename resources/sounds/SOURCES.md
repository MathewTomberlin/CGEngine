# Sounds: where they come from

- **Effects** (`break`, `place`, `hurt`, `step`, `pickup`, `bite`, `splash`, `tool_break`): generated with Stable Audio 3 small (sound effects)
  through ComfyUI, with the `comfyui-audio` skill. Each is peak-normalised to 0.8 and cut to length. To regenerate one:

  | file | prompt | seconds | seed |
  |---|---|---|---|
  | break.wav | a wooden crate cracking and breaking apart, dry short impact | 0.6 | 11 |
  | place.wav | a wooden block set down on a dirt floor, soft click | 0.5 | 12 |
  | hurt.wav | a short pained grunt, a creature hurt | 0.6 | 13 |
  | step.wav | a soft footstep on sand, quiet | 0.5 | 14 |
  | pickup.wav | a bright short chime, collecting an item from the ground | 0.7 | 15 |
  | bite.wav | a sharp animal bite, a snap and a low growl | 0.6 | 16 |
  | splash.wav | a body falling into water, a splash | 1.2 | 17 |
  | tool_break.wav | a metal tool snapping in two and clattering on stone, short | 0.8 | 21 |

- **step_grass, step_dirt, step_stone, step_sand, step_gravel, step_snow** (the footsteps of each ground block, 0.4 s): made by
  `tools/make_voxel_sounds.py` from seeded noise (a click for stone, a thump for dirt, a rustle for grass, grains for sand, a crunch
  for gravel, a soft press for snow), so they are the same files each run. `step.wav` is the default for a block with no footstep.
- **lava.wav** (the hiss in lava, 0.9 s): made by `tools/make_voxel_sounds.py` from seeded noise with a few low blips.
- **ambient.wav** (the night loop): made by `tools/make_voxel_sounds.py` from sine waves, because a generated clip does not loop
  seamlessly. Sixteen seconds, and `tools/test_voxel_sounds.py` checks the join.
