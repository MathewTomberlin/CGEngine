"""The developer console for the voxel sample (roadmap 19.1). Backtick opens a line of commands while the game is running: the
world stands still while it is open, and the commands act on the game itself (the time, the player, blocks, creatures, items,
the window). Type "help" for the list, or "help <command>" for one. A command is a function that takes the console and the
words after its name, and returns the lines to print; it is listed in COMMANDS with its usage and a summary.

Errors in a command are printed in the log, and the game goes on. Nothing here draws a screen of its own: the game calls
update() once a frame, and draw() places the log and the input line over the HUD (see VoxelGame.draw_console)."""
import math
import random

import cg_engine_bindings as cge
import VoxelGen
import VoxelWalkers

LOG_LINES = 200         # lines kept in the log
SHOWN_LINES = 12        # lines drawn above the input line
LINE_PITCH = 18         # pixels from one log line to the next
FONT_SIZE = 15          # the console's text size
MAX_HISTORY = 50        # lines remembered for the Up and Down keys
MAX_FILL = 20000        # blocks one fill may set
MAX_DESTROY = 12        # the largest radius destroy takes
MAX_STACK = 64          # items given or dropped in one command
MAX_CREATURES = 20      # creatures spawned in one command

TIME_NAMES = {"sunrise": 0.0, "noon": 0.25, "sunset": 0.5, "midnight": 0.75}
SPEED_NAMES = {"paused": 0.0, "slow": 0.25, "normal": 1.0, "fast": 4.0}
COMMANDS = {}


class CommandError(Exception):
    """A command that cannot be done: the message is printed as it is."""


def command(name, usage, summary):
    """Lists a command function under its name, with its usage and a one-line summary."""
    def register(function):
        COMMANDS[name] = (function, usage, summary)
        return function
    return register


def parse_float(text, what):
    try:
        return float(text)
    except ValueError:
        raise CommandError("%s must be a number, not '%s'" % (what, text))


def parse_int(text, what):
    try:
        return int(text)
    except ValueError:
        raise CommandError("%s must be a whole number, not '%s'" % (what, text))


def block_id(name):
    """The id of a block or item by its name (VoxelGen.NAMES and ITEM_NAMES). 'air' is no block."""
    key = name.lower()
    if key == "air":
        return VoxelGen.AIR
    for ident, label in list(VoxelGen.NAMES.items()) + list(VoxelGen.ITEM_NAMES.items()):
        if label == key:
            return ident
    raise CommandError("no block or item called '%s'" % name)


def clock_from(text):
    """A time of day as a fraction of the day, from a name (noon), HH:MM (06:00 is sunrise), or a fraction from 0 up to 1."""
    if text.lower() in TIME_NAMES:
        return TIME_NAMES[text.lower()]
    if ":" in text:
        hours, minutes = text.split(":", 1)
        hour, minute = parse_int(hours, "the hours"), parse_int(minutes, "the minutes")
        if not (0 <= hour < 24 and 0 <= minute < 60):
            raise CommandError("the time must be from 00:00 to 23:59")
        return ((hour * 60 + minute - 360) % 1440) / 1440.0
    value = parse_float(text, "the time")
    if not 0.0 <= value < 1.0:
        raise CommandError("a fraction of the day must be from 0 up to 1 (0 is sunrise)")
    return value


def clock_text(clock):
    minutes = (round(clock * 1440) + 360) % 1440
    return "%02d:%02d" % (minutes // 60, minutes % 60)


def speed_text(speed):
    return "paused" if speed == 0 else "%gx" % speed


def on_off(text):
    """on or off, for a command's first word. None when the word is missing (the command then toggles)."""
    if text is None:
        return None
    if text.lower() in ("on", "yes", "1", "true"):
        return True
    if text.lower() in ("off", "no", "0", "false"):
        return False
    raise CommandError("expected on or off, not '%s'" % text)


def item_count(args, default=1):
    """The count after an item name, from 1 up to MAX_STACK."""
    count = parse_int(args[1], "the count") if len(args) > 1 else default
    if not 1 <= count <= MAX_STACK:
        raise CommandError("the count must be from 1 to %d" % MAX_STACK)
    return count


@command("help", "help [command]", "list the commands, or say what one does")
def cmd_help(console, args):
    if args:
        name = args[0].lower()
        if name not in COMMANDS:
            raise CommandError("no command called '%s'" % args[0])
        _function, usage, summary = COMMANDS[name]
        return ["%s: %s" % (usage, summary)]
    names = ", ".join(sorted(COMMANDS))
    lines = ["Commands: " + names[:100]]
    lines += [names[i:i + 100] for i in range(100, len(names), 100)]
    return lines + ["help <command> says what one does"]


@command("echo", "echo <text>", "print the text")
def cmd_echo(console, args):
    return [" ".join(args)]


@command("time", "time [name|HH:MM|fraction]", "set the time of day (sunrise, noon, sunset, midnight, HH:MM, or a fraction from 0 to 1); no argument shows it")
def cmd_time(console, args):
    game = console.game
    if not args:
        return ["Time of day %s" % clock_text(game.clock)]
    game.set_clock(clock_from(args[0]))
    return ["Time of day set to %s" % clock_text(game.clock)]


@command("timespeed", "timespeed [paused|slow|normal|fast|multiplier]", "how fast the sun moves (0 pauses it); no argument shows it")
def cmd_timespeed(console, args):
    game = console.game
    if not args:
        return ["Time speed %s" % speed_text(game.time_speed)]
    word = args[0].lower()
    speed = SPEED_NAMES[word] if word in SPEED_NAMES else parse_float(args[0], "the speed")
    if not 0.0 <= speed <= 100.0:
        raise CommandError("the speed must be from 0 to 100")
    game.set_time_speed(speed)
    return ["Time speed set to %s" % speed_text(speed)]


@command("tp", "tp spawn | tp x z | tp x y z", "teleport the player; tp x z puts it on the ground at that place")
def cmd_tp(console, args):
    game = console.game
    if args == ["spawn"]:
        x, y, z = game.spawn_pos
    elif len(args) == 2:
        x, z = parse_float(args[0], "x"), parse_float(args[1], "z")
        y = game.ground_under(x, VoxelGen.HEIGHT, z)
    elif len(args) == 3:
        x, y, z = (parse_float(args[0], "x"), parse_float(args[1], "y"), parse_float(args[2], "z"))
    else:
        raise CommandError("usage: tp spawn | tp x z | tp x y z")
    game.teleport(x, y, z)
    return ["Teleported to %.1f %.1f %.1f" % (x, y, z)]


@command("pos", "pos", "print where the player is and the biome")
def cmd_pos(console, args):
    game = console.game
    x, y, z = game.pos
    biome = game.world.biome_at(int(x // 1), int(z // 1))
    return ["At %.1f %.1f %.1f in the %s, health %.1f, %s" % (x, y, z, biome, game.health,
            "creative" if game.creative else "survival")]


@command("heal", "heal [amount]", "restore health, all of it or the amount")
def cmd_heal(console, args):
    amount = parse_float(args[0], "the amount") if args else None
    health = console.game.heal(amount)
    return ["Health %.1f" % health]


@command("hurt", "hurt <amount>", "take damage as a bite does (creative mode takes none)")
def cmd_hurt(console, args):
    if not args:
        raise CommandError("usage: hurt <amount>")
    console.game.hurt_player(parse_float(args[0], "the amount"))
    return ["Health %.1f" % console.game.health]


@command("creative", "creative [on|off]", "creative mode: no stock, and no damage; no argument toggles it")
def cmd_creative(console, args):
    game = console.game
    value = on_off(args[0] if args else None)
    game.set_creative(not game.creative if value is None else value)
    return ["Creative mode %s" % ("on" if game.creative else "off")]


@command("fly", "fly [on|off]", "fly, or walk again; no argument toggles it")
def cmd_fly(console, args):
    game = console.game
    value = on_off(args[0] if args else None)
    game.flying = not game.flying if value is None else value
    return ["Flying %s" % ("on" if game.flying else "off")]


@command("give", "give <item> [count]", "put items in the inventory (a block or item name, up to 64)")
def cmd_give(console, args):
    if not args:
        raise CommandError("usage: give <item> [count]")
    item = block_id(args[0])
    count = item_count(args)
    left = console.game.inventory.add(item, count)
    given = count - left
    if left:
        return ["Gave %d %s, %d did not fit" % (given, VoxelGen.NAMES[item], left)]
    return ["Gave %d %s" % (given, VoxelGen.NAMES[item])]


@command("drop", "drop <item> [count]", "drop items on the ground at your feet (walk up to take them back)")
def cmd_drop(console, args):
    if not args:
        raise CommandError("usage: drop <item> [count]")
    item = block_id(args[0])
    count = item_count(args)
    game = console.game
    feet = tuple(game.pos)
    colour = VoxelGen.ITEM_COLOURS.get(item, (0.8, 0.8, 0.8))
    for _ in range(count):
        game.drops.add(item, colour, feet)
    return ["Dropped %d %s" % (count, VoxelGen.NAMES[item])]


@command("clear", "clear inventory|drops|log", "empty the inventory, the items on the ground, or this log")
def cmd_clear(console, args):
    what = args[0].lower() if args else ""
    if what == "inventory":
        console.game.inventory.load_list([])
        return ["Inventory cleared"]
    if what == "drops":
        console.game.drops.clear()
        return ["Items on the ground cleared"]
    if what == "log":
        console.log = []
        return []
    raise CommandError("usage: clear inventory|drops|log")


@command("spawn", "spawn creature [count] [biter]", "make creatures near you; a biter chases you and bites")
def cmd_spawn(console, args):
    if not args or args[0].lower() not in ("creature", "creatures"):
        raise CommandError("usage: spawn creature [count] [biter]")
    game = console.game
    if game.walkers is None:
        raise CommandError("the creatures are not made yet")
    rest = args[1:]
    biter = "biter" in [word.lower() for word in rest]
    numbers = [word for word in rest if word.lower() != "biter"]
    count = parse_int(numbers[0], "the count") if numbers else 1
    if not 1 <= count <= MAX_CREATURES:
        raise CommandError("the count must be from 1 to %d" % MAX_CREATURES)
    x, y, z = game.pos
    for _ in range(count):
        console.spawned += 1
        angle = console.rng.uniform(0.0, 2.0 * math.pi)
        spot = (x + 4.0 * math.cos(angle), y + 2.0, z + 4.0 * math.sin(angle))
        colour = VoxelWalkers.BITER_COLOUR if biter else console.rng.choice(VoxelWalkers.PALETTE)
        walker = VoxelWalkers.Walker(game.world, "creature_cmd_%d" % console.spawned, spot, colour, console.rng, biter=biter)
        game.walkers.walkers.append(walker)
    return ["Spawned %d %s" % (count, "biter" if biter else "creature") + ("s" if count != 1 else "")]


@command("kill", "kill creatures [all|near radius]", "remove creatures, all of them or those within a radius of you")
def cmd_kill(console, args):
    if not args or args[0].lower() not in ("creature", "creatures"):
        raise CommandError("usage: kill creatures [all|near radius]")
    game = console.game
    if game.walkers is None:
        raise CommandError("the creatures are not made yet")
    near = None
    if len(args) >= 3 and args[1].lower() == "near":
        near = parse_float(args[2], "the radius")
    elif len(args) >= 2 and args[1].lower() != "all":
        raise CommandError("usage: kill creatures [all|near radius]")
    removed = 0
    for walker in list(game.walkers.walkers):
        if near is not None:
            lo, hi = walker.box()
            centre = [(lo[i] + hi[i]) / 2 for i in range(3)]
            distance = sum((centre[i] - game.pos[i]) ** 2 for i in range(3)) ** 0.5
            if distance > near:
                continue
        if game.walker_target and game.walker_target[1] is walker:
            game.walker_target = None
        game.walkers.remove(walker)
        removed += 1
    return ["Removed %d creature%s" % (removed, "" if removed == 1 else "s")]


@command("destroy", "destroy [radius]", "remove the blocks within a radius (default 2) of your middle")
def cmd_destroy(console, args):
    radius = parse_float(args[0], "the radius") if args else 2.0
    if not 0.0 < radius <= MAX_DESTROY:
        raise CommandError("the radius must be from 0 to %d" % MAX_DESTROY)
    game = console.game
    cx, cy, cz = game.pos[0], game.pos[1] + 1.0, game.pos[2]
    reach = int(radius) + 1
    removed = 0
    for x in range(int(cx) - reach, int(cx) + reach + 1):
        for y in range(int(cy) - reach, int(cy) + reach + 1):
            for z in range(int(cz) - reach, int(cz) + reach + 1):
                if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 + (z + 0.5 - cz) ** 2 > radius * radius:
                    continue
                if game.world.get_block(x, y, z) != VoxelGen.AIR:
                    game.world.set_block(x, y, z, VoxelGen.AIR)
                    removed += 1
    return ["Destroyed %d block%s" % (removed, "" if removed == 1 else "s")]


@command("break", "break", "remove the block under the crosshair")
def cmd_break(console, args):
    target = console.game.target
    if target is None:
        raise CommandError("nothing in reach")
    x, y, z = target[0]
    name = VoxelGen.NAMES.get(console.game.world.get_block(x, y, z), "block")
    console.game.world.set_block(x, y, z, VoxelGen.AIR)
    return ["Broke the %s at %d %d %d" % (name, x, y, z)]


@command("setblock", "setblock x y z <block>", "set one block (air removes it)")
def cmd_setblock(console, args):
    if len(args) != 4:
        raise CommandError("usage: setblock x y z <block>")
    x, y, z = (parse_int(args[i], "the position") for i in range(3))
    block = block_id(args[3])
    console.game.world.set_block(x, y, z, block)
    return ["Set %d %d %d to %s" % (x, y, z, VoxelGen.NAMES.get(block, "air"))]


@command("fill", "fill x1 y1 z1 x2 y2 z2 <block>", "set a box of blocks (air removes them), up to %d" % MAX_FILL)
def cmd_fill(console, args):
    if len(args) != 7:
        raise CommandError("usage: fill x1 y1 z1 x2 y2 z2 <block>")
    lo = [min(parse_int(args[i], "the position"), parse_int(args[i + 3], "the position")) for i in range(3)]
    hi = [max(parse_int(args[i], "the position"), parse_int(args[i + 3], "the position")) for i in range(3)]
    volume = (hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1)
    if volume > MAX_FILL:
        raise CommandError("a fill is at most %d blocks, this one is %d" % (MAX_FILL, volume))
    block = block_id(args[6])
    world = console.game.world
    for x in range(lo[0], hi[0] + 1):
        for y in range(lo[1], hi[1] + 1):
            for z in range(lo[2], hi[2] + 1):
                world.set_block(x, y, z, block)
    return ["Filled %d blocks with %s" % (volume, VoxelGen.NAMES.get(block, "air"))]


@command("view", "view <chunks>", "set the view distance to the nearest step (6 to 60 chunks)")
def cmd_view(console, args):
    if not args:
        raise CommandError("usage: view <chunks>")
    chunks = console.game.set_view_distance(parse_int(args[0], "the view distance"))
    return ["View distance %d chunks" % chunks]


@command("fullscreen", "fullscreen [on|off]", "full screen (a borderless window the size of the desktop) or windowed; no argument toggles")
def cmd_fullscreen(console, args):
    game = console.game
    value = on_off(args[0] if args else None)
    if not game.set_fullscreen(not game.fullscreen if value is None else value):
        raise CommandError("full screen is not available here")
    return ["Window %s" % ("full screen" if game.fullscreen else "windowed")]


@command("save", "save", "save the edits and the inventory (F5 does the same)")
def cmd_save(console, args):
    console.game.save_world()
    return ["Saved"]


@command("load", "load", "load the saved edits and inventory (F9 does the same)")
def cmd_load(console, args):
    console.game.load_world()
    return ["Loaded"]


@command("stats", "stats", "print the world's chunk counts, quads, frame rate, and the creatures and items on the ground")
def cmd_stats(console, args):
    game = console.game
    world = game.world.stats()
    fps = 1.0 / game.frame_dt if game.frame_dt > 0 else 0.0
    creatures = len(game.walkers.walkers) if game.walkers is not None else 0
    return [
        "Chunks %d loaded, %d pending, %d dirty, %d meshing; %d quads; view %d" % (
            world["loaded"], world["pending"], world["dirty"], world["meshing"], world["quads"], game.view_radius),
        "%.0f frames a second; %d creatures; %d items on the ground; %d block edits" % (
            fps, creatures, len(game.drops.live), world["edits"]),
    ]


@command("seed", "seed", "print the world's seed")
def cmd_seed(console, args):
    return ["Seed %d" % console.game.seed]


@command("quit", "quit", "close the game")
def cmd_quit(console, args):
    cge.quit()
    return ["Closing"]


class Console:
    """The console's state: whether it is open, the line being typed, the log and the history of lines."""

    def __init__(self, game):
        self.game = game
        self.open = False
        self.line = ""
        self.log = []
        self.history = []
        self.history_at = None
        self.mouse_was_captured = False
        self.spawned = 0
        self.rng = random.Random()

    def open_console(self):
        game = self.game
        if game.paused or game.inventory_open:
            game.notify("Close the menu first")
            return
        self.mouse_was_captured = game.mouse_captured
        cge.set_mouse_captured(False)
        game.mouse_captured = False
        self.open = True
        self.line = ""
        self.history_at = None

    def close(self):
        self.open = False
        if self.mouse_was_captured:
            cge.set_mouse_captured(True)
            self.game.mouse_captured = True

    def update(self):
        """Once a frame: opens and closes the console on backtick, and while it is open, takes the typed characters and the
        Enter, Backspace, Up and Down keys. The typed characters are taken every frame, so none wait for the console."""
        game = self.game
        if game.pressed("ConsoleToggle", bool(cge.key_down("`"))):
            if self.open:
                self.close()
            else:
                self.open_console()
        typed = cge.typed_text()
        if not self.open:
            return
        if game.pressed("ConsoleEnter", bool(cge.key_down("Enter"))):
            self.submit()
        elif game.pressed("ConsoleBackspace", bool(cge.key_down("Backspace"))):
            self.line = self.line[:-1]
        elif game.pressed("ConsoleUp", bool(cge.key_down("Up"))):
            self.recall(-1)
        elif game.pressed("ConsoleDown", bool(cge.key_down("Down"))):
            self.recall(1)
        for ch in typed:
            if ch != "`" and " " <= ch <= "~":
                self.line += ch

    def recall(self, step):
        """Up goes back through the history, Down forward; past the newest line the input is empty."""
        if not self.history:
            return
        if self.history_at is None:
            self.history_at = len(self.history)
        self.history_at = max(0, min(len(self.history), self.history_at + step))
        self.line = self.history[self.history_at] if self.history_at < len(self.history) else ""

    def submit(self):
        text = self.line.strip()
        self.line = ""
        self.history_at = None
        if not text:
            return
        self.history.append(text)
        self.history = self.history[-MAX_HISTORY:]
        self.log.append("> " + text)
        self.log.extend(self.run(text))
        self.log = self.log[-LOG_LINES:]

    def run(self, text):
        """Runs one line and returns the lines it prints. A command that fails prints why, and the game goes on."""
        words = text.split()
        name, args = words[0].lower(), words[1:]
        if name not in COMMANDS:
            return ["no command called '%s' (help lists them)" % words[0]]
        function = COMMANDS[name][0]
        try:
            return function(self, args) or []
        except CommandError as error:
            return ["error: %s" % error]
        except Exception as error:   # a bug in a command must not stop the game
            return ["error: %s: %s" % (type(error).__name__, error)]

    def draw(self, box, layer):
        """Shows the log and the input line over the HUD while the console is open, and hides them otherwise. Each log line is
        its own label on a fixed pitch: the text body's own line spacing is tighter than the font, so lines in one body overlap."""
        game = self.game
        if not self.open:
            game.draw_rect("console_bg", box, (0, 0, 0, 0), layer=layer)
            for row in range(SHOWN_LINES):
                game.draw_text("console_line_%d" % row, "", (box[0] + 8, box[1] + 6 + row * LINE_PITCH), FONT_SIZE)
            game.draw_text("console_input", "", (box[0] + 8, box[1] + box[3] - 24), FONT_SIZE)
            return
        game.draw_rect("console_bg", box, (0, 0, 0, 185), layer=layer)
        shown = self.log[-SHOWN_LINES:]
        for row in range(SHOWN_LINES):
            text = shown[row] if row < len(shown) else ""
            game.draw_text("console_line_%d" % row, text, (box[0] + 8, box[1] + 6 + row * LINE_PITCH), FONT_SIZE,
                           (215, 215, 215, 255))
        game.draw_text("console_input", "> " + self.line + "_", (box[0] + 8, box[1] + box[3] - 24), FONT_SIZE,
                       (255, 230, 120, 255))
