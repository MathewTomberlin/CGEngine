"""
Reactions as data (roadmap 10.3). A rule file is JSON:

    {"rules": [{"block": "grass", "touches": "water", "becomes": "dirt", "chance": 0.5},
               {"block": "lava", "touches": "water", "becomes": "obsidian", "seconds": 4}]}

A rule says that a block touching one of the kinds in "touches" becomes "becomes". "diagonal": true counts the blocks that touch
only at an edge or a corner too (all 26 around the block), not just the six faces. Without "seconds" the change comes with
"chance" per simulation tick (1 by default, above 0 and at most 1). With "seconds" the block must keep touching for that long:
it shows its progress as damage stages, and then becomes the other block, as a block breaks when it is hit. Air is a block
too, so "block": "air" grows a block into the air beside it (leaves growing into the air, say).

Blocks are names from VoxelGen.NAMES (a name that belongs to one block id) or ids from 0 to 255. The families "water" and "lava"
stand for every block of that liquid, the source and its flowing levels, and can be used for "block" and in "touches".
A reaction is installed in the world by add_reaction (VoxelWorld). VoxelGen.REACTIONS are the ones the game always has.
"""
import json

import VoxelGen

KEYS = {"block", "touches", "becomes", "chance", "seconds", "diagonal"}


def _name_ids():
    # The ids of each name. A name shared by several ids (the flowing water levels) is left out, so it cannot be used.
    ids = {}
    for block, name in VoxelGen.NAMES.items():
        ids.setdefault(name, []).append(block)
    return {name: found[0] for name, found in ids.items() if len(found) == 1}


NAME_IDS = _name_ids()
FAMILIES = {"water": VoxelGen.WATER_BLOCKS, "lava": VoxelGen.LAVA_BLOCKS}


def _one(value, where):
    if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 255:
        return value
    if isinstance(value, str) and value in NAME_IDS:
        return NAME_IDS[value]
    raise ValueError("%s: %r is not a block name, a family or an id from 0 to 255" % (where, value))


def _blocks(value, where):
    """The block ids a name, a family or an id stands for."""
    if isinstance(value, str) and value in FAMILIES:
        return list(FAMILIES[value])
    return [_one(value, where)]


def parse(data):
    """The rules in a parsed file, as (block, touches, becomes, chance, seconds) tuples, one per block id a family names, with
    touches a sorted list of ids. Raises ValueError naming the first bad entry."""
    if not isinstance(data, dict) or not isinstance(data.get("rules"), list):
        raise ValueError('a rule file is an object with a "rules" list')
    rules = []
    for index, entry in enumerate(data["rules"]):
        where = "rule %d" % index
        if not isinstance(entry, dict):
            raise ValueError("%s: not an object" % where)
        unknown = sorted(set(entry) - KEYS)
        if unknown:
            raise ValueError("%s: unknown keys %s" % (where, ", ".join(unknown)))
        missing = sorted({"block", "touches", "becomes"} - set(entry))
        if missing:
            raise ValueError("%s: missing %s" % (where, ", ".join(missing)))
        blocks = _blocks(entry["block"], where + " block")
        touches = entry["touches"] if isinstance(entry["touches"], list) else [entry["touches"]]
        if not touches:
            raise ValueError("%s: touches needs at least one kind" % where)
        kinds = set()
        for kind in touches:
            kinds.update(_blocks(kind, where + " touches"))
        becomes = _one(entry["becomes"], where + " becomes")
        chance = entry.get("chance", 1)
        if isinstance(chance, bool) or not isinstance(chance, (int, float)) or not 0 < chance <= 1:
            raise ValueError("%s: chance must be above 0 and at most 1, not %r" % (where, chance))
        seconds = entry.get("seconds", 0)
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or seconds < 0:
            raise ValueError("%s: seconds must not be negative, not %r" % (where, seconds))
        diagonal = entry.get("diagonal", False)
        if not isinstance(diagonal, bool):
            raise ValueError("%s: diagonal must be true or false, not %r" % (where, diagonal))
        for block in blocks:
            rules.append((block, sorted(kinds), becomes, float(chance), float(seconds), diagonal))
    return rules


def install(world, reactions):
    """Replace the world's reactions with these (a list of entries like a rule file's) and return how many there are. Raises
    ValueError for a bad entry, and then the world keeps the reactions it had."""
    rules = parse({"rules": list(reactions)})
    world.clear_rules()
    return _add(world, rules)


def add_file(world, path):
    """Add the rules of a rule file to the world's reactions and return how many it added. Raises OSError or ValueError, and then
    nothing is added."""
    with open(path, encoding="utf-8") as f:
        rules = parse(json.load(f))
    return _add(world, rules)


def load(world, path):
    """Replace the world's reactions with the file's and return how many there are. Raises OSError or ValueError, and then the
    world keeps the reactions it had."""
    with open(path, encoding="utf-8") as f:
        rules = parse(json.load(f))
    world.clear_rules()
    return _add(world, rules)


def _add(world, rules):
    for block, kinds, becomes, chance, seconds, diagonal in rules:
        world.add_reaction(block, kinds, becomes, chance, seconds, diagonal)
    return len(rules)
