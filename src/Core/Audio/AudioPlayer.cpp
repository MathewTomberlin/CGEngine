#include <cstdlib>
#include "AudioPlayer.h"

namespace CGEngine {
	namespace {
		// Effects that can sound at once: a burst of breaks or bites keeps each one, up to this many.
		const size_t EffectVoices = 8;

		// The folder the sounds are in, beside the executable (resources/sounds is copied there at build time).
		std::string pathOf(const std::string& name) {
			return "sounds/" + name + ".wav";
		}

		bool unattended() {
			return std::getenv("CGENGINE_UNATTENDED") != nullptr;
		}
	}

	const sf::SoundBuffer* AudioPlayer::buffer(const std::string& name) {
		auto found = buffers.find(name);
		if (found != buffers.end()) return found->second.get();
		auto loaded = std::make_unique<sf::SoundBuffer>();
		if (!loaded->loadFromFile(pathOf(name))) return nullptr;
		const sf::SoundBuffer* pointer = loaded.get();
		buffers.emplace(name, std::move(loaded));
		return pointer;
	}

	bool AudioPlayer::playEffect(const std::string& name, float volume, float pitch) {
		++effects;
		if (unattended()) return true;
		const sf::SoundBuffer* sample = buffer(name);
		if (!sample) return false;
		// The next voice is used even while it still plays: the newest sound wins, and a burst keeps the last few.
		if (voices.size() < EffectVoices) {
			voices.push_back(std::make_unique<sf::Sound>(*sample));
		}
		sf::Sound& voice = *voices[nextVoice];
		nextVoice = (nextVoice + 1) % EffectVoices;
		voice.setBuffer(*sample);
		voice.setVolume(volume * 100.0f);
		voice.setPitch(pitch);
		voice.play();
		return true;
	}

	bool AudioPlayer::playMusic(const std::string& name, float volume) {
		if (unattended()) return true;
		auto track = std::make_unique<sf::Music>();
		if (!track->openFromFile(pathOf(name))) return false;
		track->setLooping(true);
		track->setVolume(volume * 100.0f);
		track->play();
		music = std::move(track);
		return true;
	}

	void AudioPlayer::stopMusic() {
		if (music) music->stop();
		music.reset();
	}
}
