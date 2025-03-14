#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Models/CommonModels.h"

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
			VertexModel model = mesh->getModel();
			if (model.path != "") {
				vector<VertexModel> importedModel = importModel(model.path);
				model = VertexModel(importedModel[0].vertices, "", importedModel[0].indices);
				mesh->setModel(model);
			}
			vector<Material*> material = mesh->getMaterials();
			Program* program = material[0]->getProgram();
			//Setup ModelData with drawCount, vertex buffer and array, and shader program
			ModelData data = ModelData(model.vertexCount, { material });
			glGenBuffers(1, &data.vbo);
			glGenBuffers(1, &data.ebo);
			glGenVertexArrays(1, &data.vao);

			glBindVertexArray(data.vao);
			//Bind the vertex buffer and pass in the vertex data
			glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
			glBufferData(GL_ARRAY_BUFFER, model.dataSpan, model.vertices.data(), GL_STATIC_DRAW);

			//Bind and buffert the element buffer, if the model has indices
			if (model.indices.size() > 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), &model.indices[0], GL_STATIC_DRAW);
			}

			auto stride = sizeof(GLfloat) * 9;
			auto textureCoordOffset = sizeof(GLfloat) * 3;
			auto normalOffset = sizeof(GLfloat) * 5;
			auto materialIdOffset = sizeof(GLfloat) * 8;
			//Enable and prepare vertex, texture coordinate, normal, and textureId attribute arrays
			glEnableVertexAttribArray(program->attrib("position"));
			glVertexAttribPointer(program->attrib("position"), 3, GL_FLOAT, GL_FALSE, stride, NULL);
			glEnableVertexAttribArray(program->attrib("texCoord"));
			glVertexAttribPointer(program->attrib("texCoord"), 2, GL_FLOAT, GL_FALSE, stride, (void*)textureCoordOffset);
			glEnableVertexAttribArray(program->attrib("vertNormal"));
			glVertexAttribPointer(program->attrib("vertNormal"), 3, GL_FLOAT, GL_FALSE, stride, (void*)normalOffset);
			glEnableVertexAttribArray(program->attrib("materialId"));
			glVertexAttribPointer(program->attrib("materialId"), 1, GL_FLOAT, GL_FALSE, stride, (void*)materialIdOffset);
			glBindVertexArray(0);

			mesh->setModelData(data);
			setGLWindowState(false);
		}
	}

	vector<VertexModel> Renderer::importModel(string path, unsigned int options) {
		//string directory = path.substr(0, path.find_last_of('/')); -- UNUSED
		const aiScene* scene = modelImporter.ReadFile(path, options);
		if (scene != nullptr) {
			//TODO: Support models with multiple meshes?
			vector<VertexModel> meshes = processNode(scene->mRootNode, scene);
			if (meshes.size() > 0) {
				cout << "Done importing " << meshes.size() << " meshes from " << path << "\n";
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

	vector<VertexModel> Renderer::processNode(aiNode* node, const aiScene* scene) {
		vector<VertexModel> meshes = {};
		// process all the node's meshes (if any)
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			vector<float> vertexArray = {};
			vector<unsigned int> indices;
			vector<Texture> textures;
			for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
				//Pos
				vertexArray.push_back(mesh->mVertices[i].x);
				vertexArray.push_back(mesh->mVertices[i].y);
				vertexArray.push_back(mesh->mVertices[i].z);
				//TexCoord
				//TODO: Support multiple texture coordinates?
				glm::vec2 uv = { 0,0 };
				if (mesh->mTextureCoords[0])  {
					uv = { mesh->mTextureCoords[0][i].x,mesh->mTextureCoords[0][i].y };
				}
				vertexArray.push_back(uv.x);
				vertexArray.push_back(uv.y);
				//Normals
				vertexArray.push_back(mesh->mNormals[i].x);
				vertexArray.push_back(mesh->mNormals[i].y);
				vertexArray.push_back(mesh->mNormals[i].z);
				//TODO: Fix mesh materials
				//TODO: Support external material definitions?
				vertexArray.push_back(0);
				
				//TODO: Import Logging
				//cout << "Vertex" << i << ": (" << mesh->mVertices[i].x << "," << mesh->mVertices[i].y << "," << mesh->mVertices[i].z << "),(" << uv.x<<","<<uv.y<<"),(" << mesh->mNormals[i].x << "," << mesh->mNormals[i].y << "," << mesh->mNormals[i].z << ")\n";
			}
			//Import indices
			for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (unsigned int j = 0; j < face.mNumIndices; j++) {
					indices.push_back(face.mIndices[j]);
				}
			}

			meshes.push_back(VertexModel(vertexArray,"",indices));
			cout << "Imported mesh with " << vertexArray.size() << " vertex array positions and " << indices.size() << " indices " << "\n";
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++) {
			vector<VertexModel> m = processNode(node->mChildren[i], scene);
			meshes.insert(meshes.end(), m.begin(), m.end());
		}
		return meshes;
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

	void Renderer::renderMesh(VertexModel model, Transformation3D transform, ModelData data) {
		glm::mat4 modelPos = glm::translate(glm::vec3(transform.position.x, transform.position.y, transform.position.z));
		glm::mat4 modelRotX = glm::rotate(degrees(transform.rotation.x).asRadians(), glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 modelRotY = glm::rotate(degrees(transform.rotation.y).asRadians(), glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 modelRotZ = glm::rotate(degrees(transform.rotation.z).asRadians(), glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 modelScale = glm::scale(glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));
		glm::mat4 modelRotation = modelRotZ * modelRotY * modelRotX;
		glm::mat4 modelTransform = modelPos * modelRotation * modelScale;

		if (renderer.setGLWindowState(true)) {
			glBindVertexArray(data.vao);
			boundTextures = 0;
			//TODO: Move Shader out to model
			Program* program = data.materials[0]->getProgram();
			//Bind the shaders.
			program->use();

			//Set the uniforms for the shader to use
			Vector3f camPos = currentCamera->getPosition();
			program->setUniform("model", modelTransform);
			program->setUniform("camera", currentCamera->getMatrix());
			program->setUniform("cameraPosition", { camPos.x,camPos.y,camPos.z });
			program->setUniform("timeSec", time.getElapsedSec());

			for (int i = 0; i < data.materials.size(); ++i) {
				setMaterialUniforms(data.materials.at(i), program, i);
			}

			program->setUniform("lightCount", (int)lights.size());
			for (size_t i = 0; i < lights.size(); ++i) {
				setLightUniforms(lights.get(i), i, program);
			}
			// Draw the cube
			if (model.indices.size()) {
				glDrawElements(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT, 0);
			} else {
				glDrawArrays(GL_TRIANGLES, 0, model.vertices.size() / 5);
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
}