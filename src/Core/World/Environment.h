#pragma once

#include <optional>
#include "../Types/Types.h"

namespace CGEngine {
	// The look of the whole frame: the colour the screen is cleared to, and distance fog that fades far surfaces into a
	// colour. Surfaces drawn with the standard shader take the fog. The defaults (black sky, no fog) leave a scene as it was.
	struct Environment {
		Vector3f skyColour = { 0.0f, 0.0f, 0.0f };   // the sky at the horizon; the whole sky when no zenith colour is set
		std::optional<Vector3f> zenithColour;        // the sky straight up; unset, the sky is one colour
		Vector3f fogColour = { 0.0f, 0.0f, 0.0f };
		float fogStart = 0.0f;   // the fog begins this far from the camera
		float fogEnd = 0.0f;     // and is complete this far away; the fog is off unless fogEnd is past fogStart
		Vector3f sunDirection = { 0.0f, 1.0f, 0.0f }; // the direction towards the sun
		Vector3f sunColour = { 0.0f, 0.0f, 0.0f };    // black: no sun disc
		float sunSize = 1.5f;                         // the sun's angular radius, in degrees
		bool shadows = false;                         // the sun casts shadows, from a map that covers shadowRadius around the camera
		float shadowRadius = 48.0f;                   // half the width of the shadowed area, in blocks

		// Whether a sky pass is needed: anything but the default black sky with no sun.
		bool drawsSky() const {
			return zenithColour.has_value() || skyColour.x != 0.0f || skyColour.y != 0.0f || skyColour.z != 0.0f
				|| sunColour.x != 0.0f || sunColour.y != 0.0f || sunColour.z != 0.0f;
		}
	};
}
