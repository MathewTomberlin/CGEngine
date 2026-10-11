#pragma once

#include "../Types/Types.h"

namespace CGEngine {
	// The look of the whole frame: the colour the screen is cleared to, and distance fog that fades far surfaces into a
	// colour. Surfaces drawn with the standard shader take the fog. The defaults (black sky, no fog) leave a scene as it was.
	struct Environment {
		Vector3f skyColour = { 0.0f, 0.0f, 0.0f };
		Vector3f fogColour = { 0.0f, 0.0f, 0.0f };
		float fogStart = 0.0f;   // the fog begins this far from the camera
		float fogEnd = 0.0f;     // and is complete this far away; the fog is off unless fogEnd is past fogStart
	};
}
