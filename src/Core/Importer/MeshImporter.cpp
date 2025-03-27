#include "../World/Renderer.h"
#include "../Engine/Engine.h"
#include "MeshImporter.h"
#include "../Mesh/Mesh.h"
#include "../Mesh/Model.h"
#include "../Animation/Animation.h"

namespace CGEngine {
    ImportResult MeshImporter::importModel(string path, unsigned int options) {
		cout << "\n=== Importing Model: " << path << " ===\n";
        string type = path.substr(path.find_last_of('.') + 1, path.size());
        if (type == "fbx") {
            options |= aiProcess_RemoveRedundantMaterials;
        }
        const aiScene* scene = modelImporter.ReadFile(path, options);
        if (scene != nullptr) {
			// Track skeletal data during node processing
			SkeletalData skeletalData;
			ImportResult result = processNode(scene->mRootNode, scene, type, skeletalData);
			result.skeletalMesh = skeletalData.isSkeletal;

			// If skeletal, ensure all meshes have access to complete bone data
			if (skeletalData.isSkeletal) {
				propagateBoneData(result.rootNode, skeletalData.allBones);
			}

			return result;
        } else {
            cout << "ERROR: Failed to import from " << path << "\n";
        }
        return ImportResult();
    }

    // Move the implementation of processNode from Renderer.cpp
	ImportResult MeshImporter::processNode(aiNode* node, const aiScene* scene, string type, SkeletalData& skeletalData) {
		if (node->mNumMeshes > 0) {
			cout << "Processing node '" << node->mName.C_Str()
				<< "' [Meshes: " << node->mNumMeshes
				<< ", Children: " << node->mNumChildren << "]\n";
		}

		MeshNodeData* currentNode = new MeshNodeData();
		currentNode->nodeName = node->mName.C_Str();
		currentNode->transformation = fromAiMatrix4toGlm(node->mTransformation);
		currentNode->parent = nullptr;

		//Only handle the first mesh at each node
		//TODO: How should we handle multiple meshes per node?
		if (node->mNumMeshes > 0) {
			// Create empty MeshData by default
			currentNode->meshData = new MeshData();
			currentNode->meshData->meshName = node->mName.C_Str();
			// Verify mesh index is valid
			if (node->mMeshes[0] >= scene->mNumMeshes) {
				cout << "ERROR: Invalid mesh index " << node->mMeshes[0] << " for node '"
					<< node->mName.C_Str() << "'\n";
				return ImportResult();
			}
			aiMesh* imported = scene->mMeshes[node->mMeshes[0]];
			
			//Read vertices
			vector<VertexData> vertices;
			vertices.reserve(imported->mNumVertices);
			for (unsigned int i = 0; i < imported->mNumVertices; i++) {
				glm::vec3 position = glm::vec3(
					imported->mVertices[i].x,
					imported->mVertices[i].y,
					imported->mVertices[i].z
				);

				glm::vec2 texCoord(0, 0);
				if (imported->HasTextureCoords(0) && imported->mTextureCoords[0]) {
					texCoord.x = imported->mTextureCoords[0][i].x;
					texCoord.y = imported->mTextureCoords[0][i].y;
				}

				glm::vec3 normal = glm::vec3(0, 1, 0);
				if (imported->HasNormals()) {
					glm::vec3 normal = glm::vec3(imported->mNormals[i].x, imported->mNormals[i].y, imported->mNormals[i].z);
				}

				int materialId = imported->mMaterialIndex;
				if (type == "obj" && materialId > 0) {  // Only adjust if positive
					materialId -= 1;
				}

				vertices.push_back(VertexData(position, texCoord, normal, (float)materialId));
			}

			//Read indices
			vector<unsigned int> indices;
			for (unsigned int i = 0; i < imported->mNumFaces; i++) {
				aiFace face = imported->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++) {
					indices.push_back(face.mIndices[j]);
				}
			}

			id_t materialId = 0;
			if (imported->mMaterialIndex >= 0 && scene->HasMaterials()) {
				aiMaterial* material = scene->mMaterials[imported->mMaterialIndex];
				//Extract the first found diffuse, specular and opacity textures
				vector<string> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE);
				string diffuseTexture = "";
				if (diffuseMaps.size() > 0) {
					diffuseTexture = diffuseMaps[0];
				}
				vector<string> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR);
				string specularTexture = "";
				if (specularMaps.size() > 0) {
					specularTexture = specularMaps[0];
				}
				vector<string> opacityMaps = loadMaterialTextures(material, aiTextureType_OPACITY);
				string opacityTexture = "";
				if (opacityMaps.size() > 0) {
					opacityTexture = opacityMaps[0];
				}
				//Extract the diffuse & specular colors and the opacity, shininess, and roughess
				aiColor4D* diffuseColor = new aiColor4D(1, 1, 1, 1);
				aiColor4D* specularColor = new aiColor4D(1, 1, 1, 1);
				ai_real* opacity = new ai_real(1);
				ai_real* shininess = new ai_real(1);
				ai_real* roughness = new ai_real(1);
				//aiGetMaterialColor(material,AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
				aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, diffuseColor);
				aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, specularColor);
				aiGetMaterialFloat(material, AI_MATKEY_SHININESS, shininess);
				aiGetMaterialFloat(material, AI_MATKEY_OPACITY, opacity);
				aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, roughness);

				//Create the material and assign it to the mesh
				SurfaceDomain diffuseDomain = SurfaceDomain(diffuseTexture, fromAiColor4(diffuseColor));
				SurfaceDomain specularDomain = SurfaceDomain(specularTexture, fromAiColor4(specularColor), (*roughness) * (*shininess));
				SurfaceDomain opacityDomain = SurfaceDomain(opacityTexture, Color::White, *opacity);
				SurfaceParameters importedSurfParams = SurfaceParameters(diffuseDomain, specularDomain);
				materialId = world->createMaterial(importedSurfParams);
				cout << "  Material: " << material->GetName().C_Str() << " (ID: " << materialId << ")\n";
			}
			//Import bones and weights
			map<int, vector<pair<int, float>>> vertexWeights;  // vertex_id -> [(bone_id, weight)]
			map<string, BoneData> bones;
			if (imported->HasBones()) {
				skeletalData.isSkeletal = true;

				cout << "  Bones ("<< imported->mNumBones<<"):\n";
				// First collect all weights per vertex
				for (int boneIndex = 0; boneIndex < imported->mNumBones; ++boneIndex) {
					auto bone = imported->mBones[boneIndex];
					string boneName = bone->mName.C_Str();

					for (int weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
						int vertexId = bone->mWeights[weightIndex].mVertexId;
						float weight = bone->mWeights[weightIndex].mWeight;
						vertexWeights[vertexId].push_back({ boneIndex, weight });

						string boneName = imported->mBones[boneIndex]->mName.C_Str();
						if (bones.find(boneName) == bones.end()) {
							BoneData boneData;
							boneData.id = boneIndex;
							boneData.offset = fromAiMatrix4toGlm(imported->mBones[boneIndex]->mOffsetMatrix);
							bones[boneName] = boneData;
							skeletalData.allBones[boneName] = boneData;
						}
					}
					cout << "    " << boneName << " (Weights: " << imported->mBones[boneIndex]->mNumWeights << ")\n";
				}

				// Then assign the strongest weights to each vertex
				for (auto& [vertexId, weights] : vertexWeights) {
					// Sort weights in descending order
					std::sort(weights.begin(), weights.end(),
						[](const auto& a, const auto& b) { return a.second > b.second; });

					float totalWeight = 0.0f;
					int assignedInfluences = 0;

					// Assign up to MAX_BONE_INFLUENCE weights
					for (size_t i = 0; i < std::min(weights.size(), size_t(MAX_BONE_INFLUENCE)); ++i) {
						vertices[vertexId].boneIds[i] = weights[i].first;
						vertices[vertexId].weights[i] = weights[i].second;
						totalWeight += weights[i].second;
						assignedInfluences++;
					}

					// Fill remaining slots with -1 and 0
					for (int i = assignedInfluences; i < MAX_BONE_INFLUENCE; ++i) {
						vertices[vertexId].boneIds[i] = -1;
						vertices[vertexId].weights[i] = 0.0f;
					}

					// Normalize weights only if they don't sum to 1
					if (std::abs(totalWeight - 1.0f) > 0.001f) {
						for (int i = 0; i < assignedInfluences; ++i) {
							vertices[vertexId].weights[i] /= totalWeight;
						}
					}

					// Debug output for weight verification -- Disabled, possibly deprecated
					//if (vertexId % 1000 == 0) {  // Print every 1000th vertex to avoid spam
					//	cout << "Vertex " << vertexId << " weights: ";
					//	for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
					//		if (vertices[vertexId].weights[i] > 0) {
					//			cout << vertices[vertexId].weights[i] << " ";
					//		}
					//	}
					//	cout << "\n";
					//}
				}
			}

			currentNode->meshData->vertices = vertices;
			currentNode->meshData->indices = indices;
			currentNode->meshData->bones = bones;
			currentNode->meshData->skeletalMesh = bones.size() > 0;
			currentNode->materialId = materialId;
		} else {
			currentNode->meshData = nullptr;
		}
		
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			ImportResult childResult = processNode(node->mChildren[i], scene, type, skeletalData);
			childResult.rootNode->parent = currentNode;
			currentNode->children.push_back(childResult.rootNode);
		}
		if (currentNode->meshData) {
			currentNode->meshData->skeletalMesh = skeletalData.isSkeletal;
			cout << "  Created node with "
				<< currentNode->meshData->vertices.size() << " vertices, "
				<< currentNode->meshData->indices.size() << " indices, "
				<< currentNode->meshData->bones.size() << " bones\n";
		}
		ImportResult result = ImportResult();
		result.rootNode = currentNode;
		return result;
    }

	Model* MeshImporter::createModel(MeshData* meshData, string name) {
		Model* model = new Model(meshData, name);

		// Import animations if path available
		if (!meshData->sourcePath.empty()) {
			importAnimation(meshData->sourcePath, model);
		}

		return model;
	}

    // Move the implementation of loadMaterialTextures from Renderer.cpp
    vector<string> MeshImporter::loadMaterialTextures(aiMaterial* mat, aiTextureType type) {
		vector<string> importedTextures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			importedTextures.push_back(str.C_Str());
		}
		return importedTextures;
    }

	const aiScene* MeshImporter::readFile(string path, unsigned int options) {
		return modelImporter.ReadFile(path, options);
	}

    Color MeshImporter::fromAiColor4(aiColor4D* c) {
        return Color(c->r * 255.f, c->g * 255.f, c->b * 255.f, c->a * 255.f);
    }

	bool MeshImporter::importAnimation(const string& path, Model* targetModel) {
		cout << "\n=== Importing Animations ===\n";
		if (!targetModel) {
			cout << "ERROR: Cannot import animations without a valid model\n";
			return false;
		}

		// Import scene with animation data
		const aiScene* scene = modelImporter.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
		if (!scene) {
			cout << "ERROR: Failed to load scene for animation import\n";
			return false;
		}

		cout << "Scene has " << scene->mNumAnimations << " animations\n";
		if (!scene->HasAnimations()) {
			cout << "WARNING: No animations found in " << path << "\n";
			return false;
		}

		//TODO: findSkeletalMesh is reused in Model.setupAnimations, consider moving to a higher-level function
		// Find the mesh with bone data first
		MeshData* skeletalMesh = nullptr;
		std::function<void(ModelNode*)> findSkeletalMesh =
			[&skeletalMesh, &findSkeletalMesh](ModelNode* node) {
			if (node && node->meshData && !node->meshData->bones.empty()) {
				skeletalMesh = node->meshData;
				return;
			}
			if (node) {
				for (auto* child : node->children) {
					findSkeletalMesh(child);
					if (skeletalMesh) return;
				}
			}
			return;
		};
		findSkeletalMesh(targetModel->rootNode);
		if (!skeletalMesh) {
			cout << "ERROR: No skeletal mesh found during animation import\n";
			return false;
		}

		// Process each animation in the file
		for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
			if (!scene) continue;
			aiAnimation* anim = scene->mAnimations[i];
			string animName = anim->mName.length > 0 ? anim->mName.C_Str() : scene->mAnimations[i]->mName.C_Str();
			if (animName.empty()) {
				animName = "Animation_" + to_string(i);
			}

			// Create animation through factory method
			Animation* animation = createAnimation(path, skeletalMesh, animName);
			if (!animation) continue;

			//Map animation nodes to model hierarchy
			targetModel->mapAnimationNodes(animation);
			targetModel->addAnimation(animation);

			cout << "  Imported animation '" << animName << "' with " << animation->bones.size() << " bone channels\n";
		}

		return true;
	}

	Animation* MeshImporter::createAnimation(const string& path, MeshData* mesh, const string& animationName) {
		if (!mesh) {
			cout << "Error: Cannot create animation without valid mesh data\n";
			return nullptr;
		}

		unsigned int flags = aiProcess_Triangulate |
			aiProcess_GenNormals |
			aiProcess_LimitBoneWeights |
			aiProcess_ConvertToLeftHanded |
			aiProcess_ValidateDataStructure |
			aiProcess_JoinIdenticalVertices |
			aiProcess_OptimizeGraph |
			aiProcess_OptimizeMeshes;

		// Load scene
		currentScene = modelImporter.ReadFile(path, flags);
		if (!currentScene || !currentScene->HasAnimations()) {
			cout << "Error: No animations found in " << path << "\n";
			return nullptr;
		}

		try {
			// Create animation with scene data
			Animation* animation = new Animation();
			aiAnimation* aiAnim = currentScene->mAnimations[0];

			// Setup animation data
			animation->setName(!animationName.empty() ? animationName : (aiAnim->mName.length > 0 ? aiAnim->mName.C_Str() : "Animation"));
			animation->duration = aiAnim->mDuration;
			animation->ticksPerSecond = aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 24.0f;
			// Read hierarchy
			animation->readHeirarchyData(animation->root, currentScene->mRootNode);
			// Process bones
			animation->readMissingBones(aiAnim, mesh);

			return animation;
		}
		catch (const std::exception& e) {
			cout << "Error creating animation: " << e.what() << "\n";
			return nullptr;
		}
	}

	void MeshImporter::propagateBoneData(MeshNodeData* node, const map<string, BoneData>& allBones) {
		if (!node) return;

		if (node->meshData) {
			// Ensure this mesh has all bones
			for (const auto& [boneName, boneData] : allBones) {
				if (node->meshData->bones.find(boneName) == node->meshData->bones.end()) {
					node->meshData->bones[boneName] = boneData;
				}
			}
			node->meshData->skeletalMesh = true;
		}

		// Process children
		for (auto* child : node->children) {
			propagateBoneData(child, allBones);
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