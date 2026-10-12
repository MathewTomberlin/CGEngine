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
