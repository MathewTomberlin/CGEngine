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
import VoxelEquipment
import VoxelRecipes
import VoxelPhysics as physics
from VoxelShapes import boxes_arrays, mix, outline_arrays
import VoxelWalkers
import StateFile
from VoxelConfig import *


# The armour slots: a column on the left of the screen, beside the crafting panel. Each slot is a box with its name and the worn
# piece's name beside it (window pixels).
EQUIPMENT_LEFT = 8
EQUIPMENT_TOP = 76
EQUIPMENT_ROW = 44
EQUIPMENT_BOX = 36
EQUIPMENT_LABELS = {"head": "Head", "chest": "Chest", "legs": "Legs", "feet": "Feet", "gloves": "Gloves"}


class InventoryMixin:
    craft_scroll = 0           # the first recipe the scrollbox shows (its offset into the visible recipes)
    craft_wheel = 0.0          # wheel notches not yet applied to the scroll (a trackpad turns in fractions)
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
                self.hide_rect("wear_%d" % index, box)
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
            # A tool or weapon shows its uses left as a bar along the foot of the swatch: green while it is new, red as it nears breaking.
            wear = self.inventory.durability(index) if stack else None
            bar_box = (left + inset, top + SLOT_SIZE - inset - 5, SLOT_SIZE - 2 * inset, 4)
            if wear is None:
                self.hide_rect("wear_%d" % index, bar_box)
            else:
                colour = (int(60 + 195 * (1.0 - wear)), int(40 + 200 * wear), 60, 235)
                self.draw_rect("wear_%d" % index, (bar_box[0], bar_box[1], int(bar_box[2] * wear), bar_box[3]), colour,
                               (0, 0, 0, 0), 0, layer=993)
            count = str(stack[1]) if stack and stack[1] > 1 else ""
            self.draw_text("count_%d" % index, count, (left + 5, top + SLOT_SIZE - 21), 14)
            key = hotbar_key(index) if row == 0 else ""
            self.draw_text("key_%d" % index, key, (left + 4, top + 1), 12, (200, 200, 200, 255))

    def equipment_box_at(self, x, y):
        """The armour slot whose box is under the window point, or None."""
        for index, slot in enumerate(VoxelEquipment.SLOTS):
            left, top = EQUIPMENT_LEFT, EQUIPMENT_TOP + EQUIPMENT_ROW * index
            if left <= x < left + EQUIPMENT_BOX and top <= y < top + EQUIPMENT_BOX:
                return slot
        return None

    def click_equipment(self, slot):
        """A click on an armour box. With a piece of armour held (the cursor, which follows the mouse), the piece goes on, and the
        piece it replaces is held in its place; with nothing held, the worn piece is taken off into the cursor."""
        held = self.inventory.held
        worn = self.equipment.worn[slot]
        if held and self.equipment.is_armour(held[0]):
            item = held[0]
            if self.equipment.slot_of(item) != slot:
                self.notify("The %s goes on the %s" % (VoxelGen.NAMES.get(item, "piece"), self.equipment.slot_of(item)))
                return
            if worn is not None and held[1] > 1 and not self.inventory.fits(worn):
                self.notify("No room for the %s" % VoxelGen.NAMES.get(worn, "piece"))
                return
            if held[1] > 1:
                held[1] -= 1
            else:
                self.inventory.held = None
            self.equipment.wear(item)
            if worn is not None:
                if self.inventory.held is None:
                    self.inventory.held = [worn, 1]
                else:
                    self.inventory.add(worn)
            self.notify("Wearing the %s" % VoxelGen.NAMES.get(item, "piece"))
        elif held:
            self.notify("Only armour goes on the armour slots")
        elif worn is not None:
            self.inventory.held = [worn, 1]
            self.equipment.take_off(slot)

    def wear_from_slot(self, index):
        """Puts on the armour in a slot (one piece). The piece it replaces goes back into the inventory; with no room for it, nothing
        changes."""
        stack = self.inventory.slots[index]
        if not stack or self.inventory.held or not self.equipment.is_armour(stack[0]):
            return
        item = stack[0]
        replaced = self.equipment.worn[self.equipment.slot_of(item)]
        stack[1] -= 1
        if stack[1] <= 0:
            self.inventory.slots[index] = None
        if replaced is not None and not self.inventory.fits(replaced):
            # Undo: the piece goes back to its slot.
            if self.inventory.slots[index] is None:
                self.inventory.slots[index] = [item, 1]
            else:
                self.inventory.slots[index][1] += 1
            self.notify("No room for the %s" % VoxelGen.NAMES.get(replaced, "piece"))
            return
        self.equipment.wear(item)
        if replaced is not None:
            self.inventory.add(replaced)
        self.notify("Wearing the %s" % VoxelGen.NAMES.get(item, "piece"))

    def update_equipment_bar(self):
        """The armour slots, beside the crafting panel while the inventory is open: a box a slot with the worn piece's colour, its
        name beside it and the piece's name under it. The bonuses are always shown at the top left, in a small font."""
        hover = self.equipment_box_at(*cge.mouse_position()) if self.inventory_open else None
        for index, slot in enumerate(VoxelEquipment.SLOTS):
            left, top = EQUIPMENT_LEFT, EQUIPMENT_TOP + EQUIPMENT_ROW * index
            if not self.inventory_open:
                self.hide_rect("equip_box_%d" % index, (left, top, EQUIPMENT_BOX, EQUIPMENT_BOX))
                self.draw_text("equip_name_%d" % index, "", (0, 0), 11)
                self.draw_text("equip_piece_%d" % index, "", (0, 0), 11)
                continue
            item = self.equipment.worn[slot]
            fill = tuple(int(c * 255) for c in VoxelGen.ITEM_COLOURS[item]) + (255,) if item is not None else (30, 30, 36, 225)
            outline = (255, 255, 255, 255) if hover == slot else (130, 130, 140, 255)
            self.draw_rect("equip_box_%d" % index, (left, top, EQUIPMENT_BOX, EQUIPMENT_BOX), fill, outline, 2, layer=985)
            self.draw_text("equip_name_%d" % index, EQUIPMENT_LABELS[slot], (left + EQUIPMENT_BOX + 6, top + 2), 11, (230, 230, 220, 255))
            piece = VoxelGen.NAMES.get(item, "") if item is not None else "empty"
            self.draw_text("equip_piece_%d" % index, piece, (left + EQUIPMENT_BOX + 6, top + 16), 11,
                           (200, 200, 190, 255) if item is not None else (130, 130, 130, 255))
        self.draw_text("equip_bonus", self.equipment.summary(), (14, 54), 11, (220, 220, 200, 255))

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
        self.update_tooltip()
        self.update_equipment_bar()
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

    def tooltip_text(self):
        """The name of the item in the slot under the mouse, with its count, or a tool's uses left; None when the slot is empty or
        an item is held (the held stack covers the mouse)."""
        if not self.inventory_open or self.inventory_hover is None or self.inventory.held is not None:
            return None
        stack = self.inventory.slots[self.inventory_hover]
        if not stack:
            return None
        ident, count = stack[0], stack[1]
        name = VoxelGen.NAMES.get(ident) or VoxelGen.ITEM_NAMES.get(ident) or "item"
        text = name if count == 1 else "%s x%d" % (name, count)
        if VoxelGen.durability_of(ident) is not None and len(stack) >= 3:
            text += "   %d of %d uses left" % (stack[2], VoxelGen.durability_of(ident))
        return text

    def update_tooltip(self):
        """A small label beside the mouse naming the item under it on the inventory screen."""
        text = self.tooltip_text()
        if text is None:
            self.hide_rect("tooltip_box", (0, 0, 1, 1))
            self.draw_text("tooltip", "", (0, 0), 14)
            return
        mouse_x, mouse_y = cge.mouse_position()
        box = (mouse_x + 18, mouse_y + 16, 14 + 7.5 * len(text), 26)
        self.draw_rect("tooltip_box", box, (12, 12, 16, 235), (150, 150, 160, 255), 1, layer=996)
        self.draw_text("tooltip", text, (box[0] + 7, box[1] + 5), 14)

    # ----- the crafting panel: above the inventory screen, a category dropdown and the recipes ------------

    def craft_layout(self, count):
        """The crafting panel's boxes in window pixels, for count recipes: the panel, the dropdown button, its options (one a
        category), and a row per recipe shown. Under the header is the crafting grid (craft_grid_boxes), then the rows. The panel
        sits above the inventory screen."""
        inv_left, inv_top = self.slot_origin(2 * COLUMNS)
        inv_width = COLUMNS * SLOT_SIZE + (COLUMNS - 1) * SLOT_GAP
        shown = min(max(count, 1), CRAFT_ROWS)
        height = CRAFT_HEADER + CRAFT_GRID + shown * CRAFT_ROW + CRAFT_FOOTER
        left = inv_left - 14
        top = inv_top - 46 - 12 - height
        panel = (left, top, inv_width + 28, height)
        button = (left + 12, top + 8, CRAFT_BUTTON_WIDTH, 24)
        options = [(left + 12, top + CRAFT_HEADER + k * 24, CRAFT_BUTTON_WIDTH, 22) for k in range(len(VoxelRecipes.CATEGORIES))]
        rows = [(left + 8, top + CRAFT_HEADER + CRAFT_GRID + i * CRAFT_ROW, inv_width + 12, CRAFT_ROW - 4) for i in range(shown)]
        return panel, button, options, rows

    def craft_scroll_range(self, count):
        """The most the recipe list can scroll: the recipes beyond the CRAFT_ROWS shown."""
        return max(0, count - CRAFT_ROWS)

    def craft_track(self, panel):
        """The scrollbar's track at the right of the recipe rows, in window pixels: (x, y, width, height)."""
        left, top, width, height = panel
        rows_top = top + CRAFT_HEADER + CRAFT_GRID
        return (left + width - 12, rows_top, 6, CRAFT_ROWS * CRAFT_ROW - 4)

    def scroll_crafting(self, notches):
        """Moves the recipe list by whole notches (positive is up), clamped to its range."""
        visible = self.crafting.visible(self.creative)
        self.craft_scroll = min(max(self.craft_scroll - notches, 0), self.craft_scroll_range(len(visible)))

    def craft_grid_boxes(self, panel):
        """The crafting grid's cells (row by row, GRID by GRID) and the output box, in window pixels, for a panel from craft_layout.
        The output sits to the right of the grid, with an arrow between them."""
        left, top = panel[0], panel[1]
        grid_left, grid_top = left + 12, top + CRAFT_HEADER + 6
        step = CRAFT_GRID_CELL + CRAFT_GRID_GAP
        span = VoxelRecipes.GRID * CRAFT_GRID_CELL + (VoxelRecipes.GRID - 1) * CRAFT_GRID_GAP
        cells = [(grid_left + col * step, grid_top + row * step, CRAFT_GRID_CELL, CRAFT_GRID_CELL)
                 for row in range(VoxelRecipes.GRID) for col in range(VoxelRecipes.GRID)]
        output = (grid_left + span + 60, grid_top, 220, span)
        return cells, output

    def crafting_at(self, x, y):
        """What a window point is on the crafting panel, while the inventory screen is open: ("button", None), ("option", the
        category), ("row", the index of the recipe), or None. While the dropdown is open its list covers the rows, so only the
        list counts."""
        if not self.inventory_open:
            return None
        visible = self.crafting.visible(self.creative)
        panel, button, options, rows = self.craft_layout(len(visible))
        if inside(button, x, y):
            return ("button", None)
        if self.crafting.dropdown_open:
            for name, box in zip(VoxelRecipes.CATEGORIES, options):
                if inside(box, x, y):
                    return ("option", name)
            return None
        cells, output = self.craft_grid_boxes(panel)
        for index, box in enumerate(cells):
            if inside(box, x, y):
                return ("cell", index)
        if inside(output, x, y):
            return ("output", None)
        for index, box in enumerate(rows):
            if inside(box, x, y):
                return ("row", index)
        return None

    def click_crafting(self, target, secondary=False):
        """A click on the crafting panel: the button opens or closes the dropdown, an option picks the category, and a grid cell
        takes or puts an item (secondary: the right button returns it to the inventory). The output makes the recipe the grid
        matches. A shaped recipe's row shows its pattern in the grid; a shapeless recipe's row makes it. A recipe that cannot be
        made says what is missing."""
        kind, value = target
        if kind == "button":
            self.crafting.dropdown_open = not self.crafting.dropdown_open
        elif kind == "option":
            self.crafting.category = value
            self.crafting.dropdown_open = False
            self.craft_scroll = 0
        elif kind == "cell":
            self.crafting.click_cell(value, self.inventory, secondary)
        elif kind == "output":
            recipe = self.crafting.grid_recipe(self.creative)
            if recipe is None:
                self.notify("The grid matches no recipe")
            elif self.crafting.craft_grid(self.inventory, self.creative):
                self.notify("Made %s" % VoxelRecipes.describe(recipe["makes"]))
            else:
                self.notify("Inventory full: no room for the %s" % VoxelRecipes.describe(recipe["makes"]))
        elif kind == "row":
            recipes = self.crafting.visible(self.creative)
            value += self.craft_scroll   # a row is one of the rows shown, from the scroll
            if value >= len(recipes):
                return
            recipe = recipes[value]
            if recipe.get("pattern"):
                if self.crafting.toggle_pattern(recipe["name"]):
                    self.notify("Fill the grid as shown: %s" % recipe["name"])
                return
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
        self.craft_scroll = min(self.craft_scroll, self.craft_scroll_range(len(visible)))
        shown_recipes = visible[self.craft_scroll:self.craft_scroll + CRAFT_ROWS]
        panel, button, options, rows = self.craft_layout(len(visible))
        cells, output = self.craft_grid_boxes(panel)
        track = self.craft_track(panel)
        if self.inventory_open and self.craft_scroll_range(len(visible)) > 0:
            span = self.craft_scroll_range(len(visible))
            thumb_height = max(12, track[3] * CRAFT_ROWS // len(visible))
            thumb_top = track[1] + round((track[3] - thumb_height) * self.craft_scroll / span)
            self.draw_rect("craft_track", track, (40, 40, 48, 255), (0, 0, 0, 0), 0, layer=992)
            self.draw_rect("craft_thumb", (track[0], thumb_top, track[2], thumb_height), (150, 150, 160, 255), (0, 0, 0, 0), 0, layer=993)
        else:
            self.hide_rect("craft_track", track)
            self.hide_rect("craft_thumb", track)
        if not self.inventory_open:
            for index in range(VoxelRecipes.GRID * VoxelRecipes.GRID):
                self.hide_rect("grid_cell_%d" % index, (0, 0, 1, 1))
                self.hide_rect("grid_swatch_%d" % index, (0, 0, 1, 1))
            self.hide_rect("grid_output", (0, 0, 1, 1))
            self.draw_text("grid_output_name", "", (0, 0), 15)
            self.draw_text("grid_output_detail", "", (0, 0), 13)
            self.draw_text("grid_arrow", "", (0, 0), 16)
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
        self.draw_text("craft_hint", "Click a recipe to make it; a shaped one shows its pattern. I or Esc: close",
                       (panel[0] + 12, panel[1] + panel[3] - 20), 12, (200, 200, 200, 255))
        self.draw_grid(cells, output)
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
            if index >= len(shown_recipes):
                self.hide_rect("craft_row_%d" % index, (0, 0, 1, 1))
                self.draw_text("craft_name_%d" % index, "", (0, 0), 15)
                self.draw_text("craft_detail_%d" % index, "", (0, 0), 13)
                continue
            recipe = shown_recipes[index]
            lacking = self.crafting.missing(recipe, self.inventory, self.creative)
            box = rows[index]
            edge = (255, 255, 255, 255) if index == self.craft_hover and not lacking else (70, 70, 80, 255)
            fill = (24, 24, 30, 220) if not lacking else (16, 16, 20, 200)
            self.draw_rect("craft_row_%d" % index, box, fill, edge, 1, layer=991)
            name_colour = (255, 255, 255, 255) if not lacking else (140, 140, 140, 255)
            if recipe["name"] == self.crafting.pattern:
                name_colour = (255, 217, 51, 255)   # the pattern shown in the grid
            self.draw_text("craft_name_%d" % index, recipe["name"], (box[0] + 8, box[1] + 5), 15, name_colour)
            if lacking:
                detail, colour = "needs %s" % VoxelRecipes.describe(lacking), (230, 150, 120, 255)
            else:
                detail, colour = "%s  ->  %s" % (VoxelRecipes.describe(recipe["takes"]), VoxelRecipes.describe(recipe["makes"])), (200, 200, 200, 255)
            self.draw_text("craft_detail_%d" % index, detail, (box[0] + 150, box[1] + 7), 13, colour)

    def draw_grid(self, cells, output):
        """The crafting grid: a cell per box, with the item in it. A shaped recipe whose pattern is shown puts its items as faint
        swatches in the cells it needs, from the top left. The output box names the recipe the grid matches, or waits for one."""
        ghost = self.crafting.pattern_recipe()
        guide = {}
        if ghost is not None:
            for row, cells_row in enumerate(ghost["pattern"]):
                for col, item in enumerate(cells_row):
                    if item is not None:
                        guide[row * VoxelRecipes.GRID + col] = item
        for index, box in enumerate(cells):
            item = self.crafting.grid[index]
            wanted = guide.get(index) if item is None else None
            edge = (200, 180, 90, 255) if wanted is not None else (110, 110, 120, 255)
            self.draw_rect("grid_cell_%d" % index, box, (24, 24, 30, 235), edge, 1, layer=991)
            inset = 4
            swatch = (box[0] + inset, box[1] + inset, box[2] - 2 * inset, box[3] - 2 * inset)
            if item is not None:
                self.draw_rect("grid_swatch_%d" % index, swatch, self.item_colour(item), (0, 0, 0, 170), 1, layer=992)
            elif wanted is not None:
                r, g, b, _ = self.item_colour(wanted)
                self.draw_rect("grid_swatch_%d" % index, swatch, (r, g, b, 90), (0, 0, 0, 0), 0, layer=992)
            else:
                self.hide_rect("grid_swatch_%d" % index, swatch)
        recipe = self.crafting.grid_recipe(self.creative)
        if recipe is None:
            self.draw_rect("grid_output", output, (24, 24, 30, 220), (70, 70, 80, 255), 1, layer=991)
            self.draw_text("grid_output_name", "Output", (output[0] + 8, output[1] + 8), 15, (140, 140, 140, 255))
            self.draw_text("grid_output_detail", "Fill the grid to match a pattern",
                           (output[0] + 8, output[1] + 32), 12, (140, 140, 140, 255))
        else:
            self.draw_rect("grid_output", output, (40, 34, 20, 235), (255, 217, 51, 255), 2, layer=991)
            self.draw_text("grid_output_name", recipe["name"], (output[0] + 8, output[1] + 8), 15)
            self.draw_text("grid_output_detail", "click to make  %s" % VoxelRecipes.describe(recipe["makes"]),
                           (output[0] + 8, output[1] + 32), 13, (255, 217, 51, 255))
        self.draw_text("grid_arrow", "->", (output[0] - 40, output[1] + output[3] / 2 - 9), 16, (200, 200, 200, 255))

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
        self.craft_scroll = 0
        self.craft_hover = None
        self.inventory_open = False
        self.inventory_hover = None
        self.crafting.return_grid(self.inventory)
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
        # The recipe list scrolls with the wheel over the panel, and by a click on its scrollbar (the track, by where it is).
        self.craft_wheel += cge.mouse_wheel()
        notches = int(self.craft_wheel)
        self.craft_wheel -= notches
        visible = self.crafting.visible(self.creative)
        panel, _, _, _ = self.craft_layout(len(visible))
        if notches and inside(panel, mouse_x, mouse_y):
            self.scroll_crafting(notches)
        track = self.craft_track(panel)
        if left and self.craft_scroll_range(len(visible)) > 0 and inside(track, mouse_x, mouse_y):
            fraction = min(max((mouse_y - track[1]) / track[3], 0.0), 1.0)
            self.craft_scroll = round(fraction * self.craft_scroll_range(len(visible)))
            return
        if target is not None:
            if left:
                self.click_crafting(target)
            elif right and target[0] == "cell":
                self.click_crafting(target, secondary=True)
            return
        box = self.equipment_box_at(mouse_x, mouse_y) if self.inventory_open else None
        if box is not None:
            if left:
                self.click_equipment(box)
            return
        shift = bool(cge.key_down("LShift") or cge.key_down("RShift"))
        if shift and left and self.inventory_hover is not None:
            # Shift and a left click on a piece of armour in the grid puts it on.
            self.wear_from_slot(self.inventory_hover)
            return
        if self.inventory_hover is None:
            return
        if left:
            self.inventory.click(self.inventory_hover)
        if right:
            self.inventory.click(self.inventory_hover, secondary=True)
