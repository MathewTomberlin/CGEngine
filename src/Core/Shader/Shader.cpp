#include "../Engine/Engine.h"
#include "Shader.h"
#include <fstream>
#include <sstream>
#include <cassert>
namespace CGEngine {
	Shader::Shader(const string& shaderCode, GLenum shaderType) : objectId(0), refCount(NULL) {
		init();

		// Get shader type name for logging
		switch (shaderType) {
			case GL_VERTEX_SHADER: shaderTypeName = "vertex"; break;
			case GL_FRAGMENT_SHADER: shaderTypeName = "fragment"; break;
			case GL_GEOMETRY_SHADER: shaderTypeName = "geometry"; break;
			case GL_COMPUTE_SHADER: shaderTypeName = "compute"; break;
			case GL_TESS_CONTROL_SHADER: shaderTypeName = "tessellation control"; break;
			case GL_TESS_EVALUATION_SHADER: shaderTypeName = "tessellation evaluation"; break;
			default: shaderTypeName = "unknown"; break;
		}

		objectId = glCreateShader(shaderType);
		if (objectId == 0) {
			log(this, LogError, "Failed to create {} shader", shaderTypeName);
			return;
		}

		const char* code = shaderCode.c_str();
		GL_CHECK(glShaderSource(objectId, 1, (const GLchar**)&code, NULL));

		//compile
		GL_CHECK(glCompileShader(objectId));

		GLint status;
		GL_CHECK(glGetShaderiv(objectId, GL_COMPILE_STATUS, &status));
		if (status == GL_FALSE) {
			GLint infoLength;
			GL_CHECK(glGetShaderiv(objectId, GL_INFO_LOG_LENGTH, &infoLength));
			vector<char> infoLog(infoLength + 1);
			GL_CHECK(glGetShaderInfoLog(objectId, infoLength, NULL, infoLog.data()));
			log(this, LogError, "{} shader compilation failed: {}", shaderTypeName, infoLog.data());

			GL_CHECK(glDeleteShader(objectId));
			objectId = 0;
		} else {
			log(this, LogInfo, "{} shader compiled successfully (ID: {})", shaderTypeName, objectId);
		}

		refCount = new unsigned;
		*refCount = 1;
	}

	Shader::Shader(const Shader& other) : objectId(other.objectId), refCount(other.refCount) {
		retain();
	}

	Shader::~Shader() {
		if (refCount) {
			release();
		}
	}

	GLuint Shader::getObjectId() const {
		return objectId;
	}

	Shader& Shader::operator=(const Shader& other) {
		release();
		objectId = other.objectId;
		refCount = other.refCount;
		retain();
		return *this;
	}

	Shader Shader::readFile(const string& filePath, GLenum shaderType) {
		ifstream f;
		f.open(filePath.c_str(), ios::in | ios::binary);
		if (!f.is_open()) {
			log(LogError, "Shader", "Failed to open shader file '{}'", filePath);
			return Shader("", shaderType); // Return invalid shader
		}

		stringstream buffer;
		buffer << f.rdbuf();
		f.close();

		Shader shader(buffer.str(), shaderType);
		return shader;
	}

	bool Shader::isValid() const {
		return objectId != 0;
	}

	void Shader::retain() {
		assert(refCount);
		*refCount += 1;
	}

	void Shader::release() {
		assert(refCount && *refCount > 0);
		*refCount -= 1;
		if (*refCount == 0) {
			log(this, LogDebug, "Deleting shader (ID: {})", objectId);
			GL_CHECK(glDeleteShader(objectId));
			objectId = 0;
			delete refCount;
			refCount = NULL;
		}
	}

	GLenum Shader::checkGLError(const char* operation, const char* file, int line) {
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