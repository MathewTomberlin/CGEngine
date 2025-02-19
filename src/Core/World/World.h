#pragma once

#include "../Body/Body.h"
#include "../World/Screen.h"
#include "../Types/UniqueDomain.h"
#include "../Mesh/Mesh.h"
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

        /// <summary>
        /// Returns the position of the Body in world space
        /// </summary>
        /// <returns>The Body's position in world space</returns>
        V2f getGlobalPosition(Transform transform) const;
        /// <summary>
        /// Returns the rotation (from 0 to 360 degrees) of the Body in world space
        /// </summary>
        /// <returns>The Body's rotation in world space</returns>
        Angle getGlobalRotation(Transform transform) const;
        /// <summary>
        /// Returns the scale of the Body in world space
        /// </summary>
        /// <returns>The Body's scale in world space</returns>
        V2f getGlobalScale(Transform transform) const;
        /// <summary>
        /// Return 1 / global scale (inverse global scale)
        /// </summary>
        /// <returns>The Body's scale in world space</returns>
        V2f getInverseGlobalScale(Transform transform) const;
        /// <summary>
        /// Return the negative (inverse) global rotation
        /// </summary>
        /// <returns>The Body's inverse global rotation</returns>
        Angle getInverseGlobalRotation(Transform transform) const;
        /// <summary>
        /// Returns the normalized world space direction of the Body's forward vector
        /// </summary>
        /// <returns>The Body's rotation in world space as a normalized direction vector</returns>
        V2f getForward(Transform transform) const;
        /// <summary>
        /// Returns the normalized world space direction of the Body's right vector
        /// </summary>
        /// <returns>The Body's rotation in world space as a normalized direction vector</returns>
        V2f getRight(Transform transform) const;
    private:
        RenderWindow* window = nullptr;

        //World Root
        Body* root = nullptr;

        bool running = false;
        
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