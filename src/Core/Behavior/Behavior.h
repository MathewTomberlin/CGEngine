#pragma once

#include <stack>
#include <optional>
#include <any>
#include "../Scripts/ScriptMap.h"
#include "../Body/Body.h"
#include "../Types/DataControllers/InputDataController.h"
#include "../Types/DataControllers/OutputDataController.h"
#include "../Types/DataControllers/ProcessDataController.h"

namespace CGEngine {
	/// <summary>
	/// Behavior contains a ScriptMap, like a Body, but does not hold a reference to a Drawable entity and
	/// has not transform properties, like a Body. A Behavior is stateless or stateful, like a Script, via
	/// its Input/Output/ProcessDataController. A Behavior's ScriptMap behaves in the same way as a Body and 
	/// its "start", "update", and "delete" domains are called whenever the Body's are called during World Execution.
	/// Behaviors are typically owned by a Body and used to handle data and events for that Body.
	/// </summary>
	class Behavior : public InputDataController, public OutputDataController, public ProcessDataController {
	public:
		/// <summary>
		/// Construct a Behavior with the display name, assigning the owning Body (if present) to the ScriptMap's owner pointer and to
		/// the Behavior's owner pointer. The Behavior is assigned a unique id, if an owner is present, then the ScriptMap is initialized.
		/// </summary>
		/// <param name="owning">The Behavior's owning Body</param>
		/// <param name="displayName">The display name of the Behavior</param>
		Behavior(Body* owning, string displayName = "");

		/// <summary>
		/// Add a Script to the Behavior's ScriptMap at the given domain.
		/// </summary>
		/// <param name="domain">The string key of the ScriptDomain to add the Script to</param>
		/// <param name="script">The Script to add to the domain</param>
		/// <returns>The id of the Script within the ScriptDomain</returns>
		id_t addScript(string domain, Script* script);
		/// <summary>
		/// Remove a Script with scriptId from the Behavior's ScriptMap at the given domain. If shouldDelete is true, delete the ScriptDomain
		/// if it is empty after removing the Script.
		/// </summary>
		/// <param name="domain">The string key of the ScriptDomain to remove the Script from</param>
		/// <param name="scriptId">The id of the Script to remove within the ScriptDomain</param>
		/// <param name="shouldDelete">Whether or not the ScriptDomain should be deleted if empty after removal</param>
		void removeScript(string domain, id_t scriptId, bool shouldDelete = false);
		/// <summary>
		/// For each string key and ScriptEvent in the provided map, create a Script and assign the ScriptEvent to it, then add the Script to the ScriptDomain
		/// at the string key.
		/// </summary>
		/// <param name="scripts">The map of string domain key to ScriptEvents to add (as Scripts)</param>
		void addScriptEventsByDomain(map<string, ScriptEvent> scripts);
		/// <summary>
		/// Call each Script in the ScriptDomain with the indicated string key
		/// </summary>
		/// <param name="domain">The string key of the ScriptDomain to call</param>
		void callDomain(string domain);
		/// <summary>
		/// Call each Script in the ScriptDomain with the indicated string key
		/// </summary>
		/// <param name="domain">The string key of the ScriptDomain to call</param>
		void callStaticDomain(StaticScriptDomain domainId);
		/// <summary>
		/// Call each Script iin the ScriptDomain with the indicated string key, passing the DataMap as input to each Script
		/// </summary>
		/// <param name="domain">The string key of the ScriptDomain to call</param>
		/// <param name="data">The input DataMap to pass to each Script called</param>
		void callDomainWithData(string domain, DataMap data);

		/// <summary>
		/// Get the id of the Behavior within the Body's Behavior list, if added to a Body.
		/// </summary>
		/// <returns>The id of the Behavior or nullopt, if not set</returns>
		optional<id_t> getId();
		/// <summary>
		/// Return the Behavior's owning Body or nullptr, if not set
		/// </summary>
		/// <returns>The owning Body or nullptr</returns>
		Body* getOwner();
		/// <summary>
		/// Return the Behavior's display name
		/// </summary>
		/// <returns>The Behavior's display name</returns>
		string getName();
	private:
		string displayName = "";
		Body* owner;
		optional<id_t> behaviorId = nullopt;
		ScriptMap scripts;
	};
}