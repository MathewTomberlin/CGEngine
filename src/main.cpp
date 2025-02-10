#include "World/WorldInstance.h"
#include "Scripts/CommonScripts.h"
#include "Body/ButtonBody.h"
#include "Body/SpriteAnimBody.h"
#include <iostream>
#include <fstream>

using namespace CGEngine;

int main() {
    beginWorld();

    Body* worldGrid = nullptr;
    vector<int> tileTypes = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54 };
    int assignedType = 0;
    optional<Vector2f> clickedTilemapPos = nullopt;
    string tilemapDataPath = "mapData_.txt";

    ScriptEvent saveEvt = [](ScArgs args) { args.caller->get<Tilemap*>()->saveMapData(); };
    
    //Create the Body and setup its properties, including alignment and transform
    worldGrid = new Body(new Tilemap("tilemap.png", { 32,32 }, { 10,10 }, vector<int>(), tilemapDataPath));
    worldGrid->setName("map");
    worldGrid->move({ 400,400 });
    worldGrid->moveToAlignment({ 0.5, 0.5 });
    worldGrid->setScale({ 1.5f,1.5f });
    
    //Add OnDelete script to save map data
    worldGrid->addDeleteScript(new Script(saveEvt));
    
    //Add OnClick (Left and Right) scripts to assign tile type
    Script* tileClickScript = new Script([&clickedTilemapPos, &assignedType](ScArgs args) {
        MouseReleaseInput* mouseEvt = args.script->pullOutInputPtr<MouseReleaseInput>();
        Tilemap* tilemap = args.caller->get<Tilemap*>();
        if (mouseEvt == nullptr || tilemap == nullptr) return;
    
        //Convert the mouse click position to global space, then to tilemap local space, then as a flattened id
        clickedTilemapPos = args.caller->viewToLocal(mouseEvt->position);
        int tileId = tilemap->localPosToTileID(clickedTilemapPos.value());
        //Set assignedType on RMB or assign the tile type on LMB
        if (mouseEvt->button == Mouse::Button::Right) {
            assignedType = tilemap->query(tileId);
        } else {
            tilemap->assign(tileId, assignedType);
        }
    });
    //Add a left click listener and the tileClickScript script
    worldGrid->addOverlapMouseReleaseScript(tileClickScript, Mouse::Button::Left);
    //Add a right click listener and the tileClickScript
    worldGrid->addOverlapMouseReleaseScript(tileClickScript, Mouse::Button::Right);
    
    //Add "LoadMapData" domain script that expects 1 input string (the map data path)
    //This script is meant to be called from the console
    worldGrid->addScript("LoadMapData", new Script([&tilemapDataPath](ScArgs args) {
        vector<string> strings = args.script->pullOutInput<vector<string>>();
        Tilemap* tilemap = args.caller->get<Tilemap*>();
        if (strings.size() < 1 && tilemap == nullptr) return;
        
        tilemapDataPath = strings.at(0);
        tilemap->setMapData(tilemap->loadMapData(tilemapDataPath), true);
    }));
    
    //Add "SaveMapData" domain script
    //This script is meant to be called from the
    worldGrid->addScript("SaveMapData", new Script(saveEvt));
    
    Body* tileTypeToolbox = new Body(new Tilemap("tilemap.png", { 32,32 }, { 8,7 }, tileTypes));
    tileTypeToolbox->setName("toolbox");
    tileTypeToolbox->setOrigin({ 0,0.5 });
    tileTypeToolbox->setScale({ 1.25f,1.25f });
    tileTypeToolbox->setBoundsRenderingEnabled(true);
    tileTypeToolbox->moveToAlignment({ 0,0.5 });
    
    Script* toolboxTileClickScript = new Script([&](ScArgs args) {
        MouseReleaseInput* mouseEvt = args.script->pullOutInputPtr<MouseReleaseInput>();
        Tilemap* tilemap = args.caller->get<Tilemap*>();
        if (mouseEvt == nullptr || tilemap == nullptr) return;
    
        //Convert the mouse click position to global space, then to tilemap local space, then as a flattened id
        V2f localPos = args.caller->viewToLocal(mouseEvt->position);
        int tileId = tilemap->localPosToTileID(localPos);
        //Determine tile type based on button press
        assignedType = tilemap->query(tileId);
    });
    
    //Add a left click listener and the toolboxTileClickScript script
    tileTypeToolbox->addOverlapMouseReleaseScript(toolboxTileClickScript, Mouse::Button::Left);
    
    ScriptEvent clickUpdateScript = [&clickedTilemapPos, &worldGrid](ScArgs args) {
        MouseReleaseInput* mouseEvt = args.script->pullOutInputPtr<MouseReleaseInput>();
        if (mouseEvt == nullptr || clickedTilemapPos == nullopt) return;
    
        V2f mouseGlobalPos = world->viewToGlobal(mouseEvt->position);
        if (worldGrid->contains(mouseGlobalPos)) {
            SpriteAnimBody* body = new SpriteAnimBody("animation.png", AnimationParameters({ 32,32 }, 30.f), Transformation(), worldGrid);
            body->setTimer(0.5, new Script([](ScArgs args) { delete args.caller; }));
            Vector2f pos = (Vector2f)clickedTilemapPos.value();
            int d = (int)(pos.x) % 32;
            pos.x = pos.x - d;
            d = (int)(pos.y) % 32;
            pos.y = pos.y - d;
            body->setPosition(pos*1.5f);
        }
    };
    world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Left);
    world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Right);
    
    float playerSpeed = 100.f;
    float updateInterval = -1.f;
    //Create a new instance of the player input layout
    Script* keyboardMoveController = new Script(keyboardMovementController);
    Script* keyboardRotateController = new Script(keyboardRotationController);
    //Setup its input datastack with the desired input parameters
    keyboardMoveController->setInput(DataStack(stack<any>({ playerSpeed})));
    keyboardRotateController->setInput(DataStack(stack<any>({ playerSpeed })));
    
    Vector2f facing = { 0,1 };
    Vector2f lastMove = { 0,0 };
    SpriteAnimBody* player = new SpriteAnimBody("triceratops.png", AnimationParameters({ 32,32 }, 15.f,1.0f,true,false,7), Transformation(), worldGrid);
    player->setName("player");
    player->zOrder = 10;
    player->addStartScript(keyboardMoveController);
    player->addStartScript(keyboardRotateController);
    player->addUpdateScript(new Script([&lastMove, &facing,&playerSpeed](ScArgs args) {
        if (facing != lastMove && lastMove != V2f({ 0,0 })) {
            if (lastMove == Vector2f({ 0, 1 })) {
                ((SpriteAnimBody*)args.caller)->setAnimationStartPos({ 0,0 });
            }
            else if (lastMove == Vector2f({ 0, -1 })) {
                ((SpriteAnimBody*)args.caller)->setAnimationStartPos({ 0,96 });
            }
            else if (lastMove == Vector2f({ 1, 0 })) {
                ((SpriteAnimBody*)args.caller)->setAnimationStartPos({ 0,32 });
            }
            else {
                ((SpriteAnimBody*)args.caller)->setAnimationStartPos({ 0,64 });
            }
            facing = lastMove;
        }
        lastMove = { 0,0 };
    }));
    player->addScript("OnTranslate", new Script([&facing, &lastMove](ScArgs args) {
        if (((SpriteAnimBody*)args.caller)->getState() != AnimationState::Running) {
            ((SpriteAnimBody*)args.caller)->start();
        }
        lastMove = args.script->pullOutInput<Vector2f>();
    }));
    
    player->addKeyReleaseScript([](ScArgs args) {
        ((SpriteAnimBody*)args.caller)->end();
    }, Keyboard::Scan::W);
    player->addKeyReleaseScript([](ScArgs args) {
        ((SpriteAnimBody*)args.caller)->end();
    }, Keyboard::Scan::S);
    player->addKeyReleaseScript([](ScArgs args) {
        ((SpriteAnimBody*)args.caller)->end();
    }, Keyboard::Scan::A);
    player->addKeyReleaseScript([](ScArgs args) {
        ((SpriteAnimBody*)args.caller)->end();
    }, Keyboard::Scan::D);
    
    Body* rectObj = new Body(new RectangleShape({ 32,32 }), Transformation(), worldGrid);
    rectObj->setName("Rectangle");
    rectObj->zOrder = 1;
    rectObj->setIntersectEnabled(true);
    rectObj->get()->setFillColor(Color(80, 80, 80,0));
    rectObj->translate({ 48,48 });

    //The world steps through the Body hierarchy from the world root and renders each body
    updateWorld();
}