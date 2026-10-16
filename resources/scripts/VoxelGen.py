"""
Block ids and colours for the voxel sample (docs/games/voxels.md). The terrain itself is generated in C++
(src/Core/Voxel/VoxelTerrain.cpp) on worker threads; the ids here match its block numbers.

Air is 0 and is never stored. The colours are tints: the base texture is a light grey noise.
"""
import json
import os

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
LAVA = 22                           # a producing spring: it puts a unit of lava volume into the air beside it (the caves use ENDLESS_LAVA)
ENDLESS_WATER = 31                  # an endless spring: it makes flowing water that never accumulates, and drains when nothing feeds it
ENDLESS_LAVA = 32                   # an endless lava spring: the same as ENDLESS_WATER, for lava (its flows are LAVA_FLOW_FIRST to LAVA_FLOW_LAST)
LAVA_FLOW_FIRST, LAVA_FLOW_LAST = 23, 29   # flowing lava, levels 7 to 1 (the block id is 30 - level), as the water's are
LAVA_BLOCKS = (LAVA, ENDLESS_LAVA) + tuple(range(LAVA_FLOW_FIRST, LAVA_FLOW_LAST + 1)) + tuple(range(136, 144))   # the lava family: one mesh, named after the source; its volumes (136 to 143) too
DIRECTIONS = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))   # a prop's dir, to the block it leans on (PropDirs)
FLOW_FIRST, FLOW_LAST = 9, 15       # flowing water, levels 7 to 1 (the block id is 16 - level); WATER is the full source
WATER_BLOCKS = (WATER, ENDLESS_WATER) + tuple(range(FLOW_FIRST, FLOW_LAST + 1)) + tuple(range(128, 136))   # the water family: one mesh, drawn see-through; its volumes (128 to 135) too
# Volumes: a placed amount of liquid, one block id per unit (1 to 8). A bucket places a full volume, which spreads and levels out;
# a producing spring (WATER, LAVA) makes volume units; an endless spring (ENDLESS_WATER, ENDLESS_LAVA) makes flows, as the springs did before.
# Ids 128 and up are not items (the item ids run to 52).
WATER_VOLUME_FIRST, WATER_VOLUME_FULL = 128, 135
LAVA_VOLUME_FIRST, LAVA_VOLUME_FULL = 136, 143
VOLUME_BLOCKS = tuple(range(WATER_VOLUME_FIRST, LAVA_VOLUME_FULL + 1))   # every volume's unit ids (water and lava)
LIQUIDS = WATER_BLOCKS + LAVA_BLOCKS    # every liquid: swimming and the fog under the surface
STILL_LIQUIDS = ()                   # liquids that do not spread (define_block flows=False): none now, lava flows like water
OBSIDIAN = 30                       # a hard cube: a lava block that water touches for a while becomes it (REACTIONS)
GROUND = (GRASS, DIRT, STONE, SAND, GRAVEL, SNOW)   # natural ground
# A trunk base stays in the trunk's own skin (no join below): a base joined to the terrain pinched the drawn trunk to 0.8 of a
# block where it left the ground (roadmap 27.1). The own skin runs down into the ground, so the trunk flares out of it.
JOINS_BELOW = {}

# The block textures (resources/voxel_tiles.png, written by tools/make_voxel_tiles.py): one layer per tile, in this order. A
# textured block shows its tile instead of its colour: the top, the sides and the bottom each have a layer (TILES).
TILE_NAMES = ["grass_top", "grass_side", "dirt", "stone", "sand", "water", "snow_top", "snow_side", "bark", "wood_rings",
              "leaves", "gravel", "lamp", "brick", "coal_ore", "iron_ore", "lava"]
TILE_SIZE = 32

def _block_table():
    """The block table from resources/voxel_blocks.json (VOXEL_BLOCKS picks another file), one dict per block id. An entry's
    'key' must be the constant of its id in this file, so the two cannot drift apart."""
    if os.environ.get("VOXEL_BLOCKS"):
        path = os.environ["VOXEL_BLOCKS"]
    else:
        here = os.path.dirname(os.path.abspath(__file__))
        candidates = [os.path.join(here, "voxel_blocks.json"), os.path.join(here, os.pardir, "voxel_blocks.json")]
        path = next((c for c in candidates if os.path.exists(c)), None)
        if path is None:
            raise FileNotFoundError("voxel_blocks.json was not found beside the scripts or in resources")
    with open(path, encoding="utf-8") as f:
        entries = json.load(f)["blocks"]
    table = {}
    for entry in entries:
        ids = range(entry["ids"][0], entry["ids"][1] + 1) if "ids" in entry else [entry["id"]]
        for block in ids:
            if block in table:
                raise ValueError("block %d is listed twice in %s" % (block, path))
            if "key" in entry and globals()[entry["key"]] != block:
                raise ValueError("%s is %d in VoxelGen.py, not %d as %s has it" % (entry["key"], globals()[entry["key"]], block, path))
            table[block] = dict(entry, id=block)
    return table


BLOCK_TABLE = _block_table()
COLOURS = {b: tuple(e["colour"]) for b, e in BLOCK_TABLE.items() if "colour" in e}
NAMES = {b: e["name"] for b, e in BLOCK_TABLE.items()}
TILES = {}
for _b, _e in BLOCK_TABLE.items():
    if "tiles" in _e:
        _names = _e["tiles"] * 3 if len(_e["tiles"]) == 1 else _e["tiles"]
        TILES[_b] = tuple(TILE_NAMES.index(_n) for _n in _names)
HITS = {b: e["hits"] for b, e in BLOCK_TABLE.items() if "hits" in e}   # hits to break a block (the others take one); see VoxelWorld.hit_block
EMITS = {b: e["emits"] for b, e in BLOCK_TABLE.items() if e.get("emits")}   # the light each block gives off at its own cell, 0 to 255
FALLS = tuple(b for b in sorted(BLOCK_TABLE) if BLOCK_TABLE[b].get("falls"))   # blocks that fall into air or liquid
_LIQUID_IDS = tuple(b for b in sorted(BLOCK_TABLE) if BLOCK_TABLE[b].get("liquid"))
if _LIQUID_IDS != tuple(sorted(LIQUIDS)):
    raise ValueError("the liquid blocks in voxel_blocks.json are %s, but the id ranges above are %s" % (_LIQUID_IDS, sorted(LIQUIDS)))
# How each block is drawn (BlockType::own and cube). A block in OWN has its own mesh per chunk, drawn with MATERIALS[block]
# (a material in scenes/voxels.json); the rest are the terrain's smooth skin. A cube block is always in OWN. Liquids are in their
# own meshes too, so the ground under them has a skin to be seen by.
OWN = tuple(sorted({b for b, e in BLOCK_TABLE.items() if e.get("own") or e.get("cube")} | set(LIQUIDS)))
CUBES = tuple(b for b in sorted(BLOCK_TABLE) if BLOCK_TABLE[b].get("cube"))
MATERIALS = {b: e["material"] for b, e in BLOCK_TABLE.items() if "material" in e}
JOINS = {b: e["join_above"] for b, e in BLOCK_TABLE.items() if "join_above" in e}   # a block whose top is covered by the mesh of this block joins that mesh (the trunk top joins the canopy)
TRANSPARENT = tuple(b for b in sorted(BLOCK_TABLE) if BLOCK_TABLE[b].get("transparent"))   # drawn blended: the water's mesh is named after the source
_SPLAT_CHANNELS = {e["splat"]: b for b, e in BLOCK_TABLE.items() if "splat" in e}
if sorted(_SPLAT_CHANNELS) != list(range(len(_SPLAT_CHANNELS))):
    raise ValueError("the splat channels in voxel_blocks.json must be 0 to n-1")
SPLAT = tuple(_SPLAT_CHANNELS[c] for c in range(len(_SPLAT_CHANNELS)))
# The splat blocks, in channel order (at most 16: Renderer::kSplatChannels). The smooth skin carries one weight per channel at each
# vertex, and the fragment shader blends their tiles per pixel by them, so a boundary between two blocks that both have a channel
# has no triangle edge: the ground blends with all of these. The water is a splat block too (its flowing levels take its channel),
# so the water surface joins the ground beside it. A block with no channel (brick, wood) keeps an edge against them. Every block
# of the smooth terrain must be one of these (or in OWN, with its own mesh).

def splat_channel(block, textured):
    """The splat channel of a block (BlockType::splat): its own, the water's for every flowing level of the water, -1 for none
    (a block that is not textured, or not a splat block)."""
    if not textured:
        return -1
    key = WATER if block in WATER_BLOCKS else block
    return SPLAT.index(key) if key in SPLAT else -1
# The reactions every world has (VoxelRules): a lava block that water touches takes four seconds to become obsidian, showing its
# damage as it goes. Other reactions are data too: a rule file (VOXEL_RULES) adds to them.
REACTIONS = [{"block": "lava", "touches": "water", "becomes": "obsidian", "seconds": 4, "diagonal": True}]


# Items that are not blocks: a creature leaves a hide when it dies. They are held in the inventory like blocks, but a slot
# holding one cannot place it (the game checks COLOURS, which lists the blocks only), and the world never defines them.
HIDE = 40
PICKAXE = 41                        # a tool: it does not stack, and when it is the selected slot its swings on PICKAXE_BLOCKS count double
IRON_PICKAXE = 42                   # a tool that mines obsidian, and lasts four times as long as a pickaxe
SWORD = 43                          # a weapon: it strikes creatures for its damage (WEAPONS); it mines like a hand, and does not wear
SHIELD = 44                         # held in the left hand (VoxelGame.offhand): raised with Q, it takes the bites of creatures in front
SHIELD_USES = 120                   # the blocked bites a shield takes before it breaks (its uses, as a tool's are its swings)
TOOTH = 45                          # a biter's tooth: a crafting item (the bone blade), left where a biter falls
BONE_BLADE = 46                     # a weapon made from teeth and wood: it strikes like a sword, and wears out sooner
SHOVEL = 47                         # a tool for the soft ground: it breaks dirt, sand and gravel faster (TOOLS)
BUCKET = 48                         # empty: a strike on water or lava fills it; it is made from iron
WATER_BUCKET = 49                   # filled with water: a right click places a water block and leaves the bucket empty
LAVA_BUCKET = 50                    # filled with lava: a right click places a lava block and leaves the bucket empty
BOW = 51                            # held in the right hand: hold the place button to draw, let go to fire an arrow (VoxelArrows)
ARROW = 52                          # the bow's ammunition: it stacks; one that misses a block lies where it lands
ITEM_COLOURS = {HIDE: (0.72, 0.56, 0.40), PICKAXE: (0.62, 0.62, 0.68), IRON_PICKAXE: (0.74, 0.72, 0.70), SWORD: (0.80, 0.82, 0.86),
                SHIELD: (0.55, 0.40, 0.25), TOOTH: (0.92, 0.88, 0.76), BONE_BLADE: (0.80, 0.76, 0.62), SHOVEL: (0.70, 0.66, 0.58),
                BUCKET: (0.75, 0.77, 0.80), WATER_BUCKET: (0.30, 0.50, 0.90), LAVA_BUCKET: (0.95, 0.45, 0.10),
                BOW: (0.50, 0.34, 0.18), ARROW: (0.80, 0.78, 0.70)}
ITEM_NAMES = {HIDE: "hide", PICKAXE: "pickaxe", IRON_PICKAXE: "iron pickaxe", SWORD: "sword", SHIELD: "shield", TOOTH: "tooth",
              BONE_BLADE: "bone blade", SHOVEL: "shovel", BUCKET: "bucket", WATER_BUCKET: "water bucket", LAVA_BUCKET: "lava bucket",
              BOW: "bow", ARROW: "arrow"}
NAMES.update(ITEM_NAMES)
# Armour: a piece is worn in its slot (VoxelEquipment). Its data is resources/voxel_equipment.json; each name in that file must be a
# constant here, so a typo is an error at load and not a silent gap. Its name, colour and recipe come from the same file and the recipes.
LEATHER_CAP, LEATHER_TUNIC, LEATHER_LEGGINGS, LEATHER_BOOTS, LEATHER_GLOVES = range(53, 58)


def _equipment_table():
    """The armour table from resources/voxel_equipment.json (VOXEL_EQUIPMENT picks another file): item id -> {slot, speed, jump, name}.
    Also sets the pieces' names and colours, so the inventory shows them."""
    if os.environ.get("VOXEL_EQUIPMENT"):
        path = os.environ["VOXEL_EQUIPMENT"]
    else:
        here = os.path.dirname(os.path.abspath(__file__))
        candidates = [os.path.join(here, "voxel_equipment.json"), os.path.join(here, os.pardir, "voxel_equipment.json")]
        path = next((c for c in candidates if os.path.exists(c)), None)
        if path is None:
            raise FileNotFoundError("voxel_equipment.json was not found beside the scripts or in resources")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    table = {}
    for constant, entry in data["items"].items():
        if constant not in globals() or not isinstance(globals()[constant], int):
            raise ValueError("voxel_equipment.json names %r, which VoxelGen does not define" % constant)
        item = globals()[constant]
        table[item] = {"slot": entry["slot"], "speed": float(entry.get("speed", 0.0)), "jump": float(entry.get("jump", 0.0)),
                       "damage": float(entry.get("damage", 0.0)), "break": float(entry.get("break", 0.0)), "name": entry["name"]}
        ITEM_NAMES[item] = entry["name"]
        NAMES[item] = entry["name"]
        ITEM_COLOURS[item] = tuple(entry["colour"])
    return table


EQUIPMENT = _equipment_table()
PICKAXE_BLOCKS = (STONE, COAL, IRON, BRICK, OBSIDIAN)   # the blocks a pickaxe mines faster (its block_damage, below)
# A tool's durability is the swings that hit a block it can use before it breaks (one use a swing), and its tier is the hardest
# block it mines (BLOCK_TIER). A block that is not listed in BLOCK_TIER needs no tool: stone, coal and iron are mined by hand.
# block_damage: how much one strike breaks each block (a whole number of hits is made, and a part carries to the next strike);
# a block it does not list takes harvest strikes' worth (1 for the hand, and the tool's harvest where it has one).
# creature_damage: the health one strike takes from a creature (its health is VoxelCreatures' "health"); the hand does 1.
def _tools_table():
    """The tool table, the mining tiers and the weapons from resources/voxel_tools.json (VOXEL_TOOLS picks another file). Each name
    must be a constant of this file, so a typo in the data is an error here and not a silent gap in the game."""
    if os.environ.get("VOXEL_TOOLS"):
        path = os.environ["VOXEL_TOOLS"]
    else:
        here = os.path.dirname(os.path.abspath(__file__))
        candidates = [os.path.join(here, "voxel_tools.json"), os.path.join(here, os.pardir, "voxel_tools.json")]
        path = next((c for c in candidates if os.path.exists(c)), None)
        if path is None:
            raise FileNotFoundError("voxel_tools.json was not found beside the scripts or in resources")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    def ident(name):
        value = globals().get(name)
        if not isinstance(value, int) or isinstance(value, bool):
            raise ValueError("%s in %s is not a constant of VoxelGen.py" % (name, path))
        return value

    tools = {}
    for entry in data["tools"]:
        fields = {k: v for k, v in entry.items() if k != "key"}
        fields["block_damage"] = {ident(block): hits for block, hits in entry.get("block_damage", {}).items()}
        tools[ident(entry["key"])] = fields
    tier = {ident(block): level for block, level in data["block_tier"].items()}
    weapons = {ident(entry["key"]): {"damage": entry["damage"], "durability": entry["durability"]} for entry in data["weapons"]}
    return tools, tier, weapons


# The tools, the mining tiers and the weapons are data (resources/voxel_tools.json): a tool's durability, tier and damage, the blocks
# it mines faster, and a weapon's damage and durability. See the file for what each field means.
TOOLS, BLOCK_TIER, WEAPONS = _tools_table()
# The buckets: an empty one is filled by a strike on a liquid, and a filled one places its liquid (VoxelGame, placement_check).
BUCKETS = (BUCKET, WATER_BUCKET, LAVA_BUCKET)
FILLED_BUCKET = {WATER: WATER_BUCKET, LAVA: LAVA_BUCKET}   # the bucket a liquid fills, by the liquid's source block
# How many units a vessel holds when full, by its item. A bucket holds 8 units, which is one full volume. Change the numbers to make
# bigger or smaller vessels. A filled vessel's stack is [item, 1, units]: the units it holds now (VoxelInventory).
VESSEL_CAPACITY = {BUCKET: 8, WATER_BUCKET: 8, LAVA_BUCKET: 8}
FILLED_VESSELS = (WATER_BUCKET, LAVA_BUCKET)   # the vessels that hold a liquid
# The volume a filled vessel pours, by its item: the id of one unit of the liquid (the units it holds are its own).
BUCKET_VOLUME = {WATER_BUCKET: WATER_VOLUME_FIRST, LAVA_BUCKET: LAVA_VOLUME_FIRST}


def vessel_capacity(item):
    """The units a vessel holds when full, or None for an item that is not a vessel."""
    return VESSEL_CAPACITY.get(item)
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
    """The uses an item has when new, for a tool (it wears when it swings at a block), a weapon (it wears when it strikes a
    creature) or a shield (it wears when it blocks a bite), or None for an item that does not wear."""
    if item in TOOLS:
        return TOOLS[item]["durability"]
    if item in WEAPONS:
        return WEAPONS[item]["durability"]
    if item == SHIELD:
        return SHIELD_USES
    return None


def mining_tier(item):
    """The tier a tool mines at; 0 for no tool, and for an item that is not a tool."""
    return TOOLS[item]["tier"] if item in TOOLS else 0


def can_mine(item, block):
    """Whether the item in the selected slot (or None) can break the block: its tier must be at least the block's."""
    return mining_tier(item) >= BLOCK_TIER.get(block, 0)
