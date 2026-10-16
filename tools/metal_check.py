"""
Compares a metal wall with a dielectric one of the same colour, for the metallic term (roadmap 35.2). A dielectric's highlight is
white, so it adds green and blue to a red wall; a metal's takes the wall's red, so its green and blue stay low. The metal must have
a lower green-to-red ratio across the lit wall than the dielectric, by a clear margin.

Usage: python -I tools/metal_check.py METAL.png DIELECTRIC.png
Exits 0 when the check passes, 1 otherwise. Prints the numbers.
"""
import sys

import numpy as np
from PIL import Image

# The metal's green-to-red ratio must be at most this share of the dielectric's.
MAX_SHARE = 0.9


def ratio(path):
    rgb = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    red, green = rgb[..., 0].sum(), rgb[..., 1].sum()
    return green / max(red, 1.0)


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 1
    metal, dielectric = ratio(argv[1]), ratio(argv[2])
    share = metal / dielectric if dielectric > 0 else float("inf")
    ok = share <= MAX_SHARE
    print("%s the metal's highlight takes the wall's colour (green-to-red %.3f against %.3f for the dielectric: %.0f%%, at most %.0f%%)" % (
        "PASS" if ok else "FAIL", metal, dielectric, 100 * share, 100 * MAX_SHARE))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
