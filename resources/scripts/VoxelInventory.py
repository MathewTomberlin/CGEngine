"""
The player's inventory in the voxel sample: a grid of slots, each holding one kind of item and a count. An item is a block id
(VoxelGen), so breaking a block gives its item and placing takes one back. The first row is the hotbar: the keys 1 to 0, - and =
choose its slots. The inventory screen (I) holds a stack in the cursor while it moves one.

Pure Python with no engine calls, so tools/test_voxel_inventory.py checks it without a window.
"""

COLUMNS = 12                # slots in a row; the hotbar is the first row
ROWS = 3
SLOTS = COLUMNS * ROWS
HOTBAR_SLOTS = COLUMNS
STACK_LIMIT = 64            # the most of one item in one slot


class Inventory:
    def __init__(self):
        self.slots = [None] * SLOTS     # each slot is None (empty) or [item, count]
        self.held = None                # the stack the cursor holds on the inventory screen, [item, count] or None

    def item(self, index):
        """The item in a slot, or None when it is empty."""
        return self.slots[index][0] if self.slots[index] else None

    def count(self, item):
        """How many of an item the inventory holds, in all its slots."""
        return sum(s[1] for s in self.slots if s and s[0] == item)

    def fits(self, item, amount=1):
        """True when amount of item can be added: into stacks of it that are not full, or into empty slots."""
        room = sum(STACK_LIMIT - s[1] for s in self.slots if s and s[0] == item)
        room += STACK_LIMIT * sum(1 for s in self.slots if s is None)
        return room >= amount

    def add(self, item, amount=1):
        """Add amount of item: topping up the stacks of it first, then filling empty slots. Returns what did not fit."""
        left = amount
        for s in self.slots:
            if left > 0 and s and s[0] == item and s[1] < STACK_LIMIT:
                moved = min(left, STACK_LIMIT - s[1])
                s[1] += moved
                left -= moved
        for index, s in enumerate(self.slots):
            if left > 0 and s is None:
                moved = min(left, STACK_LIMIT)
                self.slots[index] = [item, moved]
                left -= moved
        return left

    def take(self, index, amount=1):
        """Remove up to amount from a slot. Returns how many were removed; a slot that empties becomes None."""
        s = self.slots[index]
        if s is None:
            return 0
        taken = min(amount, s[1])
        s[1] -= taken
        if s[1] == 0:
            self.slots[index] = None
        return taken

    def take_item(self, item, amount=1):
        """Remove up to amount of an item from the slots that hold it, first slot first. Returns how many were removed."""
        removed = 0
        for index, s in enumerate(self.slots):
            if removed < amount and s and s[0] == item:
                removed += self.take(index, amount - removed)
        return removed

    def click(self, index, secondary=False):
        """A click on a slot, as the inventory screen uses it. With the cursor empty, the left button takes the whole stack
        and the right takes half of it (rounded up). With a stack held, the left puts it down (into an empty slot, onto the
        same item up to the limit, or swapping with a different item) and the right puts one item down."""
        s = self.slots[index]
        if self.held is None:
            if s is None:
                return
            if secondary:
                half = (s[1] + 1) // 2
                self.held = [s[0], half]
                self.take(index, half)
            else:
                self.held = list(s)
                self.slots[index] = None
            return
        item, count = self.held
        if secondary:
            if s is None:
                self.slots[index] = [item, 1]
            elif s[0] == item and s[1] < STACK_LIMIT:
                s[1] += 1
            else:
                return
            self.held[1] -= 1
        elif s is None:
            self.slots[index] = self.held
            self.held = None
            return
        elif s[0] == item:
            moved = min(count, STACK_LIMIT - s[1])
            s[1] += moved
            self.held[1] -= moved
        else:
            self.slots[index], self.held = self.held, list(s)
            return
        if self.held is not None and self.held[1] == 0:
            self.held = None

    def return_held(self):
        """Put the stack the cursor holds back into the inventory (when the screen closes, or before a save)."""
        if self.held is not None:
            self.add(self.held[0], self.held[1])
            self.held = None

    def start(self, items, amount):
        """A starting kit: each item in its own slot, from the first slot, amount of each."""
        for index, item in enumerate(items[:SLOTS]):
            self.slots[index] = [item, amount]

    def to_list(self):
        """The slots as a list, for the save file: None for an empty slot, else [item, count]."""
        return [None if s is None else [s[0], s[1]] for s in self.slots]

    def load_list(self, data):
        """Replace the slots with a saved list (see to_list). Entries that are not a valid stack become empty, so a damaged
        file loses only those entries."""
        self.slots = [None] * SLOTS
        self.held = None
        for index, entry in enumerate(data[:SLOTS]):
            try:
                item, count = int(entry[0]), int(entry[1])
            except (TypeError, ValueError, IndexError, KeyError):
                continue
            if 0 < item < 256 and 0 < count <= STACK_LIMIT:
                self.slots[index] = [item, count]
