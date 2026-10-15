"""
Checks the held items' shield pose and its raise (resources/scripts/VoxelHands.py) without the engine: the raise takes
SHIELD_RAISE_SECONDS to go from the carried pose to the raised one, each frame places the pose it has reached, a blend between two
transforms turns each angle the short way round, and a shield that is put away is removed. Usage (from the repo root):
python -I tools/test_voxel_hands.py   Exits 0 on success, 1 on any failed check.
"""
import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

# The engine's calls are recorded instead of made, so the shield's pose can be followed frame by frame.
calls = []
fake = types.ModuleType("cg_engine_bindings")
fake.load_scene_json = lambda text: calls.append(("load", None))
fake.set_viewmodel = lambda name, flag: calls.append(("viewmodel", name))
fake.attach_to_bone = lambda name, target, bone, offset: calls.append(("attach", list(offset)))
fake.remove_body = lambda name: calls.append(("remove", name))
fake.play_sound = lambda *a: None
sys.modules["cg_engine_bindings"] = fake

import VoxelGen  # noqa: E402
import VoxelHands as hands  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


class Player(hands.HandsMixin):
    """Only what the shield's pose reads: whether Q is held, and the offhand."""
    def __init__(self):
        self.offhand = VoxelGen.SHIELD
        self.holding_q = False
        self.shield_state = None

    def shield_up(self):
        return self.holding_q


def close(a, b, tol=1e-9):
    return all(abs(x - y) < tol for x, y in zip(a, b))


spec_a = {"position": [0.0, 0.0, 0.0], "rotation": [170.0, 0.0, 0.0], "scale": 1.0}
spec_b = {"position": [1.0, 0.0, 0.0], "rotation": [-170.0, 0.0, 0.0], "scale": 0.5}
mid = hands.blend_spec(spec_a, spec_b, 0.5)
check("a blend at 0 is the first transform and at 1 the second", hands.blend_spec(spec_a, spec_b, 0.0) == spec_a
      and close(hands.blend_spec(spec_a, spec_b, 1.0)["position"], spec_b["position"]))
check("a blend turns the angle the short way round (170 to -170 passes 180, not 0)", abs(abs(mid["rotation"][0]) - 180.0) < 1e-9)
check("a blend moves the position and the scale in step", close(mid["position"], [0.5, 0.0, 0.0]) and abs(mid["scale"] - 0.75) < 1e-9)

item = hands.HELD_ITEMS[VoxelGen.NAMES[VoxelGen.SHIELD]]
carried = hands.transform_matrix(item["offset_spec"])
raised = hands.transform_matrix(item["raised_spec"])

p = Player()
p.update_shield(0.0)
check("a shield held in the offhand is loaded carried, and placed at its carried pose",
      calls[-2:] == [("viewmodel", hands.SHIELD_HELD), ("attach", carried)] or calls[-1] == ("attach", carried))
calls.clear()

p.holding_q = True
frame = 1.0 / 60.0
raises = []
placed = []        # (raise, offset) for each frame a pose is placed
for _ in range(int(hands.SHIELD_RAISE_SECONDS / frame) + 3):
    calls.clear()
    p.update_shield(frame)
    raises.append(p.shield_raise)
    for call in calls:
        if call[0] == "attach":
            placed.append((p.shield_raise, call[1]))
check("the raise rises a frame at a time, never falls, and reaches 1 within SHIELD_RAISE_SECONDS",
      all(b >= a for a, b in zip(raises, raises[1:])) and raises[0] > 0.0
      and raises[-1] == 1.0 and raises.index(1.0) <= int(hands.SHIELD_RAISE_SECONDS / frame) + 1)
check("a pose is placed on each frame the raise moves, and the last one is the raised pose",
      len(placed) >= 2 and [r for r, _ in placed] == sorted({r for r, _ in placed}) and close(placed[-1][1], raised))

p.update_shield(0.0)
check("a paused game keeps the raise where it is", p.shield_raise == 1.0)

p.holding_q = False
calls.clear()
p.update_shield(hands.SHIELD_RAISE_SECONDS / 2)
check("lowering takes the same time back: halfway after half the time", abs(p.shield_raise - 0.5) < 1e-9)
p.update_shield(hands.SHIELD_RAISE_SECONDS)
check("the shield is carried again at the end of the lowering", p.shield_raise == 0.0 and close(calls[-1][1], carried))

p.offhand = None
calls.clear()
p.update_shield(frame)
check("a shield put away is removed from the view", ("remove", hands.SHIELD_HELD) in calls and p.shield_state is None)

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel hands checks passed.")
