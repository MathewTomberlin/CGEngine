#pragma once

#include "Shader.h"
#include <vector>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>

#define GL_CHECK(x) x; checkGLError(#x, __FILE__, __LINE__)

namespace CGEngine {
	struct ShaderProgramPath {
		ShaderProgramPath(string vShaderPath = "shaders/StdVertexShader.txt", string fShaderPath = "shaders/StdFragShader.txt") :vertexShaderPath(vShaderPath), fragmentShaderPath(fShaderPath) {};
		string vertexShaderPath = "shaders/StdVertexShader.txt";
		string fragmentShaderPath = "shaders/StdFragShader.txt";
	};

	struct UniformInfo {
		std::string name;
		GLenum type;
		GLint size;
		GLint location;
	};

	struct AttributeInfo {
		std::string name;
		GLenum type;
		GLint size;
		GLint location;
	};

	class Program : public EngineSystem {
	public:
		Program(const vector<Shader>& shaders);
		Program(string vertexShader, string fragmentShader);
		Program(ShaderProgramPath shaderPath);
		~Program();
		GLuint getObjectId() const;
		GLint attrib(const GLchar* attribName) const;
		GLint uniform(const GLchar* uniformName) const;

		void setAttrib(const GLchar* attribName, GLfloat v0) {
			glVertexAttrib1f(attrib(attribName), v0);
		}
		void setAttrib(const GLchar* attribName, GLfloat* v0) {
			glVertexAttrib1fv(attrib(attribName), v0);
		}
		void setAttrib(const GLchar* attribName, GLint v0) {
			glVertexAttribI1i(attrib(attribName), v0);
		}
		void setAttrib(const GLchar* attribName, GLint* v0) {
			glVertexAttribI1iv(attrib(attribName), v0);
		}
		void setUniform(const GLchar* name, GLfloat v0) {
			glUniform1f(uniform(name), v0);
		}
		void setUniform(const GLchar* name, GLint v0) {
			glUniform1i(uniform(name), v0);
		}
		void setUniform(const GLchar* name, GLfloat v0, GLfloat v1) {
			glUniform2f(uniform(name), v0, v1);
		}
		void setUniform(const GLchar* name, GLint v0, GLint v1) {
			glUniform2i(uniform(name), v0, v1);
		}
		void setUniform(const GLchar* name, GLfloat v0, GLfloat v1, GLfloat v2) {
			glUniform3f(uniform(name), v0, v1, v2);
		}
		void setUniform(const GLchar* name, GLint v0, GLint v1, GLint v2) {
			glUniform3i(uniform(name), v0, v1, v2);
		}
		void setUniform(const GLchar* name, GLfloat v0, GLfloat v1, GLfloat v2 , GLfloat v3) {
			glUniform4f(uniform(name), v0, v1, v2, v3);
		}
		void setUniform(const GLchar* name, GLint v0, GLint v1, GLint v2, GLint v3) {
			glUniform4i(uniform(name), v0, v1, v2, v3);
		}
		void setUniform(const GLchar* name, glm::mat2& m, GLboolean transpose = GL_FALSE) {
			glUniformMatrix2fv(uniform(name), 1, transpose, glm::value_ptr(m));
		}
		void setUniform(const GLchar* name, glm::mat3& m, GLboolean transpose = GL_FALSE) {
			glUniformMatrix3fv(uniform(name), 1, transpose, glm::value_ptr(m));
		}
		void setUniform(const GLchar* name, glm::mat4& m, GLboolean transpose = GL_FALSE) {
			glUniformMatrix4fv(uniform(name), 1, transpose, glm::value_ptr(m));
		}
		void setUniform(const GLchar* uniformName, const glm::vec2& v) {
			glUniform2fv(uniform(uniformName), 1, glm::value_ptr(v));
		}
		void setUniform(const GLchar* uniformName, const glm::vec3& v) {
			glUniform3fv(uniform(uniformName), 1, glm::value_ptr(v));
		}
		void setUniform(const GLchar* uniformName, const glm::vec4& v) {
			glUniform4fv(uniform(uniformName), 1, glm::value_ptr(v));
		}
		void setUniform(const GLchar* uniformName, const glm::mat4& m, GLboolean transpose = false) {
			glUniformMatrix4fv(uniform(uniformName), 1, transpose, glm::value_ptr(m));
		}
		vector<UniformInfo> getActiveUniforms();
		vector<AttributeInfo> getActiveAttributes();
		bool isValid() const;
		void use();
		void stop();
		GLint inUse();
	private:
		GLuint objectId;

		Program(const Program&);
		const Program& operator=(const Program&);
		GLenum checkGLError(const char* operation, const char* file, int line);
	};
}