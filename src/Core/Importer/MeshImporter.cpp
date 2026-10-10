#include <filesystem>
#include "../World/Renderer.h"
#include "../Engine/Engine.h"
#include "MeshImporter.h"
#include "../Mesh/Mesh.h"
#include "../Mesh/Model.h"
#include "../Animation/Animation.h"

namespace CGEngine {
	MeshImporter::MeshImporter() {
		init();
	}
    ImportResult MeshImporter::importModel(string path, const string& skeletonName, unsigned int options) {
		string format = getFormat(path);
		options |= getFormatOptions(format);

        const aiScene* scene = modelImporter.ReadFile(path, options);
        if (scene != nullptr) {
			//Import all materials from the scene. Model materials is the ordered vector of world material ids for the imported materials
			vector<id_t> modelMaterials = importSceneMaterials(scene);
			//Tracks the model skeletal state and total bones during recursive node visits
			map<string, BoneData> modelBones;
			importPath = path;
			//Import all nodes from the scene recursively
			ImportResult result = processNode(scene->mRootNode, scene, modelBones, modelMaterials);
			result.materials = modelMaterials;
			//If skeletonName is empty or if that skeleton exists but doesn't have matching bones, use a name that will create a new skeleton
			string newSkeletonName = (skeletonName.empty() || (!skeletonName.empty() && assets.get<Skeleton>(skeletonName) && !assets.get<Skeleton>(skeletonName)->equals(modelBones))) ? path.append("_Skeleton") : skeletonName;
			auto skeletonAsset = assets.create<Skeleton>(newSkeletonName, modelBones);
			if (skeletonAsset.has_value()) {
				result.skeleton = assets.get<Skeleton>(skeletonAsset.value().first);
			}
			//Import animations for the model bones
			importAnimations(scene, result.skeleton, result.animations);
			return result;
        } else {
			log(this, LogError, "Failed to import from {}", path);
        }
        return ImportResult();
    }

	void MeshImporter::importAnimations(const aiScene* scene, Skeleton* skeleton, vector<string>& modelAnimations) {
		if (scene->HasAnimations()){
			log(this, LogInfo, "- Importing {} Animations", scene->mNumAnimations);

			int animationCount = scene->mNumAnimations;
			// Process each animation in the file
			for (int i = 0; i < animationCount; i++) {
				if (!scene) continue;
				aiAnimation* anim = scene->mAnimations[i];
				string animName = anim->mName.length > 0 ? anim->mName.C_Str() : scene->mAnimations[i]->mName.C_Str();
				if (animName.empty()) animName = "Animation_" + to_string(i);

				// Create animation through factory method
				unique_ptr<IResource> animation = createAnimation(scene, skeleton, animName);
				if (!animation) continue;

				// Cache the animation using AssetManager
				optional<id_t> animationId = assets.add<Animation>(animName, move(animation));
				if (animationId.has_value()) {
					modelAnimations.push_back(animName);
					//log(this, LogInfo, "  - Successfully Imported Animation '{}' with {} Channels and {} Bones", animation->getName(), anim->mNumChannels, animation->bones.size());
				}
				else {
					log(this, LogError, "Failed to cache animation '{}'", animName);
				}
			}
			log(this, LogInfo, "- Imported {} Animations", modelAnimations.size());
		}
		return;
	}

	string MeshImporter::getFormat(string path) {
		return path.substr(path.find_last_of('.') + 1, path.size());
	}

	unsigned int MeshImporter::getFormatOptions(string format) {
		log(this, LogInfo, "- File Format: {}", format);
		if (format == "fbx") {
			return aiProcess_RemoveRedundantMaterials;
		}
		if (format == "obj") {
			return 0;
		}
		return 0;
	}

	vector<id_t> MeshImporter::importSceneMaterials(const aiScene* scene) {
		log(this, LogInfo, "- Importing Model Materials:");
		vector<id_t> modelMaterials;
		if (scene->HasMaterials()) {
			for (size_t modelMaterialId = 0; modelMaterialId < scene->mNumMaterials; ++modelMaterialId) {
				aiMaterial* modelMaterial = scene->mMaterials[modelMaterialId];
				
				//Extract material textures from imported materials
				vector<string> diffuseMaps = loadMaterialTextures(modelMaterial, aiTextureType_DIFFUSE);
				string diffuseTexture = diffuseMaps.size() > 0 ? diffuseMaps[0] : "";
				vector<string> specularMaps = loadMaterialTextures(modelMaterial, aiTextureType_SPECULAR);
				string specularTexture = specularMaps.size() > 0 ? specularMaps[0] : "";
				vector<string> opacityMaps = loadMaterialTextures(modelMaterial, aiTextureType_OPACITY);
				string opacityTexture = opacityMaps.size() > 0 ? opacityMaps[0] : "";

				//Extract the diffuse & specular colors and the opacity, shininess, and roughess
				aiColor4D* diffuseColor = new aiColor4D(1, 1, 1, 1);
				aiColor4D* specularColor = new aiColor4D(1, 1, 1, 1);
				ai_real* opacity = new ai_real(1);
				ai_real* shininess = new ai_real(1);
				ai_real* roughness = new ai_real(1);
				aiGetMaterialColor(modelMaterial, AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
				aiGetMaterialColor(modelMaterial, AI_MATKEY_COLOR_SPECULAR, specularColor);
				aiGetMaterialFloat(modelMaterial, AI_MATKEY_SHININESS, shininess);
				aiGetMaterialFloat(modelMaterial, AI_MATKEY_OPACITY, opacity);
				aiGetMaterialFloat(modelMaterial, AI_MATKEY_ROUGHNESS_FACTOR, roughness);

				//Create the material and assign it to the mesh
				SurfaceDomain diffuseDomain = SurfaceDomain(diffuseTexture, fromAiColor4(diffuseColor));
				SurfaceDomain specularDomain = SurfaceDomain(specularTexture, fromAiColor4(specularColor), (*roughness) * (*shininess));
				SurfaceDomain opacityDomain = SurfaceDomain(opacityTexture, Color::White, *opacity);
				SurfaceParameters surfaceParams = SurfaceParameters(diffuseDomain, specularDomain);
				auto worldMaterialAsset = assets.create<Material>(modelMaterial->GetName().C_Str(), surfaceParams, assets.get<Program>(assets.defaultProgramName));
				if (!worldMaterialAsset.has_value()) {
					log(this, LogWarn, "  - {} Material: '{}' Failed to create world material!", worldMaterialAsset.value().first, modelMaterial->GetName().C_Str());
				} else {
					modelMaterials.push_back(worldMaterialAsset.value().first);
					log(this, LogInfo, "  - {} Material: '{}'  (World ID: {}) Color: ({},{},{})", worldMaterialAsset.value().first, modelMaterial->GetName().C_Str(), worldMaterialAsset.value().first, diffuseColor->r, diffuseColor->g, diffuseColor->b);
				}

				// Cleanup
				delete diffuseColor;
				delete specularColor;
				delete opacity;
				delete shininess;
				delete roughness;
			}
		}
		return modelMaterials;
	}

	ImportResult MeshImporter::processNode(aiNode* fromSceneNode, const aiScene* scene, map<string, BoneData>& modelBones, vector<id_t> modelMaterials) {
		//Create a new MeshNodeData for this node and assign this node's name and transform to it
		MeshNodeData* meshNode = new MeshNodeData(fromSceneNode->mName.C_Str(), fromAiMatrix4toGlm(fromSceneNode->mTransformation));

		//A node can hold several meshes (Assimp splits a mesh per material, e.g. an OBJ object with several materials).
		//The first goes on this node; each other one goes on a child node with an identity transform, so it shares this node's placement.
		log(this, LogInfo, "- Importing MeshData for node '{}' [Meshes: {}, Children: {}]", fromSceneNode->mName.C_Str(), fromSceneNode->mNumMeshes, fromSceneNode->mNumChildren);
		for (unsigned int i = 0; i < fromSceneNode->mNumMeshes; i++) {
			unsigned int sceneMeshId = fromSceneNode->mMeshes[i];
			if (sceneMeshId >= scene->mNumMeshes) {
				log(this, LogError, "Mesh node at id is out-of-bounds for scene meshes");
				return ImportResult();
			}
			MeshNodeData* target = meshNode;
			if (i > 0) {
				target = new MeshNodeData(string(fromSceneNode->mName.C_Str()) + "." + std::to_string(i), glm::mat4(1.0f));
				target->parent = meshNode;
				meshNode->children.push_back(target);
			}
			importMesh(target, scene->mMeshes[sceneMeshId], sceneMeshId, fromSceneNode, modelBones, modelMaterials);
		}

		//Recursively visit all children, setting the child node's parent to this node and assigning the child node to this node's children
		for (unsigned int i = 0; i < fromSceneNode->mNumChildren; i++) {
			ImportResult childResult = processNode(fromSceneNode->mChildren[i], scene, modelBones, modelMaterials);
			childResult.rootNode->parent = meshNode;
			meshNode->children.push_back(childResult.rootNode);
		}
		return ImportResult(meshNode);
    }

	void MeshImporter::importMesh(MeshNodeData* toModelNode, aiMesh* mesh, unsigned int sceneMeshId, aiNode* fromSceneNode, map<string, BoneData>& modelBones, vector<id_t> modelMaterials) {
		{
			//Get position, texture coordinates, and normal from the import mesh node or, if not available, the use the default value
			vector<VertexData> vertices;
			vertices.reserve(mesh->mNumVertices);
			for (unsigned int vertexId = 0; vertexId < mesh->mNumVertices; vertexId++) {
				glm::vec3 position = aiV3toGlm(mesh->mVertices[vertexId]);
				glm::vec2 texCoord = (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0]) ? texCoord = aiV2toGlm(mesh->mTextureCoords[0][vertexId]) : glm::vec2(0, 0);
				glm::vec3 normal = (mesh->HasNormals()) ? aiV3toGlm(mesh->mNormals[vertexId]) : glm::vec3(0, 1, 0);
				vertices.push_back(VertexData(position, texCoord, normal, 0));
			}

			//Get the vertex indices for each mesh face
			vector<unsigned int> indices;
			for (unsigned int faceId = 0; faceId < mesh->mNumFaces; faceId++) {
				aiFace face = mesh->mFaces[faceId];
				for (unsigned int faceVertexId = 0; faceVertexId < face.mNumIndices; faceVertexId++) {
					indices.push_back(face.mIndices[faceVertexId]);
				}
			}

			//Map of <boneId, weight> pair by vertexId
			map<int, vector<pair<int, float>>> vertexWeights;
			if (mesh->HasBones()) {
				for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
					auto bone = mesh->mBones[boneIndex];
					string boneName = bone->mName.C_Str();
					
					//Map <boneId, weight> pair to vertex Id for each weight of this bone
					for (int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
						int vertexId = bone->mWeights[weightIndex].mVertexId;
						float weight = bone->mWeights[weightIndex].mWeight;
						vertexWeights[vertexId].push_back({boneIndex, weight});
					}

					//TODO: Instead of storing BoneData on the Model, we should create a Skeleton with the bone data
					//and assign it to the Model.
					//TODO: When importing a Skeleton, we need a way to determine if the Skeleton being imported
					//already exists.
					//Create BoneData for this bone and add it to the modelBones map, if not already added
					if (modelBones.find(boneName) == modelBones.end()) {
						modelBones[boneName] = BoneData(boneIndex, fromAiMatrix4toGlm(bone->mOffsetMatrix));
					}
				}

				// Then assign the strongest weights to each vertex
				for (auto& [vertexId, weights] : vertexWeights) {
					float totalWeight = 0.0f;
					int assignedInfluences = 0;
					// Strongest influences first: sort by weight (descending), not by bone id.
					sort(weights.begin(), weights.end(), [](const pair<int, float>& a, const pair<int, float>& b) { return a.second > b.second; });

					// Assign up to MAX_BONE_INFLUENCE weights
					for (size_t influenceId = 0; influenceId < min(weights.size(), size_t(MAX_BONE_INFLUENCE)); ++influenceId) {
						int boneId = weights[influenceId].first;
						float weight = weights[influenceId].second;
						vertices[vertexId].boneIds[influenceId] = boneId;
						vertices[vertexId].weights[influenceId] = weight;
						totalWeight += weight;
						assignedInfluences++;
					}

					// Fill remaining slots with -1 and 0
					for (int influenceId = assignedInfluences; influenceId < MAX_BONE_INFLUENCE; ++influenceId) {
						vertices[vertexId].boneIds[influenceId] = -1;
						vertices[vertexId].weights[influenceId] = 0.0f;
					}

					// Normalize weights only if they don't sum to 1
					if (totalWeight > 0.0f && abs(totalWeight - 1.0f) > 0.001f) {
						for (int influenceId = 0; influenceId < assignedInfluences; ++influenceId) {
							vertices[vertexId].weights[influenceId] /= totalWeight;
						}
					}
				}
			}
			
			//Finally, create MeshData with node name, vertices, indices, and modelBones.
			//The asset name is unique per file and scene mesh: mesh names repeat (per-material parts, "Cube" in many files), and create() returns an existing asset of the same name.
			string meshDataName = importPath + "#" + std::to_string(sceneMeshId) + ":" + mesh->mName.C_Str();
			auto meshDataAsset = assets.create<MeshData>(meshDataName, fromSceneNode->mName.C_Str(), vertices, indices, modelBones);
			if (meshDataAsset.has_value()) {
				optional<id_t> meshDataId = meshDataAsset.value().first;
				toModelNode->meshData = assets.get<MeshData>(meshDataId.value());
				//Set the node materialId to the world material id for this node
				toModelNode->materialId = modelMaterials[mesh->mMaterialIndex];
				log(this, LogDebug, "  - Created node with Material Id {}, {} vertices, {} indices, and {} bones", toModelNode->materialId, toModelNode->meshData->vertices.size(), toModelNode->meshData->indices.size(), toModelNode->meshData->bones.size());
			}
		}
	}

	Model* MeshImporter::createModel(MeshData* meshData, string name) {
		Model* model = new Model(meshData, name);

		// Import animations if path available
		if (!meshData->sourcePath.empty()) {
			const aiScene* scene = modelImporter.ReadFile(meshData->sourcePath,0U);
			vector<string> modelAnimations;
			Skeleton* skeleton = new Skeleton(meshData->bones);
			importAnimations(scene, skeleton, modelAnimations);
		}

		return model;
	}

    vector<string> MeshImporter::loadMaterialTextures(aiMaterial* mat, aiTextureType type) {
		vector<string> importedTextures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			// Models may reference textures by absolute paths from the authoring machine;
			// textures are loaded by file name from the resources folder.
			importedTextures.push_back(std::filesystem::path(str.C_Str()).filename().string());
		}
		return importedTextures;
    }

	const aiScene* MeshImporter::readFile(string path, unsigned int options) {
		return modelImporter.ReadFile(path, options);
	}

    Color MeshImporter::fromAiColor4(aiColor4D* c) {
        return Color(c->r * 255.f, c->g * 255.f, c->b * 255.f, c->a * 255.f);
    }

	unique_ptr<IResource> MeshImporter::createAnimation(const aiScene* scene, Skeleton* skeleton, const string& animationName) {
		if (!scene) {
			log(this, LogError, "Cannot create animation, mesh is invalid");
			return nullptr;
		}

		try {
			// Create animation with scene data
			unique_ptr<Animation> animation = make_unique<Animation>();
			aiAnimation* aiAnim = scene->mAnimations[0];

			// Setup animation data
			animation->setName(!animationName.empty() ? animationName : (aiAnim->mName.length > 0 ? aiAnim->mName.C_Str() : "Animation"));
			animation->duration = aiAnim->mDuration;
			animation->ticksPerSecond = aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 24.0f;
			//Import animation heirarchy from scene heirarchy
			animation->importAnimationHeirarchy(animation->root, scene->mRootNode);

			//Create a Bone for each channel in the Animation with Skeleton bone id
			animation->importAnimationBones(aiAnim, skeleton);

			return animation;
		}
		catch (const std::exception& e) {
			log(this, LogError, "Error creating animation: {}", e.what());
			return nullptr;
		}
	}

	// Helper to find nodes by name in model hierarchy
	ModelNode* findModelNode(ModelNode* root, const string& name) {
		if (!root) return nullptr;
		if (root->nodeName == name) return root;

		for (auto* child : root->children) {
			ModelNode* found = findModelNode(child, name);
			if (found) return found;
		}
		return nullptr;
	}
}