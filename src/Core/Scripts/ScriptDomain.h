#pragma once

#include "../Types/UniqueIntegerStack.h"
#include "../Types/Types.h"
#include "../Logging/Logging.h"

namespace CGEngine {
    class Script;
    class DataMap;
    class Behavior;

    class ScriptDomain : public EngineSystem {
    public:
        ScriptDomain(string name, string bodyName = "");
        ~ScriptDomain();
        string getName();
        /// <summary>
        /// For each domain script (by key array), refund its id, remove it from the map and delete it. Finally, clear the domain's script map.
        /// </summary>
        void deleteDomain();
        bool isEmpty();
        void clear();
        Script* getScript(size_t scriptId);
        vector<size_t> getScriptIds();
        size_t addScript(Script* script);
        void removeScript(size_t scriptId, bool shouldLog = true);
        void removeScript(Script* script, bool shouldLog = true);
        void eraseScript(size_t scriptId);
        void eraseScript(Script* script);
        /// <summary>
        /// Call each Script in the Domain, passing caller and others as arguments and passing the predecessor script's output DataStack as input
        /// </summary>
        /// <param name="caller">The Body calling the script</param>
        /// <param name="others">Any other Bodies passed as arguments</param>
        /// <param name="predecessor">The script to pass output data from as input</param>
        void callDomain(Body* caller, Behavior* behavior);
        /// <summary>
        /// Call each Script in the Domain, passing caller and others as arguments and passing the DataStack as input
        /// </summary>
        /// <param name="caller">The Body calling the script</param>
        /// <param name="others">Any other Bodies passed as arguments</param>
        /// <param name="input">The DataStack to pass as input</param>
        void callDomain(Body* caller, Behavior* behavior, optional<DataMap> input);
        /// <summary>
        /// Call the Script from the Domain, passing caller and others as arguments and passing the predecessor script's output DataStack as input
        /// </summary>
        /// <param name="scriptId">The id of the script in the domain to call</param>
        /// <param name="caller">The Body calling the script</param>
        /// <param name="others">Any other Bodies passed as arguments</param>
        /// <param name="predecessor">The script to pass output data from as input</param>
        void callScript(size_t scriptId, Body* caller, Behavior* behavior = nullptr);
        /// <summary>
        /// Call the Script from the Domain, passing caller and others as arguments and passing the DataStack as input
        /// </summary>
        /// <param name="scriptId">The id of the script in the domain to call</param>
        /// <param name="caller">The Body calling the script</param>
        /// <param name="others">Any other Bodies passed as arguments</param>
        /// <param name="predecessor">The DataStack to pass as input</param>
        void callScript(size_t scriptId, Body* caller, DataMap input, Behavior* behavior = nullptr);
        /// <summary>
        /// Set the input DataStack for each Domain Script to data
        /// </summary>
        /// <param name="data">The DataStack to pass as input</param>
        void setDomainInput(DataMap data);
        /// <summary>
        /// Set the input DataStack for the Script to data
        /// </summary>
        /// <param name="scriptId">The id of the Script to assign the data to</param>
        /// <param name="data">The DataStack to pass as input</param>
        void setScriptInput(size_t scriptId, DataMap data);
    private:
        string domainName;
        string ownerName = "";
        map<size_t, Script*> scripts;
        UniqueIntegerStack<size_t> domainIds = UniqueIntegerStack<size_t>(1000U);
        void deleteScript(Script* script, optional<id_t> scriptId = nullopt);
    };
}