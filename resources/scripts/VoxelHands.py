"""
The first-person hands in the voxel sample: a right arm that holds the selected item, a left arm that reaches out to place a
block, a swing of the held item on each strike, and the effect a strike throws, which depends on the item.

The arms and the items are models (resources/models/viewmodel, made in Blender from the HumanFootman arms and the sword, bow,
spear, pickaxe and lantern). Each is a body drawn last (cge.set_viewmodel), placed each frame from the camera's own basis, so the
hands stay in view as the player turns and walks.

The frame: a model's +Y is up, its +Z is forward, and its +X is to the left of the view (the camera's right is forward x up, so a
model turned to the camera's yaw and pitch needs +X to the left to keep its handedness). A hand is placed at the eye plus its
offset in that basis, and its rotation is the camera's yaw and pitch.

Mixed into VoxelGame, which calls update_hands once a frame after the camera is placed (place_camera sets cam_origin).
"""
import json
import math

import cg_engine_bindings as cge
import VoxelGen

RIGHT_ARM = "models/viewmodel/viewarm_right.fbx"    # the arm on the right of the view, reaching toward the centre
LEFT_ARM = "models/viewmodel/viewarm_left.fbx"      # the arm on the left of the view
ARM_OFFSET = (0.30, -0.28, 0.15)                    # where each shoulder sits from the eye: right, up, forward (the left mirrors the right)
HOLD = (0.19, -0.15, 0.62)                          # in the arm's frame, where the held item's middle is (the wrist is near (0.19, -0.15, 0.45))
ITEM_MODELS = {
    VoxelGen.PICKAXE: "models/viewmodel/item_pickaxe.fbx",
    VoxelGen.IRON_PICKAXE: "models/viewmodel/item_pickaxe.fbx",
    VoxelGen.TORCH: "models/viewmodel/item_lantern.fbx",   # the carried torch is the lantern
}
# How far each item's swing turns the arm, in degrees, and how long the swing lasts. An empty hand swings like a fist.
SWING = {VoxelGen.PICKAXE: (70.0, 0.45), VoxelGen.IRON_PICKAXE: (70.0, 0.45), VoxelGen.TORCH: (12.0, 0.25)}
EMPTY_SWING = (35.0, 0.35)
REACH_DISTANCE = 0.22       # how far the left arm reaches toward the block it places, in blocks
REACH_SECONDS = 0.28        # how long a placement's reach lasts
# The effect a strike throws, by the item and by what it hit ("block" or "creature"): (effect, colour). A missing entry throws none.
HIT_EFFECTS = {
    VoxelGen.PICKAXE: {"block": ("sparks", (1.0, 0.85, 0.45)), "creature": ("sparks", (1.0, 0.5, 0.3))},
    VoxelGen.IRON_PICKAXE: {"block": ("sparks", (0.8, 0.9, 1.0)), "creature": ("sparks", (1.0, 0.5, 0.3))},
}
HELD = "hand_held"
LEFT = "hand_left"
RIGHT = "hand_right"


def angles(forward):
    """The yaw and pitch, in degrees, that turn a model's +Z to the view's forward direction (pitch is the tilt about X)."""
    fx, fy, fz = forward
    yaw = math.degrees(math.atan2(fx, fz))
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, -fy))))
    return pitch, yaw


def swing_amount(t, length):
    """How far a swing has gone, 0 at its start and end and 1 at the strike: a half sine over its length."""
    if t is None or t >= length:
        return 0.0
    return math.sin(math.pi * t / length)


def to_world(origin, basis, point):
    """A point in the arm's frame (X left, Y up, Z forward of the view) as a world point, from the eye origin and the camera's basis."""
    right, up, forward = basis
    x, y, z = point
    return [origin[i] - right[i] * x + up[i] * y + forward[i] * z for i in range(3)]


class HandsMixin:
    def hands_setup(self):
        """Make the two arms, once. The held item is made when the selected item needs one."""
        if getattr(self, "hands_ready", False):
            return
        cge.load_scene_json(json.dumps({"version": 1, "bodies": [
            {"name": RIGHT, "model": RIGHT_ARM, "position": [0.0, 0.0, 0.0], "rotation": [0, 0, 0]},
            {"name": LEFT, "model": LEFT_ARM, "position": [0.0, 0.0, 0.0], "rotation": [0, 0, 0]},
        ]}))
        for name in (RIGHT, LEFT):
            cge.set_viewmodel(name, True)
        self.hands_ready = True
        self.held_model = None
        self.swing_time = None
        self.swing_length = 0.0
        self.swing_turn = 0.0
        self.reach_time = None

    def set_held_model(self, model):
        """Show the held item as model (a path, or None for an empty hand). Its body is replaced when the item changes."""
        if self.held_model is not None:
            cge.remove_body(HELD)
        self.held_model = model
        if model is not None:
            cge.load_scene_json(json.dumps({"version": 1, "bodies": [
                {"name": HELD, "model": model, "position": [0.0, 0.0, 0.0], "rotation": [0, 0, 0]}]}))
            cge.set_viewmodel(HELD, True)

    def start_swing(self, item):
        """A strike: the held item (or the empty hand) swings for its length of time."""
        turn, length = SWING.get(item, EMPTY_SWING)
        self.swing_time = 0.0
        self.swing_length = length
        self.swing_turn = turn

    def reach(self):
        """A block was placed: the left arm reaches out to it and draws back."""
        self.reach_time = 0.0

    def hands_hit(self, kind, centre, colour=(1.0, 1.0, 1.0)):
        """A strike landed on a block or a creature at centre: the held item's effect for that kind of target, if it has one."""
        entry = HIT_EFFECTS.get(self.selected(), {}).get(kind)
        if entry is None:
            return
        effect, hit_colour = entry
        if effect == "sparks":
            self.bursts.add(centre, hit_colour, 6, 0.05, 0.25)

    def update_hands(self, dt):
        """Place the arms and the held item for this frame, and run the swing and the reach."""
        self.hands_setup()
        item = self.selected()
        model = ITEM_MODELS.get(item)
        if model != self.held_model:
            self.set_held_model(model)
        if self.pressed("HandsStrike", bool(self.mouse_captured and cge.mouse_down("Left"))):
            self.start_swing(item)
        if self.swing_time is not None:
            self.swing_time += dt
            if self.swing_time >= self.swing_length:
                self.swing_time = None
        if self.reach_time is not None:
            self.reach_time += dt
            if self.reach_time >= REACH_SECONDS:
                self.reach_time = None

        basis = cge.camera_basis()
        forward = basis["forward"]
        frame = (basis["right"], basis["up"], basis["forward"])
        pitch, yaw = angles(forward)
        origin = self.cam_origin
        swung = swing_amount(self.swing_time, self.swing_length) * self.swing_turn
        reached = swing_amount(self.reach_time, REACH_SECONDS) if self.reach_time is not None else 0.0

        right_shoulder = to_world(origin, frame, (-ARM_OFFSET[0], ARM_OFFSET[1], ARM_OFFSET[2]))
        left_shoulder = to_world(origin, frame, (ARM_OFFSET[0], ARM_OFFSET[1], ARM_OFFSET[2]))
        left_shoulder = [left_shoulder[i] + frame[2][i] * reached * REACH_DISTANCE for i in range(3)]
        right_rotation = (pitch + swung, yaw, 0.0)
        left_rotation = (pitch - 15.0 * reached, yaw, 0.0)
        self.place_hand(RIGHT, right_shoulder, right_rotation)
        self.place_hand(LEFT, left_shoulder, left_rotation)
        if self.held_model is not None:
            held = to_world(right_shoulder, frame, HOLD)
            self.place_hand(HELD, held, right_rotation)

    def place_hand(self, name, position, rotation):
        """Move a hand's body to a world position, turned to the given (pitch, yaw, roll) in degrees."""
        body = cge.find_body(name)
        if body is None:
            return
        mesh = body.get_mesh()
        mesh.set_position(cge.Vector3f(*position))
        mesh.set_rotation(cge.Vector3f(*rotation))
