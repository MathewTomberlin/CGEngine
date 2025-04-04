#pragma once
#include <string>
#include <memory>
#include "../Types/Types.h"
#include "../Mesh/Model.h"
using std::string;

namespace CGEngine {
	class AssetLoader {
	public:
		virtual unique_ptr<IResource> load(const filesystem::path& resourcePath) = 0;
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
		unique_ptr<IResource> load(const filesystem::path& resourcePath) override {
			if (filesystem::exists(resourcePath)) {
				auto resource = std::make_unique<TextureResource>();
				auto texture = std::make_unique<Texture>();

				if (texture->loadFromFile(resourcePath.string())) {
					resource->setTexture(texture.release());
					return resource; // Implicitly upcast to IResource*
				}
			}
			return nullptr;
		}
	};

	class FontLoader : public AssetLoader {
	public:
		unique_ptr<IResource> load(const filesystem::path& resourcePath) override {
			if (filesystem::exists(resourcePath)) {
				auto resource = std::make_unique<FontResource>();
				auto font = std::make_unique<Font>(resourcePath);
				resource->setFont(font.release());
				return resource;
			}
			return nullptr;
		}
	};

	class VertexShaderLoader : public AssetLoader {
	public:
		unique_ptr<IResource> load(const filesystem::path& resourcePath) override {
			if (filesystem::exists(resourcePath)) {
				auto shader = std::make_unique<Shader>(Shader::readFile(resourcePath.string(), GL_VERTEX_SHADER));
				return shader;
			}
		}
	};

	class FragmentShaderLoader : public AssetLoader{
	public:
		unique_ptr<IResource> load(const filesystem::path& resourcePath) override {
			if (filesystem::exists(resourcePath)) {
				auto shader = std::make_unique<Shader>(Shader::readFile(resourcePath.string(), GL_FRAGMENT_SHADER));
				return shader;
			}
		}
	};

	class ModelLoader : public AssetLoader {
	public:
		unique_ptr<IResource> load(const filesystem::path& resourcePath) override {
			if (filesystem::exists(resourcePath)) {
				auto model = std::make_unique<Model>(resourcePath.string());
				return model;
			}
		}
	};
}