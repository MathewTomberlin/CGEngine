#include "../World/Renderer.h"
#include "../Engine/Engine.h"
#include "MeshImporter.h"
#include "../Mesh/Mesh.h"
#include "../Mesh/Model.h"

namespace CGEngine {
    ImportResult MeshImporter::importModel(string path, unsigned int options) {
        string type = path.substr(path.find_last_of('.') + 1, path.size());
        if (type == "fbx") {
            options |= aiProcess_RemoveRedundantMaterials;
        }
        const aiScene* scene = modelImporter.ReadFile(path, options);
        if (scene != nullptr) {
			return processNode(scene->mRootNode, scene, type);
        } else {
            cout << "Failed to import from " << path << "\n";
        }
        return ImportResult();
    }

    // Move the implementation of processNode from Renderer.cpp
	ImportResult MeshImporter::processNode(aiNode* node, const aiScene* scene, string type) {
		MeshNodeData* currentNode = new MeshNodeData();
		currentNode->nodeName = node->mName.C_Str();
		currentNode->transformation = fromAiMatrix4toGlm(node->mTransformation);
		currentNode->parent = nullptr;
		bool skeletal = false;
		// Create empty MeshData by default
		currentNode->meshData = new MeshData();
		currentNode->meshData->meshName = node->mName.C_Str();
		cout << "Processing node '" << node->mName.C_Str() << "' with " << node->mNumMeshes << " meshes and " << node->mNumChildren << " children\n";
		//Only handle the first mesh at each node
		//TODO: How should we handle multiple meshes per node?
		if (node->mNumMeshes > 0){
			// Verify mesh index is valid
			if (node->mMeshes[0] >= scene->mNumMeshes) {
				cout << "Invalid mesh index " << node->mMeshes[0] << " for node '"
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
				cout << "Importing material " << material->GetName().C_Str() << " at mesh node material index " << imported->mMaterialIndex << "\n";
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
			}
			//Import bones and weights
			map<int, vector<pair<int, float>>> vertexWeights;  // vertex_id -> [(bone_id, weight)]
			map<string, BoneData> bones;
			if (imported->HasBones()) {
				// First collect all weights per vertex
				for (int boneIndex = 0; boneIndex < imported->mNumBones; ++boneIndex) {
					auto bone = imported->mBones[boneIndex];
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
							cout << "Imported bone " << boneIndex << ": " << boneName << "\n";
						}
					}
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

					// Debug output for weight verification
					if (vertexId % 100 == 0) {  // Print every 100th vertex to avoid spam
						cout << "Vertex " << vertexId << " weights: ";
						for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
							if (vertices[vertexId].weights[i] > 0) {
								cout << vertices[vertexId].weights[i] << " ";
							}
						}
						cout << "\n";
					}
				}
			}

			currentNode->meshData->vertices = vertices;
			currentNode->meshData->indices = indices;
			currentNode->meshData->bones = bones;
			currentNode->meshData->skeletalMesh = bones.size() > 0;
			currentNode->materialId = materialId;
		}
		
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			ImportResult childResult = processNode(node->mChildren[i], scene, type);
			childResult.rootNode->parent = currentNode;
			skeletal = childResult.rootNode->meshData->skeletalMesh || currentNode->meshData->skeletalMesh;
			currentNode->children.push_back(childResult.rootNode);
		}
		currentNode->meshData->skeletalMesh = skeletal;

		cout << "Imported mesh node '" << currentNode->meshData->meshName << "' with " << currentNode->meshData->vertices.size() << " vertices and " << currentNode->meshData->indices.size() << " indices and " << currentNode->meshData->bones.size() << " bones" << "\n";
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
		if (!targetModel) return false;

		// Import scene with animation data
		const aiScene* scene = modelImporter.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || !scene->HasAnimations()) {
			cout << "No animations found in " << path << "\n";
			return false;
		}

		// Process each animation in the file
		for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
			aiAnimation* anim = scene->mAnimations[i];
			string animName = anim->mName.length > 0 ?
				anim->mName.C_Str() : "Animation_" + to_string(i);

			// Create animation and map to model's node hierarchy
			Animation* animation = new Animation();
			animation->setName(animName);
			animation->duration = anim->mDuration;
			animation->ticksPerSecond = anim->mTicksPerSecond;

			//Build the animation heirarchy
			animation->readHeirarchyData(animation->root, scene->mRootNode);

			//Map animation nodes to model hierarchy
			targetModel->mapAnimationNodes(animation);

			//Process bone animations
			animation->readMissingBones(anim, targetModel->rootNode->meshData);

			targetModel->addAnimation(animation);

			cout << "Imported animation '" << animName << "' with " << animation->bones.size() << " bone channels\n";
		}

		return true;
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