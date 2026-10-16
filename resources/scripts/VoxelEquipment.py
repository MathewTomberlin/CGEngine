"""
The armour the player wears in the voxel sample (roadmap 39.3, 39.4): one piece to each slot, and its speed and jump bonuses. The
pieces are data (resources/voxel_equipment.json, read by VoxelGen as VoxelGen.EQUIPMENT). Pure Python with no engine calls, so
tools/test_voxel_equipment.py checks it without a window.

The effects are the sums of the worn pieces' fractions, clamped to limits: a heavy suit slows the walk but cannot stop the jump from
clearing a block (the step-up, VoxelPhysics), so the jump never falls below JUMP_LIMITS[0].
"""

SLOTS = ("head", "chest", "legs", "feet", "gloves")
SLOT_ABBREVIATIONS = {"head": "Hd", "chest": "Ch", "legs": "Lg", "feet": "Ft", "gloves": "Gl"}
SPEED_LIMITS = (-0.3, 0.5)      # the most a suit slows or speeds the walk, as a fraction of the walk speed
JUMP_LIMITS = (-0.1, 0.2)       # the least and most it changes the jump; -0.1 keeps a jump above the one-block step


def clamp(value, low, high):
    return max(low, min(high, value))


class Equipment:
    def __init__(self, table):
        """table: item id -> {"slot", "speed", "jump", "name"} (VoxelGen.EQUIPMENT)."""
        self.table = dict(table)
        self.worn = {slot: None for slot in SLOTS}

    def is_armour(self, item):
        return item in self.table

    def slot_of(self, item):
        """The slot a piece of armour goes in, or None for an item that is not armour."""
        entry = self.table.get(item)
        return entry["slot"] if entry else None

    def wear(self, item):
        """Put a piece on. Returns the piece it replaces (None for an empty slot). Raises ValueError for an item that is not armour."""
        slot = self.slot_of(item)
        if slot is None:
            raise ValueError("not armour: %r" % (item,))
        replaced = self.worn[slot]
        self.worn[slot] = item
        return replaced

    def take_off(self, slot):
        """Take a piece off a slot. Returns it, or None when the slot is empty."""
        item = self.worn.get(slot)
        self.worn[slot] = None
        return item

    def total(self, stat):
        """The sum of a stat ('speed' or 'jump') over the worn pieces, before the limits."""
        return sum(self.table[item][stat] for item in self.worn.values() if item is not None)

    def speed_factor(self):
        """The walk speed's multiplier: 1 with nothing worn."""
        return 1.0 + clamp(self.total("speed"), *SPEED_LIMITS)

    def jump_factor(self):
        """The jump's multiplier: 1 with nothing worn."""
        return 1.0 + clamp(self.total("jump"), *JUMP_LIMITS)

    def lines(self):
        """The side bar: one line a slot, short enough for the left of the screen at the default window (1000 by 500): the slot's
        abbreviation (SLOT_ABBREVIATIONS) and the piece's name, or "-" when empty."""
        out = []
        for slot in SLOTS:
            item = self.worn[slot]
            name = "-" if item is None else self.table[item]["name"]
            out.append("%s %s" % (SLOT_ABBREVIATIONS[slot], name))
        return out

    def summary(self):
        """The bar's last line: the total speed and jump bonuses of the worn pieces, after the limits."""
        return "speed %+d%%  jump %+d%%" % (round((self.speed_factor() - 1.0) * 100), round((self.jump_factor() - 1.0) * 100))

    def to_list(self):
        """The slots as a list in SLOTS order, for the save file: the item id, or None."""
        return [self.worn[slot] for slot in SLOTS]

    def load_list(self, data):
        """Replace the worn pieces with a saved list (see to_list). An entry that is not a piece of armour for its slot is dropped."""
        self.worn = {slot: None for slot in SLOTS}
        if not isinstance(data, list):
            return
        for slot, item in zip(SLOTS, data):
            if isinstance(item, int) and item in self.table and self.table[item]["slot"] == slot:
                self.worn[slot] = item
