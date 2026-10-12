# Voxel sample

A procedural block world in chunks, with trees, a day and night cycle and distance fog. Run it with:

```
cd build/bin/Release
set VOXEL_SEED=1
$env:CGENGINE_SCENE = "scenes/voxels.json"; $env:CGENGINE_FULLSCREEN = "1"; .\main.exe
```

`VOXEL_SEED` picks the world. `VOXEL_TIME` sets the time of day at the start: 0 is sunrise, 0.25 noon, 0.5 sunset and
0.75 midnight. `VOXEL_DAY_SECONDS` sets how long a day lasts (240 seconds by default). `VOXEL_FIXED_STEP` (seconds) makes every frame advance that
much game time; the scripted tests use it. `VOXEL_SPAWN="x,z"` starts the player over that column. For the best frame
rate, build and run the Release configuration.

## Controls

- W A S D walk, Space jumps, Enter toggles flying (then Space rises and R falls).
- In water (a liquid block at the feet or chest) the player sinks slowly, Space swims up, and walking is slower. The
  player floats at the surface and cannot climb out onto a bank yet.
- Mouse: left-click captures the mouse. Move it to look. Left-click breaks the block under the crosshair, right-click
  places the selected block against it.
- Escape opens the pause menu: the mouse is freed, the world stands still, and Resume (click it) locks the mouse again.
  Quit (click it) closes the app. `VOXEL_MOUSE_SENSITIVITY` sets the look speed in degrees per pixel (0.00625 by default).
- Without the mouse: the arrow keys turn and look, J breaks, K places.
- Keys 1 to 9 choose the block to place (the hotbar: stone, dirt, grass, wood, leaves, sand, gravel, snow, water). Sand and
  gravel fall, and water falls and flows. F5 saves the edited blocks to `cg_control/voxel_edits.bin`, and F9 loads them back. F12 saves a screenshot and the
  game state to `cg_control/snapshots/` (use it when something looks wrong).

## The look: day, night and fog

- The sun crosses the sky in `VOXEL_DAY_SECONDS`. Its light, the ambient fill, the sky and the fog all follow it: a bright
  sun by day, an orange light at sunrise and sunset, and a dim blue moon at night.
- The fog starts halfway to the edge of the loaded chunks and is complete at that edge, so the land fades into the sky
  rather than ending in a line.
- Trees grow on grass, about one column in a hundred. A tree's leaves can reach into the next chunk, and each chunk
  draws the parts of the trees that reach it, so the trees have no seams.
- Caves are tunnels and rooms carved out of the stone, below the surface, from two 3D noise fields. The sky is a
  gradient from the horizon to the zenith, with the sun's disc and glow, and the fog takes the horizon colour.
- Sky light darkens the caves and the ground under trees. A block's light is full in the open and falls by a fifth for
  each block walked through air from the sky, down to a fifth beyond eight blocks. It is baked into the mesh colours, so
  it costs nothing per frame.
- The sun casts shadows (`VOXEL_SHADOWS=0` turns them off; `VOXEL_SHADOW_RADIUS`, 40 blocks by default, sets how far
  they reach). The map is drawn again when the camera moves, the sun turns, or a tenth of a second passes, and not at
  all while the sun is down.

## Block physics

Each block id has a type (`define_block`): solid (it blocks movement), falls (sand and gravel move down into air, or
swap down past water), and liquid (water flows). Water is a full source, block 5, and flows as levels 7 to 1 (blocks 9
to 15): a flow spreads one level at a time, and drains when nothing feeds it, so it stays finite.

The simulation only looks at cells that changed: a block set, or a block that moved, queues its cell and its neighbours.
A quiet world queues nothing and costs nothing, and a far chunk that nothing touches is never looked at.

```python
world.define_block(9, cge.Vector3f(0.5, 0.5, 0.5), solid=False, falls=False, liquid=True)
world.simulate(dt, 1.5)                  # run the due ticks within 1.5 ms of work (ticks every 0.1 s)
world.add_rule(1, 5, 2, 0.5)             # grass touching water becomes dirt, half the time per tick
world.set_tick_hook(lambda x, y, z, block: 7 if block == 3 and y > 60 else None)   # any Python rule
world.is_solid(x, y, z)                  # collision queries use the types too
world.active_count()                     # cells queued: 0 when everything has settled
```

## Files

- `resources/scenes/voxels.json`: the scene. It replaces the world, sets the starting sky and fog, adds a sun, and
  attaches `VoxelGame` to the root.
- `resources/scripts/VoxelGame.py`: the game. It asks for the chunks near the player (`VIEW_RADIUS` = 14 chunks each way,
  841 in all), moves the player and the camera, edits blocks, runs the day and night cycle, and writes
  `cg_control/voxel_state.json` four times a second: chunk counts, quads, the player, the frame time, the time of day, the
  sun, the fog, and the camera's own basis.
- `resources/scripts/VoxelPhysics.py`: the Python reference for the player's mover (the game runs `world.step_body` in C++), and block
  picking. Pure Python: `python -I tools/test_voxel_physics.py` checks it without the engine.
- `resources/scripts/VoxelWalkers.py`: creatures that wander the terrain (`VOXEL_WALKERS`, 12 by default). Each is a box
  mesh moved by `world.step_body`, so it stands on the ground, turns at walls, jumps a block, and waits for its chunk.
  `tools/voxel_world_check.py` checks them.
- `resources/scripts/VoxelShapes.py`: the box meshes the outline and the creatures are made from.
- `resources/scripts/VoxelGen.py`: block ids, colours and the tree chance. The terrain itself is C++ (below).
- `resources/voxel_noise.png`: the base texture, a light grey noise, tinted per block. `tools/make_voxel_texture.py`
  writes it again.

## Architecture

Chunks are built in the background, so the frame never waits for terrain or meshes:

1. `request_chunk` queues a chunk. A worker thread generates its blocks (`src/Core/Voxel/VoxelTerrain.cpp`: smooth noise
   for the columns, stone, dirt and a surface, water up to sea level, and trees).
2. `update` installs the finished blocks, marks the chunk and its neighbours dirty, and starts meshing up to 16 dirty
   chunks, nearest the player first. Meshing works on a copy: the chunk with a one-block border of its neighbours
   (`src/Core/Voxel/VoxelMesher.cpp`).
3. The mesher draws only faces against air, merges coplanar faces with the same block, shade and occlusion into one
   quad (greedy meshing), and darkens corners next to solid blocks (ambient occlusion).
4. `update` uploads the finished meshes until its time budget (2 ms) is spent, at least one per call. The upload makes
   the chunk's body (`voxel_chunk_<cx>_<cz>`, a runtime mesh). The GPU copy is replaced in place when a chunk changes.

The surface is a smooth skin by default (`VOXEL_SMOOTH`, samples per block, 1 by default; 0 draws cubes). The skin is
surface nets (`buildSmoothMesh` in `src/Core/Voxel/VoxelMesher.cpp`): the blocks' occupancy, interpolated between block
centres, is a density, and the surface is where it crosses one half. The blocks stay the truth for collision and editing;
the skin is what is drawn. Each chunk's skin is a function of the blocks around it, so chunks join without cracks.

Chunk vertices are 48 bytes, against 80 for an ordinary mesh vertex: they leave out the bone fields. Every attribute is a
float, because the NVIDIA driver read byte attributes several times slower per draw.

Trees are placed by a hash of each column, so every chunk computes the same trees. A trunk is 4 to 6 blocks of wood,
with a crown of leaves. The crown is solid, so it casts the same ambient occlusion as the stone.

```python
world = cge.VoxelWorld(16, 64, "voxel")       # chunk size, world height, material for every chunk
world.set_colour(1, cge.Vector3f(0.36, 0.72, 0.26))
world.set_terrain(seed=1, sea_level=14, base_height=8, height_range=44, snow_level=40, tree_chance=0.01)
world.request_chunk(0, 0)                      # generated on a worker; world.is_loaded(0, 0) once installed
world.update(2.0, focus_x=0, focus_z=0)        # once a frame: install, upload within 2 ms, start meshing
world.set_block(3, 10, 5, 0)                   # remove one block; its chunks are re-meshed by update()
world.get_block(3, 10, 5)                      # 0 for air, and for chunks that are not loaded
world.light_at(3, 10, 5)                       # sky light, 0 to 255 (computed on demand, for queries)
world.unload_chunk(0, 0)
world.stats()                                  # chunks, loaded, pending, dirty, meshing, quads, upload_ms

cge.set_environment(sky=cge.Vector3f(0.56, 0.76, 0.95), fog=cge.Vector3f(0.56, 0.76, 0.95),
                    fog_start=52, fog_end=104)  # the sky is the clear colour; fog fades from start to end
cge.set_light("sun", direction=cge.Vector3f(0, -1, 0), brightness=0.9)   # a scene light, changed live
```

Only `update` and the block setters touch the world; the workers see copies. Call `world.clear()` before dropping a
world, so its chunk bodies are removed too.

## Runtime meshes (`cge.create_mesh_body`)

A mesh can be made from arrays. Arrays are `array.array`: `'f'` for numbers that are float32, `'I'` for indices.

```python
from array import array
cge.create_mesh_body("tri", array("f", [0, 0, 0, 1, 0, 0, 0, 1, 0]),   # positions, 3 per vertex
                     array("f"), array("f", [1, 0, 0] * 3),           # normals (empty: up), colours (3 per vertex)
                     array("f"), array("f"),                           # uvs (2 per vertex), material ids
                     array("I", [0, 1, 2]), ["voxel"], cge.Vector3f(0, 0, -5))
cge.update_mesh_body("tri", ...)   # same arguments; the GPU copy is updated before the next frame
cge.remove_mesh_body("tri")        # removes the body and frees its GPU buffers
```

Only bodies made this way can be updated. Each vertex's colour multiplies the surface colour, so colours tint a texture.

## Player

The player is a box 0.6 wide and 1.8 tall, with its feet at `pos`. `world.step_body` (C++) moves it one axis at a time, in
steps of at most 0.1 blocks, and sets it flush against any block it runs into, so it stands on the ground without
hovering. Gravity is 28 blocks per second squared; a jump is 9 blocks per second. The eye is 1.6 above the feet. The
block picked is the first solid one along the view, within 6 blocks. A block is not placed where the player stands.

## Performance

Release, 841 chunks in view (about 1.2 million quads with the near detail): about 560 frames a second while standing
still (11.7 in the roadmap). `tools/test_voxels.ps1` checks the load, the walk,
the view's level and the worst frame.

## Limits

- Light is baked when a chunk is meshed, so an edit that lets the sky into a cave shows once the chunks around it are
  re-meshed, a few frames later.
- The simulation runs on the loaded chunks only. The player's collision is in C++ (`world.step_body`), and the Python
  version (`VoxelPhysics.step`) stays as the reference that the tests compare with.
- Water is opaque, and the player cannot climb out of a pool onto its bank. The smooth skin has two levels of detail: two samples a block within 3 chunks of the player, and one a block beyond that.
- Each chunk is one draw call. Region batching (several chunks in one mesh) is the next step if the draw count
  becomes the limit.
