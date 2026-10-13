"""
The inventory screen and the crafting panel: slots, their clicks, and the recipes.
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


class InventoryMixin:
    def item_colour(self, item):
        """An item's colour as (r, g, b, 255), for its swatch."""
        r, g, b = VoxelGen.COLOURS.get(item) or VoxelGen.ITEM_COLOURS.get(item, WHITE)
        return (int(r * 255), int(g * 255), int(b * 255), 255)

    def slot_origin(self, index):
        """The top left corner of a slot, in window pixels. Row 0, the hotbar, is at the bottom of the window; rows 1 and 2
        (slots 12 to 35) stand above it when the inventory is open."""
        row, column = divmod(index, COLUMNS)
        row_width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        left = (self.window.x - row_width) / 2 + column * (SLOT_SIZE + SLOT_GAP)
        top = self.window.y - HUD_BOTTOM - SLOT_SIZE - row * (SLOT_SIZE + SLOT_GAP)
        return left, top

    def slot_at(self, x, y):
        """The slot under a window point, or None. Only the rows on screen count: the hotbar, or all three rows with the inventory open."""
        rows = ROWS if self.inventory_open else 1
        for index in range(COLUMNS * rows):
            left, top = self.slot_origin(index)
            if left <= x < left + SLOT_SIZE and top <= y < top + SLOT_SIZE:
                return index
        return None

    def update_slots(self):
        """The hotbar, and with the inventory open its two other rows: a box per slot, a swatch of the item's colour and the count.
        The selected slot is outlined in yellow and the slot under the mouse in white. Under the pause menu they are not drawn."""
        rows = ROWS if self.inventory_open else 1
        for index in range(SLOTS):
            row = index // COLUMNS
            left, top = self.slot_origin(index)
            box = (left, top, SLOT_SIZE, SLOT_SIZE)
            shown = row < rows and not self.paused
            stack = self.inventory.slots[index] if shown else None
            if not shown:
                self.hide_rect("slot_%d" % index, box)
                self.hide_rect("swatch_%d" % index, box)
                self.draw_text("count_%d" % index, "", (left, top), 14)
                self.draw_text("key_%d" % index, "", (left, top), 12)
                continue
            if index == self.slot and row == 0:
                edge, width = (255, 217, 51, 255), 3
            elif index == self.inventory_hover:
                edge, width = (255, 255, 255, 255), 2
            else:
                edge, width = (90, 90, 100, 255), 2
            self.draw_rect("slot_%d" % index, box, (20, 20, 26, 190), edge, width, layer=991)
            inset = 7
            swatch = (left + inset, top + inset, SLOT_SIZE - 2 * inset, SLOT_SIZE - 2 * inset)
            if stack:
                self.draw_rect("swatch_%d" % index, swatch, self.item_colour(stack[0]), (0, 0, 0, 170), 1, layer=992)
            else:
                self.hide_rect("swatch_%d" % index, swatch)
            count = str(stack[1]) if stack and stack[1] > 1 else ""
            self.draw_text("count_%d" % index, count, (left + 5, top + SLOT_SIZE - 21), 14)
            key = hotbar_key(index) if row == 0 else ""
            self.draw_text("key_%d" % index, key, (left + 4, top + 1), 12, (200, 200, 200, 255))

    def update_inventory_screen(self):
        """The inventory screen: a panel over the two rows above the hotbar, its title and how to use it, and the held stack, which
        follows the mouse. All of it is hidden while the screen is closed."""
        width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        left, top = self.slot_origin(2 * COLUMNS)
        bottom = self.slot_origin(0)[1] + SLOT_SIZE
        panel = (left - 14, top - 46, width + 28, bottom + 8 - (top - 46))
        if self.inventory_open:
            self.draw_rect("inv_panel", panel, (12, 12, 16, 225), (130, 130, 140, 255), 2, layer=980)
            self.draw_text("inv_title", "Inventory", (panel[0] + 14, panel[1] + 4), 22)
            self.draw_text("inv_hint", "Left click: take or put down a stack    Right click: half, or one    I or Esc: close",
                           (panel[0] + 14, panel[1] + 28), 13, (200, 200, 200, 255))
        else:
            self.hide_rect("inv_panel", panel)
            self.draw_text("inv_title", "", (0, 0), 22)
            self.draw_text("inv_hint", "", (0, 0), 13)
        held = self.inventory.held if self.inventory_open else None
        if held is None:
            self.hide_rect("held", (0, 0, 1, 1))
            self.draw_text("held_count", "", (0, 0), 14)
            return
        mouse_x, mouse_y = cge.mouse_position()
        size = SLOT_SIZE - 14
        self.draw_rect("held", (mouse_x - size / 2, mouse_y - size / 2, size, size), self.item_colour(held[0]),
                       (0, 0, 0, 200), 1, layer=995)
        count = str(held[1]) if held[1] > 1 else ""
        self.draw_text("held_count", count, (mouse_x - size / 2 + 4, mouse_y + size / 2 - 16), 14)

    # ----- the crafting panel: above the inventory screen, a category dropdown and the recipes ------------

    def craft_layout(self, count):
        """The crafting panel's boxes in window pixels, for count recipes: the panel, the dropdown button, its options (one a
        category), and a row per recipe shown. The panel sits above the inventory screen."""
        inv_left, inv_top = self.slot_origin(2 * COLUMNS)
        inv_width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        shown = min(max(count, 1), CRAFT_ROWS)
        height = CRAFT_HEADER + shown * CRAFT_ROW + CRAFT_FOOTER
        left = inv_left - 14
        top = inv_top - 46 - 12 - height
        panel = (left, top, inv_width + 28, height)
        button = (left + 12, top + 8, CRAFT_BUTTON_WIDTH, 24)
        options = [(left + 12, top + CRAFT_HEADER + k * 24, CRAFT_BUTTON_WIDTH, 22) for k in range(len(VoxelRecipes.CATEGORIES))]
        rows = [(left + 8, top + CRAFT_HEADER + i * CRAFT_ROW, inv_width + 12, CRAFT_ROW - 4) for i in range(shown)]
        return panel, button, options, rows

    def crafting_at(self, x, y):
        """What a window point is on the crafting panel, while the inventory screen is open: ("button", None), ("option", the
        category), ("row", the index of the recipe), or None. While the dropdown is open its list covers the rows, so only the
        list counts."""
        if not self.inventory_open:
            return None
        visible = self.crafting.visible(self.creative)
        _, button, options, rows = self.craft_layout(len(visible))
        if inside(button, x, y):
            return ("button", None)
        if self.crafting.dropdown_open:
            for name, box in zip(VoxelRecipes.CATEGORIES, options):
                if inside(box, x, y):
                    return ("option", name)
            return None
        for index, box in enumerate(rows):
            if inside(box, x, y):
                return ("row", index)
        return None

    def click_crafting(self, target):
        """A click on the crafting panel: the button opens or closes the dropdown, an option picks the category, and a recipe row
        makes that recipe. A recipe that cannot be made says what is missing."""
        kind, value = target
        if kind == "button":
            self.crafting.dropdown_open = not self.crafting.dropdown_open
        elif kind == "option":
            self.crafting.category = value
            self.crafting.dropdown_open = False
        elif kind == "row":
            recipes = self.crafting.visible(self.creative)
            if value >= len(recipes):
                return
            recipe = recipes[value]
            if self.crafting.craft(recipe, self.inventory, self.creative):
                self.notify("Made %s" % VoxelRecipes.describe(recipe["makes"]))
            else:
                lacking = self.crafting.missing(recipe, self.inventory, self.creative)
                if lacking:
                    self.notify("Missing: %s" % VoxelRecipes.describe(lacking))
                else:
                    self.notify("Inventory full: no room for the %s" % VoxelRecipes.describe(recipe["makes"]))

    def update_crafting_screen(self):
        """The crafting panel: the dropdown button with the chosen category, its options while it is open, and a row per recipe
        the player knows in that category. A recipe with an item missing is drawn dim, and says what it needs. Hidden while the
        inventory screen is closed."""
        visible = self.crafting.visible(self.creative) if self.inventory_open else []
        panel, button, options, rows = self.craft_layout(len(visible))
        if not self.inventory_open:
            self.hide_rect("craft_panel", (0, 0, 1, 1))
            self.hide_rect("craft_button", (0, 0, 1, 1))
            self.draw_text("craft_button_text", "", (0, 0), 14)
            self.draw_text("craft_title", "", (0, 0), 16)
            self.draw_text("craft_hint", "", (0, 0), 12)
            self.draw_text("craft_empty", "", (0, 0), 13)
            for k in range(len(VoxelRecipes.CATEGORIES)):
                self.hide_rect("craft_option_%d" % k, (0, 0, 1, 1))
                self.draw_text("craft_option_text_%d" % k, "", (0, 0), 13)
            for index in range(CRAFT_ROWS):
                self.hide_rect("craft_row_%d" % index, (0, 0, 1, 1))
                self.draw_text("craft_name_%d" % index, "", (0, 0), 15)
                self.draw_text("craft_detail_%d" % index, "", (0, 0), 13)
            return
        self.draw_rect("craft_panel", panel, (12, 12, 16, 225), (130, 130, 140, 255), 2, layer=980)
        self.draw_rect("craft_button", button, (32, 32, 40, 255), (150, 150, 160, 255), 1, layer=982)
        self.draw_text("craft_button_text", "Category: %s  v" % self.crafting.category, (button[0] + 8, button[1] + 4), 14)
        self.draw_text("craft_title", "Crafting", (button[0] + button[2] + 14, button[1] + 3), 16)
        self.draw_text("craft_hint", "Click a recipe to make it. I or Esc: close",
                       (panel[0] + 12, panel[1] + panel[3] - 20), 12, (200, 200, 200, 255))
        for k, name in enumerate(VoxelRecipes.CATEGORIES):
            if self.crafting.dropdown_open:
                box = options[k]
                edge = (255, 255, 255, 255) if name == self.crafting.category else (100, 100, 110, 255)
                self.draw_rect("craft_option_%d" % k, box, (24, 24, 30, 250), edge, 1, layer=996)
                self.draw_text("craft_option_text_%d" % k, name, (box[0] + 8, box[1] + 3), 13)
            else:
                self.hide_rect("craft_option_%d" % k, (0, 0, 1, 1))
                self.draw_text("craft_option_text_%d" % k, "", (0, 0), 13)
        if not visible:
            text = ("No recipes yet. Pick up an item to learn the recipes that use it."
                    if not self.crafting.known and not self.creative else "No recipes in this category.")
            self.draw_text("craft_empty", text, (rows[0][0] + 8, rows[0][1] + 7), 13, (200, 200, 200, 255))
        else:
            self.draw_text("craft_empty", "", (0, 0), 13)
        for index in range(CRAFT_ROWS):
            if index >= len(visible):
                self.hide_rect("craft_row_%d" % index, (0, 0, 1, 1))
                self.draw_text("craft_name_%d" % index, "", (0, 0), 15)
                self.draw_text("craft_detail_%d" % index, "", (0, 0), 13)
                continue
            recipe = visible[index]
            lacking = self.crafting.missing(recipe, self.inventory, self.creative)
            box = rows[index]
            edge = (255, 255, 255, 255) if index == self.craft_hover and not lacking else (70, 70, 80, 255)
            fill = (24, 24, 30, 220) if not lacking else (16, 16, 20, 200)
            self.draw_rect("craft_row_%d" % index, box, fill, edge, 1, layer=991)
            name_colour = (255, 255, 255, 255) if not lacking else (140, 140, 140, 255)
            self.draw_text("craft_name_%d" % index, recipe["name"], (box[0] + 8, box[1] + 5), 15, name_colour)
            if lacking:
                detail, colour = "needs %s" % VoxelRecipes.describe(lacking), (230, 150, 120, 255)
            else:
                detail, colour = "%s  ->  %s" % (VoxelRecipes.describe(recipe["takes"]), VoxelRecipes.describe(recipe["makes"])), (200, 200, 200, 255)
            self.draw_text("craft_detail_%d" % index, detail, (box[0] + 150, box[1] + 7), 13, colour)

    def learn_recipes(self):
        """A recipe is learned the first time the player holds one of its items, and the notice says so."""
        for name in self.crafting.learn(self.inventory):
            self.notify("New recipe: %s" % name)

    def update_inventory_key(self):
        """I opens the inventory screen, or closes it. It does not open over the pause menu."""
        if self.pressed("Inventory", bool(cge.key_down("I"))):
            if self.inventory_open:
                self.close_inventory()
            elif not self.paused and not self.console.open:
                self.open_inventory()

    def open_inventory(self):
        self.inventory_open = True
        self.inventory_hover = None
        self.mouse_before_inventory = self.mouse_captured
        cge.set_mouse_captured(False)
        self.mouse_captured = False

    def close_inventory(self):
        self.crafting.dropdown_open = False
        self.craft_hover = None
        self.inventory_open = False
        self.inventory_hover = None
        self.inventory.return_held()
        if self.mouse_before_inventory:
            cge.set_mouse_captured(True)
            self.mouse_captured = True

    def update_inventory(self):
        """The inventory screen: the left button takes or puts down a whole stack in the slot under the mouse, and the right takes
        half or puts one down (VoxelInventory.click). A click on the crafting panel is handled first. The buttons are read every
        frame, so a press is counted once."""
        mouse_x, mouse_y = cge.mouse_position()
        self.inventory_hover = self.slot_at(mouse_x, mouse_y)
        target = self.crafting_at(mouse_x, mouse_y)
        self.craft_hover = target[1] if target is not None and target[0] == "row" else None
        left = self.pressed("InventoryLeft", bool(cge.mouse_down("Left")))
        right = self.pressed("InventoryRight", bool(cge.mouse_down("Right")))
        if target is not None:
            if left:
                self.click_crafting(target)
            return
        if self.inventory_hover is None:
            return
        if left:
            self.inventory.click(self.inventory_hover)
        if right:
            self.inventory.click(self.inventory_hover, secondary=True)
