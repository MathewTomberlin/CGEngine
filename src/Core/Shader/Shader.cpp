#include "Shader.h"
#include <fstream>
#include <sstream>
#include <cassert>
namespace CGEngine {
	Shader::Shader(const string& shaderCode, GLenum shaderType) : objectId(0), refCount(NULL) {
		objectId = glCreateShader(shaderType);
		if (objectId == 0) {
			cout << "glCreateShader failed" << "\n";
		}

		const char* code = shaderCode.c_str();
		glShaderSource(objectId, 1, (const GLchar**)&code, NULL);

		//compile
		glCompileShader(objectId);

		GLint status;
		glGetShaderiv(objectId, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			string msg("Shader failed to compile:\n");

			GLint infoLength;
			glGetShaderiv(objectId, GL_INFO_LOG_LENGTH, &infoLength);
			char* info = new char[infoLength + 1];
			glGetShaderInfoLog(objectId, infoLength, NULL, info);
			msg += info;
			delete[] info;

			glDeleteShader(objectId);
			objectId = 0;
			cout << msg << "\n";
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
			cout << "Failed to open shader file: " << filePath << "\n";
		}

		stringstream buffer;
		buffer << f.rdbuf();

		Shader shader(buffer.str(), shaderType);
		return shader;
	}

	void Shader::retain() {
		assert(refCount);
		*refCount += 1;
	}

	void Shader::release() {
		assert(refCount && *refCount > 0);
		*refCount -= 1;
		if (*refCount == 0) {
			glDeleteShader(objectId);
			objectId = 0;
			delete refCount;
			refCount = NULL;
		}
	}
}