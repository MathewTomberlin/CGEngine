"""
The HUD: labels, the hotbar and reticle, the menus' drawing, the console and the notices.
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


class HudMixin:
    def setup_hud(self):
        """Two labels: the top left holds what the crosshair is on and the controls; the hotbar sits along the bottom."""
        cge.load_font(HUD_FONT, "hud")
        # Each label is one text draw, which costs the renderer a state switch, so there are only two.
        cge.create_text_body("hud_target", "", "hud", cge.Vector2f(14, 10), 16, outline_thickness=HUD_OUTLINE)
        cge.create_text_body("hud_help", HELP_TEXT, "hud", cge.Vector2f(14, 34), 14, cge.Color(200, 200, 200),
                             outline_thickness=HUD_OUTLINE)
        # A small cross at the middle of the window: the block the crosshair is on is outlined, and this shows where the aim is.
        size = cge.window_size()
        self.window = size
        cge.create_text_body("hud_cross", "+", "hud", cge.Vector2f(size.x / 2 - 6, size.y / 2 - 13), 22,
                             outline_thickness=HUD_OUTLINE)
        self.target_text = ""
        # The pause menu, drawn only while the game is paused (its labels are empty otherwise). Each item has a click
        # rectangle (left, top, right, bottom) in window pixels, or None if it is not clickable, and a text position.
        self.menu_text = {}
        self.menu_hover = None
        size = cge.window_size()
        geometry = self.menu_geometry(size)
        self.menu_layout = geometry["items"]
        self.menu_titles = geometry["titles"]
        cge.create_text_body("menu_title", "", "hud", cge.Vector2f(*self.menu_titles["pause"]), 28, outline_thickness=HUD_OUTLINE)
        for item, (_rect, (x, y)) in self.menu_layout.items():
            cge.create_text_body("menu_" + item, "", "hud", cge.Vector2f(x, y), 22, outline_thickness=HUD_OUTLINE)

    def set_menu_text(self, name, text):
        if self.menu_text.get(name) != text:
            self.menu_text[name] = text
            cge.set_text_body(name, text)

    def show_menu(self, shown):
        """The labels of the current page are drawn while the game is paused, and are empty (so not drawn) otherwise."""
        title, items = MENU_PAGES[self.menu_page] if shown else ("", ())
        self.set_menu_text("menu_title", title)
        if shown and self.title_page != self.menu_page:
            # Each page has its own title place: the settings page is taller, so its title is higher.
            self.title_page = self.menu_page
            cge.set_text_body("menu_title", title, position=cge.Vector2f(*self.menu_titles[self.menu_page]))
        for item in self.menu_layout:
            self.set_menu_text("menu_" + item, self.menu_label(item) if item in items else "")

    def menu_label(self, item):
        if item == "sens_value":
            return "Mouse sensitivity: %d%%" % self.sensitivity
        if item == "view_value":
            if self.view_radius == UNLIMITED_VIEW:
                return "View distance: Unlimited (%d)" % self.view_radius
            return "View distance: %d chunks" % self.view_radius
        if item == "window_value":
            return "Window: %s" % ("full screen" if self.fullscreen else "windowed")
        if item == "speed_value":
            return "Time speed: %s" % self.time_speed_name()
        if item == "time_value":
            minutes = (round(self.clock * 1440) + 360) % 1440   # 0 is sunrise (06:00), 0.25 noon, 0.5 sunset, 0.75 midnight
            return "Time of day: %02d:%02d" % (minutes // 60, minutes % 60)
        if item == "time_track":
            knob = min(TRACK_CELLS - 1, int(self.clock * TRACK_CELLS))
            return "[" + "-" * knob + "O" + "-" * (TRACK_CELLS - 1 - knob) + "]"
        text = MENU_TEXT[item]
        if self.menu_hover == item:
            return "[%s]" % text if item.endswith(("_down", "_up")) else "> %s <" % text
        return text

    def layout_hud(self):
        """Places the crosshair and the pause menu for the window's current size."""
        geometry = self.menu_geometry(self.window)
        self.menu_layout = geometry["items"]
        self.menu_titles = geometry["titles"]
        cross_x, cross_y = geometry["cross"]
        cge.set_text_body("hud_cross", self.menu_text.get("hud_cross", ""), position=cge.Vector2f(cross_x, cross_y))
        for item, (_rect, (x, y)) in self.menu_layout.items():
            cge.set_text_body("menu_" + item, self.menu_text.get("menu_" + item, ""), position=cge.Vector2f(x, y))
        self.title_page = None   # the title goes to its place for the page on the next show_menu
        self.show_menu(self.paused)

    def menu_geometry(self, size):
        """Where the pause menu's items and titles are for a window of this size. Each item has a click rectangle (left, top,
        right, bottom) or None, and a text position. The settings rows are stacked from the middle of the window."""
        cx, cy = size.x / 2, size.y / 2
        items = {
            "resume": ((cx - 100, cy - 4, cx + 100, cy + 30), (cx - 62, cy)),
            "settings": ((cx - 100, cy + 40, cx + 100, cy + 74), (cx - 62, cy + 44)),
            "quit": ((cx - 100, cy + 84, cx + 100, cy + 118), (cx - 40, cy + 88)),
            "sens_value": (None, (cx - 120, cy - 92)),
            "view_value": (None, (cx - 120, cy - 52)),
            "window_value": (None, (cx - 120, cy - 12)),
            "speed_value": (None, (cx - 120, cy + 28)),
            "time_value": (None, (cx - 120, cy + 64)),
            "time_track": ((cx - 150, cy + 84, cx + 150, cy + 110), (cx - 150, cy + 86)),
            "back": ((cx - 100, cy + 124, cx + 100, cy + 158), (cx - 30, cy + 128)),
        }
        for row, top in (("sens", cy - 96), ("view", cy - 56), ("window", cy - 16), ("speed", cy + 24)):
            items[row + "_down"] = ((cx - 215, top, cx - 150, top + 30), (cx - 200, top + 4))
            items[row + "_up"] = ((cx + 150, top, cx + 215, top + 30), (cx + 165, top + 4))
        return {"items": items, "titles": {"pause": (cx - 80, cy - 60), "settings": (cx - 90, cy - 130)},
                "cross": (cx - 6, cy - 13)}

    def draw_console(self):
        """The console's log and input line, under the help text, at the top of the window."""
        width = min(820, self.window.x - 28)
        self.console.draw((14, 62, width, (VoxelConsole.SHOWN_LINES + 1) * 18 + 12), layer=975)

    def update_reticle(self):
        """The crosshair is coloured while it is on a surface within reach (self.target), and white otherwise."""
        aiming = self.target is not None and not (self.paused or self.inventory_open or self.console.open)
        if aiming != self.reticle_aiming:
            self.reticle_aiming = aiming
            cge.set_text_body("hud_cross", self.menu_text.get("hud_cross", ""),
                              colour=cge.Color(*(RETICLE_AIM if aiming else RETICLE_PLAIN)))

    def set_target_text(self, text):
        if self.target_text != text:
            self.target_text = text
            cge.set_text_body("hud_target", text)

    def update_hud(self):
        if not self.hud_ready:
            return
        size = cge.window_size()
        if (size.x, size.y) != (self.window.x, self.window.y):
            # The window changed size (full screen): the HUD is laid out for it again.
            self.window = size
            self.layout_hud()
        # The crosshair is for aiming, so it is hidden while a menu, the inventory or the console is open.
        self.set_menu_text("hud_cross", "" if self.paused or self.inventory_open or self.console.open else "+")
        self.update_reticle()
        biome = self.world.biome_at(math.floor(self.pos[0]), math.floor(self.pos[2])).capitalize()
        if self.paused or self.inventory_open or self.console.open:
            self.set_target_text("")
        elif not self.mouse_captured:
            self.set_target_text("%s.  Click the game to capture the mouse" % biome)
        elif self.aims_at_walker():
            hits = self.walker_target[1].health
            self.set_target_text("%s.  Looking at: creature   (left click: %d hit%s to go)" % (biome, hits, "" if hits == 1 else "s"))
        elif self.aims_at_torch():
            self.set_target_text("%s.  Looking at: torch   (left click: take it back)" % biome)
        elif self.target is None:
            self.set_target_text("%s.  Looking at: nothing in reach" % biome)
        else:
            block, normal = self.target
            text = "%s.  Looking at: %s" % (biome, VoxelGen.NAMES.get(self.world.get_block(*block), "?"))
            cell = (block[0] + normal[0], block[1] + normal[1], block[2] + normal[2])
            if normal != (0, 0, 0) and not physics.cell_in_player(cell, self.pos):
                text += "   (right click: %s)" % self.selected_name()
            self.set_target_text(text)
        self.update_slots()
        self.update_health_bar()
        self.update_hurt_flash()
        self.update_inventory_screen()
        self.update_crafting_screen()
        self.update_notice()
        self.draw_console()

    # ----- the HUD's boxes: the hotbar, the inventory screen, the held stack and notices -----------

    def draw_rect(self, name, box, fill, outline=(0, 0, 0, 0), thickness=0, layer=990):
        """A rectangle on the HUD, made the first time at the layer (its z order). box is (x, y, width, height) in window pixels;
        fill and outline are (r, g, b, a) with 0 to 255 channels. It is only moved or recoloured when its value changes."""
        key = (box, fill, outline, thickness)
        if self.drawn.get(name) == key:
            return
        self.drawn[name] = key
        position, size = cge.Vector2f(box[0], box[1]), cge.Vector2f(box[2], box[3])
        if name in self.bodies:
            cge.set_rect_body(name, position, size, cge.Color(*fill), cge.Color(*outline), float(thickness))
        else:
            self.bodies.add(name)
            cge.create_rect_body(name, position, size, cge.Color(*fill), cge.Color(*outline), float(thickness), z_order=layer)

    def draw_text(self, name, text, position, size, colour=(255, 255, 255, 255)):
        """A label on the HUD at position (window pixels). Its text, and its position, change only when they differ from last time."""
        key = (text, position)
        if self.drawn.get(name) == key:
            return
        self.drawn[name] = key
        if name in self.bodies:
            cge.set_text_body(name, text, position=cge.Vector2f(position[0], position[1]))
        else:
            self.bodies.add(name)
            cge.create_text_body(name, text, "hud", cge.Vector2f(position[0], position[1]), size, cge.Color(*colour),
                                 outline_thickness=HUD_OUTLINE)

    def hide_rect(self, name, box):
        self.draw_rect(name, box, (0, 0, 0, 0))

    def notify(self, text):
        """Show a short message on the HUD, under the crosshair, for NOTICE_SECONDS."""
        self.notice = text
        self.notice_until = time.time() + NOTICE_SECONDS

    def update_notice(self):
        text = self.notice if time.time() < self.notice_until else ""
        self.draw_text("hud_notice", text, (self.window.x / 2 - 90, self.window.y / 2 + 26), 18, (255, 220, 120, 255))

    def update_health_bar(self):
        """Ten segments above the hotbar: red for the health left, dark for the rest. Not drawn under the pause menu."""
        segments = int(PLAYER_HEALTH)
        row_width = segments * HEALTH_SEGMENT + (segments - 1) * SLOT_GAP
        left = (self.window.x - row_width) / 2
        top = self.slot_origin(0)[1] - HEALTH_SEGMENT - 6
        for i in range(segments):
            x = left + i * (HEALTH_SEGMENT + SLOT_GAP)
            box = (x, top, HEALTH_SEGMENT, HEALTH_SEGMENT)
            if self.paused:
                self.hide_rect("hp_%d" % i, box)
                self.hide_rect("hp_fill_%d" % i, box)
                continue
            fill = max(0.0, min(1.0, self.health - i))
            self.draw_rect("hp_%d" % i, box, (50, 14, 14, 200), (0, 0, 0, 220), 1, layer=985)
            inner = HEALTH_SEGMENT - 4
            self.draw_rect("hp_fill_%d" % i, (x + 2, top + 2, inner * fill, inner), (210, 45, 45, 255), layer=986)

    def update_hurt_flash(self):
        """A red wash over the whole window at the moment of a hurt, fading to nothing over HURT_FLASH_SECONDS. It sits under
        the hotbar and the health bar. Clear (alpha 0) when there is no hurt or the pause menu is up."""
        left = self.hurt_flash_until - time.time()
        alpha = 0
        if left > 0.0 and not self.paused:
            alpha = int(HURT_FLASH_ALPHA * min(1.0, left / HURT_FLASH_SECONDS))
        self.draw_rect("hurt_flash", (0, 0, self.window.x, self.window.y), (200, 20, 20, alpha), layer=970)
