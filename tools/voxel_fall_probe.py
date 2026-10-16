"""
Checks that a detached piece falls and lands (roadmap 39.1), in the running voxel sample. Phase "make" builds a pillar of stone up
from the ground at a column near the spawn, with a 2 by 2 slab on its top, and breaks the pillar's top block under the slab: the slab
is then a piece that no longer reaches the ground. Phase "read" (run later, after the piece has had time to fall) reads the slab's
blocks: it should have fallen one cell and landed on the pillar. Writes cg_control/fall_probe.json. Run by tools/voxel_fall_probe.ps1.
"""
import gc
import json
import math

import VoxelGen

with open("cg_control/fall_phase.txt") as f:
    phase = f.read().strip()
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
world = game.world
px, pz = int(math.floor(game.pos[0])), int(math.floor(game.pos[2]))
path = "cg_control/fall_probe.json"

if phase == "make":
    # A column whose ground is low enough to leave room for the pillar and the slab below the sky limit.
    column = None
    for dx in range(4, 20):
        top = next((y for y in range(VoxelGen.HEIGHT - 2, 0, -1) if world.is_solid(px + dx, y, pz)), None)
        if top is not None and top <= 46:
            column = (px + dx, top, pz)
            break
    if column is None:
        raise SystemExit("no low column near the spawn")
    sx, top, sz = column
    for y in range(top + 1, 55):
        world.set_block(sx, y, sz, VoxelGen.STONE)
    for x in range(sx - 1, sx + 1):
        for z in range(sz - 1, sz + 1):
            for y in (55, 56):
                world.set_block(x, y, z, VoxelGen.STONE)
    before = {"pillar_top": world.get_block(sx, 54, sz), "slab_low": world.get_block(sx, 55, sz)}
    world.set_block(sx, 54, sz, VoxelGen.AIR)      # the break: the slab is no longer held
    with open(path, "w") as f:
        json.dump({"column": [sx, top, sz], "before": before, "pieces_after_break": world.falling_pieces(),
                   "cells_after_break": world.falling_cells()}, f)
elif phase == "read":
    with open(path) as f:
        made = json.load(f)
    sx, top, sz = made["column"]
    result = dict(made)
    result["after"] = {"y54": world.get_block(sx, 54, sz), "y55": world.get_block(sx, 55, sz), "y56": world.get_block(sx, 56, sz),
                       "pieces": world.falling_pieces(), "cells": world.falling_cells()}
    with open(path, "w") as f:
        json.dump(result, f)
