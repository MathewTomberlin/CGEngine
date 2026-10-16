"""
Measures how far a one-unit water volume runs down a slope, inside the running voxel sample. Phase "place" finds a slope near the
player that falls at least 3 blocks over 6 cells, and puts one unit of water at its top. Phase "read" (run later, once the water has
had time to flow) finds the water units along that slope and writes how far down they reached and how many units there are to
cg_control/flow_probe.json. Run through the control channel by tools/voxel_flow_probe.ps1.
"""
import gc
import json
import math
import sys

import VoxelGen

with open("cg_control/flow_phase.txt") as f:   # written by voxel_flow_probe.ps1 before each run
    phase = f.read().strip()
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
WATER_UNITS = range(VoxelGen.WATER_VOLUME_FIRST, VoxelGen.WATER_VOLUME_FULL + 1)
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1)]


def top_of(x, z):
    return next((y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(x, y, z)), None)


def find_slope():
    px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
    for radius in range(2, 40):
        for dx in range(-radius, radius + 1):
            for dz in range(-radius, radius + 1):
                if max(abs(dx), abs(dz)) != radius:
                    continue
                x, z = px + dx, pz + dz
                h0 = top_of(x, z)
                if h0 is None or world.get_block(x, h0 + 1, z) != VoxelGen.AIR:
                    continue
                for ddx, ddz in DIRS:
                    tops = [top_of(x + ddx * k, z + ddz * k) for k in range(7)]
                    if None in tops:
                        continue
                    if tops[0] - tops[6] >= 3 and all(tops[k] >= tops[k + 1] for k in range(6)):
                        return (x, h0, z), (ddx, ddz), tops
    return None, None, None


if phase == "bucket":
    # A bucket on water: four units are not enough to fill it (nothing changes); eight units give a full bucket and the water goes.
    spot, direction, tops = find_slope()
    result = {"ok": spot is not None}
    if spot is not None:
        x, y, z = spot
        c = (x, y + 1, z)

        def units_near():
            total = 0
            for cx in range(c[0] - 3, c[0] + 4):
                for cz in range(c[2] - 3, c[2] + 4):
                    for cy in range(c[1] - 2, c[1] + 3):
                        b = world.get_block(cx, cy, cz)
                        if b in WATER_UNITS:
                            total += b - VoxelGen.WATER_VOLUME_FIRST + 1
            return total

        slot_before4 = game.inventory.slots[game.slot]
        world.set_block(c[0], c[1], c[2], VoxelGen.WATER_VOLUME_FIRST + 3)   # 4 units
        before4 = units_near()
        game.fill_bucket(c, VoxelGen.WATER_VOLUME_FIRST + 3)
        slot_after4 = game.inventory.slots[game.slot]
        units_after4 = units_near()
        world.set_block(c[0], c[1], c[2], VoxelGen.WATER_VOLUME_FULL)        # 8 units
        before8 = units_near()
        game.fill_bucket(c, VoxelGen.WATER_VOLUME_FULL)
        after8 = units_near()
        result.update({"slot_before_4": slot_before4, "units_before_4": before4, "units_after_4": units_after4, "slot_after_4": slot_after4, "units_before_8": before8,
                       "units_after_8": after8, "slot_after_8": game.inventory.slots[game.slot],
                       "cell_after_8": world.get_block(c[0], c[1], c[2])})
    with open("cg_control/flow_probe.json", "w") as f:
        json.dump(result, f, indent=1)
    print("BUCKET PROBE", json.dumps(result)[:300])
elif phase.startswith("place"):
    spot, direction, tops = find_slope()
    result = {"ok": spot is not None}
    if spot is not None:
        x, y, z = spot
        full = phase == "place8"
        world.set_block(x, y + 1, z, VoxelGen.WATER_VOLUME_FULL if full else VoxelGen.WATER_VOLUME_FIRST)
        result.update({"spot": list(spot), "direction": list(direction), "tops": tops, "placed_units": 8 if full else 1})
    with open("cg_control/flow_probe.json", "w") as f:
        json.dump(result, f, indent=1)
    print("FLOW PROBE placed", json.dumps(result)[:300])
else:
    with open("cg_control/flow_probe.json") as f:
        placed = json.load(f)
    x, y, z = placed["spot"]
    ddx, ddz = placed["direction"]
    units = 0
    furthest = 0
    cells = []
    for cx in range(x - 8, x + 9):
        for cz in range(z - 8, z + 9):
            for cy in range(y - 12, y + 4):
                block = world.get_block(cx, cy, cz)
                if block in WATER_UNITS:
                    units += block - VoxelGen.WATER_VOLUME_FIRST + 1
                    furthest = max(furthest, (cx - x) * ddx + (cz - z) * ddz)
                    cells.append([cx, cy, cz, block - VoxelGen.WATER_VOLUME_FIRST + 1])
    # The terrain's tops around the spot, so a stop can be read against the ground: rows are z, columns are x, from the spot.
    grid = []
    for cz in range(z - 6, z + 7):
        row = []
        for cx in range(x - 6, x + 7):
            top = top_of(cx, cz)
            row.append(-1 if top is None else top)
        grid.append(row)
    result = {"placed_units": placed.get("placed_units", 1), "units": units, "furthest_downhill": furthest, "cells": cells[:40], "tops": placed["tops"],
              "grid_tops": grid}
    with open("cg_control/flow_probe_result.json", "w") as f:
        json.dump(result, f, indent=1)
    print("FLOW PROBE read", json.dumps({k: v for k, v in result.items() if k != "cells"})[:300])
