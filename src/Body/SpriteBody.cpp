#include "SpriteBody.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
	SpriteBody::SpriteBody(const filesystem::path& tilesetPath, IntRect frame, Transformation handle, Body* parent, Vector2f align) : Body(new Sprite(*textures->get(tilesetPath), frameRect), handle, parent, align) {
		frameRect = frame;
		(get<Sprite*>())->setTextureRect(frameRect);
	}
}