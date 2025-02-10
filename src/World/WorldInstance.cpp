#include "WorldInstance.h"

namespace CGEngine {
    GlobalTime time;
    Renderer renderer;
    TextureCache* textures = new TextureCache();
    FontCache* fonts = new FontCache();
    Font* defaultFont = fonts->getDefaultFont();
    InputMap* input = new InputMap();
    Screen* screen = new Screen({ 1200,1000 }, "CGEngine App");
    World* world = new World({ 1200,1000 },"");
    function<void()> drawWorld = []() { world->update(); };
    Logging logging;

    const char* keys[] = {
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
        "j",
        "k",
        "l",
        "m",
        "n",
        "o",
        "p",
        "q",
        "r",
        "s",
        "t",
        "u",
        "v",
        "w",
        "x",
        "y",
        "z",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "0"
    };

    const string onUpdateEvent = "update";
    const string onStartEvent = "start";
    const string onDeleteEvent = "delete";
    const string onIntersectEvent = "intersect";
    const string onMousePressEvent = "mousePress";
    const string onMouseReleaseEvent = "mouseRelease";
    const string onKeyPressEvent = "keyPress";
    const string onKeyReleaseEvent = "keyRelease";
}