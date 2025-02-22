#include "Program.h"
#include <stdexcept>

namespace CGEngine {
	Program::Program(const vector<Shader>& shaders) : objectId(0) {
		if (shaders.size() <= 0) {
			cout << "No shaders provided to program" << "\n";
		}

		objectId = glCreateProgram();
		if (objectId == 0) {
			cout << "glCreateProgram failed" << "\n";
		}

		for (unsigned i = 0; i < shaders.size(); ++i) {
			glAttachShader(objectId, shaders[i].getObjectId());
		}

		glLinkProgram(objectId);

		for (unsigned i = 0; i < shaders.size(); ++i) {
			glDetachShader(objectId, shaders[i].getObjectId());
		}

		GLint status;
		glGetProgramiv(objectId, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			string msg("Program failed to link shaders: ");

			GLint infoLength;
			glGetProgramiv(objectId, GL_INFO_LOG_LENGTH, &infoLength);
			char* info = new char[infoLength + 1];
			glGetProgramInfoLog(objectId, infoLength, NULL, info);
			msg += info;
			delete[] info;

			glDeleteProgram(objectId);
			objectId = 0;
			cout << msg << "\n";
		}
	}

	Program::Program(string vertexShaderPath, string fragmentShaderPath) : 
		Program({ Shader::readFile(vertexShaderPath, GL_VERTEX_SHADER),Shader::readFile(fragmentShaderPath, GL_FRAGMENT_SHADER) }) { }

	Program::~Program() {
		if (objectId != 0) {
			glDeleteProgram(objectId);
		}
	}

	GLuint Program::getObjectId() const {
		return objectId;
	}

	GLint Program::attrib(const GLchar* attribName) const {
		if (!attribName) {
			cout << "attribName was NULL" << "\n";
		}

		GLint attrib = glGetAttribLocation(objectId, attribName);
		if (attrib == -1) {
			cout << "Program attribute not found: " << attribName << "\n";
		}

		return attrib;
	}

	GLint Program::uniform(const GLchar* uniformName) const {
		if (!uniformName) {
			cout << "uniformName was NULL" << "\n";
		}

		GLint uniform = glGetUniformLocation(objectId, uniformName);
		if (uniform == -1) {
			cout << "Program uniform not found: " << uniformName << "\n";
		}

		return uniform;
	}
}