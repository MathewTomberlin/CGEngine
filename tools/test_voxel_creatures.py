"""
Checks the creature kinds (resources/scripts/VoxelCreatures.py, resources/voxel_creatures.json) without the engine: the file loads
into two kinds (timid and biter) with the numbers the walkers use, the shares add up to 1, choose() draws the kinds in those shares,
by_name() names an unknown kind, and a bad file is an error that names the kind and the field: an unknown field, a missing field,
shares that do not add up, a colour out of range, a negative flee range, a duplicate name and a non-whole health.
Usage (from the repo root): python -I tools/test_voxel_creatures.py
Exits 0 on success, 1 on any failed check.
"""
import json
import os
import random
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelCreatures  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


def raises(fn, *words):
    try:
        fn()
    except (ValueError, KeyError) as error:
        return all(word in str(error) for word in words)
    return False


def write_kinds(kinds):
    handle, path = tempfile.mkstemp(suffix=".json")
    with os.fdopen(handle, "w", encoding="utf-8") as f:
        json.dump({"version": 1, "kinds": kinds}, f)
    return path


def base(**changes):
    """A valid timid kind with the fields changed, or removed when the value is None."""
    kind = {"name": "timid", "share": 0.7, "bites": False, "model": "models/voxel/horse.fbx", "material": "horse",
            "clip_prefix": "horse", "colours": [[0.9, 0.9, 0.8]], "health": 3, "pace": 1.4, "flee_range": 4.5, "flee_pace": 3.0}
    for key, value in changes.items():
        if value is None:
            kind.pop(key, None)
        else:
            kind[key] = value
    return kind


def biter(**changes):
    kind = {"name": "biter", "share": 0.3, "bites": True, "model": "models/voxel/horse.fbx", "material": "horse",
            "clip_prefix": "horse", "colours": [[0.4, 0.1, 0.1]], "health": 3, "pace": 1.4, "chase_pace": 2.0,
            "bite_range": 1.3, "bite_damage": 1.0, "bite_interval": 1.2, "prey_range": 6.0}
    for key, value in changes.items():
        if value is None:
            kind.pop(key, None)
        else:
            kind[key] = value
    return kind


def loads(kinds):
    path = write_kinds(kinds)
    try:
        return VoxelCreatures.load(path)
    finally:
        os.remove(path)


# The shipped file.
shipped = VoxelCreatures.load(VoxelCreatures.data_file())
check("the shipped file has the two kinds, timid and biter", [k["name"] for k in shipped] == ["timid", "biter"])
check("the shares of the shipped kinds add up to 1", abs(sum(k["share"] for k in shipped) - 1.0) < 1e-9)
timid, bite = VoxelCreatures.by_name("timid"), VoxelCreatures.by_name("biter")
check("the timid kind runs and does not bite", not timid["bites"] and timid["flee_range"] > 0 and timid["flee_pace"] > timid["pace"])
check("the biter bites and chases faster than it walks", bite["bites"] and bite["chase_pace"] > bite["pace"] and bite["bite_damage"] > 0)
check("the timid kind's colours are the palette the walkers draw from", len(timid["colours"]) == 4)
check("a shipped kind names its model and clips", all(k["model"].endswith(".fbx") and k["clip_prefix"] for k in shipped))
check("the shipped numbers match the game's (health 3, bite damage 1)", timid["health"] == 3 and bite["bite_damage"] == 1.0)

# choose() draws in the shares.
rng = random.Random(7)
draws = [VoxelCreatures.choose(rng)["name"] for _ in range(20000)]
biter_share = draws.count("biter") / len(draws)
check("choose() draws the biter about 30 times in 100", abs(biter_share - 0.3) < 0.02)
check("by_name() names an unknown kind", raises(lambda: VoxelCreatures.by_name("dragon"), "dragon"))

# A valid file of two kinds loads with the numbers as floats and the colours as tuples.
kinds = loads([base(), biter()])
check("a valid file loads", len(kinds) == 2 and isinstance(kinds[0]["pace"], float))
check("colours load as tuples of three floats", kinds[0]["colours"] == ((0.9, 0.9, 0.8),))

# Bad files.
check("an unknown field is an error naming it", raises(lambda: loads([base(), biter(speed=2)]), "speed"))
check("a timid kind with bite fields is an error", raises(lambda: loads([base(bite_range=1.0), biter()]), "bite_range"))
check("a biter without a bite range is an error", raises(lambda: loads([base(), biter(bite_range=None)]), "bite_range"))
check("a timid kind without a flee range is an error", raises(lambda: loads([base(flee_range=None), biter()]), "flee_range"))
check("shares that do not add up to 1 are an error", raises(lambda: loads([base(share=0.5), biter()]), "add up"))
check("a colour out of range is an error", raises(lambda: loads([base(colours=[[1.5, 0, 0]]), biter()]), "colour"))
check("a negative flee range is an error", raises(lambda: loads([base(flee_range=-1.0), biter()]), "flee_range"))
check("a zero pace is an error", raises(lambda: loads([base(pace=0.0), biter()]), "pace"))
check("a duplicate name is an error", raises(lambda: loads([base(), base(share=0.3, name="timid")]), "timid"))
check("a non-whole health is an error", raises(lambda: loads([base(health=2.5), biter()]), "health"))
check("an empty kinds list is an error", raises(lambda: loads([]), "kinds"))
check("a missing name is an error", raises(lambda: loads([base(name=None), biter()]), "name"))
check("a missing model is an error", raises(lambda: loads([base(model=None), biter()]), "model"))

# The optional fields: a kind's clips by role, and its flash material.
check("the shipped biter plays the drake's clips", VoxelCreatures.clip_for(bite, "attack") == "drake_attack"
      and VoxelCreatures.clip_for(bite, "bound") == "drake_walk" and VoxelCreatures.clip_for(bite, "die") == "drake_die")
check("a kind with no hurt clip has none, so a hit only flashes it", VoxelCreatures.clip_for(bite, "hurt") is None)
check("the timid kind's clips are its prefix's", VoxelCreatures.clip_for(timid, "hurt") == "horse_hurt")
check("the biter flashes its own red material", bite["flash_material"] == "dragon_hurt")
check("a kind with no flash material flashes its own", loads([base(), biter()])[0]["flash_material"] == "horse")
check("clips by role load", VoxelCreatures.clip_for(loads([base(clips={"walk": "w"}), biter()])[0], "walk") == "w")
check("a clip for a role the kind does not play is an error",
      raises(lambda: loads([base(clips={"jump": "j"}), biter()]), "clips"))
check("an empty clip name is an error", raises(lambda: loads([base(clips={"walk": ""}), biter()]), "clips"))
check("a flash material that is not a name is an error", raises(lambda: loads([base(flash_material=""), biter()]), "flash_material"))

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All creature kind checks passed.")
