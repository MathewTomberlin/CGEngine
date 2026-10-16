"""
Compares three screenshots of the same wall for the normal map check (tools/test_normal_maps.ps1, roadmap 35.1): the wall with a
normal map, the wall with the map at strength 0, and the wall with no map. The map must change the lit wall, and a strength of 0
must not: a 0 strength is the flat wall, so its picture matches the no-map one.

Usage: python -I tools/normal_map_check.py ON.png ZERO.png OFF.png
Exits 0 when both checks pass, 1 otherwise. Prints the numbers.
"""
import sys

import numpy as np
from PIL import Image

# The map must move the lit wall's brightness by at least this many levels on average (0 to 255).
MIN_CHANGE = 2.0
# A strength of 0 may differ from the flat wall by no more than this (rounding and the frame's own noise).
MAX_FLAT_DIFF = 1.0


def load(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 1
    on, zero, off = (load(path) for path in argv[1:4])
    if on.shape != off.shape or zero.shape != off.shape:
        print("FAIL the three screenshots differ in size")
        return 1
    changed = float(np.mean(np.abs(on - off)))
    flat = float(np.mean(np.abs(zero - off)))
    ok_changed = changed >= MIN_CHANGE
    ok_flat = flat <= MAX_FLAT_DIFF
    print("%s the normal map changes the lit wall (mean difference %.2f levels, at least %.1f)" % (
        "PASS" if ok_changed else "FAIL", changed, MIN_CHANGE))
    print("%s strength 0 matches the flat wall (mean difference %.2f levels, at most %.1f)" % (
        "PASS" if ok_flat else "FAIL", flat, MAX_FLAT_DIFF))
    return 0 if ok_changed and ok_flat else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
