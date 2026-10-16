#pragma once

namespace CGEngine {
    // True once a window's OpenGL context is current and GLEW is loaded (World::startWorld). A headless run never sets it, so the
    // code that calls OpenGL (shaders, programs, textures) makes an empty object instead of calling through null pointers.
    extern bool openGLReady;
    // True for a headless run (CGENGINE_HEADLESS=1): no window, no draws, and the loop runs at a fixed rate.
    extern bool headlessRun;
}
