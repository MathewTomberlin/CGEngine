"""
Checks the voxel inventory (resources/scripts/VoxelInventory.py) without the engine: stacks fill and split, the cursor on the
inventory screen puts stacks down and takes them up, and a saved inventory loads back, damaged entries included.
Usage (from the repo root): python -I tools/test_voxel_inventory.py
Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))
import VoxelGen  # noqa: E402
import VoxelInventory as inv  # noqa: E402

failures = []


def check(name, ok):
    print(("PASS  " if ok else "FAIL  ") + name)
    if not ok:
        failures.append(name)


STONE, TORCH, DIRT = 3, 19, 1

# Adding fills the stacks of an item first, then empty slots, and reports what does not fit.
i = inv.Inventory()
check("an empty inventory holds nothing", i.count(STONE) == 0 and i.fits(STONE, inv.SLOTS * inv.STACK_LIMIT))
left = i.add(STONE, 10)
check("adding into an empty inventory uses the first slot", left == 0 and i.slots[0] == [STONE, 10])
i.add(STONE, inv.STACK_LIMIT)
check("a full stack spills into the next slot", i.slots[0] == [STONE, inv.STACK_LIMIT] and i.slots[1] == [STONE, 10])
check("the count adds up across stacks", i.count(STONE) == inv.STACK_LIMIT + 10)
check("an added amount that does not fit is returned", i.add(STONE, inv.STACK_LIMIT * inv.SLOTS) > 0)

# Taking from a slot removes a count; a slot that empties becomes None.
i = inv.Inventory()
i.add(TORCH, 3)
check("taking one leaves two", i.take(0, 1) == 1 and i.slots[0] == [TORCH, 2])
check("taking more than the stack takes what is there", i.take(0, 5) == 2 and i.slots[0] is None)
i.add(TORCH, 4)
i.add(DIRT, 2)
check("taking by item takes from the first stacks that hold it", i.take_item(TORCH, 3) == 3 and i.count(TORCH) == 1)

# The cursor: left takes a whole stack and puts it back, right takes half and puts one at a time.
i = inv.Inventory()
i.slots[2] = [STONE, 9]
i.click(2)
check("left click takes the whole stack into the cursor", i.held == [STONE, 9] and i.slots[2] is None)
i.click(5)
check("left click puts the stack into an empty slot", i.slots[5] == [STONE, 9] and i.held is None)
i.click(5, secondary=True)
check("right click takes half of a stack, rounded up", i.held == [STONE, 5] and i.slots[5] == [STONE, 4])
i.click(6, secondary=True)
check("right click puts one into an empty slot", i.slots[6] == [STONE, 1] and i.held == [STONE, 4])
i.click(6)
check("left click tops up the same item and empties the cursor", i.slots[6] == [STONE, 5] and i.held is None)

i = inv.Inventory()
i.slots[7] = [DIRT, 3]
i.held = [STONE, 8]
i.click(7)
check("left click on a different item swaps the stacks", i.slots[7] == [STONE, 8] and i.held == [DIRT, 3])
i.click(7)
check("a second swap returns the first stack to the cursor", i.slots[7] == [DIRT, 3] and i.held == [STONE, 8])

i = inv.Inventory()
i.slots[0] = [STONE, inv.STACK_LIMIT]
i.held = [STONE, 1]
i.click(0, secondary=True)
check("one more cannot go on a full stack", i.slots[0] == [STONE, inv.STACK_LIMIT] and i.held == [STONE, 1])
i.slots[0] = [STONE, inv.STACK_LIMIT - 2]
i.held = [STONE, 5]
i.click(0)
check("a left click fills a stack to the limit and holds the rest", i.slots[0] == [STONE, inv.STACK_LIMIT] and i.held == [STONE, 3])

# Closing the screen puts the held stack back, and nothing is lost.
i = inv.Inventory()
i.slots[4] = [TORCH, 6]
i.click(4)
i.return_held()
check("closing the screen returns the held stack to the inventory", i.slots[0] == [TORCH, 6] and i.held is None)

# A tool does not stack: each pickaxe takes a slot of its own, with its full durability.
PICKAXE, IRON_PICKAXE = VoxelGen.PICKAXE, VoxelGen.IRON_PICKAXE
PICK_USES = VoxelGen.TOOLS[PICKAXE]["durability"]
i = inv.Inventory()
i.add(PICKAXE, 2)
check("a tool does not stack: two pickaxes take two slots",
      i.slots[0] == [PICKAXE, 1, PICK_USES] and i.slots[1] == [PICKAXE, 1, PICK_USES])
full = inv.Inventory()
full.slots = [[PICKAXE, 1, PICK_USES] for _ in range(inv.SLOTS)]
check("a full row of tools has no room for another", not full.fits(PICKAXE, 1))
i.slots[0] = [PICKAXE, 1, 10]
i.held = [PICKAXE, 1, PICK_USES]
i.click(0)
check("a held tool swaps with a tool in a slot, and each keeps its uses",
      i.slots[0] == [PICKAXE, 1, PICK_USES] and i.held == [PICKAXE, 1, 10])
j = inv.Inventory()
j.load_list([[PICKAXE, 3], [PICKAXE, 1]])
check("a saved tool with more than one is dropped, one is kept", j.slots[0] is None and j.slots[1] == [PICKAXE, 1, PICK_USES])

# Tools wear: each use takes one, and a tool that runs out breaks and leaves its slot.
w = inv.Inventory()
w.add(PICKAXE, 1)
check("a new tool reports all its uses", w.durability(0) == 1.0)
check("a use takes one from the tool", w.wear(0, 3) is False and w.slots[0][2] == PICK_USES - 3)
check("the durability is the share of uses left", abs(w.durability(0) - (PICK_USES - 3) / PICK_USES) < 1e-9)
w.slots[0][2] = 1
check("the last use breaks the tool and empties its slot", w.wear(0, 1) is True and w.slots[0] is None)
w.add(VoxelGen.STONE, 2)
check("wearing an item that is not a tool does nothing", w.wear(0, 1) is False and w.slots[0] == [VoxelGen.STONE, 2])
check("an empty slot has no durability", w.durability(5) is None and w.durability(0) is None)

# The uses a tool has left are kept when it is moved, saved and loaded, and a damaged value is kept within range.
m = inv.Inventory()
m.slots[0] = [PICKAXE, 1, 7]
m.click(0)
m.click(4)
check("moving a tool keeps its uses", m.slots[4] == [PICKAXE, 1, 7] and m.slots[0] is None and m.held is None)
k = inv.Inventory()
k.add(IRON_PICKAXE, 1)
k.wear(0, 50)
l = inv.Inventory()
l.load_list(k.to_list())
check("a tool keeps its uses through a save", l.slots[0] == [IRON_PICKAXE, 1, VoxelGen.TOOLS[IRON_PICKAXE]["durability"] - 50])
l2 = inv.Inventory()
l2.load_list([[IRON_PICKAXE, 1, 9999], [PICKAXE, 1, -5]])
check("a saved tool's uses stay within its durability, and at least one",
      l2.slots[0][2] == VoxelGen.TOOLS[IRON_PICKAXE]["durability"] and l2.slots[1][2] == 1)

# Mining tiers: stone, coal and iron need no tool; obsidian needs an iron pickaxe.
check("with no tool, stone, coal and iron are mined by hand",
      all(VoxelGen.can_mine(None, b) for b in (VoxelGen.STONE, VoxelGen.COAL, VoxelGen.IRON)))
check("obsidian needs an iron pickaxe: not by hand, not with a pickaxe, but with an iron pickaxe",
      not VoxelGen.can_mine(None, VoxelGen.OBSIDIAN) and not VoxelGen.can_mine(PICKAXE, VoxelGen.OBSIDIAN)
      and VoxelGen.can_mine(IRON_PICKAXE, VoxelGen.OBSIDIAN))

# The starting kit fills the hotbar, and a save loads back, with damaged entries dropped.
i = inv.Inventory()
i.start([STONE, DIRT, TORCH], 16)
check("a starting kit puts each item in its own slot", i.slots[:3] == [[STONE, 16], [DIRT, 16], [TORCH, 16]])
saved = i.to_list()
j = inv.Inventory()
j.load_list(saved + [None] * 3)
check("a saved inventory loads back the same", j.to_list()[:3] == saved[:3] and j.count(TORCH) == 16)
j.load_list([[STONE, 5], "bad", [DIRT, 999], [0, 3], None, [TORCH, 2]])
check("a damaged save keeps the valid entries only",
      j.slots[0] == [STONE, 5] and j.slots[1] is None and j.slots[2] is None and j.slots[3] is None and j.slots[5] == [TORCH, 2])

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel inventory checks passed.")
