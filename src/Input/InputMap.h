#pragma once

#include "SFML/Graphics.hpp"
#include "../Scripts/ScriptDomain.h"
#include "../Scripts/Actuator.h"
using namespace sf;
using namespace std;

namespace CGEngine {
    struct InputCondition {
        InputCondition(int i, InputType t, InputState s) :input(i), type(t), state(s) {};
        int input = -1;
        InputType type = InputType::Button;
        InputState state = InputState::Released;
        
        bool operator==(const InputCondition& r) const {
            return type == r.type && input == r.input && state == r.state;
        }

        bool operator<(const InputCondition& r) const {
            return type < r.type || type == r.type && state < r.state || type == r.type && state == r.state && input < r.input;
        }
    };

    class InputMap {
    public:
        InputMap();
        void setWindow(RenderWindow* w);
        optional<Vector2i> getCursorPosition();

        void clear();
        optional<id_t> addActuator(InputCondition domainCondition, Actuator* script);
        void removeActuator(InputCondition domainCondition, id_t scriptId);
        void removeActuator(InputCondition domainCondition, Script* script);
        void eraseActuator(InputCondition domainCondition, id_t scriptId);
        void eraseActuator(InputCondition domainCondition, Script* script);
        void eraseActuatorIds(map<InputCondition, vector<id_t>> scriptIds);
        ScriptDomain* getDomain(InputCondition domainCondition);
        ScriptDomain* getKeyDomain(int input, InputState state);
        ScriptDomain* getButtonDomain(int input, InputState state);
        ScriptDomain* getCharacterDomain();
        ScriptDomain* getCursorDomain();
        void callDomain(InputCondition domainCondition, optional<stack<any>> input = nullopt);
    protected:
        friend class World;
        RenderWindow* window;
        map<InputCondition, ScriptDomain*> domains;
        void gather();
        optional<Vector2i> cursorPosition = nullopt;
    };
}