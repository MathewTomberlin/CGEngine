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

                    V2f mouseGlobalPos = screen->viewToGlobal(mouseEvt->position);
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
                //Get the id of the grid body output by tilemapConstruction
                id_t gridId = args.script->getInput().getData<id_t>("gridId");
                Body* gridBody = world->bodies.get(gridId);

                float playerSpeed = 100.f;
                //float updateInterval = -1.f; -- UNUSED
                
                //Create a new instance of the player input layout
                Script* keyboardMoveController = new Script(keyboardMovementController);
                Script* keyboardRotateController = new Script(keyboardRotationController);
                //Setup its input datastack with the desired input parameters
                keyboardMoveController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));
                keyboardRotateController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));

                //Create a body with a Sprite
                Texture* spriteTex = textures->get("triceratops.png");
                IntRect spriteRect = IntRect({ 0,0 }, { 32,32 });
                Body* player = new Body(new Sprite(*spriteTex, spriteRect), gridBody);
                player->setName("player");
                player->zOrder = 10;
                //Add the instances of the Move and Rotate Controllers
                player->addStartScript(keyboardMoveController);
                player->addStartScript(keyboardRotateController);

                //Declare the input (unmodified) and data (modified) parameters of the animation behavior
                Behavior* animationBehavior = new Behavior(player);
                animationBehavior->input = DataMap(map<string, any>({
                    {"maxFrameRate",15.0f},
                    {"speed",1.0f},
                    {"maxFrame",7},
                    {"looping",true},
                    {"startRunning",false},
                    {"sheetSize",{spriteTex->getSize()}}
                }));
                animationBehavior->data = DataMap(map<string, any>({
                    {"rect",spriteRect},
                    {"frame",0},
                    {"state",AnimationState::Ready},
                    {"frameTime",0.f},
                    {"animationStartPos",Vector2i({0,0})}
                }));

                //When the Behavior is started (when the Body is started)
                animationBehavior->scripts.addScript(onStartEvent, new Script([](ScArgs args) {
                    IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
                    bool startRunning = args.behavior->input.getData<bool>("startRunning");

                    args.caller->get<Sprite*>()->setTextureRect(spriteRect);
                    if (startRunning) {
                        args.behavior->scripts.callDomain("startAnimation",args.behavior);
                    }
                }));

                //Animation scripts
                animationBehavior->scripts.addScript("calculateAnimLength", new Script([](ScArgs args) {
                    float maxFrameRate = args.behavior->input.getData<float>("maxFrameRate");
                    float speed = args.behavior->input.getData<float>("speed");
                    float frameRate = maxFrameRate * abs(speed);
                    float frameLength = 1.f / frameRate;
                    args.behavior->data.addData("frameLength", frameLength);
                }));

                animationBehavior->scripts.addScript("pauseAnimation", new Script([](ScArgs args) {
                    id_t animationUpdateScriptId = args.behavior->data.getData<id_t>("animUpdateId");
                    args.behavior->data.addData("state", AnimationState::Paused);
                    args.behavior->scripts.removeScript(onUpdateEvent, animationUpdateScriptId,true);
                }));

                animationBehavior->scripts.addScript("endAnimation", new Script([](ScArgs args) {
                    args.behavior->data.addData("frameTime", 0.f);

                    Vector2i startingPos = args.behavior->data.getData<Vector2i>("animationStartPos");
                    IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
                    spriteRect.position = startingPos;
                    args.behavior->data.addData("rect", spriteRect);
                    args.caller->get<Sprite*>()->setTextureRect(spriteRect);

                    args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
                    args.behavior->data.addData("state", AnimationState::Ready);

                    args.behavior->data.addData("frame", 0);
                }));

                animationBehavior->scripts.addScript("startAnimation", new Script([](ScArgs args) {
                    args.behavior->scripts.callDomain("calculateAnimLength", args.behavior);
                    args.behavior->data.addData("state", AnimationState::Running);
                    
                    id_t animationUpdateScriptId = args.behavior->scripts.addScript(onUpdateEvent, new Script([](ScArgs args) {
                        args.behavior->scripts.callDomain("animate", args.behavior);
                    }));
                    args.behavior->data.addData("animUpdateId", animationUpdateScriptId);
                }));

                animationBehavior->scripts.addScript("animate", new Script([](ScArgs args) {
                    int frame = args.behavior->data.getData<int>("frame");
                    IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
                    float frameLength = args.behavior->data.getData<float>("frameLength");
                    float state = args.behavior->data.getData<AnimationState>("state");

                    bool looping = args.behavior->input.getData<bool>("looping");
                    bool startRunning = args.behavior->input.getData<bool>("startRunning");
                    Vector2u spriteSheetSize = args.behavior->input.getData<Vector2u>("sheetSize");
                    Vector2i animationStartPos = args.behavior->data.getData<Vector2i>("animationStartPos");
                    int maxFrame = args.behavior->input.getData<int>("maxFrame");

                    if (frameLength <= 0) return;
                    float frameTime = args.behavior->data.getData<float>("frameTime") + time.getDeltaSec();
                    if (frameTime > frameLength) {
                        while ((frameTime -= frameLength) >= 0.f) {
                            if (maxFrame > 0 && frame >= maxFrame) {
                                if (looping) {
                                    frame = 0;
                                    spriteRect.position = animationStartPos;
                                } else {
                                    args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
                                    break;
                                }
                            }

                            //Advance frame rect
                            if (maxFrame < 0 || frame <= (unsigned)maxFrame) {
                                if ((unsigned)(spriteRect.position.x + spriteRect.size.x) < spriteSheetSize.x) {
                                    spriteRect.position.x += spriteRect.size.x;
                                    frame++;
                                }
                                else {
                                    if ((unsigned)(spriteRect.position.y + spriteRect.size.y) < spriteSheetSize.y) {
                                        spriteRect.position.y += spriteRect.size.y;
                                        spriteRect.position.x = 0;
                                        frame++;
                                    }
                                    else {
                                        if (looping) {
                                            spriteRect.position = animationStartPos;
                                            frame++;
                                        }
                                        else {
                                            args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        args.caller->get<Sprite*>()->setTextureRect(spriteRect);
                        args.behavior->data.addData("frame", frame);
                        args.behavior->data.addData("rect", spriteRect);
                    }
                    args.behavior->data.addData("frameTime", frameTime);
                }));

                animationBehavior->scripts.addScript(onUpdateEvent, new Script([&playerSpeed](ScArgs args) {
                    Vector2f facing = args.behavior->data.getData<Vector2f>("facing");
                    Vector2f lastMove = args.behavior->data.getData<Vector2f>("lastMove");

                    if (facing != lastMove && lastMove != V2f({ 0,0 })) {
                        if (lastMove == Vector2f({ 0, 1 })) {
                            args.behavior->data.addData("animationStartPos", Vector2i({ 0,0 }));
                        } else if (lastMove == Vector2f({ 0, -1 })) {
                            args.behavior->data.addData("animationStartPos", Vector2i({ 0,96 }));
                        } else if (lastMove == Vector2f({ 1, 0 })) {
                            args.behavior->data.addData("animationStartPos", Vector2i({ 0,32 }));
                        } else {
                            args.behavior->data.addData("animationStartPos", Vector2i({ 0,64 }));
                        }

                        facing = lastMove;
                        args.behavior->data.addData("facing", facing);
                        args.behavior->scripts.callDomain("endAnimation", args.behavior);
                    }
                    lastMove = { 0,0 };
                    args.behavior->data.addData("lastMove", lastMove);
                }));

                animationBehavior->scripts.addScript("OnTranslate", new Script([this](ScArgs args) {
                    AnimationState animState = args.behavior->data.getData<AnimationState>("state");
                    if (animState != AnimationState::Running) {
                        args.behavior->scripts.callDomain("startAnimation", args.behavior);
                    }
                    Vector2f lastMove = args.script->getInput().getData<Vector2f>("evt");
                    args.behavior->data.addData("lastMove", lastMove);
                }));

                //Finally, add the animationBehavior to the Body and get the id
                id_t animBehaviorId = player->addBehavior(animationBehavior);

                //Add KeyRelease scripts that call the "endAnimation" domain on the animationBehavior via its id
                player->addKeyReleaseScript([](ScArgs args) {
                    args.behavior->scripts.callDomain("endAnimation", args.behavior);
                }, Keyboard::Scan::W, animBehaviorId);
                player->addKeyReleaseScript([&animationBehavior](ScArgs args) {
                    args.behavior->scripts.callDomain("endAnimation", args.behavior);
                }, Keyboard::Scan::S, animBehaviorId);
                player->addKeyReleaseScript([&animationBehavior](ScArgs args) {
                    args.behavior->scripts.callDomain("endAnimation", args.behavior);
                }, Keyboard::Scan::A, animBehaviorId);
                player->addKeyReleaseScript([&animationBehavior](ScArgs args) {
                    args.behavior->scripts.callDomain("endAnimation", args.behavior);
                }, Keyboard::Scan::D, animBehaviorId);

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