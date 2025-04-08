#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>
#include <filesystem>
#include "../Engine/EngineSystem.h"
#include "../Shader/Program.h"
#include "AssetLoader.h"
#include "../Material/Material.h"
#include "../Animation/Animation.h"
namespace CGEngine {
	class VertexShaderResource : public IResource {
	private:
		CGEngine::Shader shader;
	public:
		VertexShaderResource() = default;

		bool isValid() const override {
			return shader.getObjectId() != 0;
		}

		CGEngine::Shader& getShader() { return shader; }
		const CGEngine::Shader& getShader() const { return shader; }
	};

	class FragmentShaderResource : public IResource {
	private:
		CGEngine::Shader shader;
	public:
		FragmentShaderResource() = default;

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
			registerResourceType<TextureResource>("textures", make_unique<TextureLoader>());
			registerResourceType<FontResource>("fonts", make_unique<FontLoader>());
			registerResourceType<VertexShaderResource>("vertexShaders", make_unique<VertexShaderLoader>());
			registerResourceType<FragmentShaderResource>("fragmentShaders", make_unique<FragmentShaderLoader>());
			registerResourceType<Model>("models", make_unique<ModelLoader>());
			registerResourceType<MeshData>("meshData");
			registerResourceType<Program>("programs");
			registerResourceType<Material>("materials");
			registerResourceType<Body>("bodies");
			registerResourceType<Light>("lights");
			registerResourceType<Animation>("animations");
			registerResourceType<Skeleton>("skeletons");
		}

		// Add new initialization method
		void initialize() {
			//Load default texture
			optional<id_t> defaultTextureId = setDefaultId<TextureResource>(load<TextureResource>("checkered_tile.png", defaultTextureName));
			if(!defaultTextureId.has_value()){
				logMessage(LogError, "Failed to load default texture!");
			}

			//Create default program
			optional<id_t> defaultProgramId = setDefaultId<Program>(create<Program>(defaultProgramName, "shaders/StdVertexShader.vert", "shaders/StdFragShader.frag", defaultProgramName));
			if (!defaultProgramId.has_value()) {
				logMessage(LogError, "Failed to create default program!");
			}

			//If default texture was loaded and default program was created, create default material
			if (defaultTextureId.has_value() && defaultProgramId.has_value()) {
				optional<id_t> defaultMaterialId = setDefaultId<Material>(create<Material>(defaultMaterialName, SurfaceParameters(SurfaceDomain("default_texture")), get<Program>(defaultProgramId.value())));
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
		void registerResourceType(const string& typeName, unique_ptr<AssetLoader> loader = nullptr) {
			type_index typeId = type_index(typeid(T));
			resourceContainers[typeId] = make_pair(typeName, ResourceContainer());
			resourceDefaultIds[typeId] = nullopt;
			if (loader) {
				resourceLoaders[typeId] = move(loader);
			}
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
			if (!container.resources.has(id)) {
				// Log potentially? A request for a non-existent ID might be noteworthy.
				logMessage(LogDebug, string("Attempted to get resource with non-existent ID: ").append(to_string(id)));
				return nullptr; // ID doesn't exist, return nullptr immediately.
			}
			// Get the ResourceEntry directly, not as a pointer
			const ResourceEntry& entry = container.resources.get(id);
			if (entry.resource) {
				return static_cast<T*>(entry.resource.get());
			} else {
				// Log potentially? An existing entry with a null resource might indicate an issue.
				logMessage(LogWarn, string("ResourceEntry found for ID but resource ptr is null: ").append(to_string(id)));
				return nullptr; // Entry exists, but the resource pointer is null.
			}
		}

		IResource* get(type_index typeId, id_t id) {
			if (!hasResourceType(typeId)) {
				string logMsg = string("Attempted to get unregistered resource type: ").append(typeId.name());
				logMessage(LogInfo, logMsg);
				return nullptr;
			}

			auto& container = resourceContainers[typeId].second;
			// Get the ResourceEntry directly, not as a pointer
			ResourceEntry entry = container.resources.get(id);
			// Check if it's valid (we could check if the resource pointer is not null)
			if (entry.resource) {
				return static_cast<IResource*>(entry.resource.get());
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

		template<typename T>
		size_t getResourceCount() {
			type_index typeId = type_index(typeid(T));
			if (!hasResourceType<T>()) {
				return 0;
			}
			auto& container = resourceContainers[typeId].second;
			return container.resources.size();
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
		template<typename T>
		optional<id_t> load(const filesystem::path& resourcePath, const string& resourceName = "") {
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

			unique_ptr<IResource> resource = nullptr;
			if (resourceLoaders.find(resourceTypeId) == resourceLoaders.end()) {
				string logMsg = string("No loader implemented for resource type: ").append(typeid(T).name());
				logMessage(LogError, logMsg);
				return nullopt;
			}
			else {
				logMessage(LogInfo, string("Using loader for resource type: ").append(typeid(T).name()));
				//Load the resource using the appropriate loader
				resource = resourceLoaders[resourceTypeId]->load(resourcePath);
				//If resource was not loaded successfully and the resourceType has a defaultId
				if (!resource && hasDefaultId(resourceTypeId)) {
					//Get the default id for the resource type and try to load it
					optional<id_t> defaultResourceId = getDefaultId(resourceTypeId);
					if (defaultResourceId.has_value()) {
						if (auto defaultResource = get(resourceTypeId, defaultResourceId.value())) {
							// Create a new instance of the default resource
							resource = unique_ptr<IResource>(defaultResource);
						}
					}
				}
			}

			if (resource) {
				// Get raw pointer before transferring ownership
				T* rawPtr = dynamic_cast<T*>(resource.get());
				if (!rawPtr) {
					logMessage(LogError, string("Resource type mismatch when loading: ").append(assetName));
					return nullopt;
				}
				id_t resourceId = add<T>(assetName, std::move(resource));
				rawPtr->setId(resourceId);
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
				}
				else {
					//Remove container mapping for invalid resource
					auto& container = resourceContainers[resourceTypeId].second;
					container.nameToId.erase(resourceName);
					container.resources.remove(existingResourceId.value());
					logMessage(LogWarn, "Invalid resource mapping. Deleting '" + resourceName + "'");
				}
			}

			//Create the resource with unpacked Args...
			unique_ptr<T> resource = make_unique<T>(forward<Args>(args)...);
			if (!resource) {
				logMessage(LogInfo, string("Failed to create resource: ").append(resourceName));
				return nullopt;
			}
			else if (!static_cast<IResource*>(resource.get())->isValid()) {
				logMessage(LogInfo, string("Invalid resource was created: ").append(resourceName));
				return nullopt;
			}

			// Store raw pointer for setting ID after ownership transfer
			T* rawPtr = resource.get();

			// Transfer ownership to add method
			id_t resourceId = add<T>(resourceName, std::move(resource));
			rawPtr->setId(resourceId);

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

		optional<id_t> getDefaultId(type_index typeId) {
			return resourceDefaultIds[typeId];
		}

		bool hasDefaultId(type_index typeId) {
			return resourceDefaultIds.find(typeId) != resourceDefaultIds.end();
		}

		//Create a ResourceEntry with a shared pointer to the resource of T type and its name,
		//then add that to the container.resources and container.nameToId
		template<typename T>
		id_t add(const string& name, unique_ptr<IResource> resource) {
			auto& container = getContainer<T>();
			// Create the ResourceEntry with a shared_ptr that takes ownership from the unique_ptr
			ResourceEntry entry{ std::shared_ptr<IResource>(resource.release()), name };
			id_t id = container.resources.add(entry);
			container.nameToId[name] = id;
			return id;
		}

		string defaultTextureName = "default_texture";
		string defaultProgramName = "default_program";
		string defaultMaterialName = "default_material";
		string defaultFontName = "default_font";
		unordered_map<type_index, optional<id_t>> resourceDefaultIds;
	private:
		//ResourceTypeName string, ResourceContainer mapped to ResourceType type_index
		unordered_map<type_index, pair<string, ResourceContainer>> resourceContainers;
		unordered_map<type_index, unique_ptr<AssetLoader>> resourceLoaders;

		//Check if the resource type is registered
		template<typename T>
		bool hasResourceType() {
			type_index typeId = type_index(typeid(T));
			return resourceContainers.find(typeId) != resourceContainers.end();
		}

		bool hasResourceType(type_index typeId) {
			return resourceContainers.find(typeId) != resourceContainers.end();
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