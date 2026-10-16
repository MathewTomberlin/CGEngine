"""
The night's rules for the voxel sample's creatures. The night share is 0 at noon and 1 at midnight, from the sun's height; a biter
comes after the player from further away the darker it is. Pure Python with no engine calls, so tools/test_voxel_night.py checks
it without a window.
"""

CHASE_RANGE = 7.0               # blocks: a biter this close to the player comes after it by day
NIGHT_CHASE_SHARE = 0.8         # the chase range grows by this share at full night (7 blocks become 12.6)
NIGHT_HEIGHT = 0.2              # the sun's height (-1 to 1) at which the night is complete; at the horizon it is half


def night_share(sun_height):
    """How dark it is, 0 to 1, from the sun's height (-1 at midnight, 0 at sunrise and sunset, 1 at noon)."""
    return min(1.0, max(0.0, -sun_height / NIGHT_HEIGHT))


def chase_range(night):
    """How close the player must be for a biter to chase it, for a night share of 0 to 1."""
    return CHASE_RANGE * (1.0 + NIGHT_CHASE_SHARE * min(1.0, max(0.0, night)))
