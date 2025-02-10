#pragma once

#include "Body.h"
#include <iomanip>

namespace CGEngine {
	class SpriteBody : public Body {
	public:
		SpriteBody(const filesystem::path& tilesetPath, IntRect frame, Transformation handle = Transformation(), Body* parent = nullptr, Vector2f align = {0,0});

		IntRect frameRect = IntRect();			//This rect should be sized to the frame and moved from frame to frame
	};
}