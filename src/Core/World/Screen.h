#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/V2.h"
#include "../Types/Types.h"
#include "../Engine/EngineSystem.h"

namespace CGEngine {
    class Screen : public EngineSystem {
    public:
        Screen(Vector2i size, string title, bool fullscreen = false);
        void setWindowParameters(Vector2i size, optional<string> title = "");
        void setWindowParameters(WindowParameters windowParams);
        //Window
        RenderWindow* getWindow() const;
        // Full screen as a borderless window the size of the desktop (Windows; false elsewhere). The window and its GL context
        // are kept, so the game's resources stay. Returns false if it is not supported.
        bool setFullscreen(bool on);
        bool isFullscreen() const { return fullscreen; }
        // True while the game's window is the one the user is typing into (not in the background).
        bool hasFocus() const { return window && window->hasFocus(); }
        string getWindowTitle();
        //Size
        V2f getSize() const;
        V2f getCenter() const;
        V2f getTopLeft() const;
        V2f getTopRight() const;
        V2f getBottomLeft() const;
        V2f getBottomRight() const;
        V2f getTopCenter() const;
        V2f getBottomCenter() const;
        V2f getLeftCenter() const;
        V2f getRightCenter() const;
        //Current View
        View* getCurrentView();
        void setViewPosition(Vector2f pos);
        void setViewEulerRotation(float rotation);
        void moveView(Vector2f delta);
        void rotateView(Angle delta);
        void zoomView(float delta);
        Vector2f viewToGlobal(Vector2i pixels);
    protected:
        friend class World;
        //Window
        unique_ptr<RenderWindow> window;
        RenderWindow* createWindow();
        string windowTitle;
        // The window's place and size before it went full screen, so it returns there.
        bool fullscreen = false;
        Vector2i windowedPosition = { 0, 0 };
        sf::Vector2u windowedSize = { 0u, 0u };
        //Size
        void setSize(V2f s);
        V2f size;
        V2f center;
        V2f topLeft;
        V2f topRight;
        V2f bottomLeft;
        V2f bottomRight;
        V2f topCenter;
        V2f bottomCenter;
        V2f rightCenter;
        V2f leftCenter;
        //Current View
        View* currentView = nullptr;
        void initView();
    };
}