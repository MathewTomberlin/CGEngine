#pragma once

#include "../Body/Body.h"
#include "../World/Screen.h"
#include "../Types/UniqueDomain.h"
#include "../Mesh/Mesh.h"
#include "../Light/Light.h"
#include "../Engine/EngineSystem.h"
#include <sstream>
#include <memory>
#include <queue>
using namespace sf;
using namespace std;

namespace CGEngine {
    class World : public EngineSystem {
    public:
        World();
        //World state
        void runWorld();
        void startWorld();
        void endWorld();

        //Scripts
        void callScripts(string scriptDomain, Body* body = nullptr);
        void callStaticScripts(StaticScriptDomain scriptDomainId, Body* body = nullptr);
        void addDefaultExitActuator();

        //Utility
        vector<id_t> zRayCast(Vector2f worldPos, optional<int> startZ = nullopt, int distance = -1, bool backward = false, bool linecast = false);
        vector<id_t> raycast(Vector2f worldPos, Vector2f castDir, int zIndex = 0, float distance = -1.f, bool linecast = false);

        //Console
        void initializeConsole();
        bool consoleInputEnabled = false;

        //Bounds Renderings
        void setBoundsRenderingEnabled(bool enabled);
        bool getBoundsRenderingEnabled() const;
        void setBoundsColor(Color color);
        void setBoundsThickness(float thickness);

        //Scenes
        void addScene(string sceneName, Behavior* scene);
        void loadScene(string sceneName);
        void loadSceneWithInput(string sceneName, DataMap input);
        optional<DataMap> getSceneInput(string sceneName);
        optional<DataMap> getSceneOutput(string sceneName);
        optional<DataMap> getSceneProcess(string sceneName);

        //Bodies
        vector<Body*> uninitialized;
        void addUninitialized(Body* body);

        //Root Body
        Body* getRoot();
        void addWorldScript(string domain, Script* script);
    private:
        /// <summary>
        /// Observation pointer of the RenderWindow owned by Screen
        /// </summary>
        RenderWindow* window = nullptr;
        /// <summary>
        /// Observation pointer of the world root Body owned by AssetManager
        /// </summary>
        Body* root = nullptr;

        bool running = false;
        
        //World State
        //Start World
        void initSceneList();
        //Update World
        void updateTime();
        void callUninitializedStart();

        //Scenes
        map<string, Behavior*> scenes;

        //Console
        bool consoleFeatureEnabled = true;
        bool consoleInitialized = false;
        string lastConsoleTarget = "";
        string lastConsoleCommand = "";
        string lastConsoleInput = "";
        int consoleInputIndex = 0;
        Body* consoleTextBox = nullptr;
        
        //Bounds Rendering
        bool boundsRendering = false;
        Color boundsColor = Color::White;
        float boundsThickness = 3.f;
    };
}