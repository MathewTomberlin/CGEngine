#include "Core/Engine/Engine.h"
using namespace CGEngine;

// The voxel game as a program of its own (voxel_game.exe): it starts in the voxel scene, with no tilemap scene.
// CGENGINE_SCENE still overrides the scene, as it does for main.exe.
int main() {
    defaultScene = "scenes/voxels.json";
    beginWorld();
}
