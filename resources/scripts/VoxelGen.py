"""
Terrain for the voxel sample (docs/games/voxels.md). Pure Python with no engine imports, so tools/test_voxel_gen.py
runs it on its own. VoxelGame places the blocks it returns into the engine's cge.VoxelWorld.

Blocks are small integers. Air is 0 and is never stored. Each column is a stack of runs: (y_from, y_to, block).
"""
import math

CHUNK = 16          # blocks along x and z in a chunk
HEIGHT = 64         # blocks up in the world
SEA_LEVEL = 14      # water fills columns below this

AIR, GRASS, DIRT, STONE, SAND, WATER, SNOW = range(7)

# Tints for the engine (0 to 1 per channel). The base texture is a light grey noise, so these set the colours.
COLOURS = {
    GRASS: (0.36, 0.72, 0.26),
    DIRT: (0.52, 0.36, 0.22),
    STONE: (0.50, 0.50, 0.53),
    SAND: (0.92, 0.86, 0.58),
    WATER: (0.22, 0.45, 0.92),
    SNOW: (0.96, 0.98, 1.00),
}


def _hash(ix, iz, seed):
    """A repeatable number in [0, 1) for a lattice point."""
    h = (ix * 374761393 + iz * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, z, seed):
    """Smooth noise in [0, 1]: random values on a lattice, blended with smoothstep."""
    x0 = math.floor(x)
    z0 = math.floor(z)
    tx = _smooth(x - x0)
    tz = _smooth(z - z0)
    a = _hash(x0, z0, seed)
    b = _hash(x0 + 1, z0, seed)
    c = _hash(x0, z0 + 1, seed)
    d = _hash(x0 + 1, z0 + 1, seed)
    top = a + (b - a) * tx
    bottom = c + (d - c) * tx
    return top + (bottom - top) * tz


def fractal(x, z, seed, octaves=4, scale=32.0):
    """Several octaves of value noise, each half the size and amplitude of the last. In [0, 1]."""
    total = 0.0
    norm = 0.0
    amplitude = 1.0
    frequency = 1.0 / scale
    for octave in range(octaves):
        total += amplitude * value_noise(x * frequency, z * frequency, seed + octave * 101)
        norm += amplitude
        amplitude *= 0.5
        frequency *= 2.0
    return total / norm


def column_height(x, z, seed):
    """Number of solid blocks in the column at world (x, z): from 8 to 52, mostly low land with a few peaks."""
    return int(8 + (fractal(x, z, seed) ** 1.6) * 44)


def column_runs(x, z, seed):
    """Runs (y_from, y_to, block) for the column at world (x, z), bottom to top. Water fills the low columns."""
    h = column_height(x, z, seed)
    top = GRASS
    if h <= SEA_LEVEL + 1:
        top = SAND
    elif h >= 40:
        top = SNOW
    runs = [
        (0, h - 4, STONE),
        (h - 4, h - 1, DIRT),
        (h - 1, h, top),
    ]
    if h < SEA_LEVEL:
        runs.append((h, SEA_LEVEL, WATER))
    return runs


def chunk_columns(cx, cz, seed):
    """Every column of chunk (cx, cz) as (world x, world z, runs)."""
    columns = []
    for lz in range(CHUNK):
        for lx in range(CHUNK):
            wx = cx * CHUNK + lx
            wz = cz * CHUNK + lz
            columns.append((wx, wz, column_runs(wx, wz, seed)))
    return columns
