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
LAVA = 22                           # a liquid: lakes in the deep caves (the same id as VoxelTerrain::Lava), a source that flows as water does
LAVA_FLOW_FIRST, LAVA_FLOW_LAST = 23, 29   # flowing lava, levels 7 to 1 (the block id is 30 - level), as the water's are
LAVA_BLOCKS = (LAVA,) + tuple(range(LAVA_FLOW_FIRST, LAVA_FLOW_LAST + 1))   # the lava family: one mesh, named after the source
DIRECTIONS = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))   # a prop's dir, to the block it leans on (PropDirs)
FLOW_FIRST, FLOW_LAST = 9, 15       # flowing water, levels 7 to 1 (the block id is 16 - level); WATER is the full source
WATER_BLOCKS = (WATER,) + tuple(range(FLOW_FIRST, FLOW_LAST + 1))   # the water family: one mesh, drawn see-through
LIQUIDS = WATER_BLOCKS + LAVA_BLOCKS    # every liquid: swimming and the fog under the surface
STILL_LIQUIDS = ()                   # liquids that do not spread (define_block flows=False): none now, lava flows like water
FALLS = (SAND, GRAVEL)              # blocks that fall into air or liquid
HITS = {STONE: 3, WOOD: 4, BRICK: 3, COAL: 4, IRON: 5}   # hits to break a block (the others take one); see VoxelWorld.hit_block
EMITS = {LAMP: 255, LAVA: 255}      # the light each block gives off at its own cell, 0 to 255; the light falls off through air
# How each block is drawn (BlockType::own and cube). A block in OWN has its own mesh per chunk, drawn with MATERIALS[block]
# (a material in scenes/voxels.json); the rest are the terrain's smooth skin. A cube block is always in OWN.
OBSIDIAN = 30                       # a hard cube: a lava block that water touches for a while becomes it (REACTIONS)
OWN = (WOOD, LEAVES, BRICK, LAMP, OBSIDIAN) + LIQUIDS   # liquids are in their own meshes too, so the ground under them has a skin to be seen by
CUBES = (BRICK, OBSIDIAN)
MATERIALS = {WOOD: "voxel_wood", LEAVES: "voxel_leaves", BRICK: "voxel_brick", WATER: "voxel_water", LAVA: "voxel_lava",
             OBSIDIAN: "voxel_brick"}
JOINS = {WOOD: LEAVES}              # a block whose top is covered by the mesh of this block joins that mesh (the trunk top joins the canopy)
GROUND = (GRASS, DIRT, STONE, SAND, GRAVEL, SNOW)   # natural ground
# A trunk base stays in the trunk's own skin (no join below): a base joined to the terrain pinched the drawn trunk to 0.8 of a
# block where it left the ground (roadmap 27.1). The own skin runs down into the ground, so the trunk flares out of it.
JOINS_BELOW = {}
TRANSPARENT = (WATER,) + tuple(range(FLOW_FIRST, FLOW_LAST + 1))   # drawn blended: the water's mesh is named after the source

# The block textures (resources/voxel_tiles.png, written by tools/make_voxel_tiles.py): one layer per tile, in this order. A
# textured block shows its tile instead of its colour: the top, the sides and the bottom each have a layer (TILES).
TILE_NAMES = ["grass_top", "grass_side", "dirt", "stone", "sand", "water", "snow_top", "snow_side", "bark", "wood_rings",
              "leaves", "gravel", "lamp", "brick", "coal_ore", "iron_ore", "lava"]
TILE_SIZE = 32
# The splat blocks, in channel order (at most 16: Renderer::kSplatChannels). The smooth skin carries one weight per channel at each
# vertex, and the fragment shader blends their tiles per pixel by them, so a boundary between two blocks that both have a channel
# has no triangle edge: the ground blends with all of these. The water is a splat block too (its flowing levels take its channel),
# so the water surface joins the ground beside it. A block with no channel (brick, wood) keeps an edge against them. Every block
# of the smooth terrain must be one of these (or in OWN, with its own mesh).
SPLAT = (GRASS, DIRT, STONE, SAND, SNOW, GRAVEL, COAL, IRON, WATER)

def splat_channel(block, textured):
    """The splat channel of a block (BlockType::splat): its own, the water's for every flowing level of the water, -1 for none
    (a block that is not textured, or not a splat block)."""
    if not textured:
        return -1
    key = WATER if block in WATER_BLOCKS else block
    return SPLAT.index(key) if key in SPLAT else -1
GRASS_TILES = (0, 1, 2)                 # grass top, grass side, dirt
TILES = {
    GRASS: GRASS_TILES,
    DIRT: (2, 2, 2),
    STONE: (3, 3, 3),
    SAND: (4, 4, 4),
    WATER: (5, 5, 5),
    SNOW: (6, 7, 2),
    WOOD: (9, 8, 9),                    # the cut top shows its rings, the sides the bark
    LEAVES: (10, 10, 10),
    GRAVEL: (11, 11, 11),
    LAMP: (12, 12, 12),
    BRICK: (13, 13, 13),
    COAL: (14, 14, 14),
    IRON: (15, 15, 15),
    LAVA: (16, 16, 16),
}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    TILES[_block] = TILES[WATER]

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
    LAVA: (1.00, 0.30, 0.05),
    BRICK: (0.66, 0.30, 0.22),
}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    COLOURS[_block] = COLOURS[WATER]

NAMES = {AIR: "air", GRASS: "grass", DIRT: "dirt", STONE: "stone", SAND: "sand", WATER: "water", SNOW: "snow",
         WOOD: "wood", LEAVES: "leaves", GRAVEL: "gravel", LAMP: "lamp", BRICK: "brick", TORCH: "torch", COAL: "coal", IRON: "iron",
         LAVA: "lava"}
for _block in range(FLOW_FIRST, FLOW_LAST + 1):
    NAMES[_block] = "flowing water"
for _block in range(LAVA_FLOW_FIRST, LAVA_FLOW_LAST + 1):
    TILES[_block] = TILES[LAVA]
    COLOURS[_block] = COLOURS[LAVA]
    EMITS[_block] = EMITS[LAVA]
    NAMES[_block] = "flowing lava"
COLOURS[OBSIDIAN] = (0.10, 0.07, 0.15)
NAMES[OBSIDIAN] = "obsidian"
HITS[OBSIDIAN] = 5
for _block in LAVA_BLOCKS:
    HITS[_block] = 4                    # lava takes four stages of damage when a reaction works on it (see REACTIONS)
# The reactions every world has (VoxelRules): a lava block that water touches takes four seconds to become obsidian, showing its
# damage as it goes. Other reactions are data too: a rule file (VOXEL_RULES) adds to them.
REACTIONS = [{"block": "lava", "touches": "water", "becomes": "obsidian", "seconds": 4}]


# Items that are not blocks: a creature leaves a hide when it dies. They are held in the inventory like blocks, but a slot
# holding one cannot place it (the game checks COLOURS, which lists the blocks only), and the world never defines them.
HIDE = 40
PICKAXE = 41                        # a tool: it does not stack, and when it is the selected slot its swings on PICKAXE_BLOCKS count double
IRON_PICKAXE = 42                   # a tool that mines obsidian, and lasts four times as long as a pickaxe
SWORD = 43                          # a weapon: it strikes creatures for its damage (WEAPONS); it mines like a hand, and does not wear
SHIELD = 44                         # held in the left hand (VoxelGame.offhand): raised with Q, it takes the bites of creatures in front
TOOTH = 45                          # a biter's tooth: a crafting item (the bone blade), left where a biter falls
BONE_BLADE = 46                     # a weapon made from teeth and wood: it strikes like a sword, and wears out sooner
SHOVEL = 47                         # a tool for the soft ground: it breaks dirt, sand and gravel faster (TOOLS)
BUCKET = 48                         # empty: a strike on water or lava fills it; it is made from iron
WATER_BUCKET = 49                   # filled with water: a right click places a water block and leaves the bucket empty
LAVA_BUCKET = 50                    # filled with lava: a right click places a lava block and leaves the bucket empty
ITEM_COLOURS = {HIDE: (0.72, 0.56, 0.40), PICKAXE: (0.62, 0.62, 0.68), IRON_PICKAXE: (0.74, 0.72, 0.70), SWORD: (0.80, 0.82, 0.86),
                SHIELD: (0.55, 0.40, 0.25), TOOTH: (0.92, 0.88, 0.76), BONE_BLADE: (0.80, 0.76, 0.62), SHOVEL: (0.70, 0.66, 0.58),
                BUCKET: (0.75, 0.77, 0.80), WATER_BUCKET: (0.30, 0.50, 0.90), LAVA_BUCKET: (0.95, 0.45, 0.10)}
ITEM_NAMES = {HIDE: "hide", PICKAXE: "pickaxe", IRON_PICKAXE: "iron pickaxe", SWORD: "sword", SHIELD: "shield", TOOTH: "tooth",
              BONE_BLADE: "bone blade", SHOVEL: "shovel", BUCKET: "bucket", WATER_BUCKET: "water bucket", LAVA_BUCKET: "lava bucket"}
NAMES.update(ITEM_NAMES)
PICKAXE_BLOCKS = (STONE, COAL, IRON, BRICK, OBSIDIAN)   # the blocks a pickaxe mines faster (its block_damage, below)
# A tool's durability is the swings that hit a block it can use before it breaks (one use a swing), and its tier is the hardest
# block it mines (BLOCK_TIER). A block that is not listed in BLOCK_TIER needs no tool: stone, coal and iron are mined by hand.
# block_damage: how much one strike breaks each block (a whole number of hits is made, and a part carries to the next strike);
# a block it does not list takes harvest strikes' worth (1 for the hand, and the tool's harvest where it has one).
# creature_damage: the health one strike takes from a creature (its health is VoxelCreatures' "health"); the hand does 1.
TOOLS = {
    PICKAXE: {"durability": 640, "tier": 1, "creature_damage": 1.5,
              "block_damage": {STONE: 2, COAL: 2, IRON: 2, BRICK: 2, OBSIDIAN: 2}},
    IRON_PICKAXE: {"durability": 2560, "tier": 2, "creature_damage": 1.75,
                   "block_damage": {STONE: 3, COAL: 3, IRON: 3, BRICK: 3, OBSIDIAN: 3}},
    SHOVEL: {"durability": 640, "tier": 0, "creature_damage": 1, "harvest": 1, "attack_speed": 3,
             "block_damage": {DIRT: 8, SAND: 8, GRAVEL: 8}},
}
BLOCK_TIER = {OBSIDIAN: 2}
# A weapon's durability is the strikes that hit a creature before it breaks (each strike takes one use, as a tool's swing does).
WEAPONS = {SWORD: {"damage": 2.5, "durability": 1280}, BONE_BLADE: {"damage": 2, "durability": 30}}
# The buckets: an empty one is filled by a strike on a liquid, and a filled one places its liquid (VoxelGame, placement_check).
BUCKETS = (BUCKET, WATER_BUCKET, LAVA_BUCKET)
FILLED_BUCKET = {WATER: WATER_BUCKET, LAVA: LAVA_BUCKET}   # the bucket a liquid fills, by the liquid's source block
BUCKET_LIQUID = {WATER_BUCKET: WATER, LAVA_BUCKET: LAVA}   # the liquid a filled bucket places, by the bucket
MAX_SWINGS = 8                      # the most hits one strike can make (a shovel's harvest on the blocks it lists as harvest)


def damage_of(item):
    """The health a strike with this item takes from a creature."""
    if item in WEAPONS:
        return WEAPONS[item]["damage"]
    return TOOLS[item].get("creature_damage", 1) if item in TOOLS else 1


def attack_speed_of(item):
    """How many strikes an item makes in the time the empty hand makes one (a held break repeats this much faster). 1 for the hand."""
    return TOOLS[item].get("attack_speed", 1) if item in TOOLS else 1


def block_damage(item, block):
    """How much one strike with this item breaks a block: the tool's block_damage for that block, else its harvest, else 1 (the
    hand, and every item that is not a tool)."""
    tool = TOOLS.get(item)
    if tool is None:
        return 1
    return tool.get("block_damage", {}).get(block, tool.get("harvest", 1))


def durability_of(item):
    """The uses an item has when new, for a tool (it wears when it swings at a block) or a weapon (it wears when it strikes a
    creature), or None for an item that does not wear."""
    if item in TOOLS:
        return TOOLS[item]["durability"]
    if item in WEAPONS:
        return WEAPONS[item]["durability"]
    return None


def mining_tier(item):
    """The tier a tool mines at; 0 for no tool, and for an item that is not a tool."""
    return TOOLS[item]["tier"] if item in TOOLS else 0


def can_mine(item, block):
    """Whether the item in the selected slot (or None) can break the block: its tier must be at least the block's."""
    return mining_tier(item) >= BLOCK_TIER.get(block, 0)
