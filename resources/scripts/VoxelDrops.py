"""Items on the ground (roadmap 16.5): a creature that dies leaves its hide where it fell, and the player takes it by walking up to
it when the inventory has room. A drop is one small cube in the item's colour, one runtime mesh body (cge.create_mesh_body) that
bobs up and down and is rebuilt each frame; it is removed when it is taken. Drops do not stack, do not despawn, and are saved with
the inventory (F5), so they survive a restart. This module has no game state of its own: the game passes its inventory in."""
import math

import cg_engine_bindings as cge
from VoxelShapes import boxes_arrays

SIZE = 0.3                  # blocks, a side of the cube
LIFT = 0.1                  # blocks above the ground the cube hovers at, before the bob
BOB_HEIGHT = 0.12           # blocks the cube rises and falls
BOB_SPEED = 3.0             # radians a second
REACH = 1.2                 # blocks, horizontally, from the player to a drop it can take
REACH_UP = 1.8              # blocks, vertically: a drop a floor away is not taken


class Drop:
    def __init__(self, name, item, colour, feet, phase):
        self.name = name
        self.item = item
        self.colour = colour
        self.feet = tuple(feet)     # the ground under the drop, in world blocks
        self.phase = phase          # offsets the bob, so a heap of drops does not move in step
        self.age = 0.0
        cge.create_mesh_body(name, *self.arrays(), materials=["voxel"], position=cge.Vector3f(0.0, 0.0, 0.0))

    def arrays(self):
        x, y, z = self.feet
        bob = BOB_HEIGHT * (0.5 + 0.5 * math.sin(self.age * BOB_SPEED + self.phase))
        base = y + LIFT + bob
        lo = (x - SIZE / 2, base, z - SIZE / 2)
        hi = (x + SIZE / 2, base + SIZE, z + SIZE / 2)
        return boxes_arrays([(lo, hi, self.colour)])

    def update(self, dt):
        self.age += dt
        cge.update_mesh_body(self.name, *self.arrays())

    def remove(self):
        cge.remove_mesh_body(self.name)


def in_reach(feet, pos):
    """True when the player's feet at pos are close enough to a drop at feet to take it."""
    dx, dz = pos[0] - feet[0], pos[2] - feet[2]
    return dx * dx + dz * dz <= REACH * REACH and abs(pos[1] - feet[1]) <= REACH_UP


class Drops:
    def __init__(self):
        self.live = []      # the drops on the ground, in the order they were dropped
        self.made = 0       # counts every drop ever made, so each mesh body has a name no other has

    def add(self, item, colour, feet):
        """Drop one item on the ground at feet (the point under it)."""
        self.made += 1
        self.live.append(Drop("drop_%d" % self.made, item, colour, feet, phase=self.made * 1.3))

    def update(self, dt):
        """Bob the drops on. A paused game passes 0, so nothing moves and nothing is rebuilt."""
        if dt <= 0.0:
            return
        for drop in self.live:
            drop.update(dt)

    def take_in_reach(self, pos, inventory):
        """Put the drops within reach of pos into the inventory, while it has room for them. A drop that does not fit stays on the
        ground. Returns the items taken, in order."""
        taken = []
        for drop in list(self.live):
            if in_reach(drop.feet, pos) and inventory.fits(drop.item):
                inventory.add(drop.item)
                drop.remove()
                self.live.remove(drop)
                taken.append(drop.item)
        return taken

    def blocked_in_reach(self, pos, inventory):
        """The item of the first drop in reach that the inventory has no room for, or None. The drop is left where it is."""
        for drop in self.live:
            if in_reach(drop.feet, pos) and not inventory.fits(drop.item):
                return drop.item
        return None

    def clear(self):
        for drop in self.live:
            drop.remove()
        self.live = []

    def to_list(self):
        """The drops for the save file: [item, x, y, z] for each, the point under it."""
        return [[d.item, d.feet[0], d.feet[1], d.feet[2]] for d in self.live]

    def load_list(self, data, colour_of):
        """Replace the drops with a saved list (see to_list). colour_of(item) gives the cube's colour. An entry that is not a valid
        drop is skipped, so a damaged file loses only those entries."""
        self.clear()
        for entry in data:
            try:
                item, x, y, z = entry
                item = int(item)
                feet = (float(x), float(y), float(z))
            except (TypeError, ValueError):
                continue
            self.made += 1
            self.live.append(Drop("drop_%d" % self.made, item, colour_of(item), feet, phase=self.made * 1.3))
