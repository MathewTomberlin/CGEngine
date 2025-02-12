#pragma once

#include "../Body/Body.h"
#include "../World/Screen.h"
#include "../Scenes/Scene.h"
#include "../Types/UniqueDomain.h"
#include <sstream>
using namespace sf;
using namespace std;

namespace CGEngine {
    class World {
    public:
        World();
        World(string title);
        World(Vector2u windowSize);
        World(Vector2u windowSize, string title);

        string appTitle;

        //World Window
        Screen* createScreen(Vector2u windowSize, string appTitle);
        RenderWindow* getWindow() const;
        
        //World state
        void runWorld();
        void startWorld();
        void endWorld();

        //Scripts
        void callScripts(string scriptDomain, Body* body = nullptr);
        void addDefaultExitActuator();

        //Current View
        View* getCurrentView();
        void moveView(Vector2f delta);
        void rotateView(Angle delta);
        void zoomView(float delta);
        Vector2f viewToGlobal(Vector2i pixels);

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
        void addScene(string sceneName, Scene* scene);
        void loadScene(string sceneName, Body* sceneRoot = nullptr);
        void loadSceneWithData(string sceneName, DataMap input, Body* sceneRoot = nullptr);
        optional<DataMap> getSceneData(string sceneName);

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

        RenderWindow* window = nullptr;
    private:
        //World Root
        Body* root = nullptr;

        //Current View
        View* currentView = nullptr;
        
        //Update/World State
        void updateTime();
        void startUninitializedBodies();
        void renderWorld();
        void render();
        void endWorld(Body* body);

        //Bodies
        set<Body*> deleted;
        void addDeletedBody(Body* bodyId);

        //Scenes
        map<string, Scene*> scenes;
        
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