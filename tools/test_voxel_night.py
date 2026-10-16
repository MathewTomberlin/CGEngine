"""
Checks the night's rules for the voxel sample's creatures (resources/scripts/VoxelNight.py) without the engine: the night is dark
at midnight and not at noon, it grows smoothly through dusk, and a biter comes after the player from further away at night.
Usage (from the repo root): python -I tools/test_voxel_night.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelNight  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


check("it is not dark at noon", VoxelNight.night_share(1.0) == 0.0)
check("it is fully dark at midnight", VoxelNight.night_share(-1.0) == 1.0)
check("it is not dark at sunrise and sunset", VoxelNight.night_share(0.0) == 0.0)
check("the night is half at a sun below the horizon by half the night height",
      abs(VoxelNight.night_share(-VoxelNight.NIGHT_HEIGHT / 2) - 0.5) < 1e-9)
check("the night share stays between 0 and 1", all(0.0 <= VoxelNight.night_share(h / 10.0) <= 1.0 for h in range(-30, 31)))
check("the night share never falls as the sun sinks", all(VoxelNight.night_share(h / 100.0) >= VoxelNight.night_share(h / 100.0 + 0.01)
                                                         for h in range(-100, 100)))

check("by day a biter chases from the usual range", VoxelNight.chase_range(0.0) == VoxelNight.CHASE_RANGE)
check("at full night a biter chases from further away",
      abs(VoxelNight.chase_range(1.0) - VoxelNight.CHASE_RANGE * (1.0 + VoxelNight.NIGHT_CHASE_SHARE)) < 1e-9)
check("the chase range grows with the night", VoxelNight.chase_range(0.2) < VoxelNight.chase_range(0.6) < VoxelNight.chase_range(1.0))
check("the night share is clamped when it is passed out of range",
      VoxelNight.chase_range(5.0) == VoxelNight.chase_range(1.0) and VoxelNight.chase_range(-5.0) == VoxelNight.chase_range(0.0))

print()
if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel night checks passed.")
sys.exit(0)
