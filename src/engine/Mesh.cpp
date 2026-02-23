// Mesh.cpp — GPU mesh data wrapper for NIF model rendering

#include "Mesh.h"
#include <cstdio>
#include <utility>

Mesh::~Mesh() {
    cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_submeshes(std::move(other.m_submeshes)) {
    other.m_submeshes.clear();
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_submeshes = std::move(other.m_submeshes);
        other.m_submeshes.clear();
    }
    return *this;
}

void Mesh::cleanup() {
    for (auto& sm : m_submeshes) {
        if (sm.vao) glDeleteVertexArrays(1, &sm.vao);
        if (sm.vbo) glDeleteBuffers(1, &sm.vbo);
        if (sm.ibo) glDeleteBuffers(1, &sm.ibo);
    }
    m_submeshes.clear();
}

void Mesh::addSubmesh(const std::vector<MeshVertex>& vertices,
                      const std::vector<uint16_t>& indices,
                      GLuint textureID,
                      float diffR, float diffG, float diffB,
                      float alpha, bool alphaBlend) {
    if (vertices.empty() || indices.empty()) return;

    // Track bounds
    for (const auto& v : vertices) {
        float vals[] = {v.px, v.py, v.pz};
        for (float f : vals) {
            if (f < m_boundsMin) m_boundsMin = f;
            if (f > m_boundsMax) m_boundsMax = f;
        }
    }

    MeshSubmesh sm;
    sm.indexCount = (int)indices.size();
    sm.textureID = textureID;
    sm.diffuseR = diffR;
    sm.diffuseG = diffG;
    sm.diffuseB = diffB;
    sm.alpha = alpha;
    sm.hasAlphaBlend = alphaBlend;

    glGenVertexArrays(1, &sm.vao);
    glGenBuffers(1, &sm.vbo);
    glGenBuffers(1, &sm.ibo);

    glBindVertexArray(sm.vao);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, sm.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex),
                 vertices.data(), GL_STATIC_DRAW);

    // Position: location 0, 3 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, px));

    // Normal: location 1, 3 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, nx));

    // UV: location 2, 2 floats
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, u));

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sm.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t),
                 indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    m_submeshes.push_back(sm);
}

void Mesh::draw(GLuint shaderProgram) const {
    for (const auto& sm : m_submeshes) {
        if (!sm.vao || sm.indexCount == 0) continue;

        // Set material uniforms
        GLint locDiffuse = glGetUniformLocation(shaderProgram, "uDiffuseColor");
        if (locDiffuse >= 0)
            glUniform4f(locDiffuse, sm.diffuseR, sm.diffuseG, sm.diffuseB, sm.alpha);

        GLint locUseTex = glGetUniformLocation(shaderProgram, "uUseTexture3D");
        if (sm.textureID) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sm.textureID);
            if (locUseTex >= 0) glUniform1i(locUseTex, 1);
        } else {
            if (locUseTex >= 0) glUniform1i(locUseTex, 0);
        }

        if (sm.hasAlphaBlend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glBindVertexArray(sm.vao);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);

        if (sm.hasAlphaBlend) {
            glDisable(GL_BLEND);
        }
    }
}
