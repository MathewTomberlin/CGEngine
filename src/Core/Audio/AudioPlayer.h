#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <SFML/Audio.hpp>

namespace CGEngine {
	// Sound effects and music from the sounds/ folder beside the executable. Effects play on a few voices, so a burst of them
	// does not cut off the one before. Music streams from its file and loops until it is stopped. Under CGENGINE_UNATTENDED
	// (the tests) nothing is played, but the calls are counted, so a test sees that the game asked for a sound.
	class AudioPlayer {
	public:
		// Plays the effect sounds/<name>.wav at a volume (0 to 1) and a pitch (1 is as recorded). False if the file cannot be
		// loaded, or the audio device refuses the sound.
		bool playEffect(const std::string& name, float volume, float pitch);

		// Loops the music sounds/<name>.wav at a volume (0 to 1), replacing any music that plays. False if it cannot be opened.
		bool playMusic(const std::string& name, float volume);
		void stopMusic();

		// How many effects the game has asked for (played or not), for the tests and the state file.
		int effectCount() const { return effects; }

	private:
		const sf::SoundBuffer* buffer(const std::string& name);

		std::map<std::string, std::unique_ptr<sf::SoundBuffer>> buffers;   // a buffer stays where it is, so a voice may hold it
		std::vector<std::unique_ptr<sf::Sound>> voices;                    // the effect voices, used round-robin
		size_t nextVoice = 0;
		std::unique_ptr<sf::Music> music;
		int effects = 0;
	};
}
