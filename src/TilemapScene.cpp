#include "Core/Scripts/Script.h"
#include "Core/Engine/Engine.h"
#include "Standard/Scripts/CommonScripts.h"
#include "Standard/Models/CommonModels.h"
#include "Standard/Behaviors/AnimationBehavior.h"
#include "Standard/Behaviors/BoundsBehavior.h"
#include "Standard/Drawables/Tilemap.h"
#include "Core/Light/Light.h"

namespace CGEngine {
	class TilemapScene : public Behavior {
    public:
        //Scenes require you to add an onLoadEvent with the main ScriptEvent to call
        TilemapScene():Behavior(nullptr) { 
            addScript(onLoadEvent, new Script(mainEvt));
        }

        //This is the ScriptEvent I have assigned to be called by onLoadEvent
        ScriptEvent mainEvt = [this](ScArgs args) {
            ScriptEvent playerConstruction = [this](ScArgs args) {
                //Player movement properties
                float playerSpeed = 5.f;
                //Create a new instance of the player input layout and set their Input DataMaps with the playerSpeed
                Script* keyboardMoveController = new Script(keyboardMovementController);
                Script* keyboardRotateController = new Script(keyboardRotationController);
                //Setup their input parameters
                keyboardMoveController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));
                keyboardRotateController->setInput(DataMap(map<string, any>({ {"speed",playerSpeed} })));

                //Create a body with a Sprite
                Texture* spriteTex = textures->get("triceratops.png");
                IntRect spriteRect = IntRect({ 0,0 }, { 32,32 });
                Body* player = new Body(new Sprite(*spriteTex, spriteRect)/*, gridBody*/);
                player->setName("player");
                player->zOrder = 10;
                player->setPosition({ screen->getCurrentView()->getSize().x / 2, screen->getCurrentView()->getSize().y / 2});
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
            };

            ScriptEvent meshConstruction = [](ScArgs args) {
                VertexModel cubeModel = getCubeModel(1.f);
                Vector3f cubeScale = { 1,1,0.0000000008f };

                VertexModel planeModel = getPlaneModel(1.f, 0);
                vector<vector<int>> tilemapData =  {{0,1,2,1,0,0,1,2,1,0,0,1,2,1,0,0,1,2,1,0},
                                                    {1,2,3,2,1,1,2,3,2,1,1,2,3,2,1,1,2,3,2,1},
                                                    {2,3,3,3,2,2,3,3,3,2,2,3,3,3,2,2,3,3,3,2},
                                                    {1,2,3,2,1,1,2,3,2,1,1,2,3,2,1,1,2,3,2,1},
                                                    {2,1,2,1,2,2,1,2,1,2,2,1,2,1,2,2,1,2,1,2}};
                VertexModel tilemapModel = getTilemapModel(1.f, { 20,5 }, tilemapData);

                //Spotlight
                LightParameters lightParams = LightParameters();
                lightParams.coneAngle = 60.f;
                lightParams.lightDirection = { 0,1,-1 };
                Light* light = new Light({ 0,-20,5 }, false, lightParams);

                //Red Point Light
                LightParameters lightParams2 = LightParameters();
                lightParams2.attenuation = 0.005f;
                lightParams2.colorIntensities = { 1,0.5f,0.5f };
                Light* light2 = new Light({ 0,10,10 }, false, lightParams2);

                //Materials
                SurfaceParameters maskedParams = SurfaceParameters(SurfaceDomain(), SurfaceDomain(32.0f), SurfaceDomain("mask_tile.png"));
                maskedParams.useLighting = false;
                id_t maskedMaterialId = world->createMaterial(maskedParams);
                Material* maskedMaterial = world->getMaterial(maskedMaterialId);

                SurfaceParameters brickParams = SurfaceParameters(SurfaceDomain("brick_tile.png"), SurfaceDomain(16.0f));
                id_t brickMaterialId = world->createMaterial(brickParams);
                Material* brickMaterial = world->getMaterial(brickMaterialId);

                SurfaceParameters lavaParams = SurfaceParameters(SurfaceDomain("lava_tile.png"));
                id_t lavaMaterialId = world->createMaterial(lavaParams);
                Material* lavaMaterial = world->getMaterial(lavaMaterialId);

                SurfaceParameters mudParams = SurfaceParameters(SurfaceDomain("grass_tile.png"), SurfaceDomain(128.0f) );
                mudParams.diffuseColor = Color(250, 80, 80);
                id_t mudMaterialId = world->createMaterial(mudParams);
                Material* mudMaterial = world->getMaterial(mudMaterialId);

                SurfaceParameters grassMaterialParams = SurfaceParameters(SurfaceDomain("grass_tile.png", { 10,10 }), SurfaceDomain(8.0f));
                id_t grassMaterialId = world->createMaterial(grassMaterialParams);
                Material* grassMaterial = world->getMaterial(grassMaterialId);
                //Set material diffuse texture to repeating
                Texture* grassTexture = grassMaterial->getParameterPtr<Texture>("diffuseTexture");
                grassTexture->setRepeated(true);

                //Bodies
                id_t planeMeshId = world->create(new Mesh(tilemapModel, Transformation3D({ -15,10,-12 }, cubeScale), { grassMaterial, lavaMaterial, mudMaterial, maskedMaterial }));
                id_t planeMeshId2 = world->create(new Mesh(tilemapModel, Transformation3D({ -15,10,-8 }, cubeScale), { grassMaterial, lavaMaterial, mudMaterial, maskedMaterial }));

                id_t meshId1 = world->create(new Mesh(cubeModel, Transformation3D({ 0,5,-10 }, cubeScale), {brickMaterial}));
                id_t meshId2 = world->create(new Mesh(cubeModel, Transformation3D({ 0,-5,-10 }, cubeScale), {brickMaterial}));
                id_t meshId4 = world->create(new Mesh(cubeModel, Transformation3D({ 5,0,-10 }, cubeScale), {brickMaterial}));
                id_t meshId5 = world->create(new Mesh(cubeModel, Transformation3D({ -5,0,-10 }, cubeScale), {brickMaterial}));
                id_t meshId6 = world->create(new Mesh(cubeModel, Transformation3D({ 10,-10,-10 }, cubeScale), {brickMaterial}));
                id_t meshId7 = world->create(new Mesh(cubeModel, Transformation3D({ -10,-10,-10 }, cubeScale), {brickMaterial}));
                id_t meshId8 = world->create(new Mesh(cubeModel, Transformation3D({ 10,10,-10 }, cubeScale), {brickMaterial}));
                id_t meshId9 = world->create(new Mesh(cubeModel, Transformation3D({ -10,10,-10 }, cubeScale), {brickMaterial}));
                id_t meshId3 = world->create(new Mesh(cubeModel, Transformation3D({ 0,0,-20 }, cubeScale), {grassMaterial}));
                Body* planeBody = world->bodies.get(meshId3);
                planeBody->get<Mesh*>()->scale({ 100.f,100.f,0.0001f });
            };

            Behavior* playerScene = new Behavior(nullptr); 
            playerScene->addScript(onLoadEvent, new Script(playerConstruction));
            world->addScene("player", playerScene);

            Behavior* meshScene = new Behavior(nullptr);
            meshScene->addScript(onLoadEvent, new Script(meshConstruction));
            world->addScene("meshes", meshScene);

            //Load each scene (without input)
            world->loadScene("meshes");
            world->loadScene("player");
		};
	};
}