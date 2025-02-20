#include "Engine.h"

namespace CGEngine {
    //Size and name to give created window
    WindowParameters windowParameters = WindowParameters({ 1000,500 }, "CGEngine App");
    
    OpenGLSettings openGLSettings = OpenGLSettings(true, true);
    //List of Scenes to create, add to World and load sceneList[0]
    vector<Behavior*> sceneList = { };
    Logging logging;
    GlobalTime time;
    Renderer renderer;
    TextureCache* textures = new TextureCache();
    FontCache* fonts = new FontCache();
    Font* defaultFont = fonts->getDefaultFont();
    InputMap* input = new InputMap();
    
    //Window Size & Window Title
    Screen* screen = new Screen(windowParameters.windowSize, windowParameters.windowTitle);
    World* world = new World();
    function<void()> beginWorld = []() { world->startWorld(); world->runWorld(); };
    function<Camera* ()> getCamera = []() { return renderer.getCurrentCamera(); };

    const string onUpdateEvent = "update";
    const string onStartEvent = "start";
    const string onDeleteEvent = "delete";
    const string onIntersectEvent = "intersect";
    const string onMousePressEvent = "mousePress";
    const string onMouseReleaseEvent = "mouseRelease";
    const string onKeyPressEvent = "keyPress";
    const string onKeyReleaseEvent = "keyRelease";
    const string onLoadEvent = "load";

    const float minHolographicNearClip = 0.000000000001f;
}