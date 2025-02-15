#pragma once

#include "../../Core/World/World.h"

namespace CGEngine {
	extern ScriptEvent translateEvent;
    extern ScriptEvent rotateEvent;
	extern ScriptEvent keyboardMovementController;
    extern ScriptEvent keyboardRotationController;
	struct InputKeyMap {
        InputKeyMap(Keyboard::Scan pos = Keyboard::Scan::S, Keyboard::Scan neg = Keyboard::Scan::W) :positive(pos), negative(neg) {};
        Keyboard::Scan positive = Keyboard::Scan::S;
        Keyboard::Scan negative = Keyboard::Scan::W;
    };
}