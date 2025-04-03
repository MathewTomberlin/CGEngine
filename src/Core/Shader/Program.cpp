#include "Program.h"
#include "../Engine/Engine.h"
#include <stdexcept>

namespace CGEngine {
	//Given vert and frag shaders (possibly loaded from file) and a program name
	//programPath is empty if shaders are not loaded
	Program::Program(const vector<Shader>& shaders, string programName) : objectId(0) {
		init();
		this->programName = programName;
		if (shaders.size() <= 0) {
			log(this, LogError, "No shaders provided to program");
		}

		objectId = glCreateProgram();
		if (objectId == 0) {
			log(this, LogError, "glCreateProgram failed");
		}

		// Attach shaders
		for (const auto& shader : shaders) {
			if (shader.getObjectId() == 0) {
				log(this, LogError, "Attempting to attach invalid shader");
				continue;
			}
			GL_CHECK(glAttachShader(objectId, shader.getObjectId()));
		}

		// Link program
		GL_CHECK(glLinkProgram(objectId));

		// Detach shaders
		for (const auto& shader : shaders) {
			GL_CHECK(glDetachShader(objectId, shader.getObjectId()));
		}

		GLint status;
		GL_CHECK(glGetProgramiv(objectId, GL_LINK_STATUS, &status));
		if (status == GL_FALSE) {
			GLint infoLength;
			GL_CHECK(glGetProgramiv(objectId, GL_INFO_LOG_LENGTH, &infoLength));

			std::vector<char> infoLog(infoLength + 1);
			GL_CHECK(glGetProgramInfoLog(objectId, infoLength, NULL, infoLog.data()));

			log(this, LogError, "Program failed to link shaders: {}", infoLog.data());

			GL_CHECK(glDeleteProgram(objectId));
			objectId = 0;
		} else {
			log(this, LogInfo, "Shader program linked successfully (ID: {})", objectId);
		}
	}

	//Given vert and frag shader paths
	Program::Program(string vertexShaderPath, string fragmentShaderPath, string programName) : 
		Program({ 
			Shader::readFile(vertexShaderPath, GL_VERTEX_SHADER),
			Shader::readFile(fragmentShaderPath, GL_FRAGMENT_SHADER) 
		}, programName)
	{
		programPath = ShaderProgramPath(vertexShaderPath, fragmentShaderPath);
	}

	//Given a ShaderProgramPath (vert & frag paths)
	Program::Program(ShaderProgramPath shaderPath, string programName) :
		Program(
			shaderPath.vertexShaderPath, 
			shaderPath.fragmentShaderPath,
			programName
		) 
	{};

	Program::~Program() {
		if (objectId != 0) {
			log(this, LogDebug, "Deleting shader program (ID: {})", objectId);
			glDeleteProgram(objectId);
		}
	}

	GLuint Program::getObjectId() const {
		return objectId;
	}

	GLint Program::attrib(const GLchar* attribName) const {
		if (!attribName) {
			log(LogError, "Program", "attribName was NULL");
			return -1;
		}

		GLint attrib = glGetAttribLocation(objectId, attribName);
		if (attrib == -1) {
			// Using LogDebug instead of Error because missing attributes might be expected
			log(LogDebug, "Program", "Program attribute not found: {}", attribName);
		}

		return attrib;
	}

	GLint Program::uniform(const GLchar* uniformName) const {
		if (!uniformName) {
			log(LogError, "Program", "uniformName was NULL");
			return -1;
		}

		GLint uniform = glGetUniformLocation(objectId, uniformName);
		if (uniform == -1) {
			// Using LogDebug instead of Error because missing attributes might be expected
			log(LogDebug, "Program", "Program uniform not found: {}", uniformName);
		}

		return uniform;
	}

	bool Program::isValid() const {
		return objectId != 0;
	}

	void Program::use() {
		if (!isValid()) {
			log(this, LogError, "Attempting to use invalid shader program");
			return;
		}
		GL_CHECK(glUseProgram(getObjectId()));
	}

	void Program::stop() {
		GL_CHECK(glUseProgram(0));
	}

	GLint Program::inUse() {
		GLint currentProgram = 0;
		GL_CHECK(glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram));
		return currentProgram;
	}

	vector<UniformInfo> Program::getActiveUniforms() {
		vector<UniformInfo> uniforms;
		if (!isValid()) {
			log(this, LogError, "Attempting to get uniforms from invalid shader program");
			return uniforms;
		}

		GLint numUniforms = 0;
		GL_CHECK(glGetProgramiv(objectId, GL_ACTIVE_UNIFORMS, &numUniforms));

		GLint maxNameLength = 0;
		GL_CHECK(glGetProgramiv(objectId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength));

		for (GLint i = 0; i < numUniforms; ++i) {
			UniformInfo uniformInfo;
			vector<GLchar> nameBuffer(maxNameLength);
			GLsizei nameLength;

			GL_CHECK(glGetActiveUniform(objectId, i, maxNameLength, &nameLength,
				&uniformInfo.size, &uniformInfo.type, nameBuffer.data()));

			uniformInfo.name = string(nameBuffer.data(), nameLength);
			uniformInfo.location = glGetUniformLocation(objectId, uniformInfo.name.c_str());

			uniforms.push_back(uniformInfo);
			log(this, LogDebug, "Found uniform '{}' at location {}", uniformInfo.name, uniformInfo.location);
		}

		return uniforms;
	}

	vector<AttributeInfo> Program::getActiveAttributes() {
		vector<AttributeInfo> attributes;
		if (!isValid()) {
			log(this, LogError, "Attempting to get attributes from invalid shader program");
			return attributes;
		}

		GLint numAttributes = 0;
		GL_CHECK(glGetProgramiv(objectId, GL_ACTIVE_ATTRIBUTES, &numAttributes));

		GLint maxNameLength = 0;
		GL_CHECK(glGetProgramiv(objectId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxNameLength));

		for (GLint i = 0; i < numAttributes; ++i) {
			AttributeInfo attributeInfo;
			vector<GLchar> nameBuffer(maxNameLength);
			GLsizei nameLength;

			GL_CHECK(glGetActiveAttrib(objectId, i, maxNameLength, &nameLength,
				&attributeInfo.size, &attributeInfo.type, nameBuffer.data()));

			attributeInfo.name = string(nameBuffer.data(), nameLength);
			attributeInfo.location = glGetAttribLocation(objectId, attributeInfo.name.c_str());

			attributes.push_back(attributeInfo);
			log(this, LogDebug, "Found attribute '{}' at location {}", attributeInfo.name, attributeInfo.location);
		}

		return attributes;
	}

	GLenum Program::checkGLError(const char* operation, const char* file, int line) {
		GLenum errorCode;
		while ((errorCode = glGetError()) != GL_NO_ERROR) {
			std::string error;
			switch (errorCode) {
			case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
			case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
			case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
			case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
			case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
			case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
			default:                               error = "Unknown Error"; break;
			}
			log(this, LogError, "OpenGL Error: {} [{}] ({}) at {}:{}", error, errorCode, operation, file, line);
		}
		return errorCode;
	}
}