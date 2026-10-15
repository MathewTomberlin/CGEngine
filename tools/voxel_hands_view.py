"""
Holds an item in the first-person hands of the voxel sample, for tools/voxel_hands_shot.ps1 to screenshot. Run inside the game
through the control channel (run_script), with HANDS_ITEM in the environment: pickaxe (the default), iron_pickaxe, torch or none.
The slot is the first of the hotbar; the hands are placed from the view each frame, as in play.
"""
import gc
import os

import VoxelGen
import VoxelInventory

ITEMS = {"pickaxe": VoxelGen.PICKAXE, "iron_pickaxe": VoxelGen.IRON_PICKAXE, "sword": VoxelGen.SWORD, "shovel": VoxelGen.SHOVEL, "bucket": VoxelGen.BUCKET, "water_bucket": VoxelGen.WATER_BUCKET, "lava_bucket": VoxelGen.LAVA_BUCKET, "torch": VoxelGen.TORCH, "none": None}
item = ITEMS[os.environ.get("HANDS_ITEM", "pickaxe")]
game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
game.time_speed = 0.0    # noon, held still, so the shots of one seed look the same
game.clock = 0.25
game.slot = 0
game.inventory.slots = [None] * VoxelInventory.SLOTS
if item is not None:
    durability = VoxelGen.TOOLS[item]["durability"] if item in VoxelGen.TOOLS else 1
    game.inventory.slots[0] = [item, 1, durability]
print("HANDS holding %s" % VoxelGen.ITEM_NAMES.get(item, "nothing") if item is not None else "HANDS holding nothing")


# HANDS_SHIELD=carried or raised: the shield in the left hand (raised as if Q were held).
shield = os.environ.get("HANDS_SHIELD")
if shield:
    game.offhand = VoxelGen.SHIELD
    if shield == "raised":
        game.shield_up = lambda: True
    game.update_hands(0.0)

# The left hand bone's place in the left arm's frame, for placing the shield (written beside the exe).
import json as _json
import cg_engine_bindings as cge
with open(os.path.join(os.getcwd(), "hands_left_bone.json"), "w") as f:
    _json.dump({"left_hand": cge.bone_matrix("hand_left", "hand.L"), "right_hand": cge.bone_matrix("hand_right", "hand.R"),
                "left_forearm": cge.bone_matrix("hand_left", "forearm.L")}, f)
import json as _probe_json
with open(os.path.join(os.getcwd(), "hands_shield_probe.json"), "w") as f:
    body = cge.find_body("hand_shield")
    _probe_json.dump({"shield_body": body is not None, "state": getattr(game, "shield_state", "x"), "offhand": game.offhand, "offset": getattr(game, "shield_offset", None), "bone": getattr(game, "shield_bone", None),
                      "pos": None, "children": body.get_child_count() if body is not None else None, "name": body.get_name() if body is not None else None}, f)
