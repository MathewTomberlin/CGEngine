"""
The pause menu and its settings page: clock, speed, view distance, sensitivity and the window.
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


class SettingsMixin:
    def set_clock(self, clock):
        self.clock = clock % 1.0
        self.save_prefs()

    def set_time_speed(self, speed):
        self.time_speed = speed
        self.save_prefs()

    def step_time_speed(self, step):
        """One step along TIME_SPEEDS (the settings page's arrows), from the choice nearest the current speed."""
        values = [value for _name, value in TIME_SPEEDS]
        index = min(range(len(values)), key=lambda i: abs(values[i] - self.time_speed))
        self.set_time_speed(values[max(0, min(len(values) - 1, index + step))])

    def time_speed_name(self):
        for name, value in TIME_SPEEDS:
            if value == self.time_speed:
                return name
        return "%gx" % self.time_speed

    def set_creative(self, on):
        self.creative = on
        self.notify("Creative mode on" if on else "Creative mode off")

    def set_view_distance(self, chunks):
        """The view distance nearest to chunks, from VIEW_STEPS. The chunks are asked for again at once (request_chunks)."""
        self.view_radius = min(VIEW_STEPS, key=lambda step: abs(step - chunks))
        self.wanted_centre = None
        self.save_prefs()
        return self.view_radius

    def teleport(self, x, y, z):
        self.pos = [x, y, z]
        self.vel = [0.0, 0.0, 0.0]
        self.on_ground = False
        self.fell_from = None

    def heal(self, amount=None):
        """Health back to all of it, or to amount (at most all of it). Returns the health."""
        self.health = PLAYER_HEALTH if amount is None else max(0.0, min(PLAYER_HEALTH, amount))
        return self.health

    def set_fullscreen(self, on):
        """Full screen or windowed (cge.set_fullscreen). The HUD is laid out for the new size on the next frame. False if the
        window cannot switch here."""
        if not cge.set_fullscreen(on):
            return False
        self.fullscreen = cge.is_fullscreen()
        self.save_prefs()
        return True

    def toggle_fullscreen(self):
        if not self.set_fullscreen(not self.fullscreen):
            self.notify("Full screen is not available here")

    def apply_window_settings(self):
        """At the start: the engine's own console is hidden (the game has its own, on backtick), and the saved window mode is used."""
        console_body = cge.find_body("Console")
        if console_body is not None:
            console_body.set_rendering_enabled(False)
        if self.fullscreen != cge.is_fullscreen():
            self.set_fullscreen(self.fullscreen)

    def change_sensitivity(self, step):
        index = SENSITIVITY_STEPS.index(self.sensitivity) + step
        self.sensitivity = SENSITIVITY_STEPS[max(0, min(len(SENSITIVITY_STEPS) - 1, index))]
        self.save_prefs()

    def change_view(self, step):
        """One step of view distance. The chunks are asked for again at once: a longer view requests the new ones, and a shorter
        one unloads the far ones (request_chunks)."""
        index = VIEW_STEPS.index(self.view_radius) + step
        self.view_radius = VIEW_STEPS[max(0, min(len(VIEW_STEPS) - 1, index))]
        self.wanted_centre = None
        self.save_prefs()

    def drag_time(self, mouse_x, pressing):
        """The time-of-day slider: while the button is held on the track, the time follows the mouse; the choice is saved
        when the button comes up."""
        left, right = self.menu_layout["time_track"][0][0], self.menu_layout["time_track"][0][2]
        if pressing and (self.dragging or self.menu_hover == "time_track"):
            self.dragging = True
            self.clock = min(0.999, max(0.0, (mouse_x - left) / (right - left)))
        elif self.dragging:
            self.dragging = False
            self.save_prefs()

    def look_speed(self):
        """Degrees a pixel of mouse movement turns the view."""
        return MOUSE_SENSITIVITY * self.sensitivity / 100.0

    def pause(self):
        self.paused = True
        self.menu_page = "pause"
        self.dragging = False
        cge.set_mouse_captured(False)
        self.mouse_captured = False
        self.menu_hover = None
        self.show_menu(True)

    def resume(self):
        self.paused = False
        cge.set_mouse_captured(True)
        self.mouse_captured = True
        self.show_menu(False)

    def update_pause_menu(self):
        """Hover and click the menu of the current page. Resume locks the mouse again, Quit closes the window, Settings
        opens the settings page, the arrows change the mouse sensitivity, and Back returns to the pause page."""
        mouse_x, mouse_y = cge.mouse_position()
        self.menu_hover = None
        for item in MENU_PAGES[self.menu_page][1]:
            rect = self.menu_layout[item][0]
            if rect and rect[0] <= mouse_x <= rect[2] and rect[1] <= mouse_y <= rect[3]:
                self.menu_hover = item
        pressing = bool(cge.mouse_down("Left"))
        self.drag_time(mouse_x, pressing)
        clicked = self.pressed("MenuClick", pressing)
        if clicked and self.menu_hover == "resume":
            self.resume()
            return
        if clicked and self.menu_hover == "quit":
            cge.quit()
            return
        if clicked and self.menu_hover == "save":
            self.save_world()
        if clicked and self.menu_hover == "load":
            self.load_world()
        if clicked and self.menu_hover == "settings":
            self.open_page("settings")
            return
        if clicked and self.menu_hover == "back":
            self.open_page("pause")
            return
        if clicked and self.menu_hover == "sens_down":
            self.change_sensitivity(-1)
        if clicked and self.menu_hover == "sens_up":
            self.change_sensitivity(1)
        if clicked and self.menu_hover == "view_down":
            self.change_view(-1)
        if clicked and self.menu_hover == "view_up":
            self.change_view(1)
        if clicked and self.menu_hover in ("window_down", "window_up"):
            self.toggle_fullscreen()
        if clicked and self.menu_hover == "jump_value":
            self.toggle_auto_jump()
        if clicked and self.menu_hover == "speed_down":
            self.step_time_speed(-1)
        if clicked and self.menu_hover == "speed_up":
            self.step_time_speed(1)
        self.show_menu(True)

    def update_pause_key(self):
        if self.console.open:
            # The console takes Escape (it closes), so the pause menu does not open over it.
            return
        if self.inventory_open:
            # Escape closes the inventory screen first; the menu opens on the next press.
            if self.pressed("Pause", bool(cge.key_down("Escape"))):
                self.close_inventory()
            return
        if self.pressed("Pause", bool(cge.key_down("Escape"))):
            if not self.paused:
                self.pause()
            elif self.menu_page == "settings":
                self.open_page("pause")
            else:
                self.resume()

    def open_page(self, page):
        self.menu_page = page
        self.menu_hover = None
        self.show_menu(True)

    def toggle_auto_jump(self):
        """Auto jump on or off (the settings page). Saved with the other settings."""
        self.auto_jump = not self.auto_jump
        self.save_prefs()

    def save_prefs(self):
        """The settings the game keeps between runs: the mouse sensitivity, the time of day and its speed, the view distance, the
        window's mode and Auto jump."""
        save_settings(self.sensitivity, self.clock, self.view_radius, self.time_speed, self.fullscreen, self.auto_jump)
