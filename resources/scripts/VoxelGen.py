"""
Block ids and colours for the voxel sample (docs/games/voxels.md). The terrain itself is generated in C++
(src/Core/Voxel/VoxelTerrain.cpp) on worker threads; the ids here match its block numbers.

Air is 0 and is never stored. The colours are tints: the base texture is a light grey noise.
"""

CHUNK = 16          # blocks along x and z in a chunk
HEIGHT = 64         # blocks up in the world
SEA_LEVEL = 14      # water fills the low columns up to this
SNOW_LEVEL = 40     # columns this high are snow-capped
TREE_CHANCE = 0.01  # the chance that a grass column grows a tree
CAVE_WIDTH = 0.05   # how wide the tunnels are (0 turns caves off)

AIR, GRASS, DIRT, STONE, SAND, WATER, SNOW, WOOD, LEAVES = range(9)
GRAVEL = 16
LAMP = 17                           # a full block that gives off light (the same id as VoxelTerrain::Lamp)
BRICK = 18                          # a cube block: it never merges with its neighbours, so it keeps sharp edges
TORCH = 19                          # a torch: a prop on a block's face (VoxelWorld.set_prop), not a block; only the hotbar names it
COAL, IRON = 20, 21                 # ore in the stone: coal at any depth, iron deep (the same ids as VoxelTerrain::Coal and Iron)
DIRECTIONS = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))   # a prop's dir, to the block it leans on (PropDirs)
FLOW_FIRST, FLOW_LAST = 9, 15       # flowing water, levels 7 to 1 (the block id is 16 - level); WATER is the full source
LIQUIDS = (WATER,) + tuple(range(FLOW_FIRST, FLOW_LAST + 1))
FALLS = (SAND, GRAVEL)              # blocks that fall into air or liquid
EMITS = {LAMP: 255}                 # the light each block gives off at its own cell, 0 to 255; the light falls off through air
# How each block is drawn (BlockType::own and cube). A block in OWN has its own mesh per chunk, drawn with MATERIALS[block]
# (a material in scenes/voxels.json); the rest are the terrain's smooth skin. A cube block is always in OWN.
OWN = (WOOD, LEAVES, BRICK) + LIQUIDS   # water is in its own mesh too, so the ground under it has a skin to be seen by
CUBES = (BRICK,)
MATERIALS = {WOOD: "voxel_wood", LEAVES: "voxel_leaves", BRICK: "voxel_brick", WATER: "voxel_water"}
JOINS = {WOOD: LEAVES}              # a block whose top is covered by the mesh of this block joins that mesh (the trunk top joins the canopy)
GROUND = (GRASS, DIRT, STONE, SAND, GRAVEL, SNOW)   # natural ground: a block standing on it joins the terrain (a trunk base)
JOINS_BELOW = {WOOD: GROUND}
TRANSPARENT = (WATER,) + tuple(range(FLOW_FIRST, FLOW_LAST + 1))   # drawn blended: the water's mesh is named after the source

COLOURS = {
    GRASS: (0.36, 0.72, 0.26),
    DIRT: (0.52, 0.36, 0.22),
    STONE: (0.50, 0.50, 0.53),
    SAND: (0.92, 0.86, 0.58),
    WATER: (0.22, 0.45, 0.92),
    SNOW: (0.96, 0.98, 1.00),
    WOOD: (0.44, 0.29, 0.15),
    LEAVES: (0.20, 0.50, 0.16),
    GRAVEL: (0.52, 0.50, 0.47),
    LAMP: (0.30, 0.22, 0.12),       # the block's own colour is dark: its glow (the light colour) is what shows, warm, not white
    TORCH: (1.00, 0.62, 0.25),
    COAL: (0.16, 0.16, 0.18),
    IRON: (0.66, 0.46, 0.36),
    BRICK: (0.66, 0.30, 0.22),
}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    COLOURS[_block] = COLOURS[WATER]

NAMES = {AIR: "air", GRASS: "grass", DIRT: "dirt", STONE: "stone", SAND: "sand", WATER: "water", SNOW: "snow",
         WOOD: "wood", LEAVES: "leaves", GRAVEL: "gravel", LAMP: "lamp", BRICK: "brick", TORCH: "torch", COAL: "coal", IRON: "iron"}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    NAMES[_block] = "flowing water"

