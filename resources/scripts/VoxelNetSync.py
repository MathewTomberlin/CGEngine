"""
Two processes sharing their edits: the messages, the cells sent and applied, and the control command.
A mixin: VoxelGame inherits it, so the methods read and write the game's own state, as they did in one file.
"""
import json
import math
import os
import random
import time
import traceback
import cg_engine_bindings as cge
import StateFile
import VoxelConsole
import VoxelDrops
import VoxelGen
import VoxelInventory
import VoxelNet
import VoxelParticles
import VoxelRecipes
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays, mix, outline_arrays
import VoxelWalkers
import StateFile
from VoxelConfig import *


class NetMixin:
    @property
    def net_host(self):
        """The listening game: it owns the world. It applies the edits clients ask for and sends each changed cell back."""
        return self.net is not None and self.net.mode == "listen"

    @property
    def net_client(self):
        """The connecting game: it asks the host for its edits and draws the cells the host sends."""
        return self.net is not None and self.net.mode == "connect"

    def send_request(self, request):
        """A client's edit, which the host applies with its own rules. The host's own edits need no request."""
        if self.net_client:
            self.net.send(dict(request, t="req"))

    def update_net(self, dt):
        """Read what arrived and apply it, send the cells the host changed, retry the cells for chunks not loaded yet, and send
        the player's position."""
        if self.net is None:
            return
        connected = self.net.connected
        for message in self.net.poll():
            self.apply_remote(message)
        if self.net_host:
            if self.net.connected and not connected:
                self.net_snapshot = True   # a client has just joined: it is sent every edited cell
            self.send_cells()
        self.apply_pending()
        self.net_pos_timer -= dt
        if self.net_pos_timer <= 0.0 and not self.paused:
            self.net_pos_timer = POS_INTERVAL
            self.net.send({"t": "pos", "p": [round(v, 3) for v in self.pos], "yaw": round(self.yaw, 2)})

    def cell_state(self, cell):
        """A cell as the host sends it: x, y, z, its block, its damage stage and its torch (-1 for none)."""
        return [cell[0], cell[1], cell[2], self.world.get_block(*cell), self.world.damage_at(*cell), self.world.prop_at(*cell)]

    def loaded_at(self, cell):
        return self.world.is_loaded(math.floor(cell[0] / VoxelGen.CHUNK), math.floor(cell[2] / VoxelGen.CHUNK))

    def send_cells(self):
        """The host sends the state of each cell that changed this frame, or that a request touched, in one message. A cell in a
        chunk the host has not loaded is not sent: the host cannot tell what is there."""
        changed = {tuple(c) for c in self.world.take_changes()} | self.net_touched
        self.net_touched = set()
        if self.net_snapshot:
            changed |= {tuple(c) for c in self.world.edited_cells()}
            self.net_snapshot = False
        if not self.net.connected:
            return
        cells = [self.cell_state(cell) for cell in sorted(changed) if self.loaded_at(cell)]
        if cells:
            self.net.send({"t": "cells", "cells": cells})

    def apply_remote(self, message):
        kind = message.get("t")
        if kind == "pos":
            self.remote_pos = message
            self.show_remote(message["p"])
        elif kind == "req" and self.net_host:
            self.apply_request(message)
        elif kind == "cells" and self.net_client:
            for entry in message["cells"]:
                self.set_remote(tuple(entry[:3]), tuple(entry[3:]))

    def apply_request(self, request):
        """A client's edit, under the host's rules: a hit takes its swings (two for a pickaxe at the blocks it mines), a block
        goes only into air, and a torch leans on a solid block. A request for a cell the host has not loaded is ignored. The cell
        is sent back either way, so an edit the host refused is undone on the client."""
        try:
            op = request["op"]
            cell = tuple(int(v) for v in request["c"])
            item = int(request.get("id", -1))
            support = tuple(int(v) for v in request.get("s", (0, 0, 0)))
            direction = int(request.get("dir", -1))
            swings = int(request.get("swings", 1))
        except (KeyError, TypeError, ValueError):
            return
        if not self.loaded_at(cell):
            return
        if self.net_host:
            self.net_touched.add(cell)
        if op == "hit":
            kind = self.world.get_block(*cell)
            if kind == VoxelGen.AIR:
                return
            swings = 2 if swings >= 2 and kind in VoxelGen.PICKAXE_BLOCKS else 1
            stage = -2
            for _ in range(swings):
                stage = self.world.hit_block(*cell)
                if stage < 0:
                    break
            if stage == -1:
                self.hit_times.pop(cell, None)
                for torch in self.torches_on(cell):
                    self.world.remove_prop(*torch)   # the torches leaning on it break with it, and the client gains them
            elif stage > 0:
                self.hit_times[cell] = time.time()
        elif op == "place":
            if (item in VoxelGen.COLOURS and item != VoxelGen.TORCH and self.world.get_block(*cell) == VoxelGen.AIR
                    and not physics.cell_in_player(cell, self.pos)):
                self.world.set_block(cell[0], cell[1], cell[2], item)
        elif op == "prop":
            if (self.world.get_block(*cell) == VoxelGen.AIR and self.world.is_solid(*support)
                    and DIRECTION_INDEX.get(tuple(support[i] - cell[i] for i in range(3))) == direction):
                self.world.set_prop(cell[0], cell[1], cell[2], direction)
        elif op == "unprop":
            self.world.remove_prop(*cell)

    def control_edit(self, request):
        """The voxel_edit control command: an edit made as a player's click makes it, under the same rules (apply_request). A
        client sends it to the host, which answers with the cell, so the result is the cell as the client has it now; the host,
        or a game alone, applies it at once. An edit in a chunk that is not loaded is an error, since the cell is not known."""
        if request.get("op") not in ("hit", "place", "prop", "unprop"):
            raise ValueError("op must be hit, place, prop or unprop")
        cell = tuple(int(v) for v in request["c"])
        if not self.loaded_at(cell):
            raise ValueError("the cell's chunk is not loaded")
        if self.net_client:
            self.send_request(request)
        else:
            self.apply_request(request)
        block, damage, prop = self.cell_state(cell)[3:]
        return {"sent": self.net_client, "block": block, "damage": damage, "prop": prop}

    def set_remote(self, cell, state):
        """Make a cell match the host's now if its chunk is loaded, else keep the state until the chunk loads (the world ignores
        edits to chunks that are not loaded)."""
        if not self.apply_cell(cell, state):
            self.net_pending[cell] = state

    def apply_cell(self, cell, state):
        """Make a cell's block, damage stage and torch match the host's. False if its chunk is not loaded here."""
        if not self.loaded_at(cell):
            return False
        block, damage, prop = state
        if self.world.get_block(*cell) != block:
            self.world.set_block(cell[0], cell[1], cell[2], block)
        if self.world.damage_at(*cell) != damage:
            self.world.set_damage(cell[0], cell[1], cell[2], damage)
        if self.world.prop_at(*cell) != prop:
            if prop < 0:
                self.world.remove_prop(*cell)
            else:
                self.world.set_prop(cell[0], cell[1], cell[2], prop)
        self.net_applied += 1
        return True

    def apply_pending(self):
        for cell, state in list(self.net_pending.items()):
            if self.apply_cell(cell, state):
                del self.net_pending[cell]

    def show_remote(self, feet):
        """The other player, as a box where their feet are: made the first time, then moved."""
        if "remote_player" not in self.bodies:
            self.bodies.add("remote_player")
            cge.create_mesh_body("remote_player", *boxes_arrays([((-0.3, 0.0, -0.3), (0.3, 1.8, 0.3), (0.9, 0.5, 0.2))]),
                                 materials=["voxel"], position=cge.Vector3f(*feet))
        else:
            cge.find_body("remote_player").get_mesh().set_position(cge.Vector3f(*feet))

    # ----- sky ----------------------------------------------------------------------------
