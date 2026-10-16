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


if phase in ("spring_make", "endless_make", "spring_kill", "spring_read"):
    # A water spring on a slope: make it, let it run, destroy it, and count what is left (flows and volume units) around it.
    if phase in ("spring_make", "endless_make"):
        spot, direction, tops = find_slope()
        if spot is not None:
            spring = VoxelGen.WATER if phase == "spring_make" else VoxelGen.ENDLESS_WATER
            world.set_block(spot[0], spot[1] + 1, spot[2], spring)
        with open("cg_control/flow_probe.json", "w") as f:
            json.dump({"ok": spot is not None, "spot": list(spot) if spot else None}, f, indent=1)
    else:
        with open("cg_control/flow_probe.json") as f:
            spot = tuple(json.load(f)["spot"])
        c = (spot[0], spot[1] + 1, spot[2])
        if phase == "spring_kill":
            world.set_block(c[0], c[1], c[2], VoxelGen.AIR)
        else:
            flows = 0
            volume = 0
            flow_cells = []
            for cx in range(c[0] - 8, c[0] + 9):
                for cz in range(c[2] - 8, c[2] + 9):
                    for cy in range(c[1] - 30, c[1] + 3):
                        b = world.get_block(cx, cy, cz)
                        if VoxelGen.FLOW_FIRST <= b <= VoxelGen.FLOW_LAST:
                            flows += 1
                            if len(flow_cells) < 40:
                                flow_cells.append([cx, cy, cz, b, world.get_block(cx, cy + 1, cz), world.get_block(cx, cy - 1, cz)])
                        elif b in WATER_UNITS:
                            volume += b - VoxelGen.WATER_VOLUME_FIRST + 1
            with open("cg_control/flow_probe_result.json", "w") as f:
                json.dump({"flows": flows, "volume_units": volume, "spring": world.get_block(c[0], c[1], c[2]), "flow_cells": flow_cells, "active": world.active_count()}, f, indent=1)
            print("SPRING READ", flows, volume)
elif phase == "bucket":
    # A bucket takes what a volume gives, up to its capacity, and knows its units; it tops up with more water; it pours back its
    # units and is refused when they do not fit.
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

        def slot():
            return game.inventory.slots[game.slot]

        game.inventory.slots[game.slot] = [VoxelGen.BUCKET, 1]
        world.set_block(c[0], c[1], c[2], VoxelGen.WATER_VOLUME_FIRST + 3)   # 4 units
        game.fill_bucket(c, VoxelGen.WATER_VOLUME_FIRST + 3)
        result["four_taken"] = {"slot": slot(), "units_near_after": units_near()}
        world.set_block(c[0], c[1], c[2], VoxelGen.WATER_VOLUME_FULL)        # 8 units: tops the bucket up to full
        game.fill_bucket(c, VoxelGen.WATER_VOLUME_FULL)
        result["top_up"] = {"slot": slot(), "units_near_after": units_near()}
        game.fill_bucket(c, VoxelGen.WATER_VOLUME_FULL)   # full already: refused, nothing changes
        result["full_refused"] = {"slot": slot(), "units_near_after": units_near()}
        # The placement: aimed at the solid block under c, the bucket's 8 units go into c and the cells round it.
        for cx in range(c[0] - 3, c[0] + 4):   # clear the water left near the cell, so the cell is free to place into
            for cz in range(c[2] - 3, c[2] + 4):
                for cy in range(c[1] - 2, c[1] + 3):
                    if world.get_block(cx, cy, cz) in WATER_UNITS:
                        world.set_block(cx, cy, cz, VoxelGen.AIR)
        game.target = ((x, y, z), (0, 1, 0))
        placement, reason = game.placement_check()
        result["placement"] = {"ok": placement is not None, "reason": reason, "cell": placement[0] if placement else None}
        # Pour the bucket's units into the air and check they all went in.
        placed, changed = world.pour_volume(c[0], c[1], c[2], slot()[2], VoxelGen.WATER_VOLUME_FIRST, False)
        result["pour_check"] = {"units": slot()[2], "would_place": placed, "cells_changed": len(changed)}
        result["selected_name"] = game.selected_name()
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
