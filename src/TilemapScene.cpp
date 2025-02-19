#include "Core/Scripts/Script.h"
#include "Core/Engine/Engine.h"
#include "Standard/Scripts/CommonScripts.h"
#include "Standard/Behaviors/AnimationBehavior.h"
#include "Standard/Behaviors/BoundsBehavior.h"
#include "Standard/Drawables/Tilemap.h"
#include "Standard/Meshes/CommonVArrays.h"

namespace CGEngine {
	class TilemapScene : public Behavior {
    public:
        TilemapScene():Behavior(nullptr) { 
            addScript(onLoadEvent, new Script(mainEvt));
        }

        vector<int> tileTypes = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54 };
        int assignedType = 0;
        optional<Vector2f> clickedTilemapPos = nullopt;
        string tilemapDataPath = "mapData_.txt";

        ScriptEvent saveTilemapDataEvt = [](ScArgs args) { args.caller->get<Tilemap*>()->saveMapData(); };
        ScriptEvent tileClickEvt = [this](ScArgs args) {
            MouseReleaseInput* mouseEvt = args.script->getInputDataPtr<MouseReleaseInput>("evt");
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
        };
        ScriptEvent loadTilemapDataEvt = [this](ScArgs args) {
            vector<string> strings = args.script->getInputData<vector<string>>("args");
            Tilemap* tilemap = args.caller->get<Tilemap*>();
            if (strings.size() < 1 && tilemap == nullptr) return;

            tilemapDataPath = strings.at(0);
            tilemap->setMapData(tilemap->loadMapData(tilemapDataPath), true);
        };

        ScriptEvent toolboxTileClickEvt = [this](ScArgs args) {
            MouseReleaseInput* mouseEvt = args.script->getInputDataPtr<MouseReleaseInput>("evt");
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
                id_t gridId = world->create(new Tilemap("tilemap.png", { 32,32 }, { 10,10 }, vector<int>(), tilemapDataPath), new Script([&](ScArgs args) {
                    //Create the BoundsBehavior with custom getLocalBounds and getGlobalBounds to call the Tilemap bounds functions
                    //NOTE: Do this before applying moveToAlignment to avoid out-of-order errors
                    new BoundsBehavior(args.caller, 
                    [](const Body* body) {
                        return dynamic_cast<Tilemap*>(body->get())->getGlobalBounds();
                    }, 
                    [](const Body* body) {
                        return dynamic_cast<Tilemap*>(body->get())->getLocalBounds();
                    });
                    //Basic body properties
                    args.caller->setName("map");
                    args.caller->moveToAlignment({ 0.5, 0 });
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
                    //Create the BoundsBehavior with custom getLocalBounds and getGlobalBounds to call the Tilemap bounds functions
                    //NOTE: Do this before applying moveToAlignment to avoid out-of-order errors
                    new BoundsBehavior(args.caller,
                    [](const Body* body) {
                        return dynamic_cast<Tilemap*>(body->get())->getGlobalBounds();
                    },
                    [](const Body* body) {
                        return dynamic_cast<Tilemap*>(body->get())->getLocalBounds();
                    });
                    //Basic body properties
                    args.caller->setName("toolbox");
                    args.caller->setOrigin({ 0,0.5 });
                    args.caller->setScale({ 1.25f,1.25f });
                    args.caller->setBoundsRenderingEnabled(true);
                    args.caller->moveToAlignment({ 0,0.5 });

                    //Add a left click listener and the toolboxTileClickScript script
                    args.caller->addOverlapMouseReleaseScript(new Script(toolboxTileClickEvt), Mouse::Button::Left);
                }));

                //ScriptEvent clickUpdateScript = [this](ScArgs args) {
                //    MouseReleaseInput* mouseEvt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
                //    if (mouseEvt == nullptr || clickedTilemapPos == nullopt) return;
                //
                //    V2f mouseGlobalPos = screen->viewToGlobal(mouseEvt->position);
                //    Body* worldGrid = world->bodies.get(gridId);
                //    if (worldGrid->contains(mouseGlobalPos)) {
                //        SpriteAnimBody* body = new SpriteAnimBody("animation.png", AnimationParameters({ 32,32 }, 30.f), Transformation(), worldGrid);
                //        body->setTimer(0.5, new Script([](ScArgs args) { world->deleteBody(args.caller); }));
                //        Vector2f pos = (Vector2f)clickedTilemapPos.value();
                //        int d = (int)(pos.x) % 32;
                //        pos.x = pos.x - d;
                //        d = (int)(pos.y) % 32;
                //        pos.y = pos.y - d;
                //        body->setPosition(pos * 1.5f);
                //    }
                //    };
                //world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Left);
                //world->getRoot()->addMouseReleaseScript(clickUpdateScript, Mouse::Button::Right);

                args.behavior->setOutput(map<string, any>({ {"gridId", gridId} }));
            };

            ScriptEvent playerConstruction = [this](ScArgs args) {
                //Get the id of the grid body output by tilemapConstruction
                id_t gridId = args.behavior->getInput().getData<id_t>("gridId");
                //Get a reference to the grid Body so we can parent the player to it
                Body* gridBody = world->bodies.get(gridId);
                //Player movement properties
                float playerSpeed = 100.f;
                //float updateInterval = -1.f; -- UNUSED
                //Create a new instance of the player input layout and set their Input DataMaps with the playerSpeed
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
                //Add an AnimationBehavior to the player and get the new Behavior's ID
                AnimationParameters playerAnimParams = AnimationParameters();
                playerAnimParams.startRunning = false;
                id_t animBehaviorId = (new AnimationBehavior(player, playerAnimParams))->getId();

                //Add KeyRelease scripts that call the "endAnimation" domain on the animationBehavior via its id
                player->addKeyReleaseScript([](ScArgs args) {
                    args.behavior->callDomain("endAnimation");
                }, Keyboard::Scan::W, animBehaviorId);
                player->addKeyReleaseScript([](ScArgs args) {
                    args.behavior->callDomain("endAnimation");
                }, Keyboard::Scan::S, animBehaviorId);
                player->addKeyReleaseScript([](ScArgs args) {
                    args.behavior->callDomain("endAnimation");
                }, Keyboard::Scan::A, animBehaviorId);
                player->addKeyReleaseScript([](ScArgs args) {
                    args.behavior->callDomain("endAnimation");
                }, Keyboard::Scan::D, animBehaviorId);

                id_t rectId = world->create(new RectangleShape({ 32,32 }), Transformation(), gridBody, new Script([](ScArgs args) {
                    args.caller->setName("Rectangle");
                    args.caller->zOrder = 1;
                    args.caller->setIntersectEnabled(true);
                    args.caller->get()->setFillColor(Color(80, 80, 80, 0));
                    args.caller->translate({ 48,48 });
                }));
            };

            ScriptEvent meshConstruction = [](ScArgs args) {
                //Get the id of the grid body output by tilemapConstruction
                id_t gridId = args.behavior->getInput().getData<id_t>("gridId");
                //Get a reference to the grid Body so we can parent the player to it
                Body* gridBody = world->bodies.get(gridId);

                id_t meshId2 = world->create(new Mesh(getCubeVertices(0.066f), textures->get("animation.png"), 0.000000001f), gridBody);
                Body* body2 = world->bodies.get(meshId2);
                body2->translate({ 72,72});
                body2->get<Mesh*>()->height = 0.000000001f;

                id_t planeId = world->create(new Mesh(getCubeVertices(0.066f), textures->get("image.png"), 0.00000000102f), gridBody);
                Body* planeBody = world->bodies.get(planeId);
                planeBody->translate({ 72,16 });
                planeBody->get<Mesh*>()->height = 0.0f;
                
                id_t planeId2 = world->create(new Mesh(getCubeVertices(0.066f), textures->get("image.png"), 0.00000000102f), gridBody);
                Body* planeBody2 = world->bodies.get(planeId2);
                planeBody2->translate({ 72,118 });
                planeBody2->get<Mesh*>()->height = 0.0f;
                
                id_t planeId3 = world->create(new Mesh(getCubeVertices(0.066f), textures->get("image.png"), 0.00000000102f), gridBody);
                Body* planeBody3 = world->bodies.get(planeId3);
                planeBody3->translate({ 124,72 });
                planeBody3->get<Mesh*>()->height = 0.0f;
                
                id_t planeId4 = world->create(new Mesh(getCubeVertices(0.066f), textures->get("image.png"), 0.00000000102f), gridBody);
                Body* planeBody4 = world->bodies.get(planeId4);
                planeBody4->translate({ 24,72 });
                planeBody4->get<Mesh*>()->height = 0.0f;
            };

            Behavior* tilemapScene = new Behavior(nullptr); 
            tilemapScene->addScript(onLoadEvent,new Script(tilemapConstruction));

            Behavior* playerScene = new Behavior(nullptr); 
            playerScene->addScript(onLoadEvent, new Script(playerConstruction));

            Behavior* meshScene = new Behavior(nullptr);
            meshScene->addScript(onLoadEvent, new Script(meshConstruction));

            world->addScene("tilemap", tilemapScene);
            world->addScene("player", playerScene);
            world->addScene("meshes", meshScene);

            world->loadScene("tilemap");
            world->loadSceneWithInput("player", tilemapScene->getOutput());
            world->loadSceneWithInput("meshes", tilemapScene->getOutput());
		};
	};
}