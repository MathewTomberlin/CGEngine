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
FLOW_FIRST, FLOW_LAST = 9, 15       # flowing water, levels 7 to 1 (the block id is 16 - level); WATER is the full source
LIQUIDS = (WATER,) + tuple(range(FLOW_FIRST, FLOW_LAST + 1))
FALLS = (SAND, GRAVEL)              # blocks that fall into air or liquid

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
}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    COLOURS[_block] = COLOURS[WATER]

