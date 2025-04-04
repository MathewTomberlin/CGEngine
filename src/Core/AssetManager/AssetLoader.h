#pragma once
#include <string>
#include "../Types/Types.h"
#include "../Mesh/Model.h"
using std::string;

namespace CGEngine {
	class AssetLoader {
	public:
		virtual IResource* load(const filesystem::path& resourcePath) = 0;
	protected:
		LogLevel logLevel = LogLevel::LogInfo;
		void logMessage(LogLevel level, const string& msg) {
			if (level <= logLevel) {
				cout << "[" << logLevels[(int)level] << "] AssetManager: " << msg << "\n";
			}
		}
	};

	class TextureLoader : public AssetLoader{
	public:
		TextureResource* load(const filesystem::path& resourcePath) override {
			TextureResource* resource = nullptr;
			//Ensure the resource loading path exists
			if (filesystem::exists(resourcePath)) {
				resource = new TextureResource();
				Texture* texture = new Texture();
				if (texture->loadFromFile(resourcePath.string())) {
					resource->setTexture(texture);
				}
				else {
					string logMsg = string("Failed to load texture: ").append(resourcePath.string());
					logMessage(LogError, logMsg);
					delete resource;
					delete texture;
					return nullptr;
				}
			}
			else {
				string logMsg = string("Texture file not found: ").append(resourcePath.string());
				logMessage(LogError, logMsg);
				return nullptr;
			}
			return resource;
		}
	};

	class FontLoader : public AssetLoader {
	public:
		FontResource* load(const filesystem::path& resourcePath) override {
			FontResource* resource = new FontResource();
			//Ensure the resource loading path exists
			if (filesystem::exists(resourcePath)) {
				try {
					Font* font = new Font(resourcePath);
					resource->setFont(font);
				}
				catch (sf::Exception ex) {
					string logMsg = string("Font failed to load: ").append(ex.what());
					logMessage(LogInfo, logMsg);
					delete resource;
					return nullptr;
				}
			}
			else {
				delete resource;
				return nullptr;
			}
		}
	};

	class VertexShaderLoader : public AssetLoader {
	public:
		Shader* load(const filesystem::path& resourcePath) override {
			Shader* shader = new Shader(Shader::readFile(resourcePath.string(), GL_VERTEX_SHADER));
			if (!shader->isValid()) {
				logMessage(LogWarn, string("Vertex Shader loaded from '").append(resourcePath.filename().string()).append("' is invalid!"));
				return nullptr;
			}
			return shader;
		}
	};

	class FragmentShaderLoader : public AssetLoader{
	public:
		Shader* load(const filesystem::path& resourcePath) {
			Shader* shader = new Shader(Shader::readFile(resourcePath.string(), GL_FRAGMENT_SHADER));
			if (!shader->isValid()) {
				logMessage(LogWarn, string("Fragment Shader loaded from '").append(resourcePath.filename().string()).append("' is invalid!"));
				return nullptr;
			}
			return shader;
		}
	};

	class ModelLoader : public AssetLoader {
	public:
		Model* load(const filesystem::path& resourcePath) override {
			Model* model = new Model(resourcePath.string());
			if (!model || !model->isValid()) {
				logMessage(LogError, string("Failed to load model from '").append(resourcePath.filename().string()).append("'"));
				delete model;
				return nullptr;
			}
			return model;
		}
	};
}