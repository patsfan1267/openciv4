// TerrainMesh.cpp — 3D terrain mesh generation from MapSnapshot
//
// Builds a subdivided grid mesh where each tile is a 4x4 quad patch.
// World coordinate system: X=East, Y=Up, Z=South. 1 tile = 1.0 world unit.
// Tile (col, row) maps to world center (col+0.5, height, row+0.5).
//
// M2: Height values per plot type + Laplacian smoothing + normal recomputation.

#include "TerrainMesh.h"
#include "MapSnapshot.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// Height values per plot type
static float getPlotHeight(int plotType) {
    switch (plotType) {
        case 0: return 4.5f;  // PLOT_PEAK
        case 1: return 2.5f;  // PLOT_HILLS
        case 2: return 1.0f;  // PLOT_LAND (flat)
        case 3: return 0.0f;  // PLOT_OCEAN
        default: return 0.5f;
    }
}

// Get coast height (terrainType 1 = TERRAIN_COAST in Civ4)
static float getTileHeight(const PlotData& plot) {
    float h = getPlotHeight(plot.plotType);
    // Coast tiles are shallow water — between ocean and land
    if (plot.plotType == 3 && plot.terrainType == 1) {
        h = 0.2f;
    }
    return h;
}

TerrainMesh::~TerrainMesh() {
    cleanup();
}

void TerrainMesh::cleanup() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_ibo) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
    m_indexCount = 0;
    m_heightfield.clear();
    m_gridW = m_gridH = 0;
}

void TerrainMesh::rebuild(const MapSnapshot& snapshot) {
    cleanup();

    m_mapWidth = snapshot.width;
    m_mapHeight = snapshot.height;
    if (m_mapWidth == 0 || m_mapHeight == 0) return;

    m_gridW = m_mapWidth * SUBDIV + 1;
    m_gridH = m_mapHeight * SUBDIV + 1;
    int totalVerts = m_gridW * m_gridH;

    std::vector<TerrainVertex> verts(totalVerts);
    m_heightfield.resize(totalVerts);

    // --- Pass 1: Assign base heights and colors ---
    // For each vertex, determine which tile(s) it belongs to and assign height.
    // Interior vertices get the tile's height directly.
    // Edge/corner vertices (on tile boundaries) get the average of adjacent tiles.

    for (int gz = 0; gz < m_gridH; gz++) {
        for (int gx = 0; gx < m_gridW; gx++) {
            int idx = gz * m_gridW + gx;

            float worldX = (float)gx / SUBDIV;
            float worldZ = (float)gz / SUBDIV;

            verts[idx].px = worldX;
            verts[idx].pz = worldZ;

            // Determine the tile this vertex is in (or on the boundary of)
            // For boundary vertices, average heights of adjacent tiles
            bool onEdgeX = (gx % SUBDIV == 0) && (gx > 0) && (gx < m_mapWidth * SUBDIV);
            bool onEdgeZ = (gz % SUBDIV == 0) && (gz > 0) && (gz < m_mapHeight * SUBDIV);

            float heightSum = 0.0f;
            int heightCount = 0;
            float colorR = 0, colorG = 0, colorB = 0;

            // Collect contributing tiles
            int tileXmin = (int)(worldX);
            int tileZmin = (int)(worldZ);
            int tileXmax = onEdgeX ? tileXmin : tileXmin;
            int tileZmax = onEdgeZ ? tileZmin : tileZmin;
            // On an edge, also include the tile to the left/above
            if (onEdgeX) tileXmin--;
            if (onEdgeZ) tileZmin--;

            for (int tz = tileZmin; tz <= tileZmax; tz++) {
                for (int tx = tileXmin; tx <= tileXmax; tx++) {
                    int cx = tx < 0 ? 0 : (tx >= m_mapWidth ? m_mapWidth - 1 : tx);
                    int cz = tz < 0 ? 0 : (tz >= m_mapHeight ? m_mapHeight - 1 : tz);
                    int gameY = m_mapHeight - 1 - cz;
                    if (gameY < 0) gameY = 0;
                    if (gameY >= m_mapHeight) gameY = m_mapHeight - 1;

                    const PlotData& plot = snapshot.getPlot(cx, gameY);
                    heightSum += getTileHeight(plot);
                    heightCount++;

                    TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
                    colorR += tc.r;
                    colorG += tc.g;
                    colorB += tc.b;
                }
            }

            float baseHeight = heightSum / heightCount;
            m_heightfield[idx] = baseHeight;
            verts[idx].py = baseHeight;

            verts[idx].r = (colorR / heightCount) / 255.0f;
            verts[idx].g = (colorG / heightCount) / 255.0f;
            verts[idx].b = (colorB / heightCount) / 255.0f;

            // Placeholder normals (recomputed after smoothing)
            verts[idx].nx = 0.0f;
            verts[idx].ny = 1.0f;
            verts[idx].nz = 0.0f;
        }
    }

    // --- Pass 2: Laplacian smoothing (2 passes, damping 0.3) ---
    std::vector<float> smoothed(totalVerts);
    const float damping = 0.3f;

    for (int pass = 0; pass < 2; pass++) {
        for (int gz = 0; gz < m_gridH; gz++) {
            for (int gx = 0; gx < m_gridW; gx++) {
                int idx = gz * m_gridW + gx;

                // Average of 4-neighbors
                float sum = 0.0f;
                int count = 0;
                if (gx > 0)           { sum += m_heightfield[(gz)*m_gridW + (gx-1)]; count++; }
                if (gx < m_gridW - 1) { sum += m_heightfield[(gz)*m_gridW + (gx+1)]; count++; }
                if (gz > 0)           { sum += m_heightfield[(gz-1)*m_gridW + (gx)]; count++; }
                if (gz < m_gridH - 1) { sum += m_heightfield[(gz+1)*m_gridW + (gx)]; count++; }

                float avg = sum / count;
                smoothed[idx] = m_heightfield[idx] + damping * (avg - m_heightfield[idx]);
            }
        }
        m_heightfield = smoothed;
    }

    // Apply smoothed heights to vertices
    for (int i = 0; i < totalVerts; i++) {
        verts[i].py = m_heightfield[i];
    }

    // --- Pass 3: Recompute normals from central differences ---
    for (int gz = 0; gz < m_gridH; gz++) {
        for (int gx = 0; gx < m_gridW; gx++) {
            int idx = gz * m_gridW + gx;

            // Height at neighbors (clamped at edges)
            float hL = m_heightfield[gz * m_gridW + (gx > 0 ? gx - 1 : gx)];
            float hR = m_heightfield[gz * m_gridW + (gx < m_gridW - 1 ? gx + 1 : gx)];
            float hD = m_heightfield[(gz < m_gridH - 1 ? gz + 1 : gz) * m_gridW + gx];
            float hU = m_heightfield[(gz > 0 ? gz - 1 : gz) * m_gridW + gx];

            // Central differences: the grid spacing is 1.0/SUBDIV in world units
            float spacing = 1.0f / SUBDIV;
            float dhdx = (hR - hL) / (2.0f * spacing);
            float dhdz = (hD - hU) / (2.0f * spacing);

            // Normal = normalize(-dhdx, 1, -dhdz)
            float nx = -dhdx;
            float ny = 1.0f;
            float nz = -dhdz;
            float len = sqrtf(nx*nx + ny*ny + nz*nz);
            if (len > 0.0001f) {
                nx /= len; ny /= len; nz /= len;
            }

            verts[idx].nx = nx;
            verts[idx].ny = ny;
            verts[idx].nz = nz;
        }
    }

    // --- Generate indices: two triangles per sub-quad ---
    int quadsW = m_gridW - 1;
    int quadsH = m_gridH - 1;
    int totalQuads = quadsW * quadsH;
    int totalIndices = totalQuads * 6;

    std::vector<uint32_t> indices(totalIndices);
    int ii = 0;
    for (int qz = 0; qz < quadsH; qz++) {
        for (int qx = 0; qx < quadsW; qx++) {
            uint32_t tl = qz * m_gridW + qx;
            uint32_t tr = tl + 1;
            uint32_t bl = (qz + 1) * m_gridW + qx;
            uint32_t br = bl + 1;

            indices[ii++] = tl;
            indices[ii++] = bl;
            indices[ii++] = tr;
            indices[ii++] = tr;
            indices[ii++] = bl;
            indices[ii++] = br;
        }
    }

    m_indexCount = totalIndices;

    // --- Upload to GPU ---
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
    // location 2: color (3 floats)
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
    if (m_gridW == 0 || m_gridH == 0) return 0.0f;

    // Convert world coords to grid coords
    float gx = worldX * SUBDIV;
    float gz = worldZ * SUBDIV;

    // Clamp to grid bounds
    if (gx < 0) gx = 0;
    if (gz < 0) gz = 0;
    if (gx >= m_gridW - 1) gx = (float)(m_gridW - 2);
    if (gz >= m_gridH - 1) gz = (float)(m_gridH - 2);

    int ix = (int)gx;
    int iz = (int)gz;
    float fx = gx - ix;
    float fz = gz - iz;

    // Bilinear interpolation
    float h00 = m_heightfield[iz * m_gridW + ix];
    float h10 = m_heightfield[iz * m_gridW + ix + 1];
    float h01 = m_heightfield[(iz + 1) * m_gridW + ix];
    float h11 = m_heightfield[(iz + 1) * m_gridW + ix + 1];

    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);
    return h0 + fz * (h1 - h0);
}
