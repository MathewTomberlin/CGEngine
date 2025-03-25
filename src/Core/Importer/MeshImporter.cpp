#include "../World/Renderer.h"
#include "../Engine/Engine.h"
#include "MeshImporter.h"
#include "../Mesh/Mesh.h"

namespace CGEngine {
    vector<MeshData> MeshImporter::importModel(string path, Mesh* mesh, unsigned int options) {
        string type = path.substr(path.find_last_of('.') + 1, path.size());
        if (type == "fbx") {
            options |= aiProcess_RemoveRedundantMaterials;
        }
        const aiScene* scene = modelImporter.ReadFile(path, options);
        if (scene != nullptr) {
            vector<MeshData> meshes = processNode(scene->mRootNode, scene, mesh, type);
            if (meshes.size() > 0) {
                cout << "Finished importing " << path << "\n\n";
                return meshes;
            }
            else {
                cout << "No meshes imported from " << path << "\n";
            }
        }
        else {
            cout << "Failed to import from " << path << "\n";
        }
        return {};
    }

    // Move the implementation of processNode from Renderer.cpp
    vector<MeshData> MeshImporter::processNode(aiNode* node, const aiScene* scene, Mesh* mesh, string type) {
		vector<MeshData> meshes = {};
		// process all the node's meshes (if any)
		vector<Material*> materials = {};
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			aiMesh* imported = scene->mMeshes[node->mMeshes[i]];
			cout << "Importing " << imported->mName.C_Str() << "\n";
			vector<unsigned int> indices;

			vector<VertexData> vertices;
			for (unsigned int i = 0; i < imported->mNumVertices; i++) {
				glm::vec3 position = glm::vec3(imported->mVertices[i].x, imported->mVertices[i].y, imported->mVertices[i].z);
				glm::vec2 texCoord = (imported->mTextureCoords[0]) ? glm::vec2(imported->mTextureCoords[0][i].x, imported->mTextureCoords[0][i].y) : glm::vec2(0, 0);
				glm::vec3 normal = glm::vec3(imported->mNormals[i].x, imported->mNormals[i].y, imported->mNormals[i].z);
				int materialId = imported->mMaterialIndex;
				if (type == "obj") {
					materialId -= 1;
				}

				VertexData vert = VertexData(position, texCoord, normal, (float)materialId);
				vertices.push_back(vert);
			}
			for (unsigned int i = 0; i < imported->mNumFaces; i++) {
				aiFace face = imported->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++) {
					indices.push_back(face.mIndices[j]);
				}
			}
			cout << "Imported " << vertices.size() << " vertices and " << indices.size() << " indices" << "\n";

			vector<Texture> textures;
			if (imported->mMaterialIndex >= 0 && scene->HasMaterials()) {
				aiMaterial* material = scene->mMaterials[imported->mMaterialIndex];
				cout << "Importing material index " << imported->mMaterialIndex << "\n";
				//Extract the first found diffuse, specular and opacity textures
				vector<string> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE);
				string diffuseTexture = "";
				if (diffuseMaps.size() > 0) {
					cout << "Found diffuse texture\n";
					diffuseTexture = diffuseMaps[0];
				}
				vector<string> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR);
				string specularTexture = "";
				if (specularMaps.size() > 0) {
					cout << "Found specular texture\n";
					specularTexture = specularMaps[0];
				}
				vector<string> opacityMaps = loadMaterialTextures(material, aiTextureType_OPACITY);
				string opacityTexture = "";
				if (opacityMaps.size() > 0) {
					cout << "Found opacity texture\n";
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
				cout << "Diffuse color (" << diffuseColor->r << ", " << diffuseColor->g << ", " << diffuseColor->b << ", " << diffuseColor->a << ")\n";

				//Create the material and assign it to the mesh
				SurfaceDomain diffuseDomain = SurfaceDomain(diffuseTexture, fromAiColor4(diffuseColor));
				cout << "MaterialColor: " << diffuseColor->r << "," << diffuseColor->g << "," << diffuseColor->b << "," << diffuseColor->a << "\n";
				SurfaceDomain specularDomain = SurfaceDomain(specularTexture, fromAiColor4(specularColor), (*roughness) * (*shininess));
				SurfaceDomain opacityDomain = SurfaceDomain(opacityTexture, Color::White, *opacity);
				SurfaceParameters importedSurfParams = SurfaceParameters(diffuseDomain, specularDomain);
				id_t materialId = world->createMaterial(importedSurfParams);
				Material* importedMaterial = world->getMaterial(materialId);
				mesh->addMaterial(importedMaterial);
				materials.push_back(importedMaterial);
				cout << "Imported material " << material->GetName().C_Str() << "\n";
			}
			//Import bones and weights
			map<int, vector<pair<int, float>>> vertexWeights;  // vertex_id -> [(bone_id, weight)]
			map<string, BoneData> bones;
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

			mesh->getMeshData()->bones = bones;
			meshes.push_back(MeshData(vertices, indices));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			vector<MeshData> m = processNode(node->mChildren[i], scene, mesh, type);
			meshes.insert(meshes.end(), m.begin(), m.end());
		}
		return meshes;
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
}