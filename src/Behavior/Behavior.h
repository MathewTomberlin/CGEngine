#include <stack>
#include "../Scripts/ScriptMap.h"
#include "../Types/DataStack.h"

using namespace std;

namespace CGEngine {
	class Behavior {
	public:
		Behavior(Body* owning) : scripts(owning) { owner = owning; };
		Body* owner;
		ScriptMap scripts;
		DataStack input;
		DataStack data;
		DataStack output;
	};
}