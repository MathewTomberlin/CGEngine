#pragma once

#include "SFML/Graphics.hpp"
#include "../Types/V2.h"
using namespace std;
using namespace sf;

namespace CGEngine {
    class Screen {
    public:
        Screen(V2f size, string title, bool fullscreen = false);
        //Window
        RenderWindow* getWindow() const;
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
        void moveView(Vector2f delta);
        void rotateView(Angle delta);
        void zoomView(float delta);
        Vector2f viewToGlobal(Vector2i pixels);
    protected:
        friend class World;
        //Window
        RenderWindow* window;
        RenderWindow* createWindow();
        string windowTitle;
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