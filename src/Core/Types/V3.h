#pragma once

#include "V3.h"

namespace CGEngine {
    template<typename T>
    class V3 {
    public:
        V3(T x = 0.f, T y = 0.f, T z = 0.f) :x(x), y(y), z(z) {};
        T x, y, z = 0.f;
    };

    class V3f : public V3<float> {
    public:
        V3f(float x = 0.f, float y = 0.f, float z = 0.f) :x(x), y(y), z(z) {};
        float x, y, z = 0.f;
    };
}