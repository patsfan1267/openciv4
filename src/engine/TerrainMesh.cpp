// TerrainMesh.cpp — 3D terrain mesh generation from MapSnapshot
//
// Builds a subdivided grid mesh where each tile is a 4x4 quad patch.
// World coordinate system: X=East, Y=Up, Z=South. 1 tile = 1.0 world unit.
// Tile (col, row) maps to world center (col+0.5, height, row+0.5).

#include "TerrainMesh.h"
#include "MapSnapshot.h"
#include <cstdio>
#include <cstring>

TerrainMesh::~TerrainMesh() {
    cleanup();
}

void TerrainMesh::cleanup() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ibo) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
    m_indexCount = 0;
}

void TerrainMesh::rebuild(const MapSnapshot& snapshot) {
    cleanup();

    m_mapWidth = snapshot.width;
    m_mapHeight = snapshot.height;
    if (m_mapWidth == 0 || m_mapHeight == 0) return;

    // Total vertices: each tile has (SUBDIV+1)^2 vertices
    // But neighboring tiles share edge vertices. For simplicity in M1,
    // we generate a global grid of (width*SUBDIV+1) x (height*SUBDIV+1) vertices.
    int gridW = m_mapWidth * SUBDIV + 1;
    int gridH = m_mapHeight * SUBDIV + 1;
    int totalVerts = gridW * gridH;

    std::vector<TerrainVertex> verts(totalVerts);

    // Generate vertex positions and colors
    for (int gz = 0; gz < gridH; gz++) {
        for (int gx = 0; gx < gridW; gx++) {
            int idx = gz * gridW + gx;

            // World position: each tile is 1.0 unit wide
            float worldX = (float)gx / SUBDIV;
            float worldZ = (float)gz / SUBDIV;
            float worldY = 0.0f; // M1: flat terrain

            verts[idx].px = worldX;
            verts[idx].py = worldY;
            verts[idx].pz = worldZ;

            // M1: flat normal pointing up
            verts[idx].nx = 0.0f;
            verts[idx].ny = 1.0f;
            verts[idx].nz = 0.0f;

            // Determine which tile this vertex belongs to (center tile for color)
            int tileX = (int)(worldX);
            int tileZ = (int)(worldZ);
            if (tileX >= m_mapWidth) tileX = m_mapWidth - 1;
            if (tileZ >= m_mapHeight) tileZ = m_mapHeight - 1;
            if (tileX < 0) tileX = 0;
            if (tileZ < 0) tileZ = 0;

            // Map row: game Y is flipped (game y=0 is south, but our Z increases south)
            // In MapSnapshot, plots[y * width + x] where y=0 is the south edge.
            // Our world Z=0 is north (top of screen in top-down), Z increases south.
            // So world row gz corresponds to game y = (mapHeight - 1 - tileZ).
            int gameY = m_mapHeight - 1 - tileZ;
            if (gameY < 0) gameY = 0;
            if (gameY >= m_mapHeight) gameY = m_mapHeight - 1;

            const PlotData& plot = snapshot.getPlot(tileX, gameY);

            // Get terrain color
            TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
            verts[idx].r = tc.r / 255.0f;
            verts[idx].g = tc.g / 255.0f;
            verts[idx].b = tc.b / 255.0f;
        }
    }

    // Generate indices: two triangles per sub-quad
    int quadsW = gridW - 1;
    int quadsH = gridH - 1;
    int totalQuads = quadsW * quadsH;
    int totalIndices = totalQuads * 6; // 2 tris * 3 indices

    std::vector<uint32_t> indices(totalIndices);
    int ii = 0;
    for (int qz = 0; qz < quadsH; qz++) {
        for (int qx = 0; qx < quadsW; qx++) {
            uint32_t tl = qz * gridW + qx;
            uint32_t tr = tl + 1;
            uint32_t bl = (qz + 1) * gridW + qx;
            uint32_t br = bl + 1;

            // Triangle 1: TL, BL, TR
            indices[ii++] = tl;
            indices[ii++] = bl;
            indices[ii++] = tr;
            // Triangle 2: TR, BL, BR
            indices[ii++] = tr;
            indices[ii++] = bl;
            indices[ii++] = br;
        }
    }

    m_indexCount = totalIndices;

    // Upload to GPU
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TerrainVertex),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t),
                 indices.data(), GL_STATIC_DRAW);

    // Vertex attributes:
    // location 0: position (3 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          (void*)offsetof(TerrainVertex, px));
    // location 1: normal (3 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          (void*)offsetof(TerrainVertex, nx));
    // location 2: color (3 floats) — used as UV slot temporarily in M1
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          (void*)offsetof(TerrainVertex, r));

    glBindVertexArray(0);

    fprintf(stderr, "[TerrainMesh] Built: %dx%d tiles, %d verts, %d tris\n",
            m_mapWidth, m_mapHeight, totalVerts, totalIndices / 3);
}

void TerrainMesh::draw() const {
    if (m_indexCount == 0) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

float TerrainMesh::getHeight(float worldX, float worldZ) const {
    // M1: flat terrain, always 0
    (void)worldX;
    (void)worldZ;
    return 0.0f;
}
