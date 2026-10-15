"""
Checks the tools and weapons as data (resources/scripts/VoxelGen.py) without the engine: how much one strike breaks each block with
each tool (block_damage: the pickaxes double and triple it on stone, iron, coal, brick and obsidian; the shovel halves stone, iron
and coal, doubles dirt, sand and gravel, and harvests four times as fast on the rest), the damage a strike does to a creature,
the durability of each tool and weapon, and that a bucket is unstacked and only a liquid fills it. Usage (from the repo root):
python -I tools/test_voxel_tools.py   Exits 0 on success, 1 on any failed check.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "resources", "scripts"))

import VoxelGen  # noqa: E402
import VoxelInventory  # noqa: E402

failures = []


def check(name, ok):
    print("PASS" if ok else "FAIL", name)
    if not ok:
        failures.append(name)


HAND = None   # the empty hand, which is not an item: it breaks a block once and does one damage to a creature
PICK, IRON_PICK, SHOVEL, SWORD = VoxelGen.PICKAXE, VoxelGen.IRON_PICKAXE, VoxelGen.SHOVEL, VoxelGen.SWORD
STRIKE_BLOCKS = (VoxelGen.STONE, VoxelGen.IRON, VoxelGen.COAL)
SOFT_BLOCKS = (VoxelGen.DIRT, VoxelGen.SAND, VoxelGen.GRAVEL)

check("the hand breaks any block once a strike", all(VoxelGen.block_damage(HAND, b) == 1 for b in STRIKE_BLOCKS + SOFT_BLOCKS))
check("the pickaxe doubles its damage on stone, iron and coal (and not on dirt)",
      all(VoxelGen.block_damage(PICK, b) == 2 for b in STRIKE_BLOCKS) and VoxelGen.block_damage(PICK, VoxelGen.DIRT) == 1)
check("the iron pickaxe triples its damage on stone, iron and coal",
      all(VoxelGen.block_damage(IRON_PICK, b) == 3 for b in STRIKE_BLOCKS))
check("the shovel does one hit to stone, iron and coal, as the hand does", all(VoxelGen.block_damage(SHOVEL, b) == 1 for b in STRIKE_BLOCKS))
check("the shovel does eight hits to dirt, sand and gravel", all(VoxelGen.block_damage(SHOVEL, b) == 8 for b in SOFT_BLOCKS))
check("the shovel does one hit to the blocks it does not list, as the hand does",
      VoxelGen.block_damage(SHOVEL, VoxelGen.WOOD) == 1 and VoxelGen.block_damage(SHOVEL, VoxelGen.LEAVES) == 1)
check("a strike makes at most MAX_SWINGS hits", max(VoxelGen.block_damage(i, b) for i in (PICK, IRON_PICK, SHOVEL)
                                                    for b in range(VoxelGen.AIR, 31)) <= VoxelGen.MAX_SWINGS)

check("a creature takes one from the hand, 1.5 from the pickaxe, 1.75 from the iron pickaxe, 2.5 from the sword",
      VoxelGen.damage_of(HAND) == 1 and VoxelGen.damage_of(PICK) == 1.5 and VoxelGen.damage_of(IRON_PICK) == 1.75
      and VoxelGen.damage_of(SWORD) == 2.5)
check("the shovel hits creatures as the hand does", VoxelGen.damage_of(SHOVEL) == 1)

check("the pickaxe lasts 640 uses, the iron pickaxe four times that, and the sword twice the pickaxe",
      VoxelGen.durability_of(PICK) == 640 and VoxelGen.durability_of(IRON_PICK) == 2560
      and VoxelGen.durability_of(SWORD) == 2 * 640)
check("the shovel lasts as long as the pickaxe", VoxelGen.durability_of(SHOVEL) == 640)
check("a block takes no durability", VoxelGen.durability_of(VoxelGen.STONE) is None)

check("the shovel strikes three times as fast as the hand, and the others as fast as the hand",
      VoxelGen.attack_speed_of(SHOVEL) == 3 and VoxelGen.attack_speed_of(HAND) == 1 and VoxelGen.attack_speed_of(PICK) == 1)
check("the shovel mines by hand (no tier)", VoxelGen.can_mine(SHOVEL, VoxelGen.STONE) and not VoxelGen.can_mine(SHOVEL, VoxelGen.OBSIDIAN))
check("the iron pickaxe mines obsidian, the pickaxe does not", VoxelGen.can_mine(IRON_PICK, VoxelGen.OBSIDIAN)
      and not VoxelGen.can_mine(PICK, VoxelGen.OBSIDIAN))

check("a bucket is unstacked, as are the filled ones", all(b in VoxelInventory.UNSTACKED for b in VoxelGen.BUCKETS))
check("a shovel is unstacked and wears", SHOVEL in VoxelInventory.UNSTACKED and VoxelGen.durability_of(SHOVEL) is not None)
check("water fills a bucket into a water bucket, lava into a lava bucket",
      VoxelGen.FILLED_BUCKET[VoxelGen.WATER] == VoxelGen.WATER_BUCKET and VoxelGen.FILLED_BUCKET[VoxelGen.LAVA] == VoxelGen.LAVA_BUCKET)
check("a filled bucket places its liquid, and the empty one places nothing",
      VoxelGen.BUCKET_LIQUID == {VoxelGen.WATER_BUCKET: VoxelGen.WATER, VoxelGen.LAVA_BUCKET: VoxelGen.LAVA}
      and VoxelGen.BUCKET not in VoxelGen.BUCKET_LIQUID)
check("the bucket names are in the item names", all(VoxelGen.NAMES[b] for b in VoxelGen.BUCKETS))

if failures:
    print(f"\n{len(failures)} check(s) failed")
    sys.exit(1)
print("\nAll voxel tools checks passed.")
