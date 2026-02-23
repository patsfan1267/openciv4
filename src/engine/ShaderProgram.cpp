#include "ShaderProgram.h"
#include <cstdio>
#include <vector>

ShaderProgram::~ShaderProgram() {
    destroy();
}

void ShaderProgram::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

GLuint ShaderProgram::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1);
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        fprintf(stderr, "[shader] %s compilation failed:\n%s\n",
                type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderProgram::compile(const char* vertexSrc, const char* fragmentSrc) {
    destroy();

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return false;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) { glDeleteShader(vs); return false; }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLen = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen + 1);
        glGetProgramInfoLog(m_program, logLen, nullptr, log.data());
        fprintf(stderr, "[shader] Link failed:\n%s\n", log.data());
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return m_program != 0;
}

void ShaderProgram::use() const {
    glUseProgram(m_program);
}

GLint ShaderProgram::uniformLoc(const char* name) const {
    return glGetUniformLocation(m_program, name);
}

void ShaderProgram::setInt(const char* name, int v) const {
    glUniform1i(uniformLoc(name), v);
}

void ShaderProgram::setFloat(const char* name, float v) const {
    glUniform1f(uniformLoc(name), v);
}

void ShaderProgram::setVec2(const char* name, float x, float y) const {
    glUniform2f(uniformLoc(name), x, y);
}

void ShaderProgram::setVec3(const char* name, float x, float y, float z) const {
    glUniform3f(uniformLoc(name), x, y, z);
}

void ShaderProgram::setVec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(uniformLoc(name), x, y, z, w);
}

void ShaderProgram::setMat4(const char* name, const float* m) const {
    glUniformMatrix4fv(uniformLoc(name), 1, GL_FALSE, m);
}
