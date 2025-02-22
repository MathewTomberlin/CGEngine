#include "../Engine/Engine.h"
#include "Renderer.h"
#include "../../Standard/Meshes/CommonVArrays.h"
namespace CGEngine {
	void Renderer::setWindow(RenderWindow* window) {
		this->window = window;
	}
	void Renderer::initializeOpenGL() {
		if (currentCamera == nullptr) {
			currentCamera = new Camera({0,0,0},{0,0,0});
		}

		if (!setGLWindowState(true)) return;
		// Load the default shaders
		if (program == 0) {
			loadFromMemory(defaultVertexShader, ShaderType::Vertex);
			loadFromMemory(defaultFragShader, ShaderType::Fragment);
		}

		// Enable Z-buffer read and write
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		// Setup a perspective projection
		GLfloat ratio = static_cast<float>(window->getSize().x) / window->getSize().y;
		projection = glm::frustum(-ratio, ratio, -1.f, 1.f, 1.f, 500.0f);
		
		auto stride = sizeof(GLfloat) * 5;
		auto textureCoordOffset = sizeof(GLfloat) * 3;
		//Generate and bind vertex array and vertex buffer
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vertexVBO);
		glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
		//Enable and prepare vertex and texture coordinate attribute arrays
		glEnableVertexAttribArray(static_cast<GLuint>(VertexAttribute::Position));
		glVertexAttribPointer(static_cast<GLuint>(VertexAttribute::Position), 3, GL_FLOAT, GL_FALSE, stride, 0);
		glEnableVertexAttribArray(static_cast<GLuint>(VertexAttribute::TexCoord));
		glVertexAttribPointer(static_cast<GLuint>(VertexAttribute::TexCoord), 2, GL_FLOAT, GL_FALSE, stride, (void*)textureCoordOffset);
		//Generate and bind the index buffer
		glGenBuffers(1, &indexVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVBO);
		//Make sure to bind the vertex array to null if you wish to define more objects.
		glBindVertexArray(0);
		commitGL();
		if (!setGLWindowState(false)) return;

		// Setup OPENGL viewport within window
		//FloatRect viewport = openGLSettings.viewport;
		//GLsizei viewPositionX_px = viewport.position.x;
		//GLsizei viewPositionY_px = viewport.position.y;
		//float viewportSizeX = min(1.f, (float)window->getSize().y / window->getSize().x) * (float)window->getSize().x;
		//float viewportSizeY = min(1.f, (float)window->getSize().x / window->getSize().y)* (float)window->getSize().y;
		//int xOffset = ((int)window->getSize().x - viewportSizeX)/2;
		//int yOffset = ((int)window->getSize().y - viewportSizeY)/2;

		//glMatrixMode(GL_PROJECTION);
		//glLoadIdentity();
		////This perspective is clipped by the near and far clip planes
		//glFrustum(-1,1,-1,1, openGLSettings.nearClipPlane, openGLSettings.farClipPlane);
		////ViewPosition should allow the openGL viewport to be moved on the screen, x/yOffset centers the viewport, and viewportSizeXY squares it
		//glViewport(viewPositionX_px+xOffset, viewPositionY_px+yOffset, viewportSizeX, viewportSizeY);
	}

	bool Renderer::setGLWindowState(bool state) {
		// Make the window no longer the active window for OpenGL calls
		bool success = window->setActive(state);
		if (!success) {
			std::cerr << "Failed to set window to " << ((state) ? "active" : "inactive") << std::endl;
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
		glViewport(0, 0, window->getSize().x, window->getSize().y);
		//Clear render order
		clear();
		//Collect Bodies to render
		Body* root = world->getRoot();
		root->render(*window, root->getTransform());
		//Sort and render Meshes and SFML entities
		render(window);
		//Reset OpenGL parameters
		if (setGLWindowState(true)) {
			glBindVertexArray(0);
			glUseProgram(0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		if (!setGLWindowState(false));
		window->display();
		return true;
	}

	void Renderer::renderMesh(VertexModel model, Transformation3D transform) {
		//TODO: Apply scale
		glm::mat4 matrix_pos = glm::translate(glm::vec3(transform.position.x, transform.position.y, transform.position.z));
		glm::mat4 matrix_rotX = glm::rotate(transform.rotation.x, glm::vec3(1.f, 0.f, 0.f));
		glm::mat4 matrix_rotY = glm::rotate(transform.rotation.y, glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 matrix_rotZ = glm::rotate(transform.rotation.z, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4 matrix_rotation = matrix_rotZ * matrix_rotY * matrix_rotX;
		glm::mat4 meshTransform = matrix_pos * matrix_rotation;
		glm::mat4 viewProj = projection * meshTransform;

		if (renderer.setGLWindowState(true)) {
			//Bind the shaders.
			glUseProgram(renderer.program);

			//Set the uniforms for the shader to use.
			if (renderer.uniform[(int)UniformType::TransformPVM] >= 0) {
				glUniformMatrix4fv((unsigned int)renderer.uniform[(int)UniformType::TransformPVM], 1, GL_FALSE, &viewProj[0][0]);
			}

			// Draw the cube
			glDrawElements(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT, 0);

			// Unbind varray, program, and texture
			glBindVertexArray(0);
			glUseProgram(0);
		}
		if (renderer.setGLWindowState(false));
	}

	void Renderer::bufferMeshData(VertexModel model) {
		//Apply the incoming model data
		if (renderer.setGLWindowState(true)) {
			glBindVertexArray(renderer.vao);
			//Apply model vertices and vertex texture coordinate data
			glBufferData(GL_ARRAY_BUFFER, model.vertexComponentSpan, model.vertices.data(), GL_STATIC_DRAW);
			//Apply model vertex face indices
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.vertexSpan, model.indices.data(), GL_STATIC_DRAW);
		}
		renderer.setGLWindowState(false);
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

	
	void Renderer::loadFromMemory(const std::string& shaderData, ShaderType type) {
		if (shaderData.empty()) return;

		//Detach and delete already attached shader
		if (shader[static_cast<unsigned int>(type)]) {
			glDetachShader(program, shader[static_cast<unsigned int>(type)]);
			glDeleteShader(shader[static_cast<unsigned int>(type)]);
		}

		//Build the shader by its type
		switch (type) {
		case ShaderType::Vertex:
			shader[static_cast<unsigned int>(type)] = buildShader(shaderData, GL_VERTEX_SHADER);
			break;
		case ShaderType::Geometry:
			shader[static_cast<unsigned int>(type)] = buildShader(shaderData, GL_GEOMETRY_SHADER);
			break;
		case ShaderType::Fragment:
			shader[static_cast<unsigned int>(type)] = buildShader(shaderData, GL_FRAGMENT_SHADER);
			break;
		default:
			break;
		}

		//Ceate the program if it's null
		if (program == 0) {
			program = glCreateProgram();
		}

		//Attach the shader and assign Position and TexCoord VertexAttributes to position and texCoord
		glAttachShader(program, shader[static_cast<unsigned int>(type)]);
		glBindAttribLocation(program, static_cast<GLuint>(VertexAttribute::Position), "position");
		glBindAttribLocation(program, static_cast<GLuint>(VertexAttribute::TexCoord), "texCoord");

		//Link and validate the shaders
		glLinkProgram(program);
		checkError(program, GL_LINK_STATUS, true, "Shader link error:");
		glValidateProgram(program);
		checkError(program, GL_VALIDATE_STATUS, true, "Invalid shader:");

		uniform[static_cast<unsigned int>(UniformType::TransformPVM)] = glGetUniformLocation(program, "pvm");
	}

	///Creates and compiles a shader.
	GLuint Renderer::buildShader(const std::string& l_src, unsigned int l_type) {
		GLuint shaderID = glCreateShader(l_type);
		if (!shaderID) {
			std::cout << "Bad shader type!";
			return 0;
		}
		const GLchar* sources[1];
		GLint lengths[1];
		sources[0] = l_src.c_str();
		lengths[0] = l_src.length();
		glShaderSource(shaderID, 1, sources, lengths);
		glCompileShader(shaderID);
		checkError(shaderID, GL_COMPILE_STATUS, false, "Shader compile error: ");
		return shaderID;
	}

	///Checks for any errors specific to the shaders. It will output any errors within the shader if it's not valid.
	void Renderer::checkError(GLuint l_shader, GLuint l_flag, bool l_program, const string& l_errorMsg) {
		GLint success = 0;
		GLchar error[1024] = { 0 };
		if (l_program) { glGetProgramiv(l_shader, l_flag, &success); }
		else { glGetShaderiv(l_shader, l_flag, &success); }

		if (success) { return; }
		if (l_program) {
			glGetProgramInfoLog(l_shader, sizeof(error), nullptr, error);
		}
		else {
			glGetShaderInfoLog(l_shader, sizeof(error), nullptr, error);
		}

		std::cout << l_errorMsg << "\n";
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
}