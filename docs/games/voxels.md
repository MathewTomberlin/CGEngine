# Voxel sample

A procedural block world in chunks. Run it with:

```
cd build/bin/Debug
set VOXEL_SEED=1
$env:CGENGINE_SCENE = "scenes/voxels.json"; .\main.exe
```

`VOXEL_SEED` picks the world. Controls: W A S D move, the arrow keys turn and look, Space rises, R falls.

## Files

- `resources/scenes/voxels.json`: the scene. It replaces the world, adds a sun, and attaches `VoxelGame` to the root.
- `resources/scripts/VoxelGame.py`: the game. It streams chunks around the camera, moves the camera, and writes
  `cg_control/voxel_state.json` (chunks, quads, dirty chunks, frames, camera) four times a second.
- `resources/scripts/VoxelGen.py`: the terrain, in pure Python: noise, column heights, and the block runs of a column.
  It has no engine imports, so it runs on its own: `python -I tools/test_voxel_gen.py`.
- `resources/voxel_noise.png`: the base texture, a light grey noise. Block colours are tints (`VoxelGen.COLOURS`).
  `python -I tools/make_voxel_texture.py` writes it again.

## The world (`cge.VoxelWorld`)

The world is stored in C++ and meshed there. Python decides what the blocks are.

```python
world = cge.VoxelWorld(16, 64, "voxel")       # chunk size, world height, material for every chunk
world.set_colour(1, cge.Vector3f(0.36, 0.72, 0.26))
world.load_chunk(0, 0)                         # chunk coordinates
world.fill_column(3, 5, 0, 10, 3)              # x, z, from y (inclusive), to y (exclusive), block id
world.set_block(3, 10, 5, 0)                   # remove one block
world.rebuild_dirty(4)                         # re-mesh up to 4 changed chunks
world.unload_chunk(0, 0)
```

- Block 0 is air. Blocks in chunks that are not loaded count as air.
- A chunk is a body named `voxel_chunk_<cx>_<cz>`, with a runtime mesh (below). Its body sits at the chunk's corner.
- The mesher draws a face only where the block beside it is air. Each face is shaded by direction, and each corner by
  ambient occlusion: a corner between solid blocks is darker. The quad is split along its less occluded diagonal.
- A change marks its chunk dirty, and the chunks whose faces it changes. Nothing is meshed until `rebuild_dirty`.
- Call `world.clear()` before dropping a world, so its chunk bodies are removed too.

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

## Limits

- Water is opaque, and there are no trees, caves or lighting yet. Each chunk is one draw call; there is no
  greedy meshing, so a chunk of this terrain is about 900 quads.
- Nothing stops the camera from passing through blocks. Collision, block picking and day and night are next.
