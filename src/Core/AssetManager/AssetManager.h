#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <filesystem>
#include "../Engine/EngineSystem.h"
#include "../Shader/Program.h"

namespace CGEngine {
	class ShaderResource : public IResource {
	private:
		CGEngine::Shader shader;
	public:
		ShaderResource() = default;

		bool isValid() const override {
			return shader.getObjectId() != 0;
		}

		CGEngine::Shader& getShader() { return shader; }
		const CGEngine::Shader& getShader() const { return shader; }
	};

	// Store resources by type with both name and ID lookup
	struct ResourceEntry {
		shared_ptr<IResource> resource;
		string name;
	};

	struct ResourceContainer {
		UniqueDomain<id_t, ResourceEntry> resources{ 1000 };
		unordered_map<string, id_t> nameToId;
	};

	class AssetManager : public EngineSystem {
	public:
		AssetManager() {
			// Register built-in resource types
			registerResourceType<TextureResource>("textures");
			registerResourceType<FontResource>("fonts");
			registerResourceType<ShaderResource>("shaders");
			registerResourceType<Program>("programs");
			registerResourceType<Material>("materials");
			registerResourceType<Body>("bodies");
		}

		// Add new initialization method
		void initialize() {
			//Valid default texture and program required for default material creation
			Program* defaultProgram = nullptr;
			TextureResource* defaultTexture = nullptr;

			//Load default texture
			optional<id_t> defaultTextureId = setDefaultId<TextureResource>(load<TextureResource>("checkered_tile.png", defaultTextureName));
			if(defaultTextureId.has_value()){
				defaultTexture = get<TextureResource>(defaultTextureId.value());
			} else {
				logMessage(LogError, "Failed to load default texture!");
			}

			//Create default program
			optional<id_t> defaultProgramId = setDefaultId<Program>(create<Program>(defaultProgramName, "shaders/StdVertexShader.vert", "shaders/StdFragShader.frag", defaultProgramName));
			if (defaultProgramId.has_value()) {
				defaultProgram = get<Program>(defaultProgramId.value());
			} else {
				logMessage(LogError, "Failed to create default program!");
			}

			//If default texture was loaded and default program was created, create default material
			if (defaultTexture && defaultProgram) {
				optional<id_t> defaultMaterialId = setDefaultId<Material>(create<Material>(defaultMaterialName, SurfaceParameters(SurfaceDomain("default_texture")), defaultProgram));
				if (!defaultMaterialId.has_value()) {
					logMessage(LogError, "Failed to create default material!");
				}
			}

			//Load default font
			optional<id_t> defaultFontId = setDefaultId<FontResource>(load<FontResource>("fonts/defaultFont.ttf", defaultFontName));
			if (!defaultFontId.has_value()) {
				logMessage(LogError, "Failed to load default font!");
			}
		}

		~AssetManager() {
			clear();
		}

		/**
		* Register a new resource type with the manager
		* @param typeName Name of the collection for this resource type
		*/
		template<typename T>
		void registerResourceType(const string& typeName) {
			type_index typeId = type_index(typeid(T));
			resourceContainers[typeId] = make_pair(typeName, ResourceContainer());
			resourceDefaultIds[typeId] = nullopt;
			string logMsg = string("Registered resource type: ").append(typeName);
			logMessage(LogInfo, logMsg);
		}

		void logMessage(LogLevel level, const string& msg) {
			if (level <= logLevel) {
				cout << "[" << logLevels[(int)level] << "] AssetManager: " << msg << "\n";
			}
		}

		/**
		* Get a resource of T type by name
		* @param resourceName Name of the resource to retrieve
		* @return Pointer to the resource or nullptr if not found
		*/
		template<typename T>
		T* get(const string& resourceName) {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to get unregistered resource type: ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullptr;
			}

			auto& container = resourceContainers[typeId].second;
			auto it = container.nameToId.find(resourceName);
			if (it != container.nameToId.end()) {
				return static_cast<T*>(container.resources.get(it->second).resource.get());
			}

			return nullptr;
		}

		/**
		* Get a resource of T type by id
		* @param id Id of the resource to retrieve
		* @return Pointer to the resource or nullptr if not found
		*/
		template<typename T>
		T* get(id_t id) {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to get unregistered resource type: ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullptr;
			}

			auto& container = resourceContainers[typeId].second;
			// Get the ResourceEntry directly, not as a pointer
			ResourceEntry entry = container.resources.get(id);
			// Check if it's valid (we could check if the resource pointer is not null)
			if (entry.resource) {
				return static_cast<T*>(entry.resource.get());
			}
			return nullptr;
		}

		string getResourceName(const filesystem::path& path, const string& providedName) {
			return providedName.empty() ? path.filename().string() : providedName;
		}

		/**
		* Get a resource of T type by name
		* @param resourceName Name of the resource to retrieve
		* @return Optional id of resource. Nullopt if not found.
		*/
		template<typename T>
		optional<id_t> getId(const string& resourceName) {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to get ID for unregistered resource type: ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullopt; // Invalid resource type
			}

			auto& container = resourceContainers[typeId].second;
			auto it = container.nameToId.find(resourceName);
			if (it != container.nameToId.end()) {
				return it->second;
			}

			return nullopt; // Resource not found
		}

		/**
		* Get a resource of T type by id
		* @param id Id of the resource to retrieve
		* @return Optional id of the resource. Nullopt if not found.
		*/
		template<typename T>
		optional<id_t> getId(const T* resource) {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to get ID for unregistered resource type: ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullopt; //Invalid resource type
			}

			auto& container = resourceContainers[typeId].second;
			// Search through resources to find matching pointer
			for (const auto& [id, entry] : container.resources) {
				if (static_cast<T*>(entry.resource.get()) == resource) {
					return id;
				}
			}

			return nullopt; // Resource not found
		}

		/**
		 * Check if a resource exists
		 * @param resourceName Name of the resource to check
		 * @return True if the resource exists
		 */
		template<typename T>
		bool has(const string& resourceName) {
		std::type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				return false;
			}

			auto& container = resourceContainers[typeId].second;
			return container.nameToId.find(resourceName) != container.nameToId.end();
		}

		/**
		 * Clear all resources of a specific type
		 */
		template<typename T>
		void clearType() {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				return;
			}

			auto& container = resourceContainers[typeId].second;
			container.resources.clear();
			container.nameToId.clear();
			string logMsg = string("Cleared all resources of type: ").append(resourceContainers[typeId].first);
			logMessage(LogInfo, logMsg);
		}

		/**
		* Clear all resources
		*/
		void clear() {
			for (auto& [typeId, typePair] : resourceContainers) {
				typePair.second.resources.clear();
				typePair.second.nameToId.clear();
				string logMsg = string("Cleared all resources of type : ").append(typePair.first);
				logMessage(LogInfo, logMsg);
			}
		}

		/**
		* Load a Texture, Font, or Shader resource from a file path
		* @param resourcePath Path to the resource file
		* @param resourceName Name to reference the resource (defaults to filename)
		* @param args Arguments to decode for loading. Used to pass shaderType.
		* @return Optional id of loaded resource. Nullopt if loading failed.
		*/
		//TODO: Implement AssetLoader classes to allow user-defined asset loading
		template<typename T, typename ...Args>
		optional<id_t> load(const filesystem::path& resourcePath, const string& resourceName = "", Args... args) {
			//Require a resourcePath or resourceName
			if (resourcePath.empty() && resourceName.empty()) return nullopt;
			type_index resourceTypeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to load unregistered resource type: ").append(typeid(T).name());
				logMessage(LogWarn, logMsg);
				return nullopt;
			}

			//Use resource filename if resourceName is empty
			string assetName = resourceName.empty() ? resourcePath.filename().string() : resourceName;

			//Find existing resource with assetName
			optional<id_t> existingResourceId = getId<T>(assetName);
			if (existingResourceId.has_value()) {
				T* existingResource = get<T>(existingResourceId.value());
				//Return if existingResource is valid or remove the container mapping if not
				if (existingResource && existingResource->isValid()) {
					logMessage(LogInfo, string("Found '").append(resourceContainers[resourceTypeId].first).append("' Resource '").append(resourceName).append("' with path '").append(resourcePath.filename().string()).append("'"));
					return existingResourceId.value();
				} else {
					//Remove container mapping for invalid resource
					auto& container = resourceContainers[resourceTypeId].second;
					container.nameToId.erase(assetName);
					container.resources.remove(existingResourceId.value());
					logMessage(LogWarn, "Invalid resource mapping. Deleting '" + assetName+"'");
				}
			}

			//TODO: Replace hard-coded resource loading with AssetLoader classes for each resource type to allow extensibility
			T* resource = nullptr;
			if constexpr (is_same_v<T, TextureResource>) {
				//Ensure the resource loading path exists
				if (filesystem::exists(resourcePath)) {
					resource = new TextureResource();
					Texture* texture = new Texture();
					if (texture->loadFromFile(resourcePath.string())) {
						resource->setTexture(texture);
					} else {
						string logMsg = string("Failed to load texture: ").append(resourcePath.string());
						logMessage(LogError, logMsg);
						delete resource;
						delete texture;
						return getDefaultId<TextureResource>();
					}
				}
				else {
					string logMsg = string("Texture file not found: ").append(resourcePath.string());
					logMessage(LogError, logMsg);
					return getDefaultId<TextureResource>();
				}
			}
			else if constexpr (is_same_v<T, FontResource>) {
				resource = new FontResource();
				if (filesystem::exists(resourcePath)) {
					try {
						Font* font = new Font(resourcePath);
						resource->setFont(font);
					} catch (sf::Exception ex) {
						string logMsg = string("Font failed to load: ").append(ex.what());
						logMessage(LogInfo, logMsg);
						delete resource;
						return getDefaultId<FontResource>();
					}
				}
				else {
					return getDefaultId<FontResource>();
				}
			} else if constexpr (is_same_v<T, Shader>) {
				auto args = { (forward<Args>(args))... };
				GLenum shaderType = args[0];
				Shader shader = Shader::readFile(resourcePath, shaderType);
				if (!shader.isValid()) {
					logMessage(LogWarn, string("Shader loaded from '").append(resourcePath.filename().string()).append("' is invalid!"));
				}
			} else {
				string logMsg = string("No loader implemented for resource type ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullopt;
			}

			if (resource) {
				id_t resourceId = addResource<T>(assetName, resource);
				resource->setId(resourceId);
				string logMsg = string("Loaded '").append(resourceContainers[resourceTypeId].first).append("' Resource '").append(assetName).append("' from '").append(resourcePath.filename().string()).append("' ID:").append(to_string(resourceId));
				logMessage(LogInfo, logMsg);
				logMessage(LogInfo, string("Resource '").append(resourceContainers[resourceTypeId].first).append("' Count:").append(to_string(resourceContainers[resourceTypeId].second.resources.size())));
				return resourceId;
			}

			return nullopt;
		}

		/**
		* Create a resource from memory rather than loading from disk
		* @param resourceName Name to reference the resource
		* @param args Arguments to forward to the resource constructor
		* @return Optional id of created resource. Nullopt if creation failed.
		*/
		template<typename T, typename... Args>
		optional<id_t> create(const std::string& resourceName, Args&&... args) {
			std::type_index resourceTypeId = std::type_index(typeid(T));
			if (!hasResourceType<T>()) {
				string logMsg = string("Attempted to create unregistered resource type: ").append(typeid(T).name());
				logMessage(LogInfo, logMsg);
				return nullopt;
			}

			//Find existing resource with assetName
			optional<id_t> existingResourceId = getId<T>(resourceName);
			if (existingResourceId.has_value()) {
				//Return if existingResource is valid or remove the container mapping if not
				T* existingResource = get<T>(existingResourceId.value());
				if (existingResource && existingResource->isValid()) {
					logMessage(LogInfo, string("Found '").append(resourceContainers[resourceTypeId].first).append("' Resource: ").append(resourceName));
					return existingResourceId.value();
				} else {
					//Remove container mapping for invalid resource
					auto& container = resourceContainers[resourceTypeId].second;
					container.nameToId.erase(resourceName);
					container.resources.remove(existingResourceId.value());
					logMessage(LogWarn, "Invalid resource mapping. Deleting '" + resourceName + "'");
				}
			}

			//Create the resource with unpacked Args...
			T* resource = new T(std::forward<Args>(args)...);
			if (!resource){
				logMessage(LogInfo, string("Failed to create resource: ").append(resourceName));
				delete resource;
				return nullopt;
			} else if (!static_cast<IResource*>(resource)->isValid()) {
				logMessage(LogInfo, string("Invalid resource was created: ").append(resourceName));
				delete resource;
				return nullopt;
			}

			id_t resourceId = addResource<T>(resourceName, resource);
			resource->setId(resourceId);
			string logMsg = string("Created '").append(resourceContainers[resourceTypeId].first).append("' Resource '").append(resourceName).append("' ID:").append(to_string(resourceId));
			logMessage(LogInfo, logMsg);
			logMessage(LogInfo, string("Resource '").append(resourceContainers[resourceTypeId].first).append("' Count: ").append(to_string(resourceContainers[resourceTypeId].second.resources.size())));
			return resourceId;
		}

		//Get the default id for T type
		template<typename T>
		optional<id_t> getDefaultId() {
			type_index typeId = type_index(typeid(T));
			return resourceDefaultIds[typeId];
		}
		string defaultTextureName = "default_texture";
		string defaultProgramName = "default_program";
		string defaultMaterialName = "default_material";
		string defaultFontName = "default_font";
		unordered_map<type_index, optional<id_t>> resourceDefaultIds;
	private:
		//ResourceTypeName string, ResourceContainer mapped to ResourceType type_index
		unordered_map<type_index, pair<string, ResourceContainer>> resourceContainers;

		//Check if the resource type is registered
		template<typename T>
		bool hasResourceType() {
			type_index typeId = type_index(typeid(T));
			return resourceContainers.find(typeId) != resourceContainers.end();
		}

		//Create a ResourceEntry with a shared pointer to the resource of T type and its name,
		//then add that to the container.resources and container.nameToId
		template<typename T>
		id_t addResource(const string& name, T* resource) {
			auto& container = getContainer<T>();
			ResourceEntry entry{shared_ptr<IResource>(resource),name};
			id_t id = container.resources.add(entry);
			container.nameToId[name] = id;
			return id;
		}

		//Get the ResourceContainer of the indicated T type
		template<typename T>
		ResourceContainer& getContainer() {
			type_index typeId = type_index(typeid(T));
			return resourceContainers[typeId].second;
		}

		//Set the default id for T type
		template<typename T>
		optional<id_t> setDefaultId(optional<id_t> defaultId) {
			type_index typeId = type_index(typeid(T));
			resourceDefaultIds[typeId] = defaultId;
			return defaultId;
		}
	};
}