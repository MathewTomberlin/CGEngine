#pragma once

#include "../../Scripts/ScriptMap.h"

namespace CGEngine {
	class ScriptController {
    public:
        ScriptController(Body* owner) : scripts(owner) {};
        /// <summary>
        /// Add the script to the ScriptDomain, creating the domain if it doesn't already exist
        /// </summary>
        /// <param name="domain">The name of the domain to add the script to</param>
        /// <param name="script">The script to add to the domain</param>
        /// <returns>The unique id of the script within the domain</returns>
		id_t addScript(string domain, Script* script);
        /// <summary>
        /// Add each script to the indicated ScriptDomain, creating the domain if it doesn't already exist
        /// </summary>
        /// <param name="domain">The name of the domain to add the script to</param>
        /// <param name="script">The script to add to the domain</param>
        /// <returns>The unique id of the script within the domain</returns>
        void addScriptEventsByDomain(map<string, ScriptEvent> sc);
        /// <summary>
        /// Add the script to the "start" ScriptDomain to be called when the Body is created or when the world starts
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
		id_t addStartScript(Script* script);
        /// <summary>
        /// Add the script to the "update" ScriptDomain to be called each update cycle
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
		id_t addUpdateScript(Script* script);
        /// <summary>
        /// Add the script to the "delete" ScriptDomain to be called when the Body is deleted
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
		id_t addDeleteScript(Script* script);
        /// <summary>
        /// Erase and delete the script with the id in the domain.
        /// </summary>
        /// <param name="domain">The domain to erase the script from</param>
        /// <param name="scriptId">The unique id of the script to delete within the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseScript(string domain, id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the domain.
        /// </summary>
        /// <param name="domain">The domain to erase the script from</param>
        /// <param name="script">The script to delete</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseScript(string domain, Script* script, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "start" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseStartScript(id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "update" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseUpdateScript(id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "delete" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseDeleteScript(id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase a Domain from the ScriptMap and then delete the Domain
        /// </summary>
        /// <param name="domain">The name of the domain to delete</param>
        void deleteDomain(string domain);
        /// <summary>
        /// Erase and refund ids for all scripts in the domain, but don't delete the domain or the Scripts
        /// </summary>
        /// <param name="domain">The name of the domain to delete</param>
        void clearDomain(string domain);
		/// <summary>
		/// Remove a Script from a domain without deleting the Script
		/// </summary>
		/// <param name="domain">The domain to remove the Script from</param>
		/// <param name="scriptId">The id of the Script to remove</param>
		/// <param name="shouldDelete">Whether the Domain should be deleted when empty</param>
		void removeScript(string domain, id_t scriptId, bool shouldDelete = false);
		/// <summary>
		/// Call each Script in the indicated domain
		/// </summary>
		/// <param name="domain">The name of the domain to call Scripts in</param>
		void callDomain(string domain);
		/// <summary>
		/// Provide each Script with data, then call it
		/// </summary>
		/// <param name="domain">The name of the domain to call Scripts in</param>
		/// <param name="data">The data to provide the Scripts when calling</param>
		void callDomainWithData(string domain, DataMap data);
	protected:
		ScriptMap scripts;
	};
}