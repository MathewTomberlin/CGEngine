"""
The creature kinds in the voxel sample: what each kind of creature is, how it moves, how hard it hits and what it looks like.

A kind is data: resources/voxel_creatures.json (or the file named by VOXEL_CREATURES) lists them. A kind that bites has the
bite fields (chase_pace, bite_range, bite_damage, bite_interval, prey_range); a kind that does not bite runs away from the player
when it comes close (flee_range, flee_pace). Every kind names its model, its material and the prefix of its clips
(<prefix>_walk, <prefix>_idle, <prefix>_attack, <prefix>_bound), and the shares of the kinds add up to 1.

Pure Python with no engine calls, so tools/test_voxel_creatures.py checks it without a window. A bad file is an error that names
the kind and the field.
"""

import json
import os

COMMON = {"name", "share", "bites", "model", "material", "clip_prefix", "colours", "health", "pace"}
FLEE = {"flee_range", "flee_pace"}
BITE = {"chase_pace", "bite_range", "bite_damage", "bite_interval", "prey_range"}
# Optional fields: "clips" names the clip each role plays (a kind whose model's clips are not named <clip_prefix>_<role>; a role it
# leaves out is not played, so a kind with no hurt clip only flashes when hit); "flash_material" is the material drawn while a hit
# flashes the creature (default: its own material).
OPTIONAL = {"clips", "flash_material", "sight"}
# "sight" is how far (in blocks, by day) a biter notices the player and chases it; it is scaled by the night like the default
# (VoxelNight.CHASE_RANGE). A kind that does not bite has no sight: it runs away at its flee_range.
CLIP_ROLES = {"walk", "bound", "attack", "hurt", "die"}
POSITIVE = {"share", "pace", "flee_pace", "chase_pace", "bite_range", "bite_damage", "bite_interval", "prey_range", "sight"}
NON_NEGATIVE = {"flee_range"}


def _number(kind, key, value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("creature %r: %r must be a number" % (kind, key))
    if key in POSITIVE and not value > 0:
        raise ValueError("creature %r: %r must be more than 0" % (kind, key))
    if key in NON_NEGATIVE and value < 0:
        raise ValueError("creature %r: %r must not be negative" % (kind, key))
    return float(value)


def load(path):
    """The creature kinds in a JSON file, checked. Each kind is a dict; its colours are tuples of three floats."""
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    entries = data.get("kinds")
    if not isinstance(entries, list) or not entries:
        raise ValueError("%s: 'kinds' must be a non-empty list" % path)
    kinds = []
    names = set()
    for entry in entries:
        name = entry.get("name") if isinstance(entry, dict) else None
        if not isinstance(name, str) or not name:
            raise ValueError("%s: every creature needs a name" % path)
        if name in names:
            raise ValueError("creature %r is listed twice" % name)
        names.add(name)
        if not isinstance(entry.get("bites"), bool):
            raise ValueError("creature %r: 'bites' must be true or false" % name)
        allowed = COMMON | OPTIONAL | (BITE if entry["bites"] else FLEE)
        unknown = set(entry) - allowed
        if unknown:
            raise ValueError("creature %r: unknown or not allowed for this kind: %s" % (name, ", ".join(sorted(unknown))))
        if "sight" in entry and not entry["bites"]:
            raise ValueError("creature %r: 'sight' is for a biter (a kind that does not bite runs away at its flee_range)" % name)
        missing = (allowed - OPTIONAL) - set(entry)
        if missing:
            raise ValueError("creature %r: missing %s" % (name, ", ".join(sorted(missing))))
        for key in ("model", "material", "clip_prefix"):
            if not isinstance(entry[key], str) or not entry[key]:
                raise ValueError("creature %r: %r must be a non-empty string" % (name, key))
        colours = entry["colours"]
        if not isinstance(colours, list) or not colours:
            raise ValueError("creature %r: 'colours' must be a non-empty list" % name)
        clean_colours = []
        for colour in colours:
            if not isinstance(colour, list) or len(colour) != 3 or not all(
                    isinstance(c, (int, float)) and not isinstance(c, bool) and 0.0 <= c <= 1.0 for c in colour):
                raise ValueError("creature %r: each colour is three numbers from 0 to 1" % name)
            clean_colours.append(tuple(float(c) for c in colour))
        health = entry["health"]
        if isinstance(health, bool) or not isinstance(health, int) or health < 1:
            raise ValueError("creature %r: 'health' must be a whole number of 1 or more" % name)
        clips = entry.get("clips")
        if clips is not None and (not isinstance(clips, dict) or not clips or not set(clips) <= CLIP_ROLES
                                  or not all(isinstance(c, str) and c for c in clips.values())):
            raise ValueError("creature %r: 'clips' maps some of %s to clip names" % (name, ", ".join(sorted(CLIP_ROLES))))
        flash_material = entry.get("flash_material", entry["material"])
        if not isinstance(flash_material, str) or not flash_material:
            raise ValueError("creature %r: 'flash_material' must be a non-empty string" % name)
        kind = {"name": name, "bites": entry["bites"], "model": entry["model"], "material": entry["material"],
                "clip_prefix": entry["clip_prefix"], "colours": tuple(clean_colours), "health": health,
                "clips": dict(clips) if clips is not None else None, "flash_material": flash_material}
        for key in (allowed - OPTIONAL) - {"name", "bites", "model", "material", "clip_prefix", "colours", "health"}:
            kind[key] = _number(name, key, entry[key])
        if "sight" in entry:
            kind["sight"] = _number(name, "sight", entry["sight"])
        kinds.append(kind)
    total = sum(kind["share"] for kind in kinds)
    if abs(total - 1.0) > 1e-6:
        raise ValueError("%s: the shares of the creatures add up to %g, not 1" % (path, total))
    return tuple(kinds)


def clip_for(kind, role):
    """The clip a kind plays for a role (walk, bound, attack, hurt, die): the kind's named clip, or <clip_prefix>_<role> when the kind
    names none. None when the kind has no clip for the role, so nothing plays."""
    if kind["clips"] is not None:
        return kind["clips"].get(role)
    return kind["clip_prefix"] + "_" + role


def data_file():
    """VOXEL_CREATURES if it is set; else voxel_creatures.json beside this script (the game folder) or in resources (the repository)."""
    if os.environ.get("VOXEL_CREATURES"):
        return os.environ["VOXEL_CREATURES"]
    here = os.path.dirname(os.path.abspath(__file__))
    for path in (os.path.join(here, "voxel_creatures.json"), os.path.join(here, os.pardir, "voxel_creatures.json")):
        if os.path.isfile(path):
            return path
    raise FileNotFoundError("voxel_creatures.json was not found beside the scripts or in resources")


KINDS = load(data_file())
BY_NAME = {kind["name"]: kind for kind in KINDS}


def by_name(name):
    """The kind with that name. An unknown name is an error, named."""
    if name not in BY_NAME:
        raise KeyError("no creature kind named %r" % name)
    return BY_NAME[name]


def choose(rng):
    """A kind drawn by share: a random number picks the kind whose share it falls in."""
    point = rng.random()
    for kind in KINDS:
        point -= kind["share"]
        if point < 0.0:
            return kind
    return KINDS[-1]
