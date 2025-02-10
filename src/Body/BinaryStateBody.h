#pragma once

#include "Body.h"

namespace CGEngine {
    class BinaryStateBody : public Body {
    public:
        BinaryStateBody() : Body() {};
        BinaryStateBody(Transformable* b, Transformation handle = Transformation(), Body* p = nullptr, Vector2f align = {0,0}) : Body(b, handle, p, align) {};

        bool state = false;
        bool dirty = false;
    };
}