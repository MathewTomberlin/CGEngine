#include "TextureCache.h"

namespace CGEngine {
	void TextureCache::loadPaths() {
		for (auto iterator = texturePaths.begin(); iterator != texturePaths.end(); iterator++) {
			const filesystem::path& path = (*iterator).second;
			load((*iterator).second, (*iterator).first);
		}
	}

	void TextureCache::loadPaths(map<string, const filesystem::path&> paths) {
		for (auto iterator = paths.begin(); iterator != paths.end(); iterator++) {
			const filesystem::path& path = (*iterator).second;
			load((*iterator).second, (*iterator).first);
		}
	}

	bool TextureCache::has(string textureName) {
		return textures.find(textureName) != textures.end();
	}

	Texture* TextureCache::get(const filesystem::path& path, string textureName) {
		if (path != "") {
			if (textureName == "") {
				textureName = path.filename().string();
			}
		}

		if (textures.find(textureName) != textures.end()) {
			return textures[textureName];
		}

		return load(path, textureName);;
	};

	Texture* TextureCache::load(const filesystem::path& path, string textureName) {
		if (path == "") return nullptr;

		Texture* loaded = new Texture();
		if (!loaded->loadFromFile(path)) {
			cout << "Failed to load textures at path '" << path << "'";
		}
		if (textureName == "") textureName = path.filename().string();
		textures[textureName] = loaded;
		return loaded;
	}

	void TextureCache::addTexturePath(string textureName, const filesystem::path& path) {
		texturePaths[textureName] = path;
	}
	void TextureCache::removeTexturePath(string textureName) {
		texturePaths.erase(textureName);
	}

	bool TextureCache::unload(string textureName) {
		if (textures.find(textureName) != textures.end()) {
			delete textures[textureName];
			textures.erase(textureName);
			return true;
		}
		return false;
	}
}