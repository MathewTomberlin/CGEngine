"""
Checks the sample game's inventory and block use from inside the engine (resources/scripts/VoxelGame.py): a break gives the
block to the inventory, a placed block takes one from the selected slot, creative mode keeps the stock, a full inventory refuses
the hit that would break a block, torches go in and out of the inventory, the inventory screen holds and puts down a stack, and
F5 and F9 keep the inventory with the edits. test_voxels.ps1 runs it through the control channel (run_script) and reads its result.

The game is paused while this runs, so the frames do not move the world or the player. The buttons are pressed by hand: the
checks set the state the game reads (mouse, keys, target) and call its own edit_blocks and update_inventory. The game's input
functions are replaced for the run and put back afterwards. The result goes to cg_control/voxel_game_check.json as
{"check": {name: bool}, "errors": [..]}.
"""
import gc
import json
import os
import random
import time

import cg_engine_bindings as cge
import VoxelGame
import VoxelGen
import VoxelInventory
import VoxelWalkers

RESULT = "cg_control/voxel_game_check.json"
STONE, DIRT, TORCH = VoxelGen.STONE, VoxelGen.DIRT, VoxelGen.TORCH
DOWN = (0, -1, 0)
UP = (0, 1, 0)

PRESSED = {}            # "key:<name>" and "mouse:<button>" -> bool, read by the replaced input functions
MOUSE = [0, 0]


def find_game():
    games = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"]
    if not games:
        raise RuntimeError("no VoxelGame in the engine (the scene must run it)")
    return games[0]


def click(game, button, secondary=False):
    """One press and release of a mouse button: the game's edit_blocks sees the press on its first call."""
    name = "Right" if secondary else "Left"
    PRESSED["mouse:" + name] = True
    game.edit_blocks()
    PRESSED["mouse:" + name] = False
    game.edit_blocks()


def screen_click(game, slot, secondary=False):
    """A click on an inventory slot, through the inventory screen's own update."""
    left, top = game.slot_origin(slot)
    MOUSE[0], MOUSE[1] = left + 20, top + 20
    name = "Right" if secondary else "Left"
    PRESSED["mouse:" + name] = True
    game.update_inventory()
    PRESSED["mouse:" + name] = False
    game.update_inventory()


def main():
    checks = {}
    errors = []
    game = find_game()
    world = game.world
    inv = game.inventory
    saved = {
        "paused": game.paused, "creative": game.creative, "slot": game.slot, "slots": inv.to_list(),
        "mouse_captured": game.mouse_captured, "inventory_open": game.inventory_open,
        "pos": list(game.pos), "health": game.health, "landed_once": game.landed_once, "fell_from": game.fell_from,
    }
    originals = {name: getattr(cge, name) for name in ("key_down", "mouse_down", "mouse_position", "mouse_delta",
                                                       "set_mouse_captured")}
    cge.key_down = lambda name: PRESSED.get("key:" + name, False)
    cge.mouse_down = lambda name: PRESSED.get("mouse:" + name, False)
    cge.mouse_position = lambda: (MOUSE[0], MOUSE[1])
    cge.mouse_delta = lambda: (0.0, 0.0)
    cge.set_mouse_captured = lambda captured: None
    game.paused = True
    game.creative = False
    game.mouse_captured = True
    game.torch_target = None
    game.walker_target = None
    game.target = None
    game.hit_times = {}
    inv.slots = [None] * VoxelInventory.SLOTS
    inv.held = None
    inv.start(VoxelGame.START_ITEMS, VoxelGame.START_COUNT)
    game.slot = 0
    MOUSE[0], MOUSE[1] = game.window.x / 2, game.window.y / 2
    # A column of test blocks in the loaded chunk at (0, 0), high above the ground the player stands on.
    test_cells = []

    def put(x, y, z, block):
        world.set_block(x, y, z, block)
        test_cells.append((x, y, z))

    def hit(block, normal=UP):
        game.target = (block, normal)
        click(game, "Left")

    try:
        # A break gives the block to the inventory: stone takes three hits, the last one gives a stone.
        base = (2, 58, 2)
        put(*base, STONE)
        stone0 = inv.count(STONE)
        hit(base)
        checks["a hit on stone leaves it standing"] = world.get_block(*base) == STONE and inv.count(STONE) == stone0
        checks["a hit flashes the aimed block's outline"] = game.flash_until > time.time()
        hit(base)
        hit(base)
        checks["the third hit breaks the stone and adds it to the inventory"] = (
            world.get_block(*base) == 0 and inv.count(STONE) == stone0 + 1)
        checks["a break throws chips (a burst is live)"] = len(game.bursts.live) > 0
        live_before = len(game.bursts.live)
        game.bursts.add((2.5, 58.5, 2.5), (0.5, 0.5, 0.5), 4, 0.14, 0.5)
        game.bursts.update(0.05)
        checks["a live burst moves and is rebuilt without an error"] = len(game.bursts.live) == live_before + 1
        game.bursts.update(10.0)
        checks["the chips are gone once their time is up"] = len(game.bursts.live) == 0

        # A placed block takes one from the selected slot.
        floor = (2, 58, 6)
        put(*floor, DIRT)
        cell = (2, 59, 6)
        game.slot = 0
        before = inv.slots[0][1]
        game.target = (floor, UP)
        click(game, "Right", secondary=True)
        checks["a placed block is in the cell in front of the one aimed at"] = world.get_block(*cell) == STONE
        checks["placing takes one from the selected slot"] = inv.slots[0][1] == before - 1
        world.set_block(*cell, 0)

        # An empty slot places nothing and says so.
        slot5 = inv.slots[5]
        inv.slots[5] = None
        game.slot = 5
        click(game, "Right", secondary=True)
        checks["an empty slot places nothing"] = world.get_block(*cell) == 0
        checks["an empty slot says so on the HUD"] = game.notice == "This slot is empty"
        inv.slots[5] = slot5
        game.slot = 0

        # Creative mode: placing and breaking do not touch the stock.
        game.creative = True
        before = inv.slots[0][1]
        click(game, "Right", secondary=True)
        checks["creative: a placed block is there and the stock stays"] = (
            world.get_block(*cell) == STONE and inv.slots[0][1] == before)
        world.set_block(*cell, 0)
        game.target = (floor, UP)
        stone_creative = inv.count(STONE)
        put(4, 58, 2, STONE)
        game.target = ((4, 58, 2), UP)
        for _ in range(3):
            click(game, "Left")
        checks["creative: a broken block gives nothing to the inventory"] = (
            world.get_block(4, 58, 2) == 0 and inv.count(STONE) == stone_creative)
        game.creative = False

        # A full inventory refuses the hit that would break a block, and not the hits before it.
        full = [[DIRT, VoxelInventory.STACK_LIMIT] for _ in range(VoxelInventory.SLOTS)]
        inv.slots = full
        put(6, 58, 2, STONE)
        game.target = ((6, 58, 2), UP)
        click(game, "Left")
        click(game, "Left")
        checks["a full inventory still lets the first hits land"] = world.get_block(6, 58, 2) == STONE
        click(game, "Left")
        checks["a full inventory refuses the hit that would break the block"] = (
            world.get_block(6, 58, 2) == STONE and game.notice == "Inventory full")
        inv.slots = [None] * VoxelInventory.SLOTS

        # Torches: placed from a slot onto a face, taken back by breaking them, and given back when the block under them breaks.
        torch_base = (8, 58, 6)
        put(*torch_base, DIRT)
        torch_cell = (8, 59, 6)
        inv.slots[0] = [TORCH, 3]
        game.slot = 0
        game.target = (torch_base, UP)
        click(game, "Right", secondary=True)
        checks["a torch goes onto the face, from its slot"] = (
            world.prop_at(*torch_cell) != -1 and inv.slots[0] == [TORCH, 2])
        game.torch_target = (1.0, torch_cell, (0.0, 0.0, 0.0), (1.0, 1.0, 1.0))
        game.target = None
        click(game, "Left")
        checks["breaking a torch takes it back into the inventory"] = (
            world.prop_at(*torch_cell) == -1 and inv.count(TORCH) == 3)
        game.torch_target = None

        under = (10, 58, 6)
        put(*under, STONE)
        above = (10, 59, 6)
        world.set_prop(above[0], above[1], above[2], 3)
        test_cells.append(above)
        game.target = (under, UP)
        torches0 = inv.count(TORCH)
        for _ in range(3):
            click(game, "Left")
        checks["breaking a block gives back the torches that leaned on it"] = (
            world.prop_at(*above) == -1 and inv.count(TORCH) == torches0 + 1 and world.get_block(*under) == 0)

        # The inventory screen: a click takes a whole stack, and a click on another slot puts it down.
        game.open_inventory()
        checks["the inventory screen opens and frees the mouse"] = game.inventory_open and not game.mouse_captured
        inv.slots[3] = [STONE, 9]
        screen_click(game, 3)
        checks["a click on a slot takes its stack into the cursor"] = inv.held == [STONE, 9] and inv.slots[3] is None
        screen_click(game, 20)
        checks["a click on another slot puts the stack down there"] = inv.slots[20] == [STONE, 9] and inv.held is None
        screen_click(game, 25, secondary=True)
        checks["a right click on an empty slot takes nothing"] = inv.held is None and inv.slots[25] is None
        game.close_inventory()
        checks["closing the screen recaptures the mouse and leaves nothing held"] = (
            not game.inventory_open and game.mouse_captured and inv.held is None)
        inv.slots[20] = None

        # F5 saves the inventory with the edits, and F9 loads it back.
        inv.slots[0] = [DIRT, 7]
        game.slot = 2
        PRESSED["key:F5"] = True
        game.save_and_load()
        PRESSED["key:F5"] = False
        with open("cg_control/voxel_inventory.json") as f:
            data = json.load(f)
        checks["F5 saves the inventory beside the edits"] = data["slots"][0] == [DIRT, 7] and data["slot"] == 2
        inv.slots = [None] * VoxelInventory.SLOTS
        game.slot = 0
        PRESSED["key:F9"] = True
        game.save_and_load()
        PRESSED["key:F9"] = False
        checks["F9 loads the inventory back"] = inv.slots[0] == [DIRT, 7] and game.slot == 2

        # C toggles creative mode.
        PRESSED["key:C"] = True
        game.update_modes()
        PRESSED["key:C"] = False
        checks["C turns creative mode on"] = game.creative is True
        game.creative = False

        # Creatures: a hit takes one of the creature's health, the last one takes it out of the world and leaves a hide.
        checks["creatures are in the world"] = game.walkers is not None and len(game.walkers.walkers) > 0
        if game.walkers is None:
            raise RuntimeError("the creatures are not made yet")
        spawn = (game.spawn_pos[0] + 3.0, 40.0, game.spawn_pos[2] + 3.0)
        test_walker = VoxelWalkers.Walker(world, "test_creature", spawn, (0.5, 0.5, 0.5), random.Random(1))
        game.walkers.walkers.append(test_walker)
        hides0 = inv.count(VoxelGen.HIDE)
        game.walker_target = (1.0, test_walker)
        game.target = None
        click(game, "Left")
        checks["a hit on a creature takes one of its health"] = test_walker.health == VoxelWalkers.HEALTH - 1
        checks["the hit flashes the creature's outline"] = game.flash_until > time.time()
        click(game, "Left")
        click(game, "Left")
        checks["the last hit takes the creature out of the world"] = (
            test_walker not in game.walkers.walkers and cge.find_body("test_creature") is None)
        checks["a dead creature leaves a hide in the inventory"] = inv.count(VoxelGen.HIDE) == hides0 + 1
        game.walker_target = None
        game.slot = 0
        inv.slots[0] = [VoxelGen.HIDE, 2]
        game.target = (floor, UP)
        before_hide = inv.count(VoxelGen.HIDE)
        click(game, "Right", secondary=True)
        checks["a hide cannot be placed"] = game.notice == "A hide cannot be placed" and inv.count(VoxelGen.HIDE) == before_hide
        inv.slots[0] = None

        # A biter bites when it is close, and then waits for its next bite.
        biter = VoxelWalkers.Walker(world, "test_biter", spawn, (0.4, 0.1, 0.1), random.Random(2), biter=True)
        player_xz = (biter.state()[0][0], biter.state()[0][2])
        first_bite = biter.update(0.0, player_xz)
        second_bite = biter.update(0.0, player_xz)
        checks["a biter bites when it is close, not again at once"] = first_bite == VoxelWalkers.BITE_DAMAGE and second_bite == 0.0
        biter.remove()

        # The player's health: bites and falls lower it, it heals once it has not been hurt for a while, and a death puts it
        # back at the spawn with the inventory kept.
        game.health = VoxelGame.PLAYER_HEALTH
        game.hurt_player(3.0)
        checks["a hit takes health from the player"] = abs(game.health - (VoxelGame.PLAYER_HEALTH - 3.0)) < 1e-6
        game.hurt_at = time.time() - 100.0
        game.update_health(1.0)
        checks["the player heals after a while without a hit"] = abs(game.health - (VoxelGame.PLAYER_HEALTH - 3.0 + VoxelGame.REGEN_RATE)) < 1e-6
        game.health = VoxelGame.PLAYER_HEALTH
        game.landed_once = True
        game.flying = False
        game.swimming = False
        game.on_ground = False
        game.fell_from = None
        game.pos[1] = 30.0
        game.track_fall()
        game.on_ground = True
        game.pos[1] = 20.0
        game.track_fall()
        expected = VoxelGame.PLAYER_HEALTH - (10.0 - VoxelGame.FALL_SAFE) * VoxelGame.FALL_DAMAGE
        checks["a fall past the safe height hurts, by the blocks past it"] = abs(game.health - expected) < 1e-6
        game.health = VoxelGame.PLAYER_HEALTH
        inv_total = sum(stack[1] for stack in inv.slots if stack)
        game.pos = [5.5, 33.0, 5.5]
        game.hurt_player(VoxelGame.PLAYER_HEALTH + 1.0)
        checks["a death puts the player back at the spawn, healed"] = (
            game.health == VoxelGame.PLAYER_HEALTH and abs(game.pos[0] - game.spawn_pos[0]) < 1e-6)
        checks["a death keeps the inventory"] = sum(stack[1] for stack in inv.slots if stack) == inv_total
        game.health = VoxelGame.PLAYER_HEALTH
        game.landed_once = False
    except Exception as error:   # the checks above are the record; an error is a failure too
        errors.append("%s: %s" % (type(error).__name__, error))
    finally:
        for cell in test_cells:
            world.set_block(cell[0], cell[1], cell[2], 0)
        for burst in game.bursts.live:
            burst.remove()
        game.bursts.live = []
        for name in ("test_creature", "test_biter"):
            if cge.find_body(name) is not None:
                cge.remove_mesh_body(name)
                cge.remove_voxel_walker(name)
        for name, value in originals.items():
            setattr(cge, name, value)
        inv.slots = [None] * VoxelInventory.SLOTS
        inv.held = None
        inv.load_list(saved["slots"])
        game.slot = saved["slot"]
        game.creative = saved["creative"]
        game.mouse_captured = saved["mouse_captured"]
        game.inventory_open = saved["inventory_open"]
        game.paused = saved["paused"]
        game.pos = saved["pos"]
        game.health = saved["health"]
        game.landed_once = saved["landed_once"]
        game.fell_from = saved["fell_from"]
        game.walker_target = None
        game.target = None
        game.torch_target = None
        os.makedirs("cg_control", exist_ok=True)
        with open(RESULT, "w") as f:
            json.dump({"check": checks, "errors": errors}, f, indent=2)
    for name, ok in checks.items():
        print(("PASS  " if ok else "FAIL  ") + name)
    for error in errors:
        print("ERROR " + error)


main()
