#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "Screen.h"
#include "../Engine/Engine.h"
#include <cstdlib>
#include <string>

namespace CGEngine {
    Screen::Screen(Vector2i size, string title, bool fullscreen) : windowTitle(title) {
        setWindowParameters(size, title);
    }

    void Screen::setWindowParameters(Vector2i size, optional<string> title) {
        setSize(size);
        if (title.has_value()) {
            windowTitle = title.value();
        }
        log(this, LogInfo, "Screen Size:({},{}), and Title: '{}'", size.x, size.y, windowTitle);
    }

    void Screen::setWindowParameters(WindowParameters windowParams) {
        setWindowParameters(windowParams.windowSize, windowParams.windowTitle);
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
        return window.get();
    }

    RenderWindow* Screen::createWindow() {
        // CGENGINE_FULLSCREEN=1 opens the window full screen at the desktop's resolution. It is read once, at start-up:
        // changing the mode later would recreate the window and its GL context.
        const char* fullscreenSetting = std::getenv("CGENGINE_FULLSCREEN");
        if (fullscreenSetting && std::string(fullscreenSetting) == "1") {
            const VideoMode desktop = VideoMode::getDesktopMode();
            setSize(Vector2i(static_cast<int>(desktop.size.x), static_cast<int>(desktop.size.y)));
            window = make_unique<RenderWindow>(desktop, windowTitle, Style::Default, State::Fullscreen);
            fullscreen = true;
        } else {
            window = make_unique<RenderWindow>(VideoMode(size), windowTitle);
        }
        initView();
        return window.get();
    }

    bool Screen::setFullscreen(bool on) {
#ifdef _WIN32
        if (!window) return false;
        if (on == fullscreen) return true;
        HWND handle = static_cast<HWND>(window->getNativeHandle());
        if (on) {
            windowedPosition = window->getPosition();
            windowedSize = window->getSize();
            const Vector2u desktop = VideoMode::getDesktopMode().size;
            SetWindowLongPtr(handle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(handle, HWND_TOP, 0, 0, static_cast<int>(desktop.x), static_cast<int>(desktop.y), SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            window->setPosition(Vector2i(0, 0));
            window->setSize(desktop);
        } else {
            SetWindowLongPtr(handle, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPos(handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            window->setSize(windowedSize);
            window->setPosition(windowedPosition);
        }
        fullscreen = on;
        // The size the view is laid out for is the window's new size; a resize event follows for the GL viewport.
        const Vector2u now = window->getSize();
        setSize(V2f(static_cast<float>(now.x), static_cast<float>(now.y)));
        initView();
        glViewport(0, 0, static_cast<GLsizei>(now.x), static_cast<GLsizei>(now.y));
        return true;
#else
        (void)on;
        return false;
#endif
    }

    void Screen::initView() {
        float viewportSizeY = min(1.f, size.x / size.y);
        float viewportSizeX = min(1.f, size.y / size.x);
        float minDim = min(size.x, size.y);
        float ratioX = (1-(minDim /window->getSize().x))/2.f;
        float ratioY = (1-(minDim / window->getSize().y)) / 2.f;
        //Puts the origin in the view left corner (by default) and squares the view
        currentView = new View({minDim/2,minDim/2}, {minDim, minDim});
        //Squares the viewport and centers it
        currentView->setViewport(FloatRect({ Vector2f({ratioX,ratioY}),{viewportSizeX,viewportSizeY} }));
        window->setView(*currentView);
    }

    View* Screen::getCurrentView() {
        return currentView;
    }

    void Screen::setViewPosition(Vector2f pos) {
        currentView->setCenter(pos);
    }

    void Screen::setViewEulerRotation(float rotation) {
        currentView->setRotation(degrees(rotation));
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