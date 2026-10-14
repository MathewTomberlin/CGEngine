"""
Reaction rules as data (roadmap 10.3). A rule file is JSON:

    {"rules": [{"block": "grass", "touches": "water", "becomes": "dirt", "chance": 0.5}]}

A rule says that a block touching the kind "touches" becomes "becomes", with this chance per simulation tick. Blocks are
names from VoxelGen.NAMES (only a name that belongs to one block id: "flowing water" is not one) or ids from 0 to 255.
"chance" is optional (1 by default) and must be above 0 and at most 1. A file is checked as a whole before any rule is
added, so a bad entry is reported and the world keeps its rules.
"""
import json

import VoxelGen

KEYS = {"block", "touches", "becomes", "chance"}


def _name_ids():
    # The ids of each name. A name shared by several ids (the flowing water levels) is left out, so it cannot be used.
    ids = {}
    for block, name in VoxelGen.NAMES.items():
        ids.setdefault(name, []).append(block)
    return {name: found[0] for name, found in ids.items() if len(found) == 1}


NAME_IDS = _name_ids()


def _block(value, where):
    if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 255:
        return value
    if isinstance(value, str) and value in NAME_IDS:
        return NAME_IDS[value]
    raise ValueError("%s: %r is not a block name or an id from 0 to 255" % (where, value))


def parse(data):
    """The rules in a parsed file, as (block, touches, becomes, chance) tuples. Raises ValueError naming the first bad entry."""
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
        missing = sorted(KEYS - {"chance"} - set(entry))
        if missing:
            raise ValueError("%s: missing %s" % (where, ", ".join(missing)))
        block = _block(entry["block"], where + " block")
        touches = _block(entry["touches"], where + " touches")
        becomes = _block(entry["becomes"], where + " becomes")
        chance = entry.get("chance", 1)
        if isinstance(chance, bool) or not isinstance(chance, (int, float)) or not 0 < chance <= 1:
            raise ValueError("%s: chance must be above 0 and at most 1, not %r" % (where, chance))
        rules.append((block, touches, becomes, float(chance)))
    return rules


def load(world, path):
    """Replace the world's rules with the file's and return how many there are. Raises OSError or ValueError, and then the
    world keeps the rules it had."""
    with open(path, encoding="utf-8") as f:
        rules = parse(json.load(f))
    world.clear_rules()
    for block, touches, becomes, chance in rules:
        world.add_rule(block, touches, becomes, chance)
    return len(rules)
