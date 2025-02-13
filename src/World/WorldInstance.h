#pragma once

#include "SFML/Graphics.hpp"
#include "World.h"
#include "../Scenes/GlobalTime.h"
#include "../ResourceCaches/FontCache.h"
#include "../ResourceCaches/TextureCache.h"

namespace CGEngine {
	extern WindowParameters windowParameters;
	extern Renderer renderer;
	extern World* world;
	extern GlobalTime time;
	extern InputMap* input;
	extern Screen* screen;
	extern TextureCache* textures;
	extern FontCache* fonts;
	extern Font* defaultFont;
	extern function<void()> updateWorld;
	extern function<void()> beginWorld;
	extern vector<Scene*> sceneList;
	extern Logging logging;

	extern const string onUpdateEvent;
	extern const string onStartEvent;
	extern const string onDeleteEvent;
	extern const string onIntersectEvent;
	extern const string onMousePressEvent;
	extern const string onMouseReleaseEvent;
	extern const string onKeyPressEvent;
	extern const string onKeyReleaseEvent;
}