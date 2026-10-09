#include "Core/Engine/Engine.h"
#include "TilemapScene.cpp"
using namespace CGEngine;

int main() {
    sceneList.push_back(new TilemapScene());
    beginWorld();
}