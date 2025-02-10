#pragma once

#include "SpriteBody.h"
#include <iomanip>

namespace CGEngine {
	struct AnimationParameters {
		AnimationParameters(Vector2i size = { 123,128 }, sec_t rate = 30.0f, float spd = 1.f, bool loop = true, bool startRun = true, int maxAnimFrame = -1) {
			frameSize = size;
			maxFrameRate = rate;
			maxFrame = maxAnimFrame;
			looping = loop;
			startRunning = startRun;
			speed = spd;
		}
		Vector2i frameSize = { 128,128 };
		sec_t maxFrameRate = 30.0f;
		int maxFrame = -1;
		bool looping = true;
		bool startRunning = true;
		float speed = 1.f;
	};

	enum AnimationState { Ready, Running, Paused };

	class SpriteAnimBody : public SpriteBody {
	public:
		SpriteAnimBody(const filesystem::path& path, AnimationParameters animParams, Transformation handle = Transformation(), Body* parent = nullptr, Vector2f align = {0,0});

		void animate(sec_t dt);
		void animateReverse(sec_t dt);
		void start();
		void pause();
		void end();
		AnimationState getState();
		void setSpeed(float speed);
		float getSpeed();
		void setAnimationStartPos(Vector2i pos);

		sec_t frameRate = 30.0f;
		int maxFrame = -1;
		bool looping = true;
		LogLevel logLevel = LogLevel::LogInfo;
	private:
		Vector2i animationStartPos = { 0,0 };
		sec_t frameTime = 0.0f;
		id_t currentFrame = 1;
		sec_t maxFrameDuration = 0.0f;			//Length of time for each frame
		float speed = 0.5f;
		sec_t maxFrameRate = 30.0f;				//If the simulation runs faster than 1/maxFrameRate, another frame won't be advanced until that time has passed
		Vector2u spriteSheetSize = Vector2u();	//The size of the sprite sheet to iterate through
		AnimationState animState = AnimationState::Ready;
		optional<id_t> animUpdateEventId = nullopt;
		void log(string msg);
	};
}