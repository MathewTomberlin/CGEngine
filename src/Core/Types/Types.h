#pragma once
#include "SFML/Graphics.hpp"
#include <any>
#include <optional>
#include <set>
#include <map>
using sf::Event;
using sf::Vector2f;
using sf::Angle;
using sf::FloatRect;
using sf::Font;
using sf::Color;
using sf::Vector2i;
using sf::Vector3f;
using sf::Vector3i;
using sf::Texture;
using sf::RenderWindow;
using sf::View;
using std::string;
using std::vector;
using std::any;
using std::optional;
using std::nullopt;
using std::set;
using std::map;
using std::max;

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

    struct VertexModel {
        VertexModel(vector<float> vertices, string path = "", vector<unsigned> indices = {}) : 
            vertices(vertices), 
            indices(indices),
            path(path),
            vertexCount(vertices.size() / 8.0f), 
            vertexSpan(vertexCount * sizeof(float)), dataSpan(vertices.size() * sizeof(float)){};

        vector<float> vertices;
        vector<unsigned> indices;
        size_t vertexCount = 0U;
        size_t vertexSpan = 0U;
        size_t dataSpan = 0U;
        string path;
    };

    struct Transformation3D {
        Transformation3D(Vector3f position = Vector3f()) :position(position), rotation(Vector3f()), scale({1,1,1}) { };
        Transformation3D(Vector3f position, Vector3f rotation, Vector3f scale) :position(position), rotation(rotation), scale(scale) { };
        Transformation3D(Vector3f position, Vector3f scale) :position(position), rotation(Vector3f()), scale({ 1,1,1 }) { };

        Vector3f position;
        Vector3f rotation;
        Vector3f scale;
    };

    enum ParamType { NA, Bool, Int, Float, V2, V3, RGBA, String, Texture2D };

    struct ParamData {
        ParamData(any data = 0, ParamType type = ParamType::NA) : data(data), type(type) {};
        any data;
        ParamType type;
    };

    enum LogLevel { LogError, LogWarn, LogInfo, LogDebug, LogDebug1, LogDebug2 };
    const string logLevels[6] = { "ERROR", "WARN", "INFO", "DEBUG", "DEBUG1", "DEBUG2" };

    class IResource {
    public:
        virtual ~IResource() = default;
        virtual bool isValid() const = 0;
		optional<id_t> getId() const { return id; }
		virtual void setId(optional<id_t> id) { this->id = id; }
    private:
		optional<id_t> id = nullopt;
    };

    class TextureResource : public IResource {
    private:
        sf::Texture* texture;
    public:
        TextureResource() = default;

        bool isValid() const override {
            return true;
        }

        sf::Texture* getTexture() { return texture; }
        const sf::Texture* getTexture() const { return texture; }
        void setTexture(sf::Texture* texture) {
            this->texture = texture;
        }
    };

    class FontResource : public IResource {
    private:
        sf::Font* font;
    public:
        FontResource() = default;

        bool isValid() const override {
            return font->getInfo().family != "";
        }

        sf::Font* getFont() { return font; }
        const sf::Font* getFont() const { return font; }
        void setFont(sf::Font* font) {
            this->font = font;
        }
    };
}