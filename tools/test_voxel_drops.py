"""
Checks the items on the ground (resources/scripts/VoxelDrops.py) without the engine: a drop is one mesh body that bobs above its
feet, the player takes the drops in reach while the inventory has room, drops out of reach or a floor away stay, and the drops
load back from their saved list, damaged entries skipped.
Usage (from the repo root): python -I tools/test_voxel_drops.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

# The engine's mesh calls are recorded instead of made, so the drops can be checked without a window.
calls = []
fake = types.ModuleType("cg_engine_bindings")
fake.create_mesh_body = lambda name, *a, **k: calls.append(("create", name))
fake.update_mesh_body = lambda name, *a, **k: calls.append(("update", name))
fake.remove_mesh_body = lambda name: calls.append(("remove", name))
fake.Vector3f = lambda *a: a
sys.modules["cg_engine_bindings"] = fake

import VoxelDrops as drops  # noqa: E402
import VoxelInventory as inv  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


HIDE = 40
COLOUR = (0.72, 0.56, 0.40)
GROUND = (5.0, 10.0, 5.0)

# A drop is made as one mesh body, and it bobs above its feet without going below them.
d = drops.Drops()
d.add(HIDE, COLOUR, GROUND)
check("a drop is made as one mesh body", calls == [("create", "drop_1")] and len(d.live) == 1)
arrays = d.live[0].arrays()
check("a drop's arrays are a box (it has vertices and indices)", len(arrays) >= 2 and all(len(a) > 0 for a in arrays))
calls.clear()
d.update(0.0)
check("a paused game (dt 0) does not rebuild the drops", calls == [])
d.update(0.5)
check("a running game rebuilds each drop once a frame", calls == [("update", "drop_1")])

# The player takes the drops within reach while the inventory has room for them.
i = inv.Inventory()
calls.clear()
near = (5.5, 10.0, 5.2)
far = (9.0, 10.0, 9.0)
upstairs = (5.0, 12.5, 5.0)
check("a drop 0.4 blocks away is in reach", drops.in_reach(GROUND, near))
check("a drop 5 blocks away is out of reach", not drops.in_reach(GROUND, far))
check("a drop a floor above is out of reach", not drops.in_reach(GROUND, upstairs))
check("nothing is taken out of reach", d.take_in_reach(far, i) == [] and len(d.live) == 1)
check("nothing is taken a floor away", d.take_in_reach(upstairs, i) == [] and len(d.live) == 1)
taken = d.take_in_reach(near, i)
check("a drop in reach is taken into the inventory", taken == [HIDE] and i.count(HIDE) == 1 and d.live == [])
check("a taken drop removes its mesh body", ("remove", "drop_1") in calls)

# A full inventory leaves the drop on the ground, and it is taken once there is room.
full = inv.Inventory()
for slot in range(inv.SLOTS):
    full.add(3, inv.STACK_LIMIT)
d = drops.Drops()
d.add(HIDE, COLOUR, GROUND)
check("a full inventory leaves the drop on the ground", d.take_in_reach(near, full) == [] and len(d.live) == 1)
full.take(0, inv.STACK_LIMIT)
check("the drop is taken once a slot is free", d.take_in_reach(near, full) == [HIDE] and d.live == [])

# Several drops in reach are all taken, in the order they were dropped.
i = inv.Inventory()
d = drops.Drops()
d.add(HIDE, COLOUR, GROUND)
d.add(HIDE, COLOUR, (5.1, 10.0, 5.1))
check("every drop in reach is taken", d.take_in_reach(near, i) == [HIDE, HIDE] and i.count(HIDE) == 2)

# The drops save as a list of [item, x, y, z] and load back, with damaged entries skipped.
d = drops.Drops()
d.add(HIDE, COLOUR, GROUND)
saved = d.to_list()
check("a drop saves as [item, x, y, z]", saved == [[HIDE, 5.0, 10.0, 5.0]])
calls.clear()
e = drops.Drops()
e.load_list(saved + ["junk", [HIDE, "x", 1, 2], [HIDE, 1.0]], lambda item: COLOUR)
check("a saved drop loads back at its place", len(e.live) == 1 and e.live[0].feet == GROUND and e.live[0].item == HIDE)
check("a damaged entry is skipped, the good one kept", len(e.live) == 1)
calls.clear()
e.load_list([], lambda item: COLOUR)
check("loading an empty list removes the drops in the world", e.live == [] and ("remove", "drop_1") in calls)
e.add(HIDE, COLOUR, GROUND)
e.load_list(saved, lambda item: COLOUR)
check("loading replaces the drops that were there", len(e.live) == 1 and e.live[0].name != "drop_1")

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel drop checks passed.")
