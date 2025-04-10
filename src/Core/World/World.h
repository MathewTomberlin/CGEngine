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
        void addDefaultExitActuator();

        //Utility
        vector<id_t> zRayCast(Vector2f worldPos, optional<int> startZ = nullopt, int distance = -1, bool backward = false, bool linecast = false);
        vector<id_t> raycast(Vector2f worldPos, Vector2f castDir, int zIndex = 0, float distance = -1.f, bool linecast = false);

        //Console
        void initializeConsole();
        bool consoleInputEnabled = false;

        //Bounds Renderings
        void setBoundsRenderingEnabled(bool enabled);
        void setBoundsRenderingEnabled(bool enabled, Body* body);
        bool getBoundsRenderingEnabled() const;
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
        vector<Body*> uninitialized;
        void addUninitialized(Body* body);

        //Root Body
        Body* getRoot();
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
        void startUninitializedBodies();
        //End World
        void endWorld(Body* body);

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