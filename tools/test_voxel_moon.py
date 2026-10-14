"""
Checks the moon's phase curve without the engine: the share of the moon's light at each phase of its cycle is 0% at a new moon,
25% at a crescent, 50% at a half moon, 75% at a gibbous moon, 100% at full, lerped between those, and the cycle wraps.
Usage (from the repo root): python -I tools/test_voxel_moon.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "resources", "scripts"))

import VoxelConfig  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


keys = [(0.0, 0.0, "a new moon gives no light"), (0.125, 0.25, "a crescent gives 25%"), (0.25, 0.5, "a half moon gives 50%"),
        (0.375, 0.75, "a gibbous moon gives 75%"), (0.5, 1.0, "a full moon gives 100%"), (0.625, 0.75, "a waning gibbous gives 75%"),
        (0.75, 0.5, "a last quarter gives 50%"), (0.875, 0.25, "a waning crescent gives 25%")]
for phase, share, name in keys:
    check(name, abs(VoxelConfig.moon_share(phase) - share) < 1e-9)
check("the share lerps between two phases", abs(VoxelConfig.moon_share(0.1875) - 0.375) < 1e-9)
check("the cycle wraps: phase 1.25 is a half moon", abs(VoxelConfig.moon_share(1.25) - 0.5) < 1e-9)
check("the share stays within 0 to 1", all(0.0 <= VoxelConfig.moon_share(i / 1000.0) <= 1.0 for i in range(1000)))

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
print("All voxel moon checks passed.")
