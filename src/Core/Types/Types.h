#pragma once
#include "SFML/Graphics.hpp"
using namespace sf;
using namespace std;

namespace CGEngine {
    class Body;

    typedef size_t id_t;
    typedef float sec_t;
    typedef const Event::MouseButtonPressed MousePressInput;
    typedef const Event::MouseButtonReleased MouseReleaseInput;
    typedef const Event::KeyReleased KeyReleaseInput;
    typedef const Event::KeyPressed KeyPressInput;
    typedef const Event::TextEntered TextEnteredInput;

    enum ShapeType {
        Circle,
        Rectangle
    };

    enum InputState {
        Pressed,
        Released,
        Held,
        Atomic
    };

    enum InputType { 
        Button, 
        Key,
        Character,
        Cursor
    };

    enum RelationSpace {
        Global,
        Local
    };

    enum KeyboardEvent {
        Delete,
        Enter,
        Escape
    };

    struct Transformation {
        Transformation(Vector2f pos = { 0,0 }, Angle a = Angle(), Vector2f s = { 1,1 }) {
            position = pos;
            angle = a;
            scale = s;
        }
        Vector2f position;
        Angle angle;
        Vector2f scale;
    };

    struct Collision {
        Body* body;
        Body* otherBody;
        FloatRect intersect;
    };

    struct TextProperties {
        TextProperties(Font* f, size_t s = 16U, bool i = false, bool b = false, bool u = false, bool st = false) {
            font = f;
            size = s;
            italic = i;
            bold = b;
            underline = u;
            strikethrough = st;
        }

        Font* font;
        size_t size;
        bool italic;
        bool bold;
        bool underline;
        bool strikethrough;
    };

    struct RectProperties {
        RectProperties(Color f = Color(80, 80, 80), Color o = Color(), float w = 0) {
            fillColor = f;
            outlineColor = o;
            outlineWidth = w;
        }
        Color fillColor = Color(80, 80, 80);
        Color outlineColor = Color();
        float outlineWidth = 0;
    };

    struct WindowParameters {
        WindowParameters(Vector2i windowSize, string windowTitle) :windowTitle(windowTitle), windowSize(windowSize) {

        };
        string windowTitle;
        Vector2i windowSize;
    };

    struct OpenGLSettings {
        OpenGLSettings(bool lightingEnabled = false, bool texture2DEnabled = true, FloatRect viewport = FloatRect({0,0},{1,1}), float nearClip = 0.000000001f, float farClip = 100.f)
            : lightingEnabled(lightingEnabled), texture2DEnabled(texture2DEnabled), viewport(viewport), nearClipPlane(max(nearClip, 0.0f)), farClipPlane(farClip) { };

        bool lightingEnabled = false;
        bool texture2DEnabled = true;
        FloatRect viewport = FloatRect({ 0,0 }, { 1,1 });
        float nearClipPlane = 0.000000001f;
        float farClipPlane = 100.f;
    };
}