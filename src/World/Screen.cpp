#include "Screen.h"

namespace CGEngine {
    Screen::Screen(V2f size, string title, bool fullscreen) {
        //createWindow(size, title, fullscreen);
        setSize(size);
    }

    void Screen::setSize(V2f s) {
        size = s;
        center = size / 2;
        topLeft = { 0, 0 };
        topRight = { size.x, 0 };
        bottomLeft = { 0, size.y };
        bottomRight = { size.x, size.y };
        topCenter = { size.x / 2U, 0 };
        bottomCenter = { size.x / 2U, size.y };
        leftCenter = { 0, size.y / 2U };
        rightCenter = { size.x, size.y / 2U };
    }

    V2f Screen::getSize() const {
        return size;
    }

    V2f Screen::getCenter() const {
        return center;
    }

    V2f Screen::getTopLeft() const {
        return topLeft;
    }

    V2f Screen::getTopRight() const {
        return topRight;
    }

    V2f Screen::getBottomLeft() const {
        return bottomLeft;
    }

    V2f Screen::getBottomRight() const {
        return bottomRight;
    }

    V2f Screen::getBottomCenter() const {
        return bottomCenter;
    }

    V2f Screen::getTopCenter() const {
        return topCenter;
    }

    V2f Screen::getLeftCenter() const {
        return leftCenter;
    }

    V2f Screen::getRightCenter() const {
        return rightCenter;
    }

    RenderWindow* Screen::getWindow() const {
        return window;
    }

    void Screen::createWindow(V2f size, string title, bool fullscreen) {
        window = new RenderWindow(sf::VideoMode({ 1920u, 1080u }), title);
        setSize(size);
    }
}