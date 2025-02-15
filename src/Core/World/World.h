#pragma once

#include "../Body/Body.h"
#include "../World/Screen.h"
#include "../Types/UniqueDomain.h"
#include <sstream>
#include <memory>
#include <queue>
using namespace sf;
using namespace std;

namespace CGEngine {
    class World {
    public:
        World();
        
        //World state
        void runWorld();
        void startWorld();
        void endWorld();

        //Scripts
        void callScripts(string scriptDomain, Body* body = nullptr);
        void addDefaultExitActuator();

        //Utility
        vector<Body*> zRayCast(Vector2f worldPos, optional<int> startZ = nullopt, int distance = -1, bool backward = false, bool linecast = false);
        vector<Body*> raycast(Vector2f worldPos, Vector2f castDir, int zIndex = 0, float distance = -1.f, bool linecast = false);

        //Console
        void initializeConsole();
        bool consoleInputEnabled = false;

        //Bounds Renderings
        void setBoundsRenderingEnabled(bool enabled);
        void setBoundsRenderingEnabled(bool enabled, Body* body);
        bool getBoundsRenderingEnabled();
        void setBoundsColor(Color color);
        void setBoundsColor(Color color, Body* body);
        void setBoundsThickness(float thickness);
        void setBoundsThickness(float thickness, Body* body);

        //Scenes
        void addScene(string sceneName, Behavior* scene);
        void loadScene(string sceneName);
        void loadSceneWithInput(string sceneName, DataMap input);
        optional<DataMap> getSceneInput(string sceneName);
        optional<DataMap> getSceneOutput(string sceneName);
        optional<DataMap> getSceneProcess(string sceneName);

        //Bodies
        UniqueDomain<id_t, Body*> bodies = UniqueDomain<id_t, Body*>(1000);
        vector<Body*> uninitialized;
        void deleteBody(Body* body, ChildrenTermination childTermination = ChildrenTermination::Orphan);
        bool isDeleted(Body* bodyId);
        void addUninitialized(Body* body);
        id_t receiveBodyId(Body* body);
        void refundBodyId(Body* body);
        id_t create(Transformable* entity = nullptr);
        id_t create(Transformable* entity, Transformation, Body* parent = nullptr, Script* startScript = nullptr);
        id_t create(Transformable* entity, Body* parent, Script* startScript = nullptr, Transformation transform = Transformation());
        id_t create(Transformable* entity, Script* startScript, Transformation transform = Transformation(), Body* parent = nullptr);

        //Root Body
        Body* getRoot();
        Body* findBodyByName(string name);
        void addWorldScript(string domain, Script* script);
    private:
        RenderWindow* window = nullptr;

        //World Root
        Body* root = nullptr;
        
        //World State
        //Start World
        void initSceneList();
        //Update World
        void updateTime();
        void startUninitializedBodies();
        void renderWorld();
        void render();
        //End World
        void endWorld(Body* body);

        //Bodies
        set<Body*> deleted;
        void addDeletedBody(Body* bodyId);

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