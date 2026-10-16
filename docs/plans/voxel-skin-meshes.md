# Voxel skin meshes: analysis and implementation plan

Branch: `f-claude-Autonomous-Round` (uncommitted work on top of `329edaa`).

## 1. Short answer

Your instinct is right, and most of it already exists. Each skin is already its own mesh *group*:

- `meshGroup()` (`src/Core/Voxel/VoxelBlocks.h:34`) gives every `own` block its own group: its `mesh` id if set, else its own block id. Group 0 is the terrain, which holds every non-own solid block.
- The build loop (`src/Core/Voxel/VoxelWorld.cpp:1165-1187`) makes one mesh per group present in the chunk.
- Each group's density counts only its own blocks: `inGroup` (`VoxelMesher.cpp:62`) and `solidAt` (`VoxelMesher.cpp:419`). So two leaf blocks join, a leaf and a wood block do not, and neither joins water. The separation is by construction.
- The `join_above` / `join_below` rules are the only deliberate exceptions (a trunk top joins the canopy mesh; a block on natural ground joins the terrain).

So the work is not "create separate meshes". It is to make the rules that decide membership, shape and blending explicit, single-sourced and orthogonal, so that a new skin type is a new case in one place. The current code conflates four separate ideas:

| Concept | Question it answers | Where it lives today |
|---|---|---|
| **Group** | Which mesh and which density does this block belong to? | `meshGroup`, `inGroup`, `skinAt`, `join_above`, `join_below` |
| **Shape** | How is the group's surface built? smooth, cube, or liquid steps | `BlockType::cube` (forces `own`), `rule.liquid`, the `cubes` test in the build loop |
| **Draw pass** | Is it blended after the opaque meshes? | `BlockType::transparent` |
| **Light passes** | Does sky and lamp light go through it? | `transparent` again, via `lightPasses` (`VoxelMesher.cpp:138`) |

Two of these use the same flag. That coupling is the source of the current regression.

Note on the list in your message: "cube" is a shape, not a mesh type. A brick block is a cube-shaped `own` group, and a leaf block is a smooth `own` group. Group and shape vary independently. The model below separates them.

## 2. What went wrong on this branch

Uncommitted changes in `VoxelMesher.cpp` made two moves to fix grass under water:

1. **`exposedBlock` now counts "light passes" as open** (`VoxelMesher.cpp:396-402`). It used to ask "is the neighbour air?". Now water counts as open, so a grass block under water is a surface block. This makes the terrain skin depend on the water's `transparent` flag, which is a light property. Nothing in the ground's own rule says "water is open to me"; it just happens to be transparent.
2. **Liquid groups bypass the smooth skin entirely** (`buildLiquidMesh`, `VoxelMesher.cpp:681`, dispatched at `VoxelWorld.cpp:1178`, `:1518`, and `probeGroundMesh` at `:92`). Water is now stepped columns with bevels, not a density surface. Its `column.ground` height is taken from *any* non-liquid block, so water next to leaves or brick treats them as ground. That is a skin-to-skin rule with no name.

Consequences:

- The smooth-water path in `smoothVertex` (the `blended` branch with `waterWeight`, `barrier`, and the splat fallback, `VoxelMesher.cpp:529-597`) is now unreachable for real liquid groups. It is dead code that still looks live.
- Water and ground no longer meet through a shared density, so the seam between them is the bevel logic, not the surface. That is why the water skin changed shape.
- I have not run the game to confirm the exact visual. The mechanism above is what the code does. Check with `tools/voxel_ground_probe.py` and `tools/voxel_shore_view.py` (both untracked, both already here).

## 3. Design

### 3.1 One membership function

Add `skinOf` to the voxel block rules (VoxelBlocks.h or a new `VoxelSkins.h`):

```
uint8_t skinOf(block, above, below, neighbourBlockType) -> group
```

It implements the join rules once: own mesh, `join_above` (top covered by a block of that mesh), `join_below` (standing on a terrain block), else terrain 0. Every caller uses it:

- `inGroup` (mesher)
- `skinAt` and `skinFast` (collision, `VoxelWorld.cpp:115-149`)
- `computeSkin` via `inGroup`
- the Python collision mirror (`VoxelWorld.cpp:231`'s comment names `VoxelPhysics.py`), which must match operation for operation

Today the join logic is written twice (`inGroup` and `skinAt`). The parity test exists precisely because they can drift.

### 3.2 Explicit shape per group

Replace the implicit shape tests with one value per group, carried in `PaddedChunk::MeshRule` and set from `BlockType`:

```
enum class SkinShape : uint8_t { Terrain, Smooth, Cube, Liquid };
```

- `Terrain`: group 0 only. Uses the corner-and-centre density (`VoxelTerrainShift`, `VoxelCentreWeight`).
- `Smooth`: an `own` group with the occupancy density, `buildSmoothMesh`.
- `Cube`: `buildChunkMesh`. Keep the rule that cube implies own.
- `Liquid`: `buildLiquidMesh`.

`BlockType::cube` and `BlockType::liquid` map to this, so the Python `define_block` signature does not change. `rule.liquid` and the `cubes` boolean in the build loop go away.

### 3.3 Separate draw pass and light

Give `BlockType` two flags with clear names:

- `blend`: drawn in the blended pass after opaque meshes (water). Today this is `transparent`.
- `passesLight`: sky and lamp light go through it (air, water). Today this is `transparent` too.

Keep `lightPasses` (`VoxelMesher.cpp:138`) as the one reader of `passesLight`. Nothing in the surface code (`exposedBlock`, `terrainAt`, `densityAt`) reads a light or draw property again.

### 3.4 Surface rule: "open" means "not in my group"

The fix for the grass-under-water problem is one line of meaning, not a light test. A block of group `g` is a surface block for group `g` if any face neighbour is not in group `g`:

```
exposedBlock(chunk, x, y, z, group): any face neighbour where !inGroup(neighbour, group)
```

For terrain, water is not in group 0, so grass under water is exposed. Leaves and brick are not in group 0 either, so they are open to the ground as well, which matches the density (they are not ground). This replaces the `lightPasses` change on this branch.

### 3.5 One dispatch

Replace the three copies of the `cubes / liquid / smooth` decision (`VoxelWorld.cpp:1178-1184`, `:1518-1524`, `:92-96`) with:

```
void buildGroupMesh(const PaddedChunk&, palette, group, resolution, stride, skirtSides, MeshArrays&)
```

It switches on `rule[group].shape`. Every caller passes the same arguments.

### 3.6 Liquid ground

`buildLiquidMesh` should not invent its own ground. Its `column.ground` should come from the terrain group's height in that column (the same `skin`/terrain data the terrain uses), so water meets the ground the terrain draws. Blocks of other groups (leaves, wood, brick) are walls to the liquid, not ground, unless the liquid's own rule says otherwise. This needs a decision (section 5, D2).

### 3.7 Remove the dead blend path

Once liquids use `buildLiquidMesh`, delete the `blended` branch, `waterWeight`, `barrier`, and the water splat logic in `smoothVertex`. Keep the terrain splat path. This is a deletion, so do it last and only after the visual check in 4.3.

### 3.8 Adding a future skin type

After this change, a new skin is:

1. a `SkinShape` value,
2. a builder, or a case in `buildGroupMesh`,
3. the `define_block` shape string,

with no edits to the terrain density, the other groups, or collision except through `skinOf`.

## 4. Implementation phases

Each phase builds and runs before the next. The rule: no visual change except where a phase says so.

### Phase 1: Stop the coupling (fixes the regression)

Status: done. `exposedBlock` is group-based; the D3 rename is in; all water (springs, flows, volumes) uses the smooth skin with blend, and `buildLiquidMesh` and `liquidUnits` are removed. Volumes render at full height (per D1). Checks: `tools/voxel_ground_probe.ps1 -ExeDir build/bin/Debug` moves 0 of 37 ground vertices under spring and volume water; `tools/test_voxel_physics.py`, `test_voxel_blocks.py`, `test_voxel_rules.py` pass; `voxel_view_shot.ps1 -View volume` shows a smooth water blob.

1. Replace the `lightPasses` test in `exposedBlock` with the group test from 3.4.
2. Keep `buildLiquidMesh` for now, but make `column.ground` take the terrain's top (3.6) so the water meets the grass again.
3. Acceptance: `tools/voxel_ground_probe.py` shows no upward bulge on grass under water. `tools/voxel_shore_view.py` shows water with a shore.

### Phase 2: Single membership function

1. Add `skinOf` (3.1). Make `inGroup`, `skinAt`, and `skinFast` call it.
2. Update the Python collision mirror to match. Run the parity check.
3. Acceptance: no change in rendered output. The parity check passes.

### Phase 3: Explicit shape and flags

1. Add `SkinShape` to `MeshRule` and `BlockType`. Map `cube` and `liquid` onto it.
2. Split `transparent` into `blend` and `passesLight` (3.3). Update `VoxelBindings.cpp` `define_block` to set both from the one JSON `transparent` key, so `resources/voxel_blocks.json` does not change.
3. Acceptance: no change in rendered output.

### Phase 4: One dispatch

1. Add `buildGroupMesh` (3.5). Replace the three call sites.
2. Acceptance: no change in rendered output. Edit a block in the game (`VoxelWorld.cpp:1518` path) and check it matches a fresh mesh.

### Phase 5: Remove the dead water path

1. Delete the blended branch (3.7).
2. Acceptance: builds clean; water, leaves, wood, and torches render as before; the shore check passes.

### Phase 6: Tests and docs

1. Add `tools/test_voxel_skins.py` (or extend `tools/test_voxel_blocks.py`, untracked): for each pair of adjacent blocks in different groups, assert the surface is closed between them (via `probeGroundMesh`). Cover leaf-leaf (joined), leaf-wood (open), wood-water, grass-water, and the join rules.
3. Update `docs/ai/`, `docs/games/voxels.md` if present, and the ROADMAP checklist (CLAUDE.md asks for this).

## 5. Decisions needed

- **D1. Water skin: smooth or stepped?** DECIDED: smooth. Water becomes a `Smooth` group with `blend = true`. `buildLiquidMesh` (roadmap 46 stepped surface) is removed, and Phase 5 is replaced by restoring the smooth water path. Recheck the roadmap-46 entry when this lands.
- **D2. Should water see leaves and brick as ground or as walls?** OPEN. Currently water sees any non-liquid block as ground (`column.ground`, the `lowestGround` rule in `buildLiquidMesh`). Recommendation: only the terrain group is ground; other groups are walls. Under smooth water (D1), the water's density already counts only water blocks, so leaves and brick close the surface by construction; the question is only what grass and sand do beside water.
- **D3. Rename `transparent`?** DECIDED: yes, to `blend` (draw pass) and `passesLight` (light). The JSON keeps `transparent`; the change is in `VoxelBindings.cpp` only.

## 6. Risks

- **Python parity.** The collision mirror must change with `skinOf`. Missing this makes the body stand on a surface the player does not see.
- **Incremental edits.** The edit path (`VoxelWorld.cpp:1518`) and the full build path must call the same dispatch, or edited chunks will look wrong until reload.
- **Joins.** `join_above` and `join_below` are the only cross-group rules. The `skinOf` tests must cover both.
- **Cache keys.** `ChunkMesh` is keyed per group. Changing a group's shape must invalidate its cached mesh; check `reuse` in the build loop (`VoxelWorld.cpp:1155`).

## 7. Files touched (expected)

- `src/Core/Voxel/VoxelBlocks.h`: `BlockType` flags, `SkinShape`, `skinOf`
- `src/Core/Voxel/VoxelMesher.h` / `.cpp`: `MeshRule`, `exposedBlock`, `inGroup`, `buildGroupMesh`, `buildLiquidMesh` ground, remove blended path
- `src/Core/Voxel/VoxelWorld.cpp`: `skinAt`, `skinFast`, dispatch sites, `probeGroundMesh`, `padded.rule[id]` setup (three copies at 1086, 1482)
- `src/Bindings/VoxelBindings.cpp`: `define_block` mapping
- `resources/scripts/VoxelPhysics.py`: `skinOf` parity; `tools/test_voxel_physics.py` runs the parity check
- `tools/test_voxel_skins.py`, `docs/ROADMAP.md`
