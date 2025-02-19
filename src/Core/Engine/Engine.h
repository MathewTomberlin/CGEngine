#pragma once

#include "SFML/Graphics.hpp"
#include "../World/World.h"
#include "../World/Renderer.h"
#include "../Input/InputMap.h"
#include "../Time/GlobalTime.h"
#include "../ResourceCaches/FontCache.h"
#include "../ResourceCaches/TextureCache.h"

namespace CGEngine {
	extern WindowParameters windowParameters;
	extern OpenGLSettings openGLSettings;
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
	extern function<Camera*()> getCamera;
	extern vector<Behavior*> sceneList;
	extern Logging logging;

	extern const string onUpdateEvent;
	extern const string onStartEvent;
	extern const string onDeleteEvent;
	extern const string onIntersectEvent;
	extern const string onMousePressEvent;
	extern const string onMouseReleaseEvent;
	extern const string onKeyPressEvent;
	extern const string onKeyReleaseEvent;
	extern const string onLoadEvent;

	extern const float minHolographicNearClip;
}