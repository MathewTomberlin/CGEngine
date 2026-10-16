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

    def send_request(self, request, pending=None):
        """A client's edit, which the host applies with its own rules. The host's own edits need no request. pending says what the
        client did at once for this edit (an item spent or gained, see settle_request); the host's answer (ack) confirms it or
        undoes it. An edit sent without pending changes no inventory, as a control command's does."""
        if self.net_client:
            self.net_request_id += 1
            if pending is not None:
                self.net_requests[self.net_request_id] = dict(pending, op=request["op"])
            self.net.send(dict(request, t="req", rid=self.net_request_id))

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
        starts = self.world.take_move_starts()   # the falling blocks that began this frame
        if not self.net.connected:
            return
        cells = [self.cell_state(cell) for cell in sorted(changed) if self.loaded_at(cell)]
        if cells:
            self.net.send({"t": "cells", "cells": cells})
        # After the cells, so the source is already clear when the client starts drawing the block leaving it.
        if starts:
            self.net.send({"t": "moves", "moves": [[m[0], *m[1], *m[2], m[3]] for m in starts]})

    def apply_remote(self, message):
        kind = message.get("t")
        if kind == "pos":
            self.remote_pos = message
            self.show_remote(message["p"])
        elif kind == "req" and self.net_host:
            self.apply_request(message)
        elif kind == "ack" and self.net_client:
            self.settle_request(message)
        elif kind == "cells" and self.net_client:
            for entry in message["cells"]:
                self.set_remote(tuple(entry[:3]), tuple(entry[3:]))
        elif kind == "moves" and self.net_client:
            for block, fx, fy, fz, tx, ty, tz, seconds in message["moves"]:
                self.world.show_move(fx, fy, fz, tx, ty, tz, block, seconds)
                self.net_moves_shown += 1

    def apply_request(self, request):
        """A client's edit, under the host's rules (decide_request). The host answers each request that has an id with an ack: whether
        it made the change and whether a hit broke the block, so the client counts the items once (settle_request). The cell is sent
        back as well, so an edit the host refused is undone on the client."""
        ok, broke = self.decide_request(request)
        if request.get("rid") is not None:
            self.net.send({"t": "ack", "rid": request["rid"], "ok": ok, "broke": broke})

    def decide_request(self, request):
        """Apply a client's edit under the host's rules: a hit takes its swings (the client's tool sets them, up to MAX_SWINGS), a
        block goes only into air, a torch leans on a solid block, and a scoop takes a liquid (a bucket). A request for a cell the host
        has not loaded is refused. Returns (ok, broke): whether the host made the change, and whether a hit broke the block."""
        try:
            op = request["op"]
            cell = tuple(int(v) for v in request["c"])
            item = int(request.get("id", -1))
            support = tuple(int(v) for v in request.get("s", (0, 0, 0)))
            direction = int(request.get("dir", -1))
            swings = int(request.get("swings", 1))
            want = int(request.get("want", 0))     # a scoop: the units the bucket can take
            units = int(request.get("units", 0))   # a pour: the units the bucket holds
        except (KeyError, TypeError, ValueError):
            return False, False
        if not self.loaded_at(cell):
            return False, False
        if self.net_host:
            self.net_touched.add(cell)
        if op == "hit":
            kind = self.world.get_block(*cell)
            if kind == VoxelGen.AIR:
                return False, False
            swings = max(0, min(swings, VoxelGen.MAX_SWINGS))
            stage = -2
            for _ in range(swings):
                stage = self.world.hit_block(*cell)
                if stage < 0:
                    break
            if stage == -1:
                self.hit_times.pop(cell, None)
                for torch in self.torches_on(cell):
                    self.world.remove_prop(*torch)   # the torches leaning on it break with it, and the client gains them
                return True, True
            if stage > 0:
                self.hit_times[cell] = time.time()
            return True, False
        elif op == "scoop":
            kind = self.world.get_block(*cell)
            if kind in VoxelGen.VOLUME_BLOCKS:
                # The units the client's bucket took from the volume. The host's changed cells are sent back to the client.
                _, changed = self.world.drain_volume(cell[0], cell[1], cell[2], max(0, want), True)
                if self.net_host:
                    self.net_touched.update(tuple(c) for c in changed)
                return True, False
            if kind in VoxelGen.LIQUIDS:
                self.world.set_block(cell[0], cell[1], cell[2], VoxelGen.AIR)
                return True, False
            return False, False
        elif op == "pour":
            # All the units go in, or none: a bucket that would not empty is refused, and the client puts its water back.
            if (self.world.get_block(*cell) == VoxelGen.AIR and not physics.cell_in_player(cell, self.pos)
                    and units > 0 and self.world.pour_volume(cell[0], cell[1], cell[2], units, item, False)[0] == units):
                _, changed = self.world.pour_volume(cell[0], cell[1], cell[2], units, item, True)
                if self.net_host:
                    self.net_touched.update(tuple(c) for c in changed)
                return True, False
            return False, False
        elif op == "place":
            if (item in VoxelGen.COLOURS and item != VoxelGen.TORCH and self.world.get_block(*cell) == VoxelGen.AIR
                    and not physics.cell_in_player(cell, self.pos)):
                self.world.set_block(cell[0], cell[1], cell[2], item)
                return True, False
            return False, False
        elif op == "prop":
            if (self.world.get_block(*cell) == VoxelGen.AIR and self.world.is_solid(*support)
                    and DIRECTION_INDEX.get(tuple(support[i] - cell[i] for i in range(3))) == direction):
                self.world.set_prop(cell[0], cell[1], cell[2], direction)
                return True, False
            return False, False
        elif op == "unprop":
            self.world.remove_prop(*cell)
            return True, False
        return False, False

    def settle_request(self, ack):
        """The host's answer to one of this client's requests. A placement or a torch the host refused gives its item back, since the
        client spent it at once. A hit is counted once: the client gained the block when its own strike broke it, and the host's break
        is the truth, so a difference is put right (a block the host broke that this client did not, or one this client broke that the
        host did not). The cells themselves arrive as cells messages."""
        pending = self.net_requests.pop(ack.get("rid"), None)
        if pending is None or self.creative:
            return
        if pending["op"] in ("place", "prop") and pending.get("spent") and not ack.get("ok"):
            self.inventory.add(pending["item"], 1)
            self.notify("The other player's game refused that placement")
        elif pending["op"] == "hit":
            if ack.get("broke") and not pending.get("gained"):
                self.inventory.add(pending["kind"], 1)
            elif pending.get("gained") and not ack.get("broke"):
                self.inventory.take_item(pending["kind"], 1)

    def control_edit(self, request):
        """The voxel_edit control command: an edit made as a player's click makes it, under the same rules (apply_request). A
        client sends it to the host, which answers with the cell, so the result is the cell as the client has it now; the host,
        or a game alone, applies it at once. An edit in a chunk that is not loaded is an error, since the cell is not known."""
        if request.get("op") not in ("hit", "place", "prop", "unprop", "scoop"):
            raise ValueError("op must be hit, place, prop, unprop or scoop")
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
