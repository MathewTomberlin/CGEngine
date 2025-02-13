#pragma once

#include <stack>
#include "../Scripts/ScriptMap.h"

using namespace std;
class Body;

namespace CGEngine {
	class Behavior {
	public:
		Behavior(Body* owning) : scripts(owning) {
			owner = owning; 
		};
		Body* owner;
		ScriptMap scripts;

		DataMap input;
		DataMap data;
		DataMap output;
	};
}