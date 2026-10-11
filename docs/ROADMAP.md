# CGEngine roadmap: AI-first engine

Goal: an engine that an AI (or a person working with one) can use to create models, scenes,
skeletons, animations and games through structured data and a control interface, with
performance and documentation treated as first-class requirements.

Status: `[ ]` not started, `[~]` in progress, `[x]` done.

## Phase 1: data-driven content (in progress)
- [x] **1.1** JSON scene loader (materials, lights, model bodies) - `src/Core/Scene/SceneLoader.*`
- [x] **1.2** Example scene and startup hook (`CGENGINE_SCENE`) - `resources/scenes/example.json`
- [x] **1.3** Scene format documentation - `docs/ai/scene-format.md`
- [x] **1.4** Scene validation tests: syntax errors, bad version, bad vectors, unknown material/primitive, missing model (`resources/scenes/invalid/`, checked by `tools/test_control_channel.ps1`)
- [x] **1.5** Primitive bodies in the scene format: `"primitive": "cube"` or `"plane"` with `"size"` (no sphere yet). Example: `resources/scenes/primitives.json`

## Phase 2: AI control channel (done except set_material / remove_body)
- [x] **2.1** File-based command inbox/outbox (`cg_control/inbox`, `cg_control/outbox`), one JSON command per file, polled each frame. No sockets, no dependencies, works with any agent that can write files.
- [x] **2.2** Commands: `load_scene`, `describe_scene` (bodies, transforms, materials, lights as JSON), `set_transform`, `set_material`, `remove_body`.
- [x] **2.3** `screenshot` command: render to PNG so an agent can look at the result.
- [x] **2.4** `get_stats` command (frame time, draw calls, asset counts).
- [x] **2.5** Control API documentation (`docs/ai/control-api.md`); integration test `tools/test_control_channel.ps1`.

## Phase 3: performance
- [x] **3.1** Baseline frame time in smoke output (`avgFrameMs`). Baseline: Release, example scene, 1500 frames: **4.2 ms/frame** (3 runs: 4.21, 4.20, 4.20). Draw-call count not yet reported.
- [x] **3.2** Renderer hot path. Release, example scene: **3.8 ms -> 0.86 ms per frame**, draw calls **22 -> 8**. Causes fixed: (a) Body re-parenting left stale entries in the root child list, so models were queued and drawn twice (fixed in `Body::attachBody`/`detachBody`); (b) shader uniform locations were looked up with `glGetUniformLocation` on every call (now cached per program). Further batching not yet done.
- [x] **3.3** Asset load timing and caching review. Measured with `tools/measure_scene_load.ps1` (Debug, `loadMs` from `load_scene`). Fixed: (a) every `Mesh` construction re-uploaded its VBO/EBO/VAO and leaked the previous ones, now uploaded once per `MeshData` (`Renderer::getModelData`); (b) `Model::instantiate` appended to its own `sourcePath`, so body names grew with every child and never hit the cache (now a short per-instance key); (c) a reloaded primitive returned the cached Body with its old transform, now a fresh body per load. Body creation was about 1.3 ms per Body (fixed in 5.4). Still open: a Caveman FBX takes about 86 ms per load, cold or warm; a dungeon room load with no bodies still takes about 31 ms warm; a model that fails to import is re-imported on every load (no negative cache).

## Phase 4: skeletons, animation, games
- [x] **4.1** Skeleton and animation authoring. Done: clip listing, play/pause/speed/looping via control channel and scene files; bone-matrix fix for multi-mesh models; clips defined as JSON keyframes (`load_clip`, scene `"clips"`, `docs/ai/clip-format.md`).
- [x] **4.2** Script hooks reachable from the control channel: `run_script` and `attach_script` for PyScript modules (`docs/ai/script-hooks.md`). C++ behaviours are not exposed yet.
- [x] **4.3** Example game built only from scene files and scripts: a top-down action adventure with procedural rooms, enemies, a sword and a goal (`docs/games/dungeon.md`). Needed the bindings embedded in `main.exe` (see Notes).
- [x] **4.4** Dungeon: three enemy kinds (slime, archer with bolts, brute) and item drops (hearts, coins), checked offline by `tools/test_dungeon_gen.py` and in the engine by `tools/test_dungeon_game.ps1`. Needed an engine fix: a Body deleted in the frame it was created stayed in the start list and crashed the next frame.
- [x] **4.5** Dungeon models made in Blender (knight, slime, archer, brute, arrow, heart, coin, goal, sword arc), built by `tools/blender/dungeon_models.py` and exported as OBJ to `resources/models/dungeon/`. Needed engine fixes: the importer kept only the first mesh of each node (an OBJ with several materials lost every part but one), mesh assets with the same name overwrote each other, and hiding a body did not hide its children.

## Phase 5: art pipeline and fidelity (from building the dungeon with Blender models)
Making real models showed where the engine misrepresents content. These come first, because every asset an agent makes goes through them.
- [x] **5.1** Material colours and lighting. Colours reached the shader as 0-255 bytes and were `normalize()`d, so only the hue survived (white at 58%, dark colours bright, black a division by zero); they are now 0-1 and used as given. Fixing that exposed the lighting: every lit surface also got its full unlit colour, and the shipped lights sat at y = -20, below the floor, lighting the undersides. Lit surfaces now get only ambient fill plus direct light; the default light has a usable ambient fill (`ambiance` 0.1, `brightness` 3.5); the demo and example lights are above the scene; the dungeon has a directional sun; `replaceWorld` also removes the old scene's lights; scene materials take `specular` (0-1); and `uvScale`, which had been landing in the colour intensity, now scales the texture.
- [x] **5.2** The green 2D figure was the demo's player sprite. `replaceWorld` removed only bodies already attached to the root, but a new body attaches in `Body::start()` on the first frame, after a startup scene has loaded; the demo's sprite and its rotating test mesh survived every startup `replaceWorld`. Bodies with no parent yet are now removed too, and `tools/test_dungeon_game.ps1` checks that only dungeon bodies remain.
- [x] **5.3** Animated characters from Blender. The knight (`tools/blender/knight_rig.py`, clips in `tools/clips/knight_clips.py`) and the slime (`tools/blender/slime_rig.py`, clips in `tools/clips/slime_clips.py`) are rigged and play in the dungeon. The rig stood on its side because Blender's FBX export puts a -90 degree X rotation on the armature; turning the armature object +90 degrees in Blender cancels it (found by printing the pose matrices, which all carried the same rotation). `play_clip` starts clips from Python. Limit: bodies that share a model share its animator, so every slime hops in step. Next: per-enemy phase would need one model per slime, or a bone-based offset per instance.
- [x] **5.4** Body cost. Measured with `load_scene_json` of 100 bodies in the running engine (Debug). The cost was not the body structure: each `UniqueIntegerStack` filled a 1000-entry `std::set` on construction, and every Body owns several such stacks, so each Body allocated thousands of nodes. The stacks now hand out ids lazily. Per body: cube 1.3 ms -> 0.14 ms, heart 3.8 -> 0.25 ms, knight 9.1 -> 0.48 ms (load); find and remove dropped by a similar factor. A knight is still six bodies, one per material part, which is fine at this cost.
- [x] **5.5** Reproducible exports. Blender writes OBJ normals and faces in a varying order. `tools/blender/dungeon_models.py` now rewrites each OBJ with sorted normals and sorted faces per material group; three exports in a row are byte-identical, so a model diff means the model changed.
## Phase 6: performance and the next round of gaps (from building the rigged dungeon)
- [x] **6.1** Frame rate. The dungeon ran at 28 FPS (35 ms) with 137 draw calls. The draw path had two per-mesh costs, which are fixed: SFML GL state was pushed and popped around every mesh, and material parameters were read by name every draw (the material entries are now read once a frame). The camera matrix is computed once a frame, and culling (7.1) and instancing (7.2) cut the dungeon to 28 draws and about 7 ms per frame (Debug). Batching by material and merged room meshes are not needed for now.
- [ ] **6.2** Bodies made from Python during the update lose their animator in one case: the player, created in `DungeonGame.setup`, reported "model has no animation clips" while a body made from a control command, and the same rig in a scene file, both played clips. The player now comes from `dungeon.json` as a workaround. Find the cause (suspect: the first load of a skinned model while the update runs).
- [ ] **6.3** Bodies that share a model share one animator, so every slime hops in step. Give instances their own pose, or accept it and make the sync part of the look.
- [x] **6.4** Frame-rate independent test. `DUNGEON_FIXED_STEP` (seconds) makes every frame advance the same game time, and the state file reports `frames`. `tools/test_dungeon_game.ps1` counts its scripted keys in frames and waits on the frame count, so the walk checks hold at any frame rate. Found while doing it: the old checks only passed on slow frames, because the dungeon caps the frame delta.
- [ ] **6.5** Archers and brutes are still static OBJ models. Rig them like the knight and the slime; give the archer a bow-draw clip for its shots.
- [ ] **6.6** The game process exited twice between tool calls with no crash record. Watch for it; if it recurs, capture a dump.
## Phase 7: rendering at scale (engine)
Goal: a world with thousands of objects, and procedurally made geometry, drawn in few calls.
- [x] **7.1** Frustum culling. A mesh whose bounds (transformed by its body's matrix) lie outside the camera's view is not drawn. Skinned meshes are not culled (their bind pose does not bound the animation). `get_stats` reports `culledDraws`.
- [x] **7.2** Instancing. A primitive body can list `"instances"` (position, rotation, scale each); one draw covers all of them, with the visible instances culled each frame. The dungeon's floors and walls use it.
- [x] **7.3** Runtime meshes from Python: `cge.create_mesh_body`, `update_mesh_body` and `remove_mesh_body` take flat arrays (array.array). An update re-uploads the mesh to the GPU before the next frame; removing a body frees its GPU buffers. Checked by `tools/runtime_mesh_check.py` in the control test.
- [x] **7.4** Vertex colours and per-vertex ambient occlusion. Each vertex has a colour that multiplies the surface colour (white by default). The voxel mesher (8.1) darkens face corners next to solid blocks.

## Phase 8: voxel world sample (`docs/games/voxels.md`)
A procedural block world with chunks, terrain, collision, block editing and a day/night cycle, built on Phase 7.
- [x] **8.1** C++ voxel mesher (`cge.VoxelWorld`, `src/Core/Voxel/`): chunks of block ids meshed into runtime meshes, with per-face culling across chunk borders, per-vertex ambient occlusion, and per-block colours. Dirty chunks are re-meshed on demand. Checked offline for the terrain (`tools/test_voxel_gen.py`) and by a screenshot. Not yet: greedy meshing, a transparent water block.
- [~] **8.2** Terrain: done are the noise, height and sand, grass, stone, snow and water per column (C++, on worker threads), chunks streamed around the camera, trees (one column in a hundred, drawn across chunk borders), and caves (tunnels and rooms carved by 3D noise, `cave_width`). Not yet: a biome map beyond height.
- [x] **8.3** Player: gravity, jumping and collision with the blocks (`VoxelPhysics.py`, pure Python and tested offline), a ray cast to pick the block in view, and break (J) and place (K). Edited chunks are re-meshed. Flying (Enter) for exploring.
- [~] **8.4** Day and night: done. The sun's direction and light, the ambient fill, the sky and the fog change over `VOXEL_DAY_SECONDS`, from the script (`cge.set_light`, `cge.set_environment`). A hotbar of seven block types (keys 1 to 7). Not yet: an on-screen hotbar.
- [x] **8.5** Save and load of the edited blocks. `VoxelWorld` keeps every edit apart from its chunks, so an edit survives the chunk unloading and is applied when it loads again. `save_edits` and `load_edits` write a binary file; F5 and F9 use it in the sample. Checked by `tools/voxel_world_check.py` from `tools/test_voxels.ps1`.

## Phase 9: voxel performance and scale (from walking the voxel world)
Goal: enormous worlds with no hitching while chunks load.
- [x] **9.1** Terrain and meshing run on worker threads. `update` installs finished chunks and uploads meshes within a 2 ms budget, at least one per call. The frame no longer waits for terrain (it was Python, about 30 ms a chunk) or for meshing.
- [x] **9.2** Greedy meshing on a padded copy of the chunk: no world lookup per face, and coplanar faces merge. About 2.3 times fewer quads than per-face meshing.
- [x] **9.3** Compact voxel vertices, 48 bytes against 80. Byte attributes, even normalised, made each draw several times slower on the NVIDIA driver, so every attribute is a float (measured: render 50 ms to 5 ms with only the position enabled).
- [x] **9.4** Mouse look. The arrow keys' turn past 90 degrees flipped the camera upside down (`Camera::lookAt` only reached yaw in [-90, 90]); fixed. Static meshes no longer skin with the bone matrices an animated model left behind (a `skinned` uniform).
- [ ] **9.5** Region batching (several chunks in one mesh, fewer draws). Deferred: at 700 chunks in view the Release build still runs at about 630 frames a second with a 3.5 ms worst frame, so it is not needed yet. Revisit when there are many more draws per chunk.
- [x] **9.6** Measured and tested. Release, 169 chunks: loaded in 1.2 s, about 1100 frames a second when still, worst frame 8 ms while walking and turning. Debug: about 100 frames a second, worst frame about 15 ms. Release, 700 chunks: about 630 frames a second. `tools/test_voxels.ps1` loads the world, walks and turns with scripted keys, and checks the chunks, the pitch and the worst frame.
- [~] **9.7** Lighting and atmosphere for the voxel world. Done: trees, caves, distance fog, a sky gradient from the horizon colour to the zenith colour, a sun disc with glow (`environment` zenith and sun in scenes, `cge.set_environment`), and the day and night light. Not yet: shadows.
- [x] **9.8** The camera turns level at any yaw. The view applied the pitch before the yaw, so once the yaw passed 90 degrees the horizon tilted by the pitch; and the script's look direction (pitch after yaw) disagreed with `Camera::lookAt` off yaw 0. Now the orientation is Rz * Rx * Ry, and `lookAt` reads the angles back in that order. `tools/test_voxels.ps1` checks that the right vector stays level and that the camera looks where the player does.
- [x] **9.9** View radius 14: 841 chunks, about 460,000 quads. Release: all loaded in 1.3 s, about 580 frames a second still, worst frame about 5 ms while walking. The fog ends at the edge of the loaded area, so the edge does not show.
- [x] **9.10** A runtime voxel mesh's arrays are freed once they are on the GPU. Its bounds are computed first, and its counts are kept. At 841 chunks (about 717,000 quads with the caves) the process holds about 295 MB.

## Phase 10: smooth terrain, physics and the next voxel steps (from the voxel sample's growth)
Goal: the terrain looks rounded and real, the blocks obey physics the scripts can change, and the far world stays cheap.
- [~] **10.1** Smooth skin: done. Surface nets over the blocks' occupancy (`buildSmoothMesh`, `set_smoothing`), so the
  ground, hills and tree crowns are rounded and textured; shaded from the density gradient, coloured by the dominant
  block, with occlusion. One sample per block: 1.08 million quads for 841 chunks, 655 frames a second in Release, load
  1.4 s, 206 MB. Two samples per block gives half-block detail but 4.7 million quads, so it is not the default. Single-block
  features keep their surface (the samples sit at block centres at one sample per block). Not yet:
  a finer skin near the camera with a coarser one far away (the seams need skirts or matched edges).
- [x] **10.2** Block physics (`VoxelSimulation`, `VoxelBlocks.h`): block types (solid, falls, liquid). Sand and gravel
  fall, and swap down past water. Water is a full source with flowing levels 7 to 1, so it spreads and drains and stays
  finite. Only changed cells are queued, so an idle world costs nothing. Checked by `tools/voxel_world_check.py`.
- [~] **10.3** Programmable rules: done are reaction rules as data (`add_rule`, a block touching a kind becomes another, with
  a chance) and a Python hook that sees each queued block per tick. The simulation runs only on loaded chunks. Not yet: a
  rule file format, and rules that move blocks other than by falling.
- [x] **10.4** Collision in C++ (`VoxelBody.cpp`, `world.step_body`): the same mover as `VoxelPhysics.step`, on the block
  types. The player uses it. `tools/voxel_world_check.py` checks 300 random moves against the Python version and they agree
  to 1e-9. The world's dirty chunks are a set, so `update` no longer scans every chunk each frame.
- [x] **10.5** Shadows: the sun's shadow map (`renderShadowMap`), drawn from the sun around the camera with a 2048 map
  and a 3 by 3 texel filter. Meshes draw into it with the standard shader, which stops early in that pass. Redrawn when the
  camera moves, the sun turns or a tenth of a second passes; skipped while the sun is down. Release: 650 frames a second
  with shadows on (about the same as without, after the caching).
- [ ] **10.6** Region batching (9.5). Not needed yet: with the smooth skin, 841 chunks run at 650 frames a second in Release.
- [ ] **10.7** An on-screen hotbar (8.4).

## Phase 11: making the voxel sample feel right to play (from playing it)
Goal: the world reads well at a human pace, and the engine's own state is easy to see and to test.
- [x] **11.1** Mouse look at a quarter of the first setting (0.00625 degrees a pixel, `VOXEL_MOUSE_SENSITIVITY`). Escape
  opens a pause menu: the mouse is freed and the world stands still; Resume locks the mouse; Quit closes the app.
  `tools/test_pause_menu.ps1` checks each step with scripted input.
- [x] **11.2** Aim feedback: the block under the crosshair is outlined in yellow (the block that breaks), and the cell a
  new block would fill is outlined in the selected block's colour. A label names the block in view, and a hotbar along
  the bottom shows the nine choices with the selected one in brackets.
- [x] **11.3** Full screen with `CGENGINE_FULLSCREEN=1`, at the desktop resolution. It is read once at start-up.
- [x] **11.4** Falling blocks are slower and visible in flight. A block takes most of a tick (0.25 s by default, `VOXEL_SIM_TICK`)
  to move a cell, and the game draws it moving between the cells (`VoxelSimulation` moves, `world.moving_blocks()`).
  Water still steps level by level on the tick.
- [x] **11.5** Textures on voxel meshes are projected in world space and blended by facing (triplanar), so slopes and
  walls keep the block's texture shape instead of stretching.
- [ ] **11.6** Light for caves: propagate sky and torch light through the blocks so caves and overhangs are dark. Without
  it the shadow map is the only sense of depth underground.
- [ ] **11.7** LOD for the smooth skin: two samples a block near the camera and one far away, with the seams matched or
  skirted. Two samples everywhere is 4.7 million quads for 841 chunks.
- [ ] **11.8** Frame rate: the labels and outlines together cut the Release rate from about 650 to about 360 frames a second
  (measured after the HUD landed). Find out which part costs it (the per-frame text state switch, or the outline uploads)
  before anything else is optimised.
- [ ] **11.9** Entities: the player's box mover is C++ (`step_body`), but other bodies have no collision. A body component
  that uses the same mover, so scripts can make many walking or falling bodies.
- [ ] **11.10** Player in water: water is not solid, so the player falls through it. A slower fall and buoyancy in liquid.

## Tooling and quality
- [x] CI build on Windows (`.github/workflows/build.yml`) with Mesa software OpenGL smoke run
- [~] Unit tests for pure logic runnable without a GPU. Done for the dungeon generator (`python -I tools/test_dungeon_gen.py`) and for the scene format and command list (`python -I tools/test_scene_format.py`: example scenes, invalid fixtures, documented commands). The C++ parsers in `SceneLoader` and `ControlChannel` are still covered only by the GPU integration test.
- [x] Documentation index (`docs/ai/README.md`) for agents

## Open questions
- "JEV" as an AI app controller was mentioned but not specified. Phase 2 is designed so an
  external controller (JEV or otherwise) can drive the engine through the file channel.

## Notes from development (living section)
- The render cost was not where it looked: a body re-parenting bug quietly doubled the work (fixed). Measure before optimising; the first guess was wrong twice.
- Undefined behaviour hides until a Debug build runs it. The asset loaders and `Body::deleteBody` each had one. Smoke and control tests are the safety net; add a check whenever a path is touched.
- The "giant blue shapes" seen around the caveman were not a skinning bug. They came from a scene caveman loaded at too large a scale, so the bodies could not be told apart. Keep scene scales sane when debugging, and name bodies distinctly.
- The Python bindings were a separate `.pyd` with its own copy of the engine's globals. Scripts loaded bodies into a world the renderer never saw. They are now embedded in `main.exe` (`PYBIND11_EMBEDDED_MODULE`), so scripts and the engine share one state. Any new `.cpp` under `src/` needs a CMake configure run, because the source list is a glob.
- A wrong value can hide behind another wrong value. Hue-only colours, an always-on unlit base and a light under the floor each made the others look plausible; fixing one at a time looked worse until all three were fixed. Compare screenshots of every shipped scene before and after any rendering change.
- Look at the scene before trusting it. The dungeon floor sat half a tile above y = 0 for its whole life, because a plane primitive's surface is offset by its size. Cubes hid it; the first real models sank to the waist. Screenshots through the control channel caught it.
- Remaining big-ticket items: Phase 5, batching draws by material (3.2 continuation), and the dungeon's next steps (an inventory, a shop for coins, and sound).
- Voxel notes (from the voxel sample, later work): the smooth skin looks much better than the cubes, and shadows read well once the map is cached. The HUD took several passes: the screen's own view is a square in the middle of the window, so window-wide text needs its own view, and text draws over meshes only with a high z order. A closed window used to restart the scene (the run loop now ends).
- How I feel about it: the engine is in good shape for building games quickly from Python, and the voxel sample feels like a real game now. My worries are the frame-rate drop from the HUD (11.8), the `Body` draw path that casts a transformable to a shape to draw text (it works, but it is undefined behaviour and should store a drawable), and tests that depend on frame timing. I want to fix those before adding more features.
- The tests run through the control channel with scripted input. They are the best guard against regressions in the game, and they caught a real engine bug (the run loop), so keep adding one for each new interaction.
