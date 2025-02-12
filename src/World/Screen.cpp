#include "Screen.h"

namespace CGEngine {
    Screen::Screen(V2f size, string title, bool fullscreen) : windowTitle(title) {
        setSize(size);
        windowTitle = title;
    }

    string Screen::getWindowTitle() {
        return windowTitle;
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

    RenderWindow* Screen::createWindow() {
        window = new RenderWindow(VideoMode(size), windowTitle);
        initView();
        return window;
    }

    void Screen::initView() {
        currentView = new View(size / 2.f, size);
        window->setView(*currentView);
    }

    View* Screen::getCurrentView() {
        return currentView;
    }

    void Screen::moveView(Vector2f delta) {
        currentView->move(delta);
        window->setView(*currentView);
    }

    void Screen::rotateView(Angle delta) {
        currentView->rotate(delta);
        window->setView(*currentView);
    }

    void Screen::zoomView(float delta) {
        currentView->zoom(delta);
        window->setView(*currentView);
    }

    Vector2f Screen::viewToGlobal(Vector2i pixels) {
        if (window != nullptr) {
            return window->mapPixelToCoords(pixels);
        }
        return Vector2f();
    }
}