#pragma once

#include "Shader.h"
#include <vector>
#include <glm.hpp>
#include <gtc/type_ptr.hpp>

namespace CGEngine {
	class Program {
	public:
		Program(const vector<Shader>& shaders);
		Program(string vertexShader, string fragmentShader);
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
	private:
		GLuint objectId;

		Program(const Program&);
		const Program& operator=(const Program&);
	};
}