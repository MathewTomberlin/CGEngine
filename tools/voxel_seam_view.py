"""
Puts the player where a level-of-detail seam of the voxel sample is in view, for tools/voxel_seam_shot.ps1 to screenshot. Run
inside the game through the control channel (run_script), with the settings in the environment:

  SEAM_CHUNKS  the seam: the chunk ring the player is moved out of, so the boundary is at ring SEAM_CHUNKS + 1. The near skin
               ends at ring 3 (VOXEL_NEAR_RADIUS) and the far skins (stride 2) begin at ring 12 (VOXEL_FAR_RADIUS), so 3 and 12
               are the two seams.
  SEAM_PITCH   the camera's pitch in degrees (negative looks down).
  SEAM_YAW     the camera's yaw in degrees (0 looks down -z; 90 looks along +x).

The player stands 14 blocks short of the boundary, on the ground at the spawn's z, and looks across it.
"""
import gc
import math
import os

import cg_engine_bindings as cge
import VoxelGen

game = [o for o in gc.get_objects() if type(o).__name__ == "VoxelGame"][0]
SEAM = int(os.environ.get("SEAM_CHUNKS", "3"))
PITCH = float(os.environ.get("SEAM_PITCH", "-18"))
YAW = float(os.environ.get("SEAM_YAW", "90"))
plane = (math.floor(game.spawn_pos[0] / 16) + SEAM + 1) * 16
x = plane - 14.0
z = game.spawn_pos[2]
y = game.ground_under(x, VoxelGen.HEIGHT, z)
game.teleport(x, y + 1.0, z)
game.yaw = YAW
game.pitch = PITCH
print("SEAM ring %d, boundary at x %d, player at %.1f %.1f %.1f" % (SEAM, plane, x, y + 1.0, z))
