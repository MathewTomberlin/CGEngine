#pragma once

#include "../World/Renderer.h"
#include "../World/World.h"
#include "../Input/InputMap.h"
#include "../Time/GlobalTime.h"
#include "SFML/Graphics.hpp"
#include "../AssetManager/AssetManager.h"

namespace CGEngine {
	extern WindowParameters windowParameters;
	extern OpenGLSettings openGLSettings;
	extern Renderer renderer;
	extern World* world;
	extern GlobalTime time;
	extern InputMap* input;
	extern Screen* screen;
	extern function<void()> updateWorld;
	extern function<void()> beginWorld;
	extern function<Camera*()> getCamera;
	extern vector<Behavior*> sceneList;
	extern Logging log;
	extern AssetManager assets;

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