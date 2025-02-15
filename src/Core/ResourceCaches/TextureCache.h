#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/System.hpp"
#include <map>
#include <iostream>
using namespace sf;
using namespace std;

namespace CGEngine {
	class TextureCache {
	public:
		bool has(string textureName);
		Texture* get(const filesystem::path& path, string textureName = "");
		Texture* load(const filesystem::path& path, string textureName = "");
		bool unload(string textureName);
		void addTexturePath(string textureName, const filesystem::path& path);
		void removeTexturePath(string textureName);
		void loadPaths();
		void loadPaths(map<string, const filesystem::path&> paths);
	private:
		map<string, Texture*> textures;
		map<string, filesystem::path> texturePaths;
	};
}