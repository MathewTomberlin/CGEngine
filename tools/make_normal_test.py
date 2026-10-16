"""
Makes resources/normal_test.png, the tangent-space normal map the normal map check uses (roadmap 35.1). The map is a grid of round
bumps that tile seamlessly: its height is a sum of Gaussian bumps, and its normals come from the height's slopes. Red and green
tilt the normal, blue points out of the surface, and a flat patch is (128, 128, 255).

Usage (from the repo root): python -I tools/make_normal_test.py
Needs Pillow and numpy. Writes the same bytes each run, so a change in the map is a change in the file.
"""
import os

import numpy as np
from PIL import Image

SIZE = 256            # pixels, square
BUMPS = 4             # bumps across the image (the map tiles, so the edges match)
RADIUS = 0.18         # a bump's radius, as a share of its cell
STRENGTH = 3.0        # how steep the bumps are (the normal's tilt scale)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "resources", "normal_test.png")


def height_field():
    """The bumps' height on a SIZE by SIZE grid, periodic over the image."""
    cell = SIZE / BUMPS
    ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float64)
    height = np.zeros((SIZE, SIZE))
    for by in range(BUMPS):
        for bx in range(BUMPS):
            cx = (bx + 0.5) * cell
            cy = (by + 0.5) * cell
            # The nearest copy of the bump, so the bump is periodic across the tile's edges.
            dx = (xs - cx + SIZE / 2) % SIZE - SIZE / 2
            dy = (ys - cy + SIZE / 2) % SIZE - SIZE / 2
            r2 = (dx * dx + dy * dy) / (cell * cell)
            height += np.exp(-r2 / (2 * RADIUS * RADIUS))
    return height


def main():
    h = height_field()
    # Slopes per pixel, wrapped at the edges (np.roll), so the normal map has no seam.
    dhdx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) / 2.0
    dhdy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) / 2.0
    nx = -dhdx * STRENGTH
    ny = -dhdy * STRENGTH
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack([nx / length, ny / length, nz / length], axis=-1)
    rgb = np.clip(np.round((normal * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(rgb, "RGB").save(OUT)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
