#include "Scenes/Scene.h"
#include "Scripts/Script.h"
#include "World/WorldInstance.h"
#include "Body/SpriteAnimBody.h"
#include "Scripts/CommonScripts.h"

namespace CGEngine {
	class TilemapScene : public Scene {
    public:
        TilemapScene() { loadEvent = new Script(mainEvt); displayName = "TilemapScene"; }

        SpriteAnimBody* player = nullptr;
        id_t gridId = 0U;
        vector<int> tileTypes = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54 };
        int assignedType = 0;
        optional<Vector2f> clickedTilemapPos = nullopt;
        string tilemapDataPath = "mapData_.txt";
        Vector2f facing = { 0,1 };
        Vector2f lastMove = { 0,0 };

        ScriptEvent saveTilemapDataEvt = [](ScArgs args) { args.caller->get<Tilemap*>()->saveMapData(); };
        ScriptEvent tileClickEvt = [this](ScArgs args) {
            MouseReleaseInput* mouseEvt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
            Tilemap* tilemap = args.caller->get<Tilemap*>();
            if (mouseEvt == nullptr || tilemap == nullptr) return;

            //Convert the mouse click position to global space, then to tilemap local space, then as a flattened id
            clickedTilemapPos = args.caller->viewToLocal(mouseEvt->position);
            int tileId = tilemap->localPosToTileID(clickedTilemapPos.value());
            //Set assignedType on RMB or assign the tile type on LMB
            if (mouseEvt->button == Mouse::Button::Right) {
                assignedType = tilemap->query(tileId);
            }
            else {
                tilemap->assign(tileId, assignedType);
            }
        };
        ScriptEvent loadTilemapDataEvt = [this](ScArgs args) {
            vector<string> strings = args.script->getInput().getData<vector<string>>("args");
            Tilemap* tilemap = args.caller->get<Tilemap*>();
            if (strings.size() < 1 && tilemap == nullptr) return;

            tilemapDataPath = strings.at(0);
            tilemap->setMapData(tilemap->loadMapData(tilemapDataPath), true);
        };

        ScriptEvent toolboxTileClickEvt = [this](ScArgs args) {
            MouseReleaseInput* mouseEvt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
            Tilemap* tilemap = args.caller->get<Tilemap*>();
            if (mouseEvt == nullptr || tilemap == nullptr) return;

            //Convert the mouse click position to global space, then to tilemap local space, then as a flattened id
            V2f localPos = args.caller->viewToLocal(mouseEvt->position);
            int tileId = tilemap->localPosToTileID(localPos);
            //Determine tile type based on button press
            assignedType = tilemap->query(tileId);
        };

        ScriptEvent mainEvt = [this](ScArgs args) {
            ScriptEvent tilemapConstruction = [this](ScArgs args) {
                //Create the Body and setup its properties, including alignment and transform
                gridId = world->create(new Tilemap("tilemap.png", { 32,32 }, { 10,10 }, vector<int>(), tilemapDataPath), new Script([&](ScArgs args) {
                    //Basic body properties
                    args.caller->setName("map");
                    args.caller->moveToAlignment({ 0.5, 0.5 });
                    args.caller->setScale({ 1.5f,1.5f });

                    //Add OnDelete script to save map data on game end or tilemap deleted
                    args.caller->addDeleteScript(new Script(saveTilemapDataEvt));

                    //Add a left click listener and the tileClickScript script
                    args.caller->addOverlapMouseReleaseScript(new Script(tileClickEvt), Mouse::Button::Left);
                    //Add a right click listener and the tileClickScript
                    args.caller->addOverlapMouseReleaseScript(new Script(tileClickEvt), Mouse::Button::Right);

                    //Console scripts
                    //Add "LoadMapData" that expects 1 input string (the map data path)
                    args.caller->addScript("LoadMapData", new Script(loadTilemapDataEvt));
                    //Add "SaveMapData" that expects no input strings
                    args.caller->addScript("SaveMapData", new Script(saveTilemapDataEvt));
                    }));

                id_t toolboxId = world->create(new Tilemap("tilemap.png", { 32,32 }, { 8,7 }, tileTypes), new Script([&](ScArgs args) {
                    //Basic body properties
                    args.caller->setName("toolbox");
                    args.caller->setOrigin({ 0,0.5 });
                    args.caller->setScale({ 1.25f,1.25f });
                    args.caller->setBoundsRenderingEnabled(true);
                    args.caller->moveToAlignment({ 0,0.5 });

                    //Add a left click listener and the toolboxTileClickScript script
                    args.caller->addOverlapMouseReleaseScript(new Script(toolboxTileClickEvt), Mouse::Button::Left);
                    }));

                ScriptEvent clickUpdateScript = [this](ScArgs args) {
                    MouseReleaseInput* mouseEvt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
                    if (mouseEvt == nullptr || clickedTilemapPos == nullopt) return;

                    V2f mouseGlobalPos = world->viewToGlobal(mouseEvt->position);
                    Body* worldGrid = world->bodies.get(gridId);
                    if (worldGrid->contains(mouseGlobalPos)) {
                        SpriteAnimBody* body = new SpriteAnimBody("animation.png", AnimationParameters({ 32,32 }, 30.f), Transformation(), worldGrid);
                        body->setTimer(0.5, new Script([](ScArgs args) { world->deleteBody(args.caller); }));
                        Vector2f pos = (Vector2f)clickedTilemapPos.value();
                        int d = (int)(pos.x) % 32;
                        pos.x = pos.x - d;
                        d = (int)(pos.y) % 32;
                        pos.y = pos.y - d;
                        body->setPosition(pos * 1.5f);
                    }
                    };
                world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Left);
                world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Right);

                args.script->setOutput(map<string, any>({ {"gridId", gridId} }));
            };

            ScriptEvent playerConstruction = [this](ScArgs args) {
                id_t gridId = args.script->getInput().getData<id_t>("gridId");
                Body* gridBody = world->bodies.get(gridId);

                float playerSpeed = 100.f;
                float updateInterval = -1.f;
                //Create a new instance of the player input layout
                Script* keyboardMoveController = new Script(keyboardMovementController);
                Script* keyboardRotateController = new Script(keyboardRotationController);
                //Setup its input datastack with the desired input parameters
                keyboardMoveController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));
                keyboardRotateController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));

                //Texture* spriteTex = textures->get("triceratops.png");
                //IntRect spriteRect = IntRect({ 0,0 }, { 32,32 });
                //Body* player = new Body(new Sprite(*spriteTex, IntRect({ 0,0 }, { 1, 1 })), Transformation(), gridBody);
                //Behavior* animationBehavior = new Behavior(player);
                //animationBehavior->input = DataStack(stack<any>({ spriteRect }));
                //animationBehavior->scripts.addScript(onStartEvent, new Script([&animationBehavior](ScArgs args) {
                //    IntRect newRect = any_cast<IntRect>(animationBehavior->input.pullOut().value());
                //    args.caller->get<Sprite*>()->setTextureRect(newRect);
                //}));
                //player->addBehavior(animationBehavior);
                Body* player = new SpriteAnimBody("triceratops.png", AnimationParameters({ 32,32 }, 15.f, 1.0f, true, false, 7), Transformation(), gridBody);
                player->setName("player");
                player->zOrder = 10;
                player->addStartScript(keyboardMoveController);
                player->addStartScript(keyboardRotateController);
                player->addUpdateScript(new Script([this, &playerSpeed](ScArgs args) {
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
                player->addScript("OnTranslate", new Script([this](ScArgs args) {
                    if (((SpriteAnimBody*)args.caller)->getState() != AnimationState::Running) {
                        ((SpriteAnimBody*)args.caller)->start();
                    }
                    lastMove = args.script->getInput().getData<Vector2f>("evt");
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

                id_t rectId = world->create(new RectangleShape({ 32,32 }), Transformation(), gridBody, new Script([](ScArgs args) {
                    args.caller->setName("Rectangle");
                    args.caller->zOrder = 1;
                    args.caller->setIntersectEnabled(true);
                    args.caller->get()->setFillColor(Color(80, 80, 80, 0));
                    args.caller->translate({ 48,48 });
                    }));
            };

            Scene* tilemapScene = new Scene(tilemapConstruction);
            Scene* playerScene = new Scene(playerConstruction);

            world->addScene("tilemap", tilemapScene);
            world->addScene("player", playerScene);

            world->loadScene("tilemap");
            world->loadSceneWithData("player", tilemapScene->getOutput());
		};
	};
}