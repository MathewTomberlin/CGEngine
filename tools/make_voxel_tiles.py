"""
Writes resources/voxel_tiles.png: the voxel sample's block textures, one 32 by 32 tile per layer, stacked top to bottom. The
engine reads it as a texture array (Renderer::setVoxelTiles); VoxelGen.TILES says which layer each block uses. Every tile is
seamless, so it repeats across the blocks. The art is procedural and seeded, so the file is the same every time it is written.

Needs numpy and Pillow. Usage (from the repo root): python -I tools/make_voxel_tiles.py
"""
import os

import numpy as np
from PIL import Image

N = 32                                  # pixels a side of a tile
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resources", "voxel_tiles.png")
# The layer order. VoxelGen.TILE_NAMES must list the same names in the same order.
TILE_NAMES = ["grass_top", "grass_side", "dirt", "stone", "sand", "water", "snow_top", "snow_side", "bark", "wood_rings",
              "leaves", "gravel", "lamp", "brick", "coal_ore", "iron_ore", "lava"]


def noise(rng, cells_x, cells_y):
    """Seamless value noise in 0..1, N by N: random values on a lattice that wraps, blended with a smooth curve."""
    grid = rng.random((cells_y, cells_x))
    x = np.arange(N) * cells_x / N
    y = np.arange(N) * cells_y / N
    ix, tx = np.floor(x).astype(int), x - np.floor(x)
    iy, ty = np.floor(y).astype(int), y - np.floor(y)
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    ix0, ix1 = ix % cells_x, (ix + 1) % cells_x
    iy0, iy1 = iy % cells_y, (iy + 1) % cells_y
    rows = grid[:, ix0] * (1 - tx) + grid[:, ix1] * tx               # (cells_y, N)
    return rows[iy0] * (1 - ty)[:, None] + rows[iy1] * ty[:, None]   # (N, N)


def fbm(rng, base=4):
    """Three octaves of noise, summed and scaled back to 0..1."""
    total = 0.0
    weight = 0.0
    for scale, amp in ((base, 1.0), (base * 2, 0.5), (base * 4, 0.25)):
        total = total + amp * noise(rng, scale, scale)
        weight += amp
    return total / weight


def paint(base, variation, amount=0.25):
    """A colour scaled by 1 +- amount by a 0..1 field, as N by N by 3."""
    return np.array(base, dtype=float)[None, None, :] * (1 - amount + 2 * amount * variation[:, :, None])


def grass(rng):
    v = fbm(rng)
    img = paint((0.36, 0.62, 0.22), v)
    img[rng.random((N, N)) < 0.10] = (0.26, 0.48, 0.15)
    img[rng.random((N, N)) < 0.05] = (0.50, 0.76, 0.32)
    return img


def dirt(rng):
    img = paint((0.47, 0.33, 0.21), fbm(rng), 0.22)
    pebble = rng.random((N, N))
    img[pebble < 0.05] = (0.32, 0.22, 0.14)
    img[pebble > 0.96] = (0.56, 0.41, 0.27)
    return img


def stone(rng, amount=0.3):
    img = paint((0.50, 0.50, 0.52), fbm(rng, 3), amount)
    img[np.abs(fbm(rng, 2) - 0.5) < 0.025] *= 0.7        # hairline cracks
    return img


def sand(rng):
    grain = rng.random((N, N))
    return paint((0.90, 0.84, 0.60), 0.6 * grain + 0.4 * fbm(rng, 8), 0.12)


def water(rng):
    """Water: a blue surface with soft, uneven shading and faint streaks. No regular pattern, so one block does not show a grid."""
    img = paint((0.18, 0.42, 0.82), fbm(rng, 6), 0.10)
    streaks = noise(rng, 2, 8)
    img = img * (0.94 + 0.12 * streaks)[:, :, None]
    img[fbm(rng, 4) > 0.82] *= 1.12
    return np.clip(img, 0, 1)


def snow(rng):
    v = fbm(rng, 6)
    img = paint((0.95, 0.97, 1.00), v, 0.04)
    img[v > 0.72] = (0.80, 0.86, 0.95)
    return img


def soft_cap(rng, cap, below, base_row, spread, fade):
    """A cap over the dirt that fades out instead of ending on a line: each column's edge is ragged, and the cap goes from its
    own colour to the dirt over `fade` rows, with dithered noise, so the boundary reads as soil rather than a stripe."""
    edge = base_row + fbm(rng, 8) * spread
    rows = np.arange(N)[:, None].astype(float)
    jitter = rng.random((N, N)) - 0.5
    keep = np.clip((edge - rows + jitter * fade * 0.5) / fade, 0.0, 1.0)[:, :, None]
    return keep * cap + (1.0 - keep) * below


def snow_over_dirt(rng):
    """The side of a snowy block: snow that fades into the dirt below it."""
    return soft_cap(rng, snow(rng), dirt(rng), 2.0, 1.0, 2.0)


def grass_over_dirt(rng):
    """The side of a grass block: dirt, with only a thin green edge at its very top. The top tile shows the grass, so a
    fringe down every side would stripe a cliff at each block row."""
    return soft_cap(rng, grass(rng), dirt(rng), 1.5, 1.0, 2.0)


def bark(rng):
    streaks = fbm(rng, 2)
    img = paint((0.38, 0.25, 0.13), streaks, 0.3)
    img[np.abs(noise(rng, 6, 2) - 0.5) < 0.1] = (0.22, 0.14, 0.07)   # grooves
    img[noise(rng, 12, 3) > 0.85] = (0.50, 0.36, 0.22)               # lighter ridges
    return img


def rings(rng):
    yy, xx = np.mgrid[0:N, 0:N]
    dx = np.minimum(np.abs(xx - 16), N - np.abs(xx - 16))
    dy = np.minimum(np.abs(yy - 16), N - np.abs(yy - 16))
    r = np.sqrt(dx * dx + dy * dy) / N
    band = 0.5 + 0.5 * np.sin(2 * np.pi * (r * 4.0 + 0.25 * fbm(rng, 4)))
    img = paint((0.55, 0.40, 0.22), band, 0.22)
    img[band < 0.15] *= 0.7
    return img


def leaves(rng):
    """A canopy: clusters of leaves, each with a light edge and dark gaps between them."""
    v = fbm(rng, 6)
    img = paint((0.20, 0.46, 0.15), v, 0.35)
    img[v > 0.66] = (0.12, 0.32, 0.09)
    img[v > 0.80] = (0.05, 0.16, 0.04)
    img[(v > 0.56) & (v < 0.62)] = (0.34, 0.62, 0.24)
    img[rng.random((N, N)) < 0.04] = (0.44, 0.70, 0.30)
    return img


def gravel(rng):
    """Pebbles: each cell of a wrapping Voronoi pattern is one stone, with a darker rim."""
    points = rng.random((16, 2)) * N
    yy, xx = np.mgrid[0:N, 0:N]
    best = np.full((N, N), np.inf)
    owner = np.zeros((N, N), dtype=int)
    for i, (px, py) in enumerate(points):
        dx = np.minimum(np.abs(xx - px), N - np.abs(xx - px))
        dy = np.minimum(np.abs(yy - py), N - np.abs(yy - py))
        d = dx * dx + dy * dy
        closer = d < best
        best[closer] = d[closer]
        owner[closer] = i
    shade = 0.42 + 0.3 * rng.random(16)
    tint = rng.random(16) * 0.08
    img = np.stack([shade[owner] + tint[owner], shade[owner], shade[owner] - tint[owner] * 0.5], axis=-1)
    img[best < 2.5] *= 0.6
    return np.clip(img, 0, 1)


def lamp(rng):
    img = np.zeros((N, N, 3))
    img[:, :] = (0.25, 0.18, 0.10)                                  # the frame
    inner = slice(3, N - 3)
    img[inner, inner] = paint((1.0, 0.85, 0.50), fbm(rng, 4), 0.1)[inner, inner]
    bars = np.zeros((N, N), dtype=bool)
    bars[::8, :] = True
    bars[:, ::8] = True
    img[bars] = (0.62, 0.40, 0.20)
    return img


def brick(rng):
    img = np.zeros((N, N, 3))
    mortar = (0.74, 0.70, 0.64)
    for row in range(4):
        top = row * 8
        offset = 8 if row % 2 else 0
        for start in (-16, 0, 16, 32):
            x0 = start + offset
            left, right = max(0, x0), min(N, x0 + 16)
            if right <= left:
                continue
            tint = 0.85 + 0.25 * rng.random()
            v = noise(rng, 4, 2)[:8, left:right]
            img[top:top + 8, left:right] = np.array((0.62, 0.28, 0.20)) * tint * (0.9 + 0.2 * v)[:, :, None]
            # the joints: a mortar line along the brick's right edge (its top and bottom are the rows' mortar)
            if right - 1 >= left:
                img[top:top + 8, right - 1] = mortar
        img[top, :] = mortar
    img[:, 0] = mortar
    return np.clip(img, 0, 1)


def ore(rng, colour, dark, threshold):
    """Stone with clusters of an ore: the clusters are the ore's colour, with dark flecks."""
    img = stone(rng, 0.25)
    cluster = fbm(rng, 3) > threshold
    img[cluster] = colour
    img[cluster & (rng.random((N, N)) < 0.25)] = dark
    return img


def coal(rng):
    return ore(rng, (0.07, 0.07, 0.08), (0.02, 0.02, 0.03), 0.56)


def iron(rng):
    return ore(rng, (0.80, 0.54, 0.36), (0.55, 0.32, 0.20), 0.66)


def lava(rng):
    v = fbm(rng, 4)
    img = paint((0.95, 0.36, 0.06), v, 0.15)
    img[v > 0.66] = (1.00, 0.70, 0.20)
    img[v < 0.34] = (0.42, 0.11, 0.04)
    return img


def main():
    rng = np.random.default_rng(11)
    tiles = [
        grass(rng),
        grass_over_dirt(rng),
        dirt(rng),
        stone(rng),
        sand(rng),
        water(rng),
        snow(rng),
        snow_over_dirt(rng),
        bark(rng),
        rings(rng),
        leaves(rng),
        gravel(rng),
        lamp(rng),
        brick(rng),
        coal(rng),
        iron(rng),
        lava(rng),
    ]
    assert len(tiles) == len(TILE_NAMES)
    strip = np.clip(np.concatenate(tiles, axis=0), 0, 1)
    Image.fromarray((strip * 255).round().astype(np.uint8), "RGB").save(OUT, optimize=True)
    print("wrote %s: %d tiles of %d by %d" % (os.path.normpath(OUT), len(tiles), N, N))


main()
