# Voxel sample

A procedural block world in chunks. Run it with:

```
cd build/bin/Debug
set VOXEL_SEED=1
$env:CGENGINE_SCENE = "scenes/voxels.json"; .\main.exe
```

`VOXEL_SEED` picks the world. For the best frame rate, build and run the Release configuration.

## Controls

- W A S D walk, Space jumps, Enter toggles flying (then Space rises and R falls).
- Mouse: left-click captures the mouse. Move it to look. Left-click breaks the block under the crosshair, right-click
  places stone against it, and Escape releases the mouse.
- Without the mouse: the arrow keys turn and look, J breaks, K places.

## Files

- `resources/scenes/voxels.json`: the scene. It replaces the world, adds a sun, and attaches `VoxelGame` to the root.
- `resources/scripts/VoxelGame.py`: the game. It asks for the chunks near the player, moves the player and the camera,
  edits blocks, and writes `cg_control/voxel_state.json` (chunk counts, quads, the player, frame time) four times a second.
- `resources/scripts/VoxelPhysics.py`: gravity, jumping, collision of the player's box with the blocks, and block
  picking. Pure Python: `python -I tools/test_voxel_physics.py` checks it without the engine.
- `resources/scripts/VoxelGen.py`: block ids and colours. The terrain itself is C++ (below).
- `resources/voxel_noise.png`: the base texture, a light grey noise, tinted per block. `tools/make_voxel_texture.py`
  writes it again.

## Architecture

Chunks are built in the background, so the frame never waits for terrain or meshes:

1. `request_chunk` queues a chunk. A worker thread generates its blocks (`src/Core/Voxel/VoxelTerrain.cpp`: smooth noise
   for the columns, stone, dirt and a surface, water up to sea level).
2. `update` installs the finished blocks, marks the chunk and its neighbours dirty, and starts meshing up to 16 dirty
   chunks, nearest the player first. Meshing works on a copy: the chunk with a one-block border of its neighbours
   (`src/Core/Voxel/VoxelMesher.cpp`).
3. The mesher draws only faces against air, merges coplanar faces with the same block, shade and occlusion into one
   quad (greedy meshing), and darkens corners next to solid blocks (ambient occlusion).
4. `update` uploads the finished meshes until its time budget (2 ms) is spent, at least one per call. The upload makes
   the chunk's body (`voxel_chunk_<cx>_<cz>`, a runtime mesh). The GPU copy is replaced in place when a chunk changes.

Chunk vertices are 48 bytes, against 80 for an ordinary mesh vertex: they leave out the bone fields. Every attribute is a
float, because the NVIDIA driver read byte attributes several times slower per draw.

```python
world = cge.VoxelWorld(16, 64, "voxel")       # chunk size, world height, material for every chunk
world.set_colour(1, cge.Vector3f(0.36, 0.72, 0.26))
world.set_terrain(seed=1, sea_level=14, base_height=8, height_range=44, snow_level=40)
world.request_chunk(0, 0)                      # generated on a worker; world.is_loaded(0, 0) once installed
world.update(2.0, focus_x=0, focus_z=0)        # once a frame: install, upload within 2 ms, start meshing
world.set_block(3, 10, 5, 0)                   # remove one block; its chunks are re-meshed by update()
world.get_block(3, 10, 5)                      # 0 for air, and for chunks that are not loaded
world.unload_chunk(0, 0)
world.stats()                                  # chunks, loaded, pending, dirty, meshing, quads, upload_ms
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

The player is a box 0.6 wide and 1.8 tall, with its feet at `pos`. `VoxelPhysics.step` moves it one axis at a time, in
steps of at most 0.1 blocks, and sets it flush against any block it runs into, so it stands on the ground without
hovering. Gravity is 28 blocks per second squared; a jump is 9 blocks per second. The eye is 1.6 above the feet. The
block picked is the first solid one along the view, within 6 blocks. A block is not placed where the player stands.

## Limits

- Water is opaque, and there are no trees, caves, lighting or day and night yet.
- Only stone is placed, and there is no inventory or save yet.
- Each chunk is one draw call. Region batching (several chunks in one mesh) is the next step for very large worlds.
