#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Models/CommonModels.h"
#include "../Animation/Animator.h"

namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (!setGLWindowState(true)) return;

		// Setup a camera with perspective projection
		GLfloat aspectRatio = static_cast<float>(window->getSize().x) / window->getSize().y;
		currentCamera = new Camera(aspectRatio);

		// Enable Z-buffer read and write
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		commitGL();
		if (!setGLWindowState(false)) return;
	}

	void Renderer::getModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			bool skeletalMesh = meshData->skeletalMesh;
			string path = mesh->getImportPath();
			if (path != "") {
				mesh->clearMaterials();
				vector<MeshData> importedModel = importModel(path, mesh);
				meshData = new MeshData(importedModel[0].vertices, importedModel[0].indices);
				meshData->bones = mesh->getMeshData()->bones;
				meshData->skeletalMesh = skeletalMesh;
				mesh->setMeshData(meshData);
			}
			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = world->getMaterial(fallbackMaterialId);
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			Program* program = renderMaterial->getProgram();
			glGenBuffers(1, & meshData->vbo);
			glGenBuffers(1, &meshData->ebo);
			glGenVertexArrays(1, &meshData->vao);

			glBindVertexArray(meshData->vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER,  meshData->vbo);
			glBufferData(GL_ARRAY_BUFFER,  meshData->vertices.size()*sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW);

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,  meshData->ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER,  meshData->indices.size()*sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW);
			}

			//Enable and prepare vertex, texture coordinate, normal, and textureId attribute arrays
			glEnableVertexAttribArray(program->attrib("position"));
			glVertexAttribPointer(program->attrib("position"), 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), NULL);
			glEnableVertexAttribArray(program->attrib("texCoord"));
			glVertexAttribPointer(program->attrib("texCoord"), 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData,texCoord));
			glEnableVertexAttribArray(program->attrib("vertNormal"));
			glVertexAttribPointer(program->attrib("vertNormal"), 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
			glEnableVertexAttribArray(program->attrib("materialId"));
			glVertexAttribPointer(program->attrib("materialId"), 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, materialId));
			if (meshData->skeletalMesh) {
				cout << "Enabling bones and weights for skeletal mesh" << "\n";
				glEnableVertexAttribArray(program->attrib("boneIds"));
				glVertexAttribIPointer(program->attrib("boneIds"), 4, GL_INT, sizeof(VertexData), (void*)offsetof(VertexData, boneIds));
				glEnableVertexAttribArray(program->attrib("weights"));
				glVertexAttribPointer(program->attrib("weights"), 4, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, weights));
			}
			glBindVertexArray(0);

			mesh->setMeshData(meshData);
			setGLWindowState(false);
		}
	}

	void Renderer::updateModelData(Mesh* mesh) {
		if (setGLWindowState(true)) {
			MeshData* meshData = mesh->getMeshData();
			bool skeletalMesh = meshData->skeletalMesh;
			vector<Material*> material = mesh->getMaterials();
			Material* renderMaterial = world->getMaterial(fallbackMaterialId);
			if (material.size() > 0 && material[0] && material[0]->getProgram()) {
				renderMaterial = material[0];
			}

			glBindVertexArray(meshData->vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo);
			glBufferData(GL_ARRAY_BUFFER, meshData->vertices.size() * sizeof(VertexData), meshData->vertices.data(), GL_STATIC_DRAW);

			//Bind and buffert the element buffer, if the model has indices
			if (meshData->indices.size() > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData->ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() * sizeof(unsigned int), meshData->indices.data(), GL_STATIC_DRAW);
			}
			glBindVertexArray(0);

			mesh->setMeshData(meshData);
			setGLWindowState(false);
		}
	}

	vector<MeshData> Renderer::importModel(string path, Mesh* mesh, unsigned int options) {
		string type = path.substr(path.find_last_of('.') + 1, path.size());
		if (type == "fbx") {
			options |= aiProcess_RemoveRedundantMaterials;
		}
		const aiScene* scene = modelImporter.ReadFile(path, options);
		if (scene != nullptr) {
			//TODO: Support models with multiple meshes?
			vector<MeshData> meshes = processNode(scene->mRootNode, scene, mesh, type);
			if (meshes.size() > 0) {
				cout << "Finished importing " << path << "\n\n";
				return meshes;
			}
			else {
				cout << "No meshes imported from " << path << "\n";
			}
		} else {
			cout << "Failed to import from " << path << "\n";
		}
		return {};
	}

	vector<MeshData> Renderer::processNode(aiNode* node, const aiScene* scene, Mesh* mesh, string type) {
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
				SurfaceDomain specularDomain = SurfaceDomain(specularTexture, fromAiColor4(specularColor), (*roughness)*(*shininess));
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
			meshes.push_back(MeshData(vertices,indices));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			vector<MeshData> m = processNode(node->mChildren[i], scene, mesh, type);
			meshes.insert(meshes.end(), m.begin(), m.end());
		}
		return meshes;
	}

	vector<string> Renderer::loadMaterialTextures(aiMaterial* mat, aiTextureType type) {
		vector<string> importedTextures;
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			importedTextures.push_back(str.C_Str());
		}
		return importedTextures;
	}

	id_t Renderer::addLight(Light* light) {
		return lights.add(light);
	}

	void Renderer::removeLight(id_t lightId) {
		Light* light = lights.get(lightId);
		lights.remove(lightId);
		if (light != nullptr) {
			delete light;
		}
	}

	Light* Renderer::getLight(id_t lightId) {
		return lights.get(lightId);
	}

	bool Renderer::setGLWindowState(bool state) {
		// Make the window no longer the active window for OpenGL calls
		bool success = window->setActive(state);
		if (!success) {
			cerr << "Failed to set window to " << ((state) ? "active" : "inactive") << endl;
		}
		return success;
	}

	bool Renderer::clearGL(GLbitfield mask) {
		if (!setGLWindowState(true)) return false;
		glClear(mask);
		return true;
	}

	void Renderer::commitGL() {
		window->pushGLStates();
	}

	void Renderer::pullGL() {
		window->popGLStates();
	}

	bool Renderer::processRender() {
		//Clear render order
		clear();
		//Collect Bodies to render
		Body* root = world->getRoot();
		root->render(*window, root->getTransform());
		//Sort and render Meshes and SFML entities
		render(window);
		window->display();
		return true;
	}

	void Renderer::renderMesh(Mesh* mesh, MeshData* model, Transformation3D transform) {
		glm::mat4 modelPos = glm::translate(glm::vec3(transform.position.x, transform.position.y, transform.position.z));
		glm::mat4 modelRotX = glm::rotate(degrees(transform.rotation.x).asRadians(), glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 modelRotY = glm::rotate(degrees(transform.rotation.y).asRadians(), glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 modelRotZ = glm::rotate(degrees(transform.rotation.z).asRadians(), glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 modelScale = glm::scale(glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));
		glm::mat4 modelRotation = modelRotZ * modelRotY * modelRotX;
		glm::mat4 modelTransform = modelPos * modelRotation * modelScale;

		if (renderer.setGLWindowState(true)) {
			glBindVertexArray(model->vao);
			boundTextures = 0;
			Material* renderMaterial = world->getMaterial(fallbackMaterialId);
			vector<Material*> materials = mesh->getMaterials();
			if (mesh->getMaterials().size() > 0) {
				Material* material = mesh->getMaterials().at(0);
				if (material && material->getProgram()) {
					renderMaterial = material;
				} else {
					mesh->clearMaterials();
					mesh->addMaterial(renderMaterial);
				}
			}
			materials = mesh->getMaterials();
			Animator* animator = model->animator;
			if (animator) {
				animator->updateAnimation(time.getDeltaSec());
			}
			Program* program = renderMaterial->getProgram();
			//Bind the shaders.
			program->use();

			//Set the uniforms for the shader to use
			Vector3f camPos = currentCamera->getPosition();
			program->setUniform("model", modelTransform);
			program->setUniform("camera", currentCamera->getMatrix());
			program->setUniform("cameraPosition", { camPos.x,camPos.y,camPos.z });
			program->setUniform("timeSec", time.getElapsedSec());
			if (animator) {
				vector<glm::mat4> transforms = animator->getBoneMatrices();
				for (int i = 0; i < transforms.size(); ++i) {
					program->setUniform(getUniformArrayIndexName("boneMatrices", i).c_str(), transforms[i]);
				}
			}
			for (int i = 0; i < materials.size(); ++i) {
				setMaterialUniforms(materials.at(i), program, i);
			}

			program->setUniform("lightCount", (int)lights.size());
			for (size_t i = 0; i < lights.size(); ++i) {
				setLightUniforms(lights.get(i), i, program);
			}
			// Draw the cube
			if (model->indices.size()) {
				glDrawElements(GL_TRIANGLES, model->indices.size(), GL_UNSIGNED_INT, 0);
			}
			else {
				glDrawArrays(GL_TRIANGLES, 0, model->vertices.size() / 5);
			}

			// Unbind varray, shaders, and texture
			glBindVertexArray(0);
			glActiveTexture(GL_TEXTURE0);
			program->stop();
		}
		if (renderer.setGLWindowState(false));
	}

	void Renderer::setMaterialUniforms(Material* material, Program* program, int materialId) {
		for (auto iterator = material->materialParameters.begin(); iterator != material->materialParameters.end(); ++iterator) {
			string paramName = (*iterator).first;
			optional<ParamData> paramData = material->getParameter(paramName);
			bool boolData = false;
			int intData = 0;
			float floatData = 0.0f;
			Vector2f v2Data = { 0,0 };
			Vector3f v3Data = { 0,0,0 };
			Color colorData = { 0,0,0,0 };
			Texture* textureData = nullptr;
			if (paramData.has_value()) {
				any paramVal = paramData.value().data;
				ParamType paramType = paramData.value().type;
				switch (paramType) {
				case ParamType::Bool:
					boolData = any_cast<bool>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boolData);
					break;
				case ParamType::Int:
					intData = any_cast<int>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), intData);
					break;
				case ParamType::Float:
					floatData = any_cast<float>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), floatData);
					break;
				case ParamType::V2:
					v2Data = any_cast<Vector2f>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v2Data));
					break;
				case ParamType::V3:
					v3Data = any_cast<Vector3f>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(v3Data));
					break;
				case ParamType::RGBA:
					colorData = any_cast<Color>(paramVal);
					program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), toGlm(colorData));
					break;
				case ParamType::Texture2D:
					textureData = any_cast<Texture*>(paramVal);
					if (textureData != nullptr) {
						glActiveTexture(GL_TEXTURE0 + boundTextures);
						glBindTexture(GL_TEXTURE_2D, textureData->getNativeHandle());
						program->setUniform(getUniformArrayPropertyName("materials", materialId, paramName).c_str(), boundTextures);
						boundTextures++;
					}
					break;
				//case ParamType::String:
					//	break;
				default:
					break;
				}
			}
		}
	}

	void Renderer::setLightUniforms(Light* light, size_t lightIndex, Program* program) {
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "position").c_str(), light->position);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "brightness").c_str(), light->parameters.brightness);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "intensities").c_str(), toGlm(light->parameters.colorIntensities));
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "attenuation").c_str(), light->parameters.attenuation);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "ambiance").c_str(), light->parameters.ambiance);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "coneAngle").c_str(), light->parameters.coneAngle);
		program->setUniform(getUniformArrayPropertyName("lights", lightIndex, "lightDirection").c_str(), toGlm(light->parameters.lightDirection));
	}

	string Renderer::getUniformArrayIndexName(string arrayName, int index) {
		std::ostringstream ss;
		ss << arrayName << "[" << index << "]";
		return ss.str();
	}

	string Renderer::getUniformArrayPropertyName(string arrayName, int index, string propertyName) {
		std::ostringstream ss;
		ss << arrayName << "[" << index << "]." << propertyName;
		return ss.str();
	}

	string Renderer::getUniformObjectPropertyName(string objectName, string propertyName) {
		std::ostringstream ss;
		ss << objectName << "." << propertyName;
		return ss.str();
	}

	void Renderer::add(Body* body, Transform transform) {
		renderOrder.push_back(body);
	}

	int Renderer::zMax() {
		return renderOrder.back()->zOrder;
	}

	int Renderer::zMin() {
		return (*renderOrder.begin())->zOrder;
	}

	vector<Body*> Renderer::getZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			if (renderOrder.at(i)->zOrder == zIndex) {
				found = true;
				bodies.push_back(renderOrder.at(i));
			}
			else if (found) {
				break;
			}
		}
		return bodies;
	}

	vector<Body*> Renderer::getLowerZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = 0; i < renderOrder.size(); ++i) {
			if (renderOrder.at(i)->zOrder < zIndex) {
				bodies.push_back(renderOrder.at(i));
			}
			else {
				break;
			}
		}
		return bodies;
	}

	vector<Body*> Renderer::getHigherZBodies(int zIndex) {
		vector<Body*> bodies;
		bool found = false;
		for (int i = renderOrder.size() - 1; i >= 0; --i) {
			if (renderOrder.at(i)->zOrder > zIndex) {
				bodies.push_back(renderOrder.at(i));
			}
			else {
				break;
			}
		}
		return bodies;
	}

	void Renderer::clear() {
		renderOrder.clear();
	}
	
	void Renderer::sortZ() {
		sort(renderOrder.begin(), renderOrder.end(), [](Body* a, Body* b) { return (a->zOrder < b->zOrder); });
	}

	void Renderer::render(RenderTarget* window) {
		//Sort the render order by Body Z order
		sortZ();
		//Draw each body with its calculated transform
		for (auto iterator = renderOrder.begin(); iterator != renderOrder.end(); ++iterator) {
			if (world->isDeleted(*iterator)) continue;
			(*iterator)->onDraw(*window, (*iterator)->getGlobalTransform());
		}
	}

	Camera* Renderer::getCurrentCamera() {
		return currentCamera;
	}

	void Renderer::setCurrentCamera(Camera* camera) {
		currentCamera = camera;
	}

	GLenum Renderer::initGlew() {
		// Initialise GLEW for the extended functions.
		glewExperimental = GL_TRUE;
		GLenum state = glewInit();
		if (state != GLEW_OK) {
			cout << "GLEW not ok" << "\n";
		} else {
			cout << "GLEW initialized" << "\n";
		}
		return state;
	}

	glm::vec2 Renderer::toGlm(Vector2f v) {
		return glm::vec2(v.x, v.y);
	}
	glm::vec3 Renderer::toGlm(Vector3f v) {
		return glm::vec3(v.x, v.y, v.z);
	}
	glm::vec3 Renderer::toGlm(Color c) {
		return glm::vec3(c.r, c.g, c.b);
	}
	Color Renderer::fromAiColor4(aiColor4D* c) {
		return Color(c->r * 255.f, c->g, c->b * 255.f, c->a * 255.f);
	}
}