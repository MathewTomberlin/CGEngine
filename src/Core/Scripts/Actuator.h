#pragma once

#include "Script.h"

namespace CGEngine {
	/// <summary>
	/// An Actuator is a Script which is constructed with a Body and/or Behavior. When an Actuator is called, its ScriptEvent is called with the
	/// Actuator's stored Body and/or Behavior instead of any Body or Behavior passed to it. This allows for the Actuator to pass its cached Body and/or
	/// Behavior to the ScriptEvent, regardless of where the Actuator was called from.
	/// 
	/// In the engine, when a user adds an input script to a Body or Behavior, an Actuator is created in the InputMap and assigned that Body or Behavior, then 
	/// when the input condition is met, the Actuator calls its ScriptEvent and passes the Body or Behavior to it. Without Actuators, Scipts expect to be called
	/// from a ScriptMap and passed the ScriptMap's owning Body or Behavior, which would not be functional in the InputMap (which has no owning Body or Behavior).
	/// </summary>
	class Actuator : public Script {
	public:
		/// <summary>
		/// Create the Actuator with the ScriptEvent to call when the Actuator's call method is invoked and with the calling Body and/or Behavior that will be
		/// passed to the ScriptEvent when called.
		/// </summary>
		/// <param name="script">The ScriptEvent to call when the Actuator's call method is invoked</param>
		/// <param name="calling">The Body to pass to the ScriptEvent when called</param>
		/// <param name="behavior">The Behavior to pass to the SciptEvent when called</param>
		Actuator(ScriptEvent script, Body* calling = nullptr, Behavior* behavior = nullptr) : caller(calling), behavior(behavior), Script(script) { }

		/// <summary>
		/// Call the ScriptEvent assigned to the Actuator, but pass the Actuator's assigned caller Body and/or Behavior to it instead of the Body or Behavior
		/// passed to the call method itself. Passing a caller Body or Behavior to this method has no effect.
		/// </summary>
		/// <param name="caller">Unused</param>
		/// <param name="behavior">Unused</param>
		void call(Body* caller = nullptr, Behavior* behavior = nullptr) override {
			scriptEvent(ScArgs(this, this->caller, this->behavior)); 
		}
	protected:
		Body* caller = nullptr;
		Behavior* behavior = nullptr;
	};
}