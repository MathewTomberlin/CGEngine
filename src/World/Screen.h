#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/V2.h"
using namespace std;
using namespace sf;

namespace CGEngine {
    class Screen {
    public:
        Screen(V2f size, string title, bool fullscreen = false);
        void setSize(V2f s);
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
        RenderWindow* getWindow() const;
        RenderWindow* window;
    protected:
        void createWindow(V2f size, string title, bool fullscreen);
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
    };
}