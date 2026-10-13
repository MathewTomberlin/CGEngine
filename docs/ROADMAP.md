# CGEngine roadmap: AI-first engine

Goal: an engine that an AI (or a person working with one) can use to create models, scenes,
skeletons, animations and games through structured data and a control interface, with
performance and documentation treated as first-class requirements.

Status: `[ ]` not started, `[~]` in progress, `[x]` done.

## Current priorities (lead developer, after playtesting round 2)
The order I am working in, and why:
1. **Phase 20, construction.** Requested by the player: a recipe list grouped by category, known recipes shown, recipes with a missing item disabled, unknown recipes hidden, an empty start inventory. It is small and it gives the player a reason to use what they mine.
2. **Phase 21, a smooth terrain skin.** Requested by the player: a one-block slope should be a hill, not a staircase. It is the most visible weakness in the sample, and it touches collision, so it is done with the density parity checks.
3. **Phase 22, far-view load latency.** At view 40, distant ground takes seconds to appear after a move. Measured first, then ordered.
4. **Phase 23, upkeep.** The game file is large, the tests are many scripts, and the Debug build is slow. Each item is done alongside the feature it touches.

Still waiting on the player: 11.14 (the square shadow) and 11.15 (the fall through the ground). Both need a snapshot.

## Phase 1: data-driven content (in progress)
- [x] **1.1** JSON scene loader (materials, lights, model bodies) - `src/Core/Scene/SceneLoader.*`
- [x] **1.2** Example scene and startup hook (`CGENGINE_SCENE`) - `resources/scenes/example.json`
- [x] **1.3** Scene format documentation - `docs/ai/scene-format.md`
- [x] **1.4** Scene validation tests: syntax errors, bad version, bad vectors, unknown material/primitive, missing model (`resources/scenes/invalid/`, checked by `tools/test_control_channel.ps1`)
- [x] **1.5** Primitive bodies in the scene format: `"primitive": "cube"`, `"plane"` or `"sphere"` (a UV sphere, `"size"` is its radius) with `"size"`. Example: `resources/scenes/primitives.json`

## Phase 2: AI control channel (done)
- [x] **2.1** File-based command inbox/outbox (`cg_control/inbox`, `cg_control/outbox`), one JSON command per file, polled each frame. No sockets, no dependencies, works with any agent that can write files.
- [x] **2.2** Commands: `load_scene`, `describe_scene` (bodies, transforms, materials, lights as JSON), `set_transform`, `set_material`, `remove_body`.
- [x] **2.3** `screenshot` command: render to PNG so an agent can look at the result.
- [x] **2.4** `get_stats` command (frame time, draw calls, asset counts).
- [x] **2.5** Control API documentation (`docs/ai/control-api.md`); integration test `tools/test_control_channel.ps1`.

## Phase 3: performance
- [x] **3.1** Baseline frame time in smoke output (`avgFrameMs`). Baseline: Release, example scene, 1500 frames: **4.2 ms/frame** (3 runs: 4.21, 4.20, 4.20). Draw-call count not yet reported.
- [x] **3.2** Renderer hot path. Release, example scene: **3.8 ms -> 0.86 ms per frame**, draw calls **22 -> 8**. Causes fixed: (a) Body re-parenting left stale entries in the root child list, so models were queued and drawn twice (fixed in `Body::attachBody`/`detachBody`); (b) shader uniform locations were looked up with `glGetUniformLocation` on every call (now cached per program). Further batching not yet done.
- [x] **3.3** Asset load timing and caching review. Measured with `tools/measure_scene_load.ps1` (Debug, `loadMs` from `load_scene`). Fixed: (a) every `Mesh` construction re-uploaded its VBO/EBO/VAO and leaked the previous ones, now uploaded once per `MeshData` (`Renderer::getModelData`); (b) `Model::instantiate` appended to its own `sourcePath`, so body names grew with every child and never hit the cache (now a short per-instance key); (c) a reloaded primitive returned the cached Body with its old transform, now a fresh body per load. Body creation was about 1.3 ms per Body (fixed in 5.4). Re-measured: a Caveman FBX loads in 0.3 to 3.6 ms once it is cached (the 86 ms figure predates the model cache). A failed
  model is no longer re-imported on every load: a failed load is remembered with the file's write time, and an unchanged file
  is skipped. Still open: the dungeon room load (31 ms warm, not re-measured).

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
- [x] **6.2** Bodies made from Python during the update lost their animator, so the dungeon's player came from
  `dungeon.json` as a workaround. Not reproduced on the current build: a body made from Python after its clips load
  plays every clip, and so does the dungeon's player, which is back in `DungeonGame.setup` (the scene has no bodies now).
  `tools/test_dungeon_game.ps1` checks the knight clips. The cause of the old failure is not known.
- [x] **6.3** Bodies that share a model had one animator, so every slime hopped in step. Each model instance now copies
  the model's animator onto its root body (`Body::setInstanceAnimator`), and its child meshes use it. `play_clip` and the
  control commands use the body's instance. `cge.animation_time(name)` reads the clock; two slimes started a second apart
  read different times. Instances made from one model still share its clip data.
- [x] **6.4** Frame-rate independent test. `DUNGEON_FIXED_STEP` (seconds) makes every frame advance the same game time, and the state file reports `frames`. `tools/test_dungeon_game.ps1` counts its scripted keys in frames and waits on the frame count, so the walk checks hold at any frame rate. Found while doing it: the old checks only passed on slow frames, because the dungeon caps the frame delta.
- [x] **6.5** Archers are rigged: `tools/blender/archer_rig.py` (bones Torso, Head and Arm, rigid parts) and
  `tools/clips/archer_clips.py` (archer_idle, archer_walk, and archer_draw for each shot, which holds its last pose). The
  dungeon animates every enemy from its state, one animator per enemy (6.3). Brutes are still static.
- [ ] **6.6** The game process exited twice between tool calls with no crash record. Watch for it; if it recurs, capture a dump.
## Phase 7: rendering at scale (engine)
Goal: a world with thousands of objects, and procedurally made geometry, drawn in few calls.
- [x] **7.1** Frustum culling. A mesh whose bounds (transformed by its body's matrix) lie outside the camera's view is not drawn. Skinned meshes are not culled (their bind pose does not bound the animation). `get_stats` reports `culledDraws`.
- [x] **7.2** Instancing. A primitive body can list `"instances"` (position, rotation, scale each); one draw covers all of them, with the visible instances culled each frame. The dungeon's floors and walls use it.
- [x] **7.3** Runtime meshes from Python: `cge.create_mesh_body`, `update_mesh_body` and `remove_mesh_body` take flat arrays (array.array). An update re-uploads the mesh to the GPU before the next frame; removing a body frees its GPU buffers. Checked by `tools/runtime_mesh_check.py` in the control test.
- [x] **7.4** Vertex colours and per-vertex ambient occlusion. Each vertex has a colour that multiplies the surface colour (white by default). The voxel mesher (8.1) darkens face corners next to solid blocks.

## Phase 8: voxel world sample (`docs/games/voxels.md`)
A procedural block world with chunks, terrain, collision, block editing and a day/night cycle, built on Phase 7.
- [x] **8.1** C++ voxel mesher (`cge.VoxelWorld`, `src/Core/Voxel/`): chunks of block ids meshed into runtime meshes, with per-face culling across chunk borders, per-vertex ambient occlusion, and per-block colours. Dirty chunks are re-meshed on demand. Checked offline for the terrain (`tools/test_voxel_gen.py`) and by a screenshot. Greedy meshing came in 9.2. Not yet: a transparent water block.
- [x] **8.2** Terrain: done are the noise, height and sand, grass, stone, snow and water per column (C++, on worker threads), chunks streamed around the camera, trees (one column in a hundred, drawn across chunk borders), and caves (tunnels and rooms carved by 3D noise, `cave_width`). Biomes came in later: deserts, forests and plains from a slow noise field (`biome_at`). Ores came with the stone: coal at any depth, iron only deep (`VoxelTerrain::Coal`, `Iron`, checked by `check_ores`). Rocks: boulders of stone on the ground, one column in about 700 on the plains, twice as many in the desert, half as many in
  the forest (`VoxelTerrain::placeRocks`, a ball of radius 1 or 2 that sits on the ground and goes only into air). Checked by
  `voxel_world_check.py` (some boulders, none in the sea).
- [x] **8.3** Player: gravity, jumping and collision with the blocks (`VoxelPhysics.py`, pure Python and tested offline), a ray cast to pick the block in view, and break (J) and place (K). Edited chunks are re-meshed. Flying (Enter) for exploring.
- [x] **8.4** Day and night: the sun's direction and light, the ambient fill, the sky and the fog change over `VOXEL_DAY_SECONDS`, from the script (`cge.set_light`, `cge.set_environment`). Nine block types on keys 1 to 9, with an on-screen hotbar (11.2).
- [x] **8.5** Save and load of the edited blocks. `VoxelWorld` keeps every edit apart from its chunks, so an edit survives the chunk unloading and is applied when it loads again. `save_edits` and `load_edits` write a binary file; F5 and F9 use it in the sample. Checked by `tools/voxel_world_check.py` from `tools/test_voxels.ps1`.

## Phase 9: voxel performance and scale (from walking the voxel world)
Goal: enormous worlds with no hitching while chunks load.
- [x] **9.1** Terrain and meshing run on worker threads. `update` installs finished chunks and uploads meshes within a 2 ms budget, at least one per call. The frame no longer waits for terrain (it was Python, about 30 ms a chunk) or for meshing.
- [x] **9.2** Greedy meshing on a padded copy of the chunk: no world lookup per face, and coplanar faces merge. About 2.3 times fewer quads than per-face meshing.
- [x] **9.3** Compact voxel vertices, 48 bytes against 80. Byte attributes, even normalised, made each draw several times slower on the NVIDIA driver, so every attribute is a float (measured: render 50 ms to 5 ms with only the position enabled).
- [x] **9.4** Mouse look. The arrow keys' turn past 90 degrees flipped the camera upside down (`Camera::lookAt` only reached yaw in [-90, 90]); fixed. Static meshes no longer skin with the bone matrices an animated model left behind (a `skinned` uniform).
- [ ] **9.5** Region batching (several chunks in one mesh, fewer draws). Deferred: at 700 chunks in view the Release build still runs at about 630 frames a second with a 3.5 ms worst frame, so it is not needed yet. Revisit when there are many more draws per chunk.
- [x] **9.6** Measured and tested. Release, 169 chunks: loaded in 1.2 s, about 1100 frames a second when still, worst frame 8 ms while walking and turning. Debug: about 100 frames a second, worst frame about 15 ms. Release, 700 chunks: about 630 frames a second. `tools/test_voxels.ps1` loads the world, walks and turns with scripted keys, and checks the chunks, the pitch and the worst frame.
- [x] **9.7** Lighting and atmosphere for the voxel world: trees, caves, distance fog, a sky gradient from the horizon colour to the zenith colour, a sun disc with glow (`environment` zenith and sun in scenes, `cge.set_environment`), and the day and night light. Shadows came in 10.5; cave light is 11.6.
- [x] **9.8** The camera turns level at any yaw. The view applied the pitch before the yaw, so once the yaw passed 90 degrees the horizon tilted by the pitch; and the script's look direction (pitch after yaw) disagreed with `Camera::lookAt` off yaw 0. Now the orientation is Rz * Rx * Ry, and `lookAt` reads the angles back in that order. `tools/test_voxels.ps1` checks that the right vector stays level and that the camera looks where the player does.
- [x] **9.9** View radius 14: 841 chunks, about 460,000 quads. Release: all loaded in 1.3 s, about 580 frames a second still, worst frame about 5 ms while walking. The fog ends at the edge of the loaded area, so the edge does not show.
- [x] **9.10** A runtime voxel mesh's arrays are freed once they are on the GPU. Its bounds are computed first, and its counts are kept. At 841 chunks (about 717,000 quads with the caves) the process holds about 295 MB.

## Phase 10: smooth terrain, physics and the next voxel steps (from the voxel sample's growth)
Goal: the terrain looks rounded and real, the blocks obey physics the scripts can change, and the far world stays cheap.
- [x] **10.1** Smooth skin. Surface nets over the blocks' occupancy (`buildSmoothMesh`, `set_smoothing`), so the
  ground, hills and tree crowns are rounded and textured; shaded from the density gradient, coloured by the dominant
  block, with occlusion. One sample per block: 1.08 million quads for 841 chunks, 655 frames a second in Release, load
  1.4 s, 206 MB. Two samples per block gives half-block detail but 4.7 million quads, so it is not the default. Single-block
  features keep their surface (the samples sit at block centres at one sample per block). The finer skin near the
  camera, with a coarser one far away, came in 11.7.
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
- [x] **10.6** An on-screen hotbar (8.4), done in 11.2. Region batching is 9.5.

## Phase 11: making the voxel sample feel right to play (from playing it)
Goal: the world reads well at a human pace, and the engine's own state is easy to see and to test.
Next, in order: 11.14 and 11.15 (both wait for a snapshot from the player), then Phase 15 (view distance), Phase 16
(damage) and Phase 17 (inventory). The creatures' own behaviour (they only wander now) comes with Phase 16.
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
- [x] **11.6** Sky light for caves and the shade under trees. A block's light is full in the open and falls by a fifth for
  each block walked through air from the open sky, down to a fifth beyond eight blocks. It is baked into the mesh colours,
  so it costs nothing per frame. `light_at` queries it, and `tools/voxel_world_check.py` checks that open ground is full
  and that the air far underground is darker. Torch light from block sources is not done.
- [x] **11.7** Level of detail for the smooth skin: the chunks within 3 of the player (`VOXEL_NEAR_RADIUS`) use two samples a
  block (`VOXEL_NEAR_SAMPLES`), the rest one. Seams between the levels are hidden by skirts: a strip a block deep runs down
  each chunk's side from the surface. Measured: near chunks have about 2.7 times the quads of far ones. Release: 841 chunks
  at about 1.2 million quads, 559 frames a second, 239 MB.
- [x] **11.8** Frame rate, measured with `tools/voxel_perf.ps1`: about 650 frames a second standing, turning and walking,
  with the labels and outlines on, and a worst frame of about 5 ms (Release, 841 chunks). The labels and outlines only
  change when their text or target does. The 360 figure came from an earlier build. Debug runs the same world at about 33
  frames a second, so it is not a performance build.
- [x] **11.9** Entities that walk the terrain: `cge.add_voxel_walker` makes a mesh body a walker in C++, moved once per
  update by the same mover as the player (`VoxelWalker.cpp`). Scripts set the velocity and jumps; the engine does gravity,
  collisions and the position. `VoxelWalkers.py` makes twelve creatures that wander near the spawn, run from the player,
  and turn away from each other. Still open: bodies that push each other, and a walker for any body without a script.
- [x] **11.10** Player in water: in a liquid block at the feet or chest the player sinks at 1.2 blocks a second, swims up
  at 3 while Space is held, and walks at 60%. The mover runs without gravity while swimming. At the surface, Space jumps
  out onto a bank a block high. The view under the water is a blue-green murk. `tools/test_swimming.ps1` checks the
  landing, the swim up, the sink and the walk. Not done: a view through the water (its faces are culled from inside).
- [x] **11.11** Falling blocks are drawn as smooth balls, not cubes (`buildBlob`, one mesh for all the blocks in flight, made
  in C++ each update). The settled blocks stay smooth-skinned as before.
- [x] **11.12** Thin trunks read as vines, and skirts hung flat sheets over air. A one-sample surface pulls each vertex
  toward the middle of its cell, which thins features one block wide; the surface now sits a little further out
  (iso 0.3), so trunks are solid. Skirts only go where the neighbour is at another level of detail, and only over solid.
- [x] **11.13** Trees whose trunk is outside the loaded area leave crowns floating. Closed by analysis: a crown can float
  only while its trunk's chunk is unloaded, which happens at the edge of the loaded area, where the fog is about 93 per
  cent complete. Fixing it would need each chunk to wait for its neighbours' terrain, which is not worth the cost there.
- [x] **11.16** Settings page from the pause menu: Settings opens a page with the mouse sensitivity (arrows, 25% to 300%),
  applied at once and saved to `voxel_settings.json`. Escape and Back return to the menu. `tools/test_pause_menu.ps1` checks
  the page, the arrow, the saved file and the way back. Still open: the other settings (graphics, shadows, the sun).
- [x] **11.17** Settings: a time-of-day slider (drag or click the track; it sets the sun, and the choice is saved), and the
  mouse sensitivity now goes up to 500%. `tools/test_pause_menu.ps1` checks the slider and the saved time.
- [ ] **11.14** Intermittent square shadow on the terrain at high sun, with nothing casting it. Not reproduced yet. F12 now saves a screenshot and the game state together (`cg_control/snapshots/`), so the next occurrence can be checked. The shadow edge now fades to lit at the map boundary, which may hide a square edge. Needs an F12 pair from the player.
- [ ] **11.15** Start of game: the player sometimes falls through the ground into the kill zone, then restarts at the spawn
  height and falls again until moved. Not reproduced: a run with focus in another window landed normally, and the 24-seed
  landing check (`tools/spawn_check.py`) passes. The game saves `cg_control/snapshots/fell_*.json` when the player drops
  below the kill zone. The mover now counts cells of unloaded chunks as solid, so a body cannot pass through terrain that
  has not arrived; that is a guard, not the cause, which the snapshot still has to confirm. Needs a snapshot from the player.

## Phase 12: block rendering rules (from the voxel sample, requested by the lead developer)
Goal: each block type decides how it is drawn, so cube blocks, smooth blocks, and blocks in their own mesh can sit in one world.
Today every solid block is part of one smooth skin per chunk, which merges wood and leaves into the ground.
- [x] **12.1** A per-type render rule: `define_block(..., shape="smooth"|"cube", group="terrain"|"own", material=)`. A cube
  block is always in its own mesh (`BlockType::own` and `cube`). Each chunk meshes its terrain, and one mesh per own block type
  it contains (`buildChunkMesh` and `buildSmoothMesh` take the group). Collision reads the same rule: `VoxelWorld::skinAt`
  gives each mesh's skin (`TerrainSkin`, `OwnSkin + id`, `CubeSkin + id`), and `skinDensity` is the largest of them.
- [x] **12.2** Terrain blocks merge into the chunk's one mesh: ground (stone and dirt), grass, sand, gravel, water and snow.
- [x] **12.3** Wood and leaves are separate meshes, one per type and chunk, drawn with `voxel_wood` and `voxel_leaves`. Their
  skin is built from their own blocks only, and the terrain treats them as air, so a canopy does not hide the ground under it.
- [x] **12.4** Brick: a cube block (`VoxelGen.BRICK`, hotbar key `-`). It never merges, so its faces are six quads, and a body
  stands on its top face at the drawn height (not 0.2 above it, as a smooth block would).
- [x] **12.5** The rules are data: `define_block` sets them, the hotbar shows brick, and `tools/voxel_world_check.py` checks
  them (`check_groups`: a leaf alone is in its own mesh, a brick alone is six cube faces, ground is in the terrain mesh, a body
  lands on a brick, and the engine's mover matches the Python physics on a mixed patch). `tools/test_voxel_physics.py` checks
  the cube and largest-skin densities.
- [x] **12.6** A wood block whose top is covered by leaves joins the leaves' skin (`BlockType::joinAbove`, `join_above=`):
  the trunk top and the canopy are one surface, while the trunk's sides and bottom stay wood, and so does a trunk with air
  above it. Checked by `check_groups`; seen in play (the trees worked).
- [x] **12.7** A wood block standing on natural ground (stone, dirt, grass, sand, gravel, snow) joins the terrain at its bottom
  (`join_below=`, a list of block ids). A wood block on a brick or on water keeps its own skin. Checked by `check_groups`.
- [x] **12.8** Water is a semi-transparent mesh: `define_block(..., transparent=True)` draws its mesh after the opaque ones,
  blended and without writing depth (`Body::transparent`, `Renderer::render`), and the material's `opacity` (0.55 for
  `voxel_water`, read from the scene file) multiplies the surface's alpha. The shadow map skips transparent meshes.
- Seams: a separate skin meets the terrain where they touch. The terrain treats a trunk or a leaf as air, so its surface sits
  where the block's own skin does; in the screenshot the two meet without a crack. Level-of-detail seams between own meshes use
  the same skirts as the terrain (11.12), not yet checked in play. Light is not split by mesh (every block shades behind
  it), so the ground under a trunk is shaded: a dark patch, which is what a block overhead should do.

## Phase 13: collision and the camera on the skin (from playtesting: clipping, and no walking up slopes)
Goal: the player, the creatures and the camera meet the surface that is drawn, not the blocks under it. The skin is drawn at
the iso-surface of the blocks' occupancy (density 0.3), which sits about 0.2 blocks out from each solid face, so block
collision leaves the feet 0.2 blocks inside the visible ground and the eye close to the wall it walks into.
- [x] **13.1** A skin field: `VoxelWorld::skinDensity(x, y, z)`, the density the skin is built from (the trilinear occupancy of
  the block centres, with occupancy = solid block types; water and air are open; unloaded cells are solid). A point is inside
  the skin where the density is above 0.3. `physics.density` in `VoxelPhysics.py` is the same, operation for operation.
- [x] **13.2** The body mover tests the skin: five points across at four heights (feet, middle, head and the eye), and a
  blocked move is set flush by bisection. The player and the creatures share it (`VoxelBody.cpp`). A body that starts inside
  the skin is lifted out first. `tools/voxel_world_check.py` compares it with the Python copy on random moves.
- [x] **13.3** Walking up: a walk blocked on the ground climbs a block (`stepUp` 1.0) and snaps back to the surface, so the
  player and creatures walk up hills and over one-block steps. A four-block wall still stops them. Measured while designing:
  0.6 was not enough, since the terrain's columns step by a whole block and the skin makes those steps near-vertical.
  The camera follows the eye's height over a few frames (`VIEW_SMOOTH`), so a climb is seen as a step.
- [x] **13.4** The eye is one of the body's sample points, so the box stops with its eye outside the skin: a wall keeps the
  near plane clear of it, and an overhang stops the head. Not yet seen in play: the camera at a wall corner (a check in a
  screenshot is the next step).
- [x] **13.5** Picking follows the skin: the break and place target is found by marching the ray through the density until it
  crosses the iso-surface, so the outline is on the surface under the crosshair. The block is then walked from half a block
  in front of that surface (`VoxelWorld::skinSurface`, `skin_surface` in Python). Checked by `check_skin_surface` and by eye.
- [x] **13.6** Measured on the skin, in the engine: a walk step of one body takes about 25 microseconds and a fall step about
  10 (the Python call included), so the twelve creatures and the player cost well under a millisecond a frame. No cache is
  needed yet. Still to measure: the frame time of the whole game beside the 650 frames a second of the block version.
- Open question: the level-of-detail skin (11.7) is drawn with two samples a block near the camera, while collision uses
  one. The difference is up to half a block near the player; a finer collision field is possible if it shows.

## Phase 14: light you carry and place (from playtesting: the caves are too dark to explore)
Goal: light blocks and carried torches light caves. Light is what lets a player read a cave; the sky cannot reach
underground, so the sky floor (0.2) is all a cave gets today. Steering from playtesting: the torch is a small item, not a
full block, and light blocks (full cells that give light) are wanted too.
- [x] **14.1** Light blocks: a block gives off light (`BlockType::emits`; the lamp, id 17, gives 255). Its light is full at its
  cell and falls 0.8 a block through air, for 8 blocks, the same walk as sky light; each cell takes the brighter of the two.
  The lamp is a solid block drawn as a cube, so it is placed like any block. `check_lamp` in `voxel_world_check.py` checks
  a sealed pocket: the sky floor before the lamp, 0.8 one block away, 0.8 cubed three blocks away. Not on the hotbar yet.
- [x] **14.2** Small torch as an item, not a block: a torch is a prop on one face of a block (`VoxelWorld::setProp`), drawn as
  a thin stick with a flame (`buildPropMesh`), so it takes a fraction of a block and the grid does not change. Its cell stays
  air for collision, meshing and the walk. It enters the light pass as an emitter at that cell (0.8 of full light, `VoxelTorchLight`),
  from a sparse list per chunk. Placed from the hotbar (`=`) on a block's face, broken by aiming at it (nearer than the block
  behind it), and torches leaning on a block that is broken are broken with it. Chunks in reach are re-lit as for a block edit.
  Limits: torches are kept with the edits in the world but not written to the save file (F5), and the outline is not drawn
  on a torch. Checked by `check_props` (light in the cell and beside it, air to collision, geometry, removal).
- [x] **14.3** Held torch: a point light at the eye (`held_torch` in the scene, `set_light(position=)`), lit and put out with
  L. It is a shader light, so it needs no re-mesh and stays separate from the baked light of 14.1 and 14.2. Point lights are not
  gated by the sky (the shader's `gate`), so it lights a sealed cave. Not yet seen in play: the lit torch and its walls.
- [x] **14.7** Items attach to the skin, not the voxel. A torch stands on the drawn surface of the block it leans on: its
  base is at the skin's crossing (`VoxelWorld::propAnchor`, the same density collision uses), not at the block's face. Before,
  a torch sat inside the skin on most blocks (the skin bulges 0.2 past each face), so it was hidden. `prop_boxes` gives the
  boxes the torch is drawn with, and the aim test uses them. Checked by `check_props`.
- [x] **14.8** Placed torches and lamps light like the held torch: the glow is the warm light colour at 0.8 (it used to be four
  times the block's colour, which clipped to white), and the light a lamp or torch gives its neighbours is tinted by the same
  colour. The lamp block's own colour is dark, so its glow shows as light, not as a white block.
- [x] **14.4** The light colour is warm (the held torch and the placed ones). The held torch flickers, two slow sines a few
  per cent either way. A placed torch's flame wavers too: its glow is scaled in the shader by two sines on the torch's own
  phase (a per-vertex `flicker` value, 0 for every other surface). The light a placed torch gives its neighbours is baked into
  the mesh, so it does not waver; only the flame's glow does. Seen as a still frame only, not in motion yet.
- [x] **14.5** Measured in Release (the check runs inside the engine, `measure_remesh`, median of 9 edits, busy poll): a block
  edit reaches the mesh in about 2.6 ms, a torch placed or removed in about 2.9 ms, and a block edit in a chunk with a lamp
  in it in about 3.0 ms. The light pass adds little next to the meshing itself.
- [x] **14.6** Decided: a torch needs no micro grid. It is drawn at a fraction of a block by `buildPropMesh` and its cell is
  air to collision, so the walk, the mesher and the light pass see no finer cells. The 4x4x4 micro-grid is not built; it stays
  an option for a prop that must collide at its own scale.
- [x] **14.9** Torches are saved with the edits: the edit file is version 2, with the torches after the blocks (x, y, z and
  dir for each). Version 1 files still load. Checked by the save and load round trip in `voxel_world_check.py`.
- [x] **14.10** The aim label names a torch when the crosshair is on one ("Looking at: torch   (left click: take it back)"),
  and the torch is outlined in the break colour as one box round its stick and flame. The rule is the break's own: a torch
  nearer than the block behind it is the one a break takes.
- Interim (not on the roadmap as a fix): the cave floor (0.2 of the sky) could be raised to make caves readable before the
  torches land. Decide after 14.1 if they are still too dark.

## Phase 15: view distance (from playtesting: the view is short)
Goal: the player chooses how far they can see, and a long view stays cheap enough to walk through.
- [x] **15.1** A view distance setting on the settings page: arrows from 6 to 40 chunks each way, with 14 (the old default) in
  the middle. The fog, the chunk requests and the unload radius follow it, and the choice is saved with the other settings.
  "Unlimited" is the last step and is capped at 60 (15.6), because each chunk is held in memory.
  `tools/test_pause_menu.ps1` checks the arrow and the saved file.
- [x] **15.2** Far skins: the chunks more than 6 from the player (`VOXEL_FAR_RADIUS`) are meshed with every second block
  sampled (`buildSmoothMesh` takes a stride; the chunk size is even, so the grids of neighbours line up, and the skirts hide
  the step between levels). At view 40: 2.2 M quads instead of 8.3 M, and the load takes 15 s instead of 32 s. Frame rate
  moved only from 23 to 27, so the cost at 40 is the number of chunk draws, not the quads (15.5). Unlimited stays capped at 40
  until then. Seen in a screenshot at 40 (terraces on the dunes from the coarser sampling, no open seams).
- [~] **15.3** Frame time per view distance, measured in Release (`tools/voxel_perf.ps1`, standing, turning and walking, this
  machine): view 6 (169 chunks) 399 / 380 / 334 fps, worst frame 6 to 7 ms; view 14 (841) 179 / 186 / 161 fps, worst 10 to 15 ms;
  view 24 (2,401) 69 / 70 / 66 fps, worst 24 to 37 ms; view 40 (6,561) 23 / 19 / 17 fps, worst 57 to 70 ms. The "about 650 fps at
  841 chunks" in the notes did not come back: the build from before this round measured 201 / 199 / 178 on the same machine
  today, so this round's HUD and game script cost about 10%. Still open: memory per step (the step list is chosen from these
  numbers, so the memory is the next measurement).
- [x] **15.5** Draw calls at a long view: a region is 4 by 4 chunks, drawn as one body per mesh group (`VoxelWorld.cpp`,
  `RegionChunks`), so 6,561 chunks at view 40 are about 400 regions' bodies, not one per chunk. Option B: a changed region is
  meshed again from its blocks, and no chunk's mesh is kept after upload (the memory stays as it was). A region waits two
  updates after its last change (`RegionSettleUpdates`), so a run of changes is meshed once. Measured at view 40 on this
  machine, before and after: standing 26 to 201 fps, turning 26 to 193, walking 23 to 166; load 37 s to 8 s; worst frame 55 to
  72 ms to 11 to 18 ms. The quads are unchanged (2,217,929). Checked by `voxel_world_check.py` (through `test_voxels.ps1`),
  `test_voxel_net.ps1`, `test_swimming.ps1` and `test_pause_menu.ps1`. Not measured: the cost of a block edit, which re-meshes
  up to 16 chunks on a worker, and a frontier of new chunks while walking. One turning run at view 14 had a single 108 ms
  frame; it did not come back in the next run (worst 5 ms).
- [x] **15.4** Memory on a long walk at 40 (`VOXEL_SEED=1`, Release): the player was moved round a wide circle 40 times, 90 blocks
  a hop, so the chunks behind unloaded and the ones ahead loaded (about 6,750 to 6,880 chunks held throughout). The working
  set went from 624 MB to a peak of 1,064 MB around the 24th hop, then fell back to 675 MB by the end. So the churn does not
  grow without bound: the peak is the chunks being meshed as they come in, and the heap gives most of it back. Not measured:
  a walk of hours, and the GPU's own memory (the buffers are not in the working set).
- [x] **15.6** The cap rises from 40 to 60 chunks: `VIEW_STEPS` and `UNLIMITED_VIEW` in `VoxelGame.py`. The memory figures below
  were taken before 15.7, when the camera clipped at 100 blocks: view 40 is 6,561 chunks, 0.77 GB private memory; view 60 is
  14,641 chunks, 1.46 GB; view 80 is 25,921 chunks, 2.4 GB. View 80 stays out: its memory and frame rate are too heavy for the cap.
  The frame rates in this item were not right, see 15.7.
- [x] **15.8** Far skins are off by default (`VOXEL_FAR_RADIUS=0`). The far skins (stride 2 past 6 chunks) showed dark blotches on
  the ground: each far vertex takes its occlusion and sky light from one air cell, so a shaded cell darkens the whole triangle
  between two far vertices. Averaging those values over the stride cut the blotches but was not kept, since far skins are off.
  With them off, view 40 is 6,561 chunks, 8.4 million quads, 2.2 GB private memory, 96 frames a second standing and walking, 89
  turning, worst frame 41 to 43 ms, loaded in 18 s. The cap is back at 40: 60 measured 4.6 GB without far skins (18 million
  quads). With far skins on, 60 is 1.5 GB (15.6), at the cost of the blotches.
- [x] **15.7** The camera clipped at 100 blocks, about six chunks, so nothing past it was drawn at any view distance. The camera is
  now built with a far plane of (view + 1) chunks and a near plane of 0.1 blocks (`Camera::setPlanes`, `set_camera_planes`, and
  `VoxelGame.set_clip_planes`). The fog still hides the far edge, which is the same distance as before. Screenshots at noon with
  seed 1 (`VOXEL_SEED=1`) show the terrain fading into the haze at view 14, and forest out to the horizon at view 40.
  Corrected frame rates, Release on this machine, standing / walking, worst frame:
  view 14 (841 chunks, 0.56 M quads) 369 to 462 / 354 to 416 fps, worst 5 to 9 ms in two runs; one walking run had a single 114 ms
  frame that did not come back in two later runs (a second single frame of 108 ms was seen earlier at view 14);
  view 40 (6,642 chunks, 2.2 M quads) 150 / 150 fps, worst 10 to 14 ms;
  view 60 (15,004 chunks, 4.6 M quads) 77 / 64 fps, worst 19 to 52 ms, loaded in 15 s.
  The 15.5 numbers were also measured with the 100-block plane, so the speed-up there (26 to 201 fps at view 40) is real for
  the same plane but not what the player sees now. Memory was not re-measured with the plane at its new distance.

## Phase 16: voxel health and damage (from playtesting: one hit breaks everything)
Goal: blocks and creatures take several hits, and what the player has hit looks hit.
- [x] **16.1** Block hardness: each block type has a number of hits to break (stone more than dirt, leaves less than wood,
  lamps and torches one). `define_block(..., hits=)`. The hardness is data, like the other block rules.
- [x] **16.2** Visual damage states: a damaged block shows its stage in three ways. Its colour darkens with the stage (the
  damage is per cell, kept with the edits, so the mesher tints the cell's faces). Its surface gets cracks (a Voronoi border
  pattern in world space, drawn in the voxel shader from a per-vertex damage value; the lines widen with the hits, and fade
  out on the triangles that only reach a hit block). The damage is saved with the edits (edit file version 3, after the
  torches), and a stage change re-meshes only the chunk it is in. Checked by `voxel_world_check.py` (the save and load round
  trip) and seen in screenshots of a two-hit and a one-hit block. The cracks use a 27-cell search per fragment, so the cost
  of many damaged blocks in view is not measured yet (15.3).
- [x] **16.3** Hit feedback: the aimed block's outline flashes white for 0.12 s on each hit; a hit throws three chips of the
  block's colour from the face that was hit, a break throws ten from the middle, and a creature throws chips when it is hit
  and twelve when it dies. The chips are a small particle system (`VoxelParticles.py`): each burst is one runtime mesh body,
  rebuilt each frame, with gravity and shrinking, at most eight bursts at once. Not yet: the engine has no particle system of
  its own, and a burst costs one mesh upload a frame; checked inside the game (`voxel_game_check.py`), seen in play only.
- [x] **16.4** Damage heals: a block not hit for a few seconds goes back one stage (so a half-broken wall is a choice, not a
  trap). The timer is per cell and only runs for damaged cells.
- [x] **16.5** Creatures take hits: each has `HEALTH` (3) and a left click takes one (the aimed creature is outlined, and its
  hits left are on the HUD). A hit pushes it away and flashes it red. A dead one is taken out of the world (its walker and its
  mesh) and leaves a hide on the ground where it fell (16.7). The hide is `VoxelGen.HIDE`, an item that cannot be placed. One in three is a biter: it chases
  the player inside 7 blocks and bites at 1 health each 1.2 s when within 1.3 blocks. The others still run from the player.
  Not yet: the creatures do not fight each other.
- [x] **16.6** The player has health: ten points on a bar above the hotbar. Bites and falls lower it (a fall past 4 blocks costs
  1 point a block beyond that; the spawn drop does not count). It heals 0.25 a second after 6 s without a hit, and does not heal
  while the game is paused. At zero the player is back at the spawn with the inventory kept.
  The hurt shows on screen too: the whole window reddens at the moment of a bite or a fall and fades over 0.4 s
  (`update_hurt_flash`, under the hotbar). Creative mode (C) takes no bites or falls. Not yet: lava (the sample has none).
- [x] **16.7** Dropped items: a creature's hide lies on the ground where it died (`VoxelDrops.py`), a small bobbing cube in the
  item's colour, one mesh body each. Walking within 1.2 blocks of it (and no more than a floor away) takes it, when the inventory
  has room; a full inventory leaves it there. Drops are saved with the inventory (F5 and F9), and creative mode takes none and
  leaves them where they are. A hide dropped in the air falls to the first solid block under it. Checked by
  `tools/test_voxel_drops.py` (no GPU) and `voxel_game_check.py`. Standing on a drop the inventory has no room for says so
  on the HUD. Not yet: each drop rebuilds its mesh every frame.
## Phase 17: inventory and a real hotbar (from playtesting: the hotbar is a line of text)
Goal: items have quantities, the inventory is a grid the player can see and change, and the hotbar is a UI, not text.
- [x] **17.1** An item model: an item is a block id (a torch is one too, though it is a prop), held in a slot with a count up to
  64 (`VoxelInventory.py`, which has no engine calls). Breaking a block gives its item, and a torch on a broken block comes back
  with it; placing takes one from the selected slot. The player started with 16 of each hotbar item; Phase 20.4 empties the start. Creative mode (C, or
  `VOXEL_CREATIVE=1`) keeps the unlimited placing and gives nothing for a break. A hit that would finish a block is refused
  when the inventory has no room for it; a hit that leaves it standing is not. Tools are not items yet (17.5).
- [x] **17.2** The inventory screen (I): three rows of twelve slots, the hotbar at the bottom and two rows above, each slot a box
  with a colour swatch and its count, on a panel with the controls. The world stands still and the mouse is free while it is
  open. Moving a stack is click-to-carry, not a mouse drag: a left click takes a whole stack, a left click on another slot puts
  it down (onto the same item up to 64, or swapping with a different one), and a right click takes half or puts one down. Esc
  or I closes it, and a carried stack goes back into the inventory. A real drag can come later.
- [x] **17.3** A real hotbar UI: twelve boxes at the bottom of the window, each with its key, a colour swatch, and its count when it
  holds more than one. The selected slot is outlined in yellow and the slot under the mouse in white. It is the first row of
  the inventory, so the two are one model. The boxes are overlay rectangles, a new engine feature (`create_rect_body` and
  `set_rect_body`: rectangles over the scene in window pixels, like the labels). Their draw cost is not measured yet; the
  voxel perf script should record it.
- [x] **17.4** The inventory is saved with the edits: F5 writes `cg_control/voxel_inventory.json` beside `voxel_edits.bin`, and F9
  loads both. A missing file keeps the current inventory, and a damaged entry is dropped. It is a JSON file, not part of the
  binary edit file, because the item model lives in Python. Checked by `voxel_game_check.py` (the save and load round trip).
- [x] **17.5** Tools: a pickaxe (`VoxelGen.PICKAXE`, a tool: it does not stack, and cannot be placed). It starts in the first
  row below the hotbar, so the player drags it into the hotbar. When it is the selected slot, a click on stone, coal, iron or
  brick takes two swings (so stone takes two clicks and iron three, not three to five). It uses the hardness of 16.1, so the
  ore is quicker to mine with it. Not yet: no crafting, no other tools, and no wear.
## Phase 18: multi-user interaction (long-term goal; multiplayer voxel is the first stretch goal)
Goal: several people (or an AI and a person) work in one CG Engine world at the same time.
- [~] **18.1** Engine assumptions to change first: the engine is single-process by design. The globals (`world`, `renderer`,
  `assets`, the Python interpreter) are process singletons, and the control channel is one file inbox. Found by building 18.2:
  two processes do not need the singletons changed, because each one is a separate game (a loopback link is enough). A server
  in the same process would need them changed. A headless server (18.3) also needs the engine to start without a window, which
  it does not yet: `World::startWorld` makes the window. Still open: the window-free start, and the state that a presentation
  draws from (the world is drawn straight from `VoxelWorld`). The host of 18.3 is already the one that owns the edits, so the
  window-free start is the step that makes it a server rather than a player with a server's rules.
- [x] **18.2** A transport, as a loopback test: `VoxelNet.py` links two game processes over TCP, JSON lines, non-blocking and polled
  once a frame. One process listens (`VOXEL_NET=listen:7790`), the other connects (`VOXEL_NET=connect:127.0.0.1:7790`), and both
  run the same `VOXEL_SEED`, since each generates the terrain itself. The messages are block, prop, hit (the damage stage) and
  position. A remote edit to a chunk that is not loaded waits until it loads. Checked by `tools/test_voxel_net.ps1`: a break, a
  placement and a hit each show in the other process, and each sees the other's position. Not secure, and only on one machine.
- [~] **18.3** A server that owns the world, first form: the listening game is the host. It applies the edits, torches and damage,
  and runs the block physics. A client makes its edit at once, sends it as a request (`req`), and draws the cells the host sends
  back (`cells`), so a request the host refuses is undone on the client. A client that joins later is sent every edited cell.
  The C++ side records the changed cells (`VoxelWorld::setRecordChanges`, `takeChanges`, `editedCells`, `setDamage`). Checked by
  `tools/test_voxel_net.ps1`: a client's hit, break, placement and torch; a refused placement; a hit on the host; a late client.
  Not yet: the host still has a window (the headless start is 18.1); the host does not arbitrate cells in chunks it has not
  loaded, so the two players must stay within each other's view; a client's own edits from before it connected stay on that
  client; F9 on the host removes no cell the file lacks; a falling block pops on the client (no moving-block draw); the inventory
  is local, so a placement the host refuses still spends the item, and a break the host had already made still gives one.
- [~] **18.4** Stretch goal, the multiplayer voxel game, in its first form: the two processes see each other (a box where the other
  player's feet are, with no name yet), share the edits (blocks, torches, damage), and one breaks what the other placed. Each has
  its own inventory and health, so the inventory per player is in place by construction. Not yet: a name, the creatures (each
  process has its own, so a creature hit in one is not seen in the other), the other player's damage on a player, and a
  headless server (18.3): the host now owns the edits, but it still has a window and a player in it.
- [x] **18.5** Shared authoring: an AI and a person change the same world through the same requests a second player sends. The
  engine takes commands from Python (`cge.add_command`, `docs/ai/control-api.md`), and the voxel sample registers `voxel_edit`,
  which an agent writes to a client's inbox like any command. The host applies it under the click rules and sends the cell
  back. `tools/test_voxel_net.ps1` checks it, and that a bad op is reported as an error. An edit in a chunk the host has not
  loaded is not applied (see 18.3).
## Phase 19: developer console and settings (from playtesting)
Goal: the game can be driven from inside, and its window and sun are under the player's control.
- [x] **19.1** The developer console, opened with backtick (`VoxelConsole.py`). The world stands still while it is open; the
  mouse is freed and given back when it closes. Commands: help, echo, time (names, HH:MM or a fraction), timespeed, tp (spawn, x z
  on the ground, or x y z), pos, heal, hurt, creative, fly, give, drop, clear (inventory, drops, log), spawn creature [count]
  [biter], kill creatures [all|near radius], destroy [radius], break, setblock, fill (up to 20,000 blocks), view, fullscreen, save,
  load, stats, seed and quit. A command that fails prints why, and the game goes on. Checked from inside the game by
  `tools/voxel_console_check.py` (40 checks, through `test_voxels.ps1`), which restores the time, view and window it changes.
  Not yet: tab completion, and commands for the player's speed and the sky.
- [x] **19.2** Settings page: Window (windowed or full screen) and Time speed (paused, slow, normal, fast), both saved with the
  other settings. Full screen is a borderless window the size of the desktop, switched in place (`set_fullscreen`, Windows): the
  window and its GL context are kept, the camera's aspect and the view follow the new size, and the HUD is laid out again. The
  page's rows are re-laid out, and `test_pause_menu.ps1` clicks the new rows.
- [x] **19.3** Engine support for the console: `typed_text` gives the characters typed since the last call, and the Grave and
  Backspace keys are polled by games. The engine's own console is hidden in the voxel game, so backtick opens the game's console.

## Phase 20: construction (from playtesting round 2)
Goal: the player makes items from items. A list of recipes, grouped by category from a dropdown. A known recipe is shown; a
recipe with an item missing is shown disabled; a recipe the player does not know is not shown at all.
- [x] **20.1** Recipe data in `VoxelRecipes.py` (no engine calls): a name, a category, the items it takes and the items it
  makes, as item ids with counts. The first recipe: one wood block makes one torch.
- [x] **20.2** Known recipes. The request did not say how a recipe is learned, so I chose: a recipe is learned the first time the
  player holds one of its items (the notice says "New recipe"). From an empty start a torch would otherwise be unreachable.
  Creative mode knows them all. The known set is saved in the inventory file.
- [x] **20.3** The crafting panel sits above the inventory screen (I): a category dropdown, and a row per known recipe in the
  chosen category. A recipe with an item missing is drawn dim and says what it needs; a click makes a recipe. Creative mode
  counts every item as 99, and crafting takes nothing from the inventory (the output still goes in).
- [x] **20.4** The start inventory is empty. The start pickaxe went too, since the request said nothing: a pickaxe cannot be
  made yet (no recipe), so it comes from `give` or creative until one exists.
- [x] **20.5** Checked by `tools/test_voxel_recipes.py` (offline, 21 checks), by 10 crafting checks in `voxel_game_check.py`
  (through `test_voxels.ps1`, 53 checks in all), and by a screenshot of the panel.
- Open: more recipes and categories, a recipe for the pickaxe (17.5), and other ways to learn a recipe than holding its item.

## Phase 21: a smooth terrain skin (from playtesting round 2: slopes read as steps)
Goal: a one-block rise over a one-block run is a smooth hill, and the ground reads as land, not as blocks under a skin.
- [x] **21.1** Measured first: a staircase of stone, one block per step, built in a test world and photographed. The steps read
  as bands of ledges.
- [x] **21.2** A smoothed density for the terrain: the share of the 2 by 2 by 2 blocks that meet at each block corner, less
  `VoxelTerrainShift` (0.15), interpolated between the corners. A straight 45 degree slope comes out straight, with a ripple
  of about 0.04 of a block. The other skins are unchanged (the occupancy between block centres, iso 0.3). Tried and rejected:
  a 3 by 3 by 3 mean is smoother, but a one-block wall averages to 0.33 and vanishes, so a body walked through it.
  The 2 by 2 by 2 corners keep a one-block wall solid (its middle is 0.35, above the iso).
- [x] **21.3** The mesh and the world read the same corners: `computeSkin` (mesher, padded chunk) and `terrainCorners`
  (world, a per-chunk cache cleared by `markDirty`). A flat face's surface sits 0.1 of a block out (it was 0.2). Measured
  in Release with `test_voxels.ps1`: block edits re-mesh in about 2.8 to 3.7 ms, as before; 841 chunks give 1.15 M quads
  (1.2 M before). The frame rate was measured while another program used much of the CPU: standing 94 to 254 fps across
  runs, and with the creatures off 280 to 340 fps. So the creatures' collision queries are now the main cost of a frame
  (see 22.5); the number needs a quiet machine to be final.
- [x] **21.4** Collision reads the same field: `skinDensity` and `physics.density` change together. `test_voxel_physics.py`
  checks the slab's density (0.85 deep, 0.35 on its face), the skin 0.1 out, a one-block wall, a ledge, a ramp and a wall
  that is not climbed. `voxel_world_check.py` checks the surface on flat ground, a torch on a wall, and the parity suite.
- [ ] **21.5** Open: a dark speckle appears on the surface in one screenshot of the new ramp (not shadows: it stays with shadows
  off). It is probably coal ore in the stone showing through the closer surface; to confirm with a block-type probe.
- Open: a finer field near the camera (the 11.7 question), if this is not enough.

## Phase 22: far-view loading latency (from playtesting round 2: distant ground takes seconds after a move)
Goal: a player who walks sees the ground ahead within a second or two, at any view distance the setting allows.
- [ ] **22.1** Measure where the time goes at view 40 when the player moves: terrain generation, meshing, upload, the pending
  count, and the time from a request to the first upload. The counts go in the state file; a script records them.
- [ ] **22.2** Order the work by distance: the nearest chunks' terrain and meshing first. The worker queue is first in, first
  out now, so far terrain can sit in front of near meshing.
- [ ] **22.3** Coarse first: a far region shows a stride-4 skin as soon as its terrain is in, and the detailed skin follows.
  The skirts keep the seams (11.12).
- [ ] **22.4** The lesson of 15.8 applies: a coarse level must not bring back the blotches from averaged occlusion.
- [ ] **22.5** The creatures' collision queries are the main frame cost after Phase 21 (about 4 ms of a frame with twelve
  creatures in view, on a busy machine). Count the density queries a frame makes and cache what repeats; this is a frame-rate
  item as much as a load item.

## Phase 23: upkeep (lead developer)
- [ ] **23.1** Split `VoxelGame.py` (about 2,000 lines) into modules by concern: net, physics glue, inventory and crafting, HUD,
  and the state file. Behaviour does not change; the tests are the guard.
- [ ] **23.2** One test runner, `tools/run_tests.ps1`, with the suites, a summary, and a non-zero exit on any failure. The suites
  stay separate scripts.
- [ ] **23.3** Debug speed: the mesher is about ten times slower in Debug (about 0.4 s a chunk). A pragma to optimise the file
  did not help, so find the cost first; MSVC's checked iterators are the likely one.
- [ ] **23.4** The `timespeed 0.5` crash in Debug (Phase 19's console): its root cause. It needs a stack from a Debug run, and a
  visible-window capture needs the player's agreement.
- [ ] **23.5** The window-free start (18.1), which a headless server needs before 18.3 is a real server.

## Tooling and quality
- [x] CI build on Windows (`.github/workflows/build.yml`) with Mesa software OpenGL smoke run
- [~] Unit tests for pure logic runnable without a GPU. Done for the dungeon generator (`python -I tools/test_dungeon_gen.py`) and for the scene format and command list (`python -I tools/test_scene_format.py`: example scenes, invalid fixtures, documented commands). The C++ parsers in `SceneLoader` and `ControlChannel` are still covered only by the GPU integration test.
- [x] Documentation index (`docs/ai/README.md`) for agents
- [x] Checks that depend on frame timing: the voxel walk and turn count frames (`VOXEL_FIXED_STEP`, the Enter key as
  the frame count) and default to Release, so the 60 ms worst-frame check measures a build that is meant for it. The
  dungeon walk counts frames (6.4). `tools/spawn_check.py` steps at a fixed time, so it does not depend on the frame rate.

## Open questions
- "JEV" as an AI app controller was mentioned but not specified. Phase 2 is designed so an
  external controller (JEV or otherwise) can drive the engine through the file channel.

## Notes from development (living section)
- The voxel scene crashed at exit in about half of the smoke runs. At exit the asset manager cleared its types in hash order,
  so a Body could be freed after the mesh data it refers to. `World::releaseBodies` removes every Body first; ten runs in a row
  now exit cleanly. The fix is in the order of teardown, found by reading the code (no debugger run).
- The render cost was not where it looked: a body re-parenting bug quietly doubled the work (fixed). Measure before optimising; the first guess was wrong twice.
- Undefined behaviour hides until a Debug build runs it. The asset loaders and `Body::deleteBody` each had one. Smoke and control tests are the safety net; add a check whenever a path is touched.
- The "giant blue shapes" seen around the caveman were not a skinning bug. They came from a scene caveman loaded at too large a scale, so the bodies could not be told apart. Keep scene scales sane when debugging, and name bodies distinctly.
- The Python bindings were a separate `.pyd` with its own copy of the engine's globals. Scripts loaded bodies into a world the renderer never saw. They are now embedded in `main.exe` (`PYBIND11_EMBEDDED_MODULE`), so scripts and the engine share one state. Any new `.cpp` under `src/` needs a CMake configure run, because the source list is a glob.
- A wrong value can hide behind another wrong value. Hue-only colours, an always-on unlit base and a light under the floor each made the others look plausible; fixing one at a time looked worse until all three were fixed. Compare screenshots of every shipped scene before and after any rendering change.
- Look at the scene before trusting it. The dungeon floor sat half a tile above y = 0 for its whole life, because a plane primitive's surface is offset by its size. Cubes hid it; the first real models sank to the waist. Screenshots through the control channel caught it.
- Remaining big-ticket items: Phase 5, batching draws by material (3.2 continuation), and the dungeon's next steps (an inventory, a shop for coins, and sound).
- Voxel notes, overnight round: the two checks that failed first were the test's own state (a stale creature target, a full
  inventory left behind), not the game. Three looks were wrong before they were right: the boulders at a first density were
  about six a chunk and read as a wall of rock (cut eight-fold); the cracks from a value noise read as blotches until the mask
  was a Voronoi border; and the fragment's crack spread over a triangle's whole span until it was weighted by damage. The
  "650 frames a second at 841 chunks" in these notes did not reproduce: the build from before the round gives about 200 on the
  same machine, so the number was from a different condition, and the frame rates in 15.3 are the ones to keep.
- Voxel notes, inventory round: the first in-game check failed three times, and each failure was the check's own state, not the
  game's. A check that changes shared state (a full inventory, a held stack) has to put it back before the next check reads it.
  The HUD's boxes are overlay rectangles in the engine now, so a HUD element can be a box and a swatch without a texture.
- Voxel notes (from the voxel sample, later work): the smooth skin looks much better than the cubes, and shadows read well once the map is cached. The HUD took several passes: the screen's own view is a square in the middle of the window, so window-wide text needs its own view, and text draws over meshes only with a high z order. A closed window used to restart the scene (the run loop now ends).
- Hypotheses get a run before a fix. The background-run theory for the start-of-game fall was tested (a run with focus in another window) and it landed normally. A bug that cannot be reproduced gets instrumentation that saves the state at the moment of failure, not a guess.
- A check that counts seconds fails under load, so count frames. The voxel walk and the dungeon walk hold their keys for
  frames now, and the walk's yaw lands on 180 degrees at any frame rate.
- The Debug build is not a performance build: the 841-chunk world runs at about 33 frames a second in Debug and 650 in
  Release. Measure frame times in Release, and say which build a number came from.
- A probe that places a body at a fixed point can put it behind a wall, since the dungeon's layout depends on the seed.
  The brute looked missing in three spots before it showed beside the player on a fixed seed. Check the placement first.
- How I feel about it, after this round: the sample now has what it was missing for a player: caves that read as caves,
  water to swim in, a desert to walk into, creatures on the ground, and enemies that move like they have bodies. The two
  bugs I still cannot reproduce, the start-of-game fall and the square shadow, are where my guesses have been wrong, so I
  want the snapshot before any more theory. I would keep the Release-only frame checks honest, and move bodies without a
  script (11.9) next, since every new creature currently needs its own Python.
- Asset ids are capped per type (1000 until this round), and an id domain that is full hands out id 0, which is the id of a
  live body. Once voxel chunks meshed one body per block type too, the Body type passed 1000, new bodies took id 0, and the
  parent chain looped (`Body: Reached max depth in getGlobalTransform`). Now the cap is 2^20 (ids are handed out lazily, so
  it costs nothing), and a full type refuses a new resource with an error instead of sharing an id. Found by reading the
  allocator after a crash with no log; the engine's log is written at shutdown, so a crash loses it.
- After the rendering rules (Phase 12): the sample reads as a place with things in it now. Trees are their own shapes on the
  ground, the bricks keep their edges, and the body stands on a brick at the height it is drawn. What I still do not trust is
  anything I have not seen in play: the leaf and trunk seams, and the dark patch under a trunk, are plausible but not tested
  beyond a screenshot. The one thing I would do first next is the torch as a prop (14.2), because the caves still have nothing
  in them that the player brings.
- Review, after the view distance round (Phase 15). The roadmap has three kinds of open item: the ones waiting on a player
  (11.14, 11.15), the ones that are real engine work and do not depend on anyone (15.2 far skins, 16 damage, 17 inventory),
  and the long-term goal (18). The first kind has been waiting longest; I would not invent a fix for either without a snapshot.
  The gaps I found while reading it, now on the roadmap: torches are not in the save file (14.9), the torch on a face is not
  named in the aim label (14.10), and the far view has no level of detail (15.2). How I feel about the sample: it has become a
  place with things in it (caves that read, water to swim in, trees that are their own shapes, torches that light the way),
  and the thing I trust least is what I have only seen in a screenshot. The biggest technical risk is that the engine's
  singletons (`world`, `renderer`, `assets`) assume one world in one process, so the multiplayer goal is a change to the
  engine's shape, not just a feature. I am more excited by the damage and inventory work (it turns the sample from a walk into a
  game) than by anything else on this list, and I would start there after the far skins, because the view is where the
  player first notices the sample is empty.
- The tests run through the control channel with scripted input. They are the best guard against regressions in the game, and they caught a real engine bug (the run loop), so keep adding one for each new interaction.
- Review, round two (lead developer, after the host-owned edits PR was merged). The loop is explore, mine, light, fight and
  rest, and the two things the player keeps hitting are the look of the ground and the wait for the far view. Nothing the
  player brings is used yet, which is the gap construction (Phase 20) closes. My feeling: the smooth skin is the weakest part
  of the sample to look at, and a rounded field is the change most likely to make the land read as real; it is C++ work, so its
  cost can be measured. The engine's biggest risk is still the singletons (18.1), and the multiplayer goal depends on it.
  Order: construction first (small, asked for), then the skin, then the far view, which I measure before I touch.
