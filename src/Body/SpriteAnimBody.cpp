#include "SpriteAnimBody.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
	SpriteAnimBody::SpriteAnimBody(const filesystem::path& path, AnimationParameters animParams, Transformation handle, Body* parent, Vector2f align) : SpriteBody(path, IntRect({ 0,0 }, animParams.frameSize), handle, parent, align) {
		maxFrameRate = animParams.maxFrameRate;
		setSpeed(animParams.speed);
		looping = animParams.looping;
		maxFrame = animParams.maxFrame;
		spriteSheetSize = textures->get(path)->getSize();
		if (animParams.startRunning) {
			start();
		}
	}

	void SpriteAnimBody::start() {
		if (speed != 0) {
			log("Started");
			animState = AnimationState::Running;
			animUpdateEventId = addUpdateScript(new Script([this](ScArgs args) { speed > 0 ? animate(time.getDeltaSec()) : animateReverse(time.getDeltaSec()); }));
		}
	}

	void SpriteAnimBody::pause() {
		if (animUpdateEventId.has_value()) {
			log("Paused");
			animState = AnimationState::Paused;
			eraseUpdateScript(animUpdateEventId.value(), true);
		}
	}

	void SpriteAnimBody::end() {
		log("Ended");
		frameTime = 0.f;
		frameRect.position = animationStartPos;
		get<Sprite*>()->setTextureRect(frameRect);
		pause();
		animState = AnimationState::Ready;
		currentFrame = 0;
	}

	AnimationState SpriteAnimBody::getState() {
		return animState;
	}

	void SpriteAnimBody::setSpeed(float spd) {
		speed = spd;
		frameRate = maxFrameRate * abs(speed);
		maxFrameDuration = 1.f / frameRate;
		log("Speed set to " + to_string(speed) + " with frame length of " + to_string(maxFrameDuration));
	}

	float SpriteAnimBody::getSpeed() {
		return speed;
	}

	void SpriteAnimBody::animate(sec_t dt) {
		if (maxFrameDuration <= 0) return;

		frameTime += dt;
		if (frameTime > maxFrameDuration) {
			size_t frames = 0;
			while ((frameTime -= maxFrameDuration) > 0.f) {
				frames++;

				if (maxFrame > 0 && currentFrame >= (unsigned)maxFrame) {
					if (looping) {
						currentFrame = 0;
						frameRect.position = animationStartPos;
					}
					else {
						pause();
						break;
					}
				}
				//Advance frame rect
				if (maxFrame < 0 || currentFrame <= (unsigned)maxFrame) {
					log("Frame #" + to_string(currentFrame));
					if ((unsigned)(frameRect.position.x + frameRect.size.x) < spriteSheetSize.x) {
						frameRect.position.x += frameRect.size.x;
						currentFrame++;
					}
					else {
						if ((unsigned)(frameRect.position.y + frameRect.size.y) < spriteSheetSize.y) {
							frameRect.position.y += frameRect.size.y;
							frameRect.position.x = 0;
							currentFrame++;
						}
						else {
							if (looping) {
								frameRect.position = animationStartPos;
								currentFrame++;
							}
							else {
								pause();
								break;
							}
						}
					}
				}
			}
			get<Sprite*>()->setTextureRect(frameRect);
		}
	}

	void SpriteAnimBody::animateReverse(sec_t dt) {
		if (maxFrameDuration <= 0) return;

		frameTime += dt;
		if (frameTime > maxFrameDuration) {
			size_t frames = 0;
			while ((frameTime -= maxFrameDuration) > 0.f) {
				frames++;

				if (maxFrame > 0 && currentFrame >= (unsigned)maxFrame) {
					if (looping) {
						currentFrame = 0;
						frameRect.position = animationStartPos;
					}
					else {
						pause();
						break;
					}
				}
				//Advance frame rect
				if (maxFrame < 0 || currentFrame <= (unsigned)maxFrame) {
					log("Frame #" + to_string(currentFrame));
					if (frameRect.position.x - frameRect.size.x >= 0) {
						frameRect.position.x -= frameRect.size.x;
						currentFrame++;
					}
					else {
						if (frameRect.position.y - frameRect.size.y >= 0) {
							frameRect.position.y -= frameRect.size.y;
							frameRect.position.x = spriteSheetSize.x - frameRect.size.x;
							currentFrame++;
						}
						else {
							if (looping) {
								frameRect.position = animationStartPos;
								currentFrame++;
							}
							else {
								pause();
								break;
							}
						}
					}
				}
			}
			get<Sprite*>()->setTextureRect(frameRect);
		}
	}

	void SpriteAnimBody::setAnimationStartPos(Vector2i pos) {
		animationStartPos = pos;
		end();
		start();
	}

	void SpriteAnimBody::log(string msg) {
		if (!logging.willLog(logLevel)) return;

		string bodyId = getId().has_value() ? "[" + to_string(getId().value()) + "]" : "";
		string bodyName = getName() != "" ? "(" + getName() + ")" : "";
		logging(logLevel, "SpriteAnimBody" + bodyId + bodyName, msg);
	}
}