#include "InputMap.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    InputMap::InputMap() { }

    void InputMap::clear() {
        domains.clear();
    }

    optional<id_t> InputMap::addActuator(InputCondition domainCondition, Actuator* script) {
        ScriptDomain* domain = nullptr;
        auto iterator = domains.find(domainCondition);
        if (iterator == domains.end()) {
            domains[domainCondition] = new ScriptDomain("");
            domain = domains[domainCondition];
        } else {
            domain = (*iterator).second;
        }

        if(domain!=nullptr) {
            return domain->addScript(script);
        }
        return nullopt;
    }

    void InputMap::removeActuator(InputCondition domainCondition, id_t scriptId) {
        auto iterator = domains.find(domainCondition);
        if (iterator != domains.end()) {
            ScriptDomain* domain = (*iterator).second;
            domain->removeScript(scriptId);
        }
    }

    void InputMap::removeActuator(InputCondition domainCondition, Script* script) {
        auto iterator = domains.find(domainCondition);
        if (iterator != domains.end()) {
            ScriptDomain* domain = (*iterator).second;
            domain->removeScript(script);
        }
    }

    void InputMap::eraseActuator(InputCondition domainCondition, id_t scriptId) {
        auto iterator = domains.find(domainCondition);
        if (iterator != domains.end()) {
            ScriptDomain* domain = (*iterator).second;
            domain->eraseScript(scriptId);
        }
    }

    void InputMap::eraseActuator(InputCondition domainCondition, Script* script) {
        auto iterator = domains.find(domainCondition);
        if (iterator != domains.end()) {
            ScriptDomain* domain = (*iterator).second;
            domain->eraseScript(script);
        }
    }

    void InputMap::eraseActuatorIds(map<InputCondition,vector<id_t>> scriptIds) {
        for (auto iterator = scriptIds.begin(); iterator != scriptIds.end(); ++iterator) {
            ScriptDomain* domain = getDomain((*iterator).first);
            if(domain!=nullptr){
                vector<id_t> ids = (*iterator).second;
                for (int i = ids.size() - 1; i >= 0; i--) {
                    domain->eraseScript(ids[i]);
                }
            }
        }
    }

    ScriptDomain* InputMap::getDomain(InputCondition domainCondition) {
        auto iterator = domains.find(domainCondition);
        if (iterator != domains.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    ScriptDomain* InputMap::getKeyDomain(int input, InputState state) {
        auto iterator = domains.find(InputCondition(input, InputType::Key, state));
        if (iterator != domains.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    ScriptDomain* InputMap::getButtonDomain(int input, InputState state) {
        auto iterator = domains.find(InputCondition(input, InputType::Button, state));
        if (iterator != domains.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    ScriptDomain* InputMap::getCharacterDomain() {
        auto iterator = domains.find(InputCondition(0, InputType::Character, InputState::Atomic));
        if (iterator != domains.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    ScriptDomain* InputMap::getCursorDomain() {
        auto iterator = domains.find(InputCondition(0, InputType::Cursor, InputState::Atomic));
        if (iterator != domains.end()) {
            return (*iterator).second;
        }
        return nullptr;
    }

    void InputMap::callDomain(InputCondition domainCondition, optional<stack<any>> input) {
        ScriptDomain* domain = nullptr;
        if (domainCondition.type == InputType::Key) {
            domain = getKeyDomain(domainCondition.input, domainCondition.state);
        } else if(domainCondition.type == InputType::Button) {
            domain = getButtonDomain(domainCondition.input, domainCondition.state);
        } else if (domainCondition.type == InputType::Character) {
            domain = getCharacterDomain();
        } else if (domainCondition.type == InputType::Cursor) {
            domain = getCursorDomain();
        }

        if (domain != nullptr) {
            optional<DataStack> stack = nullopt;
            if (input.has_value()) {
                stack = DataStack(input.value());
            }
            domain->callDomain(nullptr, stack);
        }
    }

    void InputMap::gather() {
        //Poll window events
        while (const optional event = window->pollEvent()) {
            if (event->is<Event::Closed>()) {
                world->end();
            } else if (const auto* keyReleased = event->getIf<Event::KeyReleased>()) {
                callDomain(InputCondition((int)keyReleased->scancode, InputType::Key, InputState::Released), stack<any>({ keyReleased }));
            }
            else if (const auto* keyPressed = event->getIf<Event::KeyPressed>()) {
                callDomain(InputCondition((int)keyPressed->scancode, InputType::Key, InputState::Pressed), stack<any>({ keyPressed }));
            }
            else if (const auto* mousePressed = event->getIf<Event::MouseButtonPressed>()) {
                callDomain(InputCondition((int)mousePressed->button, InputType::Button, InputState::Pressed), stack<any>({ mousePressed }));
            }
            else if (const auto* mouseReleased = event->getIf<Event::MouseButtonReleased>()) {
                callDomain(InputCondition((int)mouseReleased->button, InputType::Button, InputState::Released), stack<any>({ mouseReleased }));
            }
            else if (const auto* textEntered = event->getIf<Event::TextEntered>()) {
                callDomain(InputCondition(0, InputType::Character, InputState::Atomic), stack<any>({ textEntered }));
            }
            else if (const auto* mouseMoved = event->getIf<Event::MouseMoved>()) {
                cursorPosition = mouseMoved->position;
                callDomain(InputCondition(0, InputType::Cursor, InputState::Atomic), stack<any>({ mouseMoved->position }));
            }
        }
    }

    void InputMap::setWindow(RenderWindow* w) {
        window = w;
    }

    optional<Vector2i> InputMap::getCursorPosition() {
        return cursorPosition;
    }
}