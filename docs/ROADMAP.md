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
- [~] **8.2** Terrain: done are the noise, height and sand, grass, stone, snow and water per column (C++, on worker threads), chunks streamed around the camera, trees (one column in a hundred, drawn across chunk borders), and caves (tunnels and rooms carved by 3D noise, `cave_width`). Biomes came in later: deserts, forests and plains from a slow noise field (`biome_at`). Not yet: rocks and ores per biome.
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
Next, in order: 11.14 and 11.15 (both wait for a snapshot from the player), then Phase 14 (torches), and the creatures' own
behaviour (they only wander now).
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
- [ ] **12.7** A wood block joins the ground under it (dirt, sand, gravel, grass, snow, and any other natural ground block) at
  its bottom only, so a trunk base merges into the ground. Needs a join below, for a list of blocks rather than one.
- [ ] **12.8** Water's surface is a semi-transparent mesh, not a solid colour: the water skin is drawn blended over what is
  under it (the seabed and the shore show through), and its faces do not write depth, so the sky through it stays visible.
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
- [ ] **14.2** Small torch as an item, not a block: a torch is a prop on one face of a block, drawn as a thin mesh, so it
  takes a fraction of a block and the grid does not change. Its cell stays air for collision, meshing and the walk, and the
  torch enters the light pass as an emitter at that cell, from a sparse per-chunk list (no block id). Placed from the hotbar
  on a face, broken by hitting it; the chunks within reach are re-lit as for a block edit.
- [ ] **14.3** Held torch: a point light that follows the camera and lights the walls around the player, with a key to
  light or put it out. It is a shader light, so it needs no re-mesh and stays separate from the baked light of 14.1 and 14.2.
- [ ] **14.4** The light colour is warm, and a torch flickers a little (cosmetic, per torch).
- [ ] **14.5** Measured: the cost of the light pass in the mesher (a chunk with and without a light block) and of a re-mesh
  after a placement, in Release.
- [ ] **14.6** Spike: a prop finer than a block. Can a block cell hold a finer grid where a prop needs one, without finer blocks
  everywhere? Plan: a sparse 4×4×4 micro-grid per block cell, stored only where set, meshed with the same greedy mesher at
  micro scale and lit from its cell. Whole-world subdivision is not the route: it multiplies the cells the mesher, the light
  walk and the collision sample by 4 per axis (64 times the cells). Decide after 14.2 whether a prop needs micro collision.
- Interim (not on the roadmap as a fix): the cave floor (0.2 of the sky) could be raised to make caves readable before the
  torches land. Decide after 14.1 if they are still too dark.

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
- The tests run through the control channel with scripted input. They are the best guard against regressions in the game, and they caught a real engine bug (the run loop), so keep adding one for each new interaction.
