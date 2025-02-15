#pragma once

#include "SFML/Graphics.hpp"
using namespace std;
using namespace sf;

namespace CGEngine {
    class V2f : public Vector2<float> {
    public:
        V2f();

        template <typename T>
        V2f(T v) : Vector2f((Vector2)v) {};

        template <typename T>
        V2f(T x, T y) : Vector2f((float)x, (float)y) {};

        template <typename T, typename S>
        V2f(T x, S y) : Vector2f((float)x, (float)y) {};

        template <typename T>
        operator Vector2<T>() { return Vector2<T>(x, y); }

        template <typename T>
        V2f operator-(Vector2<T> right) {
            return Vector2<T>(x - right.x, y - right.y);
        }

        V2f operator-(V2f right) {
            return V2f(x - right.x, y - right.y);
        }

        template <typename T>
        V2f operator+(Vector2<T> right) {
            return Vector2<T>(x + right.x, y + right.y);
        }

        V2f operator+(V2f right) {
            return V2f(x + right.x, y + right.y);
        }

        template <typename T>
        V2f operator*(Vector2<T> right) {
            return Vector2<T>(x * right.x, y * right.y);
        }

        template <typename T>
        V2f operator/(Vector2<T> right) {
            return Vector2<T>(x / right.x, y / right.y);
        }

        template <typename T>
        V2f operator-(T right) {
            return Vector2<T>(x - right, y - right);
        }

        template <typename T>
        V2f operator+(T right) {
            return Vector2<T>(x + right, y + right);
        }

        template <typename T>
        V2f operator*(T right) {
            return Vector2<T>(x * right, y * right);
        }

        template <typename T>
        V2f operator/(T right) {
            return Vector2<T>(x / right, y / right);
        }

        void safeNormalize();
        void clampTo(Vector2f min = { 0,0 }, Vector2f max = { 1,1 });
    };
}