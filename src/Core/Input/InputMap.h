#pragma once

#include "SFML/Graphics.hpp"
#include "../Scripts/ScriptDomain.h"
#include "../Scripts/Actuator.h"
using namespace sf;


namespace CGEngine {
    /// <summary>
	/// InputCondition describes an input condition for an input ScriptDomain. It contains the input id, type (button,key,char,cursor), 
	/// and state (pressed, released, held, atomic). When input events are gathered in InputMap.gather(), the the domain with InputCondition
	/// key matching the input event is called. The input id is the id of the input event (e.g. Mouse::Button::Left, Keyboard::Scan::A, etc.)
    /// </summary>
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

    /// <summary>
	/// A variant of ScriptMap that keys Scripts by InputCondition, which is queryable by input event via input id, 
    /// type (button,key,char,cursor), and state (pressed, released, held, atomic). The InputMap gathers input events 
    /// in the gather() method and calls the domains with matching InputConditions. The InputMap primarily uses Actuators 
    /// within its ScriptDomains because, unlike the ScriptMap, the Scripts in InputMap are not called from a Body, so the 
    /// ScriptEvent cannot be passed a Body or Behavior. Instead, the Actuator passes the ScriptEvent the Body and/or 
    /// Behavior assigned when the Actuator was created.
    /// </summary>
    class InputMap {
    public:
        InputMap();
        void setWindow(RenderWindow* w);
        optional<Vector2i> getCursorPosition();
        /// <summary>
        /// Clear the the domains map.
        /// </summary>
        void clear();
        /// <summary>
		/// Add an Actuator to the ScriptDomain at the given InputCondition, if it exists. If not, create a new ScriptDomain 
        /// with the given InputCondition and add the Actuator to it.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to add the Actuator to, if it exists, or to create, if not</param>
        /// <param name="script">The Actuator to add to the ScriptDomain at InputCondition</param>
        /// <returns>The id of the Actuator in the ScriptDomain or nullopt, if not added</returns>
        optional<id_t> addActuator(InputCondition domainCondition, Actuator* script);
        /// <summary>
		/// Finds the ScriptDomain at the given InputCondition and removes the Actuator with the indicated id from it without deleting it.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to remove the Actuator from</param>
        /// <param name="scriptId">The id of the Actuator to remove within the ScriptDomain</param>
        void removeActuator(InputCondition domainCondition, id_t scriptId);
        /// <summary>
        /// Finds the ScriptDomain at the given InputCondition and removes the indicated Actuator from it without deleting it.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to erase the Actuator from</param>
        /// <param name="script">The Actuator to remove</param>
        void removeActuator(InputCondition domainCondition, Script* script);
        /// <summary>
        /// Finds the ScriptDomain at the given InputCondition and erases the Actuator with the indicated id from it, including deleting it.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to remove the Actuator from</param>
        /// <param name="scriptId">The id of the Actuator to remove within the ScriptDomain</param>
        void eraseActuator(InputCondition domainCondition, id_t scriptId);
        /// <summary>
        /// Finds the ScriptDomain at the given InputCondition and erases the indicated Actuator from it, including deleting it.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to erase the Actuator from</param>
        /// <param name="script">The Actuator to remove</param>
        void eraseActuator(InputCondition domainCondition, Script* script);
        /// <summary>
        /// Finds the ScriptDomain at the given InputCondition and erases the Actuators with the indicated ids from it, including deleting them.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain to remove the Actuator from</param>
        /// <param name="scriptIds">The ids of the Actuators to remove within the ScriptDomain</param>
        void eraseActuatorIds(map<InputCondition, vector<id_t>> scriptIds);
        /// <summary>
		/// Find the ScriptDomain by InputCondition. If not found, return nullptr.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain</param>
        /// <returns>The ScriptDomain at that InputCondition or nullptr if not found </returns>
        ScriptDomain* getDomain(InputCondition domainCondition);
        /// <summary>
        /// Find the ScriptDomain by Key input id. If not found, return nullptr.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain</param>
        /// <returns>The ScriptDomain at that Key id or nullptr if not found </returns>
        ScriptDomain* getKeyDomain(int input, InputState state);
        /// <summary>
        /// Find the ScriptDomain by Button input id. If not found, return nullptr.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain</param>
        /// <returns>The ScriptDomain at that Button id or nullptr if not found </returns>
        ScriptDomain* getButtonDomain(int input, InputState state);
        /// <summary>
		/// Find the Character ScriptDomain. If not found, return nullptr. The Character ScriptDomain has state
		/// Atomic, which means it is called once per character input event, regardless of the input id.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain</param>
        /// <returns>The Character ScriptDomain or nullptr if not found </returns>
        ScriptDomain* getCharacterDomain();
        /// <summary>
		/// Find the Cursor ScriptDomain. If not found, return nullptr. The Cursor ScriptDomain has state
		/// Atomic, which means it is called once per cursor input event, without the input id.
        /// </summary>
        /// <param name="domainCondition">The InputCondition of the ScriptDomain</param>
        /// <returns>The Cursor ScriptDomain or nullptr if not found </returns>
        ScriptDomain* getCursorDomain();
		//Find and, if found, call the domain with the input condition, passing the input data to the domain scripts
        void callDomain(InputCondition domainCondition, optional<map<string, any>> input = nullopt);
    protected:
        friend class World;
        RenderWindow* window = nullptr;
        /// <summary>
		/// The ScriptDomains are stored in a map by their InputCondition. The InputCondition is a combination of the input id, type (button,key,char,cursor), and state (pressed, released, held, atomic).
        /// InputMap ScriptDomains are called by their InputConditions when the InputMap gathers input events in the gather() method.
        /// </summary>
        map<InputCondition, ScriptDomain*> domains;
        /// <summary>
		/// Called by the World to gather input events from the window and call the ScriptDomains with matching InputConditions.
        /// </summary>
        void gather();
        optional<Vector2i> cursorPosition = nullopt;
    };
}