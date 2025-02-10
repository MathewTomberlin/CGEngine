#include "V2.h"

namespace CGEngine {
    V2f::V2f() { }

    void V2f::safeNormalize() {
        if (length() > 0.0001f) {
            Vector2f v = normalized();
            x = v.x;
            y = v.y;
        }
    }

    void V2f::clampTo(Vector2f min, Vector2f max) {
        x = clamp(x, min.x, max.x);
        y = clamp(y, min.y, max.y);
    }
}