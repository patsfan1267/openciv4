// AssetManager.cpp — Loads terrain textures from Civ4's FPK archives
//
// Terrain type indices (from BTS XML, matches CvEnums.h):
//   TERRAIN_GRASS=0, PLAINS=1, DESERT=2, TUNDRA=3, SNOW=4, COAST=5, OCEAN=6
// Plot types that override terrain visuals:
//   PLOT_PEAK=0 (uses peak texture), PLOT_HILLS=1 (uses hill texture)

#include "AssetManager.h"
#include <cstdio>

// Terrain and feature icon paths in the FPK
struct TerrainIconMap { int terrainType; const char* fpkPath; };
struct FeatureIconMap { int featureType; const char* fpkPath; };

static TerrainIconMap terrainIcons[] = {
    { 0, "art/interface/buttons/baseterrain/grassland.dds" },
    { 1, "art/interface/buttons/baseterrain/plains.dds" },
    { 2, "art/interface/buttons/baseterrain/desert.dds" },
    { 3, "art/interface/buttons/baseterrain/tundra.dds" },
    { 4, "art/interface/buttons/baseterrain/ice.dds" },
    { 5, "art/interface/buttons/baseterrain/coast.dds" },
    { 6, "art/interface/buttons/baseterrain/ocean.dds" },
};

// Feature type order from CIV4FeatureInfos.xml:
// 0=ICE, 1=JUNGLE, 2=OASIS, 3=FLOOD_PLAINS, 4=FOREST, 5=FALLOUT
static FeatureIconMap featureIcons[] = {
    { 0, "art/interface/buttons/terrainfeatures/ice.dds" },
    { 1, "art/interface/buttons/terrainfeatures/jungle.dds" },
    { 2, "art/interface/buttons/terrainfeatures/oasis.dds" },
    { 3, "art/interface/buttons/terrainfeatures/floodplains.dds" },
    { 4, "art/interface/buttons/terrainfeatures/forest.dds" },
    { 5, "art/interface/buttons/terrainfeatures/fallout.dds" },
};

// 512x512 seamless blend textures for map rendering
// TERRAIN_GRASS=0, PLAINS=1, DESERT=2, TUNDRA=3, SNOW=4, COAST=5, OCEAN=6
// Special: -1=PEAK, -2=HILL
static TerrainIconMap terrainBlends[] = {
    {  0, "art/terrain/textures/grassblend.dds" },
    {  1, "art/terrain/textures/plainsblend.dds" },
    {  2, "art/terrain/textures/desertblend.dds" },
    {  3, "art/terrain/textures/tundrablend.dds" },
    {  4, "art/terrain/textures/iceblend.dds" },
    {  5, "art/terrain/textures/coastblend.dds" },
    {  6, "art/terrain/textures/oceanblend.dds" },
    { -1, "art/terrain/textures/peakblend.dds" },
    { -2, "art/terrain/textures/hillblend.dds" },
};

// Feature blend textures (only forest and jungle have blend textures)
// 0=ICE, 1=JUNGLE, 2=OASIS, 3=FLOOD_PLAINS, 4=FOREST, 5=FALLOUT
static FeatureIconMap featureBlends[] = {
    { 1, "art/terrain/textures/jungleblend.dds" },
    { 4, "art/terrain/textures/forestblend.dds" },
};

AssetManager::~AssetManager() {
    for (auto& pair : m_terrainTextures)
        if (pair.second) SDL_DestroyTexture(pair.second);
    for (auto& pair : m_featureTextures)
        if (pair.second) SDL_DestroyTexture(pair.second);

    for (auto& pair : m_terrainTexturesGL)
        if (pair.second) glDeleteTextures(1, &pair.second);
    for (auto& pair : m_featureTexturesGL)
        if (pair.second) glDeleteTextures(1, &pair.second);
    for (auto& pair : m_terrainBlendGL)
        if (pair.second) glDeleteTextures(1, &pair.second);
    for (auto& pair : m_featureBlendGL)
        if (pair.second) glDeleteTextures(1, &pair.second);
    if (m_oceanWaterGL) glDeleteTextures(1, &m_oceanWaterGL);
    if (m_coastWaterGL) glDeleteTextures(1, &m_coastWaterGL);
    if (m_waterSurfaceGL) glDeleteTextures(1, &m_waterSurfaceGL);
    if (m_riverAtlasGL) glDeleteTextures(1, &m_riverAtlasGL);
    if (m_hillDecalGL) glDeleteTextures(1, &m_hillDecalGL);
    for (auto& pair : m_terrainDetailGL)
        if (pair.second) glDeleteTextures(1, &pair.second);
}

bool AssetManager::openArt0(const char* btsInstallDir) {
    std::string baseDir = std::string(btsInstallDir) + "/../Assets";
    std::string fpkPath = baseDir + "/Art0.FPK";
    fprintf(stderr, "[AssetManager] Opening FPK: %s\n", fpkPath.c_str());
    if (!m_art0.open(fpkPath.c_str())) {
        fprintf(stderr, "[AssetManager] WARNING: Could not open Art0.FPK\n");
        return false;
    }
    return true;
}

// ---- SDL_Renderer path (legacy 2D) ----

bool AssetManager::init(SDL_Renderer* renderer, const char* btsInstallDir) {
    m_renderer = renderer;
    if (!openArt0(btsInstallDir)) return false;

    for (auto& entry : terrainIcons) {
        SDL_Texture* tex = loadTextureFromFPK(m_art0, entry.fpkPath);
        if (tex) {
            m_terrainTextures[entry.terrainType] = tex;
            fprintf(stderr, "[AssetManager] Loaded terrain %d: %s\n", entry.terrainType, entry.fpkPath);
        }
    }

    SDL_Texture* peakTex = loadTextureFromFPK(m_art0, "art/interface/buttons/baseterrain/peak.dds");
    if (peakTex) m_terrainTextures[-1] = peakTex;
    SDL_Texture* hillTex = loadTextureFromFPK(m_art0, "art/interface/buttons/baseterrain/hill.dds");
    if (hillTex) m_terrainTextures[-2] = hillTex;

    for (auto& entry : featureIcons) {
        SDL_Texture* tex = loadTextureFromFPK(m_art0, entry.fpkPath);
        if (tex) m_featureTextures[entry.featureType] = tex;
    }

    fprintf(stderr, "[AssetManager] Loaded %d terrain textures, %d feature textures\n",
            (int)m_terrainTextures.size(), (int)m_featureTextures.size());
    return true;
}

SDL_Texture* AssetManager::getTerrainTexture(int terrainType) const {
    auto it = m_terrainTextures.find(terrainType);
    return (it != m_terrainTextures.end()) ? it->second : nullptr;
}

SDL_Texture* AssetManager::getFeatureTexture(int featureType) const {
    auto it = m_featureTextures.find(featureType);
    return (it != m_featureTextures.end()) ? it->second : nullptr;
}

SDL_Texture* AssetManager::createTextureFromRGBA(const uint8_t* pixels, int w, int h) {
    SDL_Texture* tex = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) return nullptr;
    SDL_UpdateTexture(tex, nullptr, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

SDL_Texture* AssetManager::loadTextureFromFPK(const FPKArchive& fpk, const std::string& path) {
    auto ddsData = fpk.readFile(path);
    if (ddsData.empty()) return nullptr;
    DDSImage img;
    if (!loadDDS(ddsData.data(), ddsData.size(), img)) return nullptr;
    return createTextureFromRGBA(img.pixels.data(), img.width, img.height);
}

// ---- OpenGL path ----

bool AssetManager::initGL(const char* btsInstallDir) {
    if (!openArt0(btsInstallDir)) return false;

    for (auto& entry : terrainIcons) {
        GLuint tex = loadGLTextureFromFPK(entry.fpkPath);
        if (tex) {
            m_terrainTexturesGL[entry.terrainType] = tex;
            fprintf(stderr, "[AssetManager] Loaded GL terrain %d: %s\n", entry.terrainType, entry.fpkPath);
        }
    }

    GLuint peakTex = loadGLTextureFromFPK("art/interface/buttons/baseterrain/peak.dds");
    if (peakTex) m_terrainTexturesGL[-1] = peakTex;
    GLuint hillTex = loadGLTextureFromFPK("art/interface/buttons/baseterrain/hill.dds");
    if (hillTex) m_terrainTexturesGL[-2] = hillTex;

    for (auto& entry : featureIcons) {
        GLuint tex = loadGLTextureFromFPK(entry.fpkPath);
        if (tex) m_featureTexturesGL[entry.featureType] = tex;
    }

    fprintf(stderr, "[AssetManager] Loaded %d GL terrain textures, %d GL feature textures\n",
            (int)m_terrainTexturesGL.size(), (int)m_featureTexturesGL.size());

    // Load 512x512 blend textures for map rendering
    for (auto& entry : terrainBlends) {
        GLuint tex = loadGLTextureFromFPK(entry.fpkPath);
        if (tex) {
            m_terrainBlendGL[entry.terrainType] = tex;
            fprintf(stderr, "[AssetManager] Loaded blend terrain %d: %s\n", entry.terrainType, entry.fpkPath);
        }
    }
    for (auto& entry : featureBlends) {
        GLuint tex = loadGLTextureFromFPK(entry.fpkPath);
        if (tex) {
            m_featureBlendGL[entry.featureType] = tex;
            fprintf(stderr, "[AssetManager] Loaded blend feature %d: %s\n", entry.featureType, entry.fpkPath);
        }
    }
    fprintf(stderr, "[AssetManager] Loaded %d blend terrain textures, %d blend feature textures\n",
            (int)m_terrainBlendGL.size(), (int)m_featureBlendGL.size());

    generateWaterTextures();
    loadExtraTextures();

    return true;
}

void AssetManager::generateWaterTextures() {
    // Create procedural water textures for ocean and coast tile fill.
    // The *BLEND.dds textures are designed for edge transitions, not tile fill.
    const int W = 256, H = 256;
    std::vector<uint8_t> pixels(W * H * 4);

    // Ocean: deep dark blue with subtle wave pattern
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float fx = x / (float)W, fy = y / (float)H;
            // Overlapping sine waves for organic pattern
            float w1 = sinf(fx * 14.0f + fy * 10.0f) * 0.5f + 0.5f;
            float w2 = sinf(fx * 9.0f - fy * 13.0f + 1.7f) * 0.5f + 0.5f;
            float w3 = sinf(fx * 22.0f + fy * 5.0f + 3.1f) * 0.5f + 0.5f;
            float wave = (w1 * 0.4f + w2 * 0.35f + w3 * 0.25f);
            int idx = (y * W + x) * 4;
            pixels[idx+0] = (uint8_t)(12 + wave * 25);   // R
            pixels[idx+1] = (uint8_t)(35 + wave * 45);   // G
            pixels[idx+2] = (uint8_t)(85 + wave * 70);   // B
            pixels[idx+3] = 255;
        }
    }
    m_oceanWaterGL = createGLTextureFromRGBA(pixels.data(), W, H);

    // Coast: lighter teal/turquoise with more visible wave pattern
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float fx = x / (float)W, fy = y / (float)H;
            float w1 = sinf(fx * 16.0f + fy * 8.0f + 0.5f) * 0.5f + 0.5f;
            float w2 = sinf(fx * 7.0f - fy * 15.0f + 2.3f) * 0.5f + 0.5f;
            float w3 = sinf(fx * 20.0f + fy * 6.0f + 4.0f) * 0.5f + 0.5f;
            float wave = (w1 * 0.4f + w2 * 0.35f + w3 * 0.25f);
            int idx = (y * W + x) * 4;
            pixels[idx+0] = (uint8_t)(25 + wave * 35);   // R
            pixels[idx+1] = (uint8_t)(75 + wave * 60);   // G
            pixels[idx+2] = (uint8_t)(120 + wave * 65);  // B
            pixels[idx+3] = 255;
        }
    }
    m_coastWaterGL = createGLTextureFromRGBA(pixels.data(), W, H);

    fprintf(stderr, "[AssetManager] Generated procedural water textures (ocean=%u, coast=%u)\n",
            m_oceanWaterGL, m_coastWaterGL);
}

void AssetManager::loadExtraTextures() {
    // Water surface texture (for rivers and improved water rendering)
    m_waterSurfaceGL = loadGLTextureFromFPK("art/terrain/water/water.dds");
    if (m_waterSurfaceGL)
        fprintf(stderr, "[AssetManager] Loaded water surface texture\n");

    // River atlas (512x1024 DXT3 with alpha for river shapes)
    m_riverAtlasGL = loadGLTextureFromFPK("art/terrain/routes/rivers/allriverssmaller.dds");
    if (m_riverAtlasGL)
        fprintf(stderr, "[AssetManager] Loaded river atlas texture\n");

    // Detail textures per terrain type (for zoomed-in fine detail)
    struct DetailEntry { int type; const char* path; };
    static DetailEntry detailPaths[] = {
        { 0, "art/terrain/textures/grassdetail.dds" },
        { 1, "art/terrain/textures/plainsdetail.dds" },
        { 2, "art/terrain/textures/desertdetail.dds" },
        { 3, "art/terrain/textures/tundradetail.dds" },
        { 4, "art/terrain/textures/icedetail.dds" },
        { 5, "art/terrain/textures/coastdetail.dds" },
        { 6, "art/terrain/textures/oceandetail.dds" },
    };
    for (auto& entry : detailPaths) {
        GLuint tex = loadGLTextureFromFPK(entry.path);
        if (tex) {
            m_terrainDetailGL[entry.type] = tex;
            fprintf(stderr, "[AssetManager] Loaded detail %d: %s\n", entry.type, entry.path);
        }
    }

    // Hill decal texture
    m_hillDecalGL = loadGLTextureFromFPK("art/terrain/textures/hillblend.dds");
    // Fallback: try alternative paths
    if (!m_hillDecalGL) m_hillDecalGL = loadGLTextureFromFPK("art/terrain/features/hills/hill_all_01.dds");
    if (m_hillDecalGL)
        fprintf(stderr, "[AssetManager] Loaded hill decal texture\n");

    fprintf(stderr, "[AssetManager] Loaded %d detail textures, water=%u, river=%u, hill=%u\n",
            (int)m_terrainDetailGL.size(), m_waterSurfaceGL, m_riverAtlasGL, m_hillDecalGL);
}

GLuint AssetManager::getTerrainDetailGL(int terrainType) const {
    auto it = m_terrainDetailGL.find(terrainType);
    return (it != m_terrainDetailGL.end()) ? it->second : 0;
}

GLuint AssetManager::getTerrainTextureGL(int terrainType) const {
    auto it = m_terrainTexturesGL.find(terrainType);
    return (it != m_terrainTexturesGL.end()) ? it->second : 0;
}

GLuint AssetManager::getFeatureTextureGL(int featureType) const {
    auto it = m_featureTexturesGL.find(featureType);
    return (it != m_featureTexturesGL.end()) ? it->second : 0;
}

GLuint AssetManager::getTerrainBlendGL(int terrainType) const {
    auto it = m_terrainBlendGL.find(terrainType);
    return (it != m_terrainBlendGL.end()) ? it->second : 0;
}

GLuint AssetManager::getFeatureBlendGL(int featureType) const {
    auto it = m_featureBlendGL.find(featureType);
    return (it != m_featureBlendGL.end()) ? it->second : 0;
}

GLuint AssetManager::createGLTextureFromRGBA(const uint8_t* pixels, int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

GLuint AssetManager::loadGLTextureFromFPK(const std::string& path) {
    DDSImage img;
    if (!loadDDSFromFPK(path, img)) return 0;
    return createGLTextureFromRGBA(img.pixels.data(), img.width, img.height);
}

bool AssetManager::loadDDSFromFPK(const std::string& path, DDSImage& out) {
    auto ddsData = m_art0.readFile(path);
    if (ddsData.empty()) {
        fprintf(stderr, "[AssetManager] File not found in FPK: %s\n", path.c_str());
        return false;
    }
    if (!loadDDS(ddsData.data(), ddsData.size(), out)) {
        fprintf(stderr, "[AssetManager] DDS decode failed: %s\n", path.c_str());
        return false;
    }
    return true;
}

// ---- 3D Model loading (NIF → GPU Mesh) ----

#include "NifLoader.h"
#include <algorithm>

// Helper: lowercase a string
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)tolower(c); });
    return s;
}

// Helper: extract directory from a path (e.g., "art/foo/bar.nif" → "art/foo/")
static std::string pathDir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos + 1) : "";
}

// Helper: normalize path separators to forward slash and lowercase
static std::string normalizePath(const std::string& path) {
    std::string out = path;
    for (char& c : out) { if (c == '\\') c = '/'; }
    return toLower(out);
}

GLuint AssetManager::resolveNifTexture(const std::string& nifDir, const std::string& texName) {
    if (texName.empty()) return 0;

    // Build candidate paths: relative to NIF directory, then absolute
    std::string normalizedName = normalizePath(texName);
    std::string normalizedDir = normalizePath(nifDir);

    // Try: nifDir + texName, then texName alone, then with .dds extension
    std::string candidates[4];
    int numCandidates = 0;

    candidates[numCandidates++] = normalizedDir + normalizedName;
    candidates[numCandidates++] = normalizedName;

    // Try .dds if the original was .tga or had no extension
    size_t dotPos = normalizedName.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string withDds = normalizedName.substr(0, dotPos) + ".dds";
        candidates[numCandidates++] = normalizedDir + withDds;
        candidates[numCandidates++] = withDds;
    }

    for (int i = 0; i < numCandidates; i++) {
        // Check cache
        auto it = m_nifTextureCache.find(candidates[i]);
        if (it != m_nifTextureCache.end()) return it->second;

        // Try to load from FPK
        GLuint tex = loadGLTextureFromFPK(candidates[i]);
        if (tex) {
            m_nifTextureCache[candidates[i]] = tex;
            return tex;
        }
    }

    return 0;
}

Mesh* AssetManager::getModel(const std::string& nifPath) {
    std::string key = normalizePath(nifPath);

    // Check cache
    auto it = m_modelCache.find(key);
    if (it != m_modelCache.end()) return it->second.get();

    // Load NIF from FPK
    auto nifData = m_art0.readFile(key);
    if (nifData.empty()) {
        // Also try with "art/" prefix if not present
        if (key.find("art/") != 0) {
            nifData = m_art0.readFile("art/" + key);
        }
        if (nifData.empty()) {
            m_modelCache[key] = nullptr;
            return nullptr;
        }
    }

    auto nif = nif::loadNif(nifData.data(), nifData.size());
    if (!nif) {
        fprintf(stderr, "[AssetManager] NIF parse failed: %s\n", key.c_str());
        m_modelCache[key] = nullptr;
        return nullptr;
    }

    std::string nifDir = pathDir(key);
    auto mesh = std::make_unique<Mesh>();

    // Walk scene graph to find all renderable meshes
    for (auto& blockPtr : nif->blocks) {
        auto* triShape = dynamic_cast<nif::NiTriShapeBlock*>(blockPtr.get());
        auto* triStrips = dynamic_cast<nif::NiTriStripsBlock*>(blockPtr.get());

        nif::NiGeometryBlock* geom = triShape ? (nif::NiGeometryBlock*)triShape
                                              : (nif::NiGeometryBlock*)triStrips;
        if (!geom) continue;

        // Get geometry data
        nif::NiGeometryDataCommon* geoData = nullptr;
        nif::NiTriShapeDataBlock* triData = nullptr;
        nif::NiTriStripsDataBlock* stripsData = nullptr;

        if (triShape) {
            triData = nif->getBlock<nif::NiTriShapeDataBlock>(geom->dataRef);
            geoData = triData;
        } else {
            stripsData = nif->getBlock<nif::NiTriStripsDataBlock>(geom->dataRef);
            geoData = stripsData;
        }

        if (!geoData || geoData->numVertices == 0) continue;

        // Build vertex array
        std::vector<MeshVertex> vertices(geoData->numVertices);
        for (int i = 0; i < geoData->numVertices; i++) {
            MeshVertex& v = vertices[i];
            if (geoData->hasVertices && i < (int)geoData->vertices.size()) {
                v.px = geoData->vertices[i].x;
                v.py = geoData->vertices[i].y;
                v.pz = geoData->vertices[i].z;
            }
            if (geoData->hasNormals && i < (int)geoData->normals.size()) {
                v.nx = geoData->normals[i].x;
                v.ny = geoData->normals[i].y;
                v.nz = geoData->normals[i].z;
            }
            if (geoData->uvSetCount() > 0 && !geoData->uvSets.empty() &&
                i < (int)geoData->uvSets[0].size()) {
                v.u = geoData->uvSets[0][i].x;
                v.v = geoData->uvSets[0][i].y;
            }
        }

        // Build index array
        std::vector<uint16_t> indices;
        if (triData && triData->hasTriangles) {
            indices.reserve(triData->triangles.size() * 3);
            for (auto& t : triData->triangles) {
                indices.push_back(t.v0);
                indices.push_back(t.v1);
                indices.push_back(t.v2);
            }
        } else if (stripsData) {
            auto tris = stripsData->toTriangleList();
            indices.reserve(tris.size() * 3);
            for (auto& t : tris) {
                indices.push_back(t.v0);
                indices.push_back(t.v1);
                indices.push_back(t.v2);
            }
        }

        if (indices.empty()) continue;

        // Find texture and material properties
        GLuint texID = 0;
        float diffR = 0.8f, diffG = 0.8f, diffB = 0.8f, alpha = 1.0f;
        bool hasAlpha = false;

        for (auto propRef : geom->propertyRefs) {
            auto* texProp = nif->getBlock<nif::NiTexturingPropertyBlock>(propRef);
            if (texProp && texProp->hasTexture[0]) {  // Base texture (slot 0)
                auto* srcTex = nif->getBlock<nif::NiSourceTextureBlock>(texProp->textures[0].sourceRef);
                if (srcTex && srcTex->useExternal && !srcTex->fileName.empty()) {
                    texID = resolveNifTexture(nifDir, srcTex->fileName);
                }
            }

            auto* matProp = nif->getBlock<nif::NiMaterialPropertyBlock>(propRef);
            if (matProp) {
                diffR = matProp->diffuse.r;
                diffG = matProp->diffuse.g;
                diffB = matProp->diffuse.b;
                alpha = matProp->alpha;
            }

            auto* alphaProp = nif->getBlock<nif::NiAlphaPropertyBlock>(propRef);
            if (alphaProp && alphaProp->blendEnabled()) {
                hasAlpha = true;
            }
        }

        mesh->addSubmesh(vertices, indices, texID, diffR, diffG, diffB, alpha, hasAlpha);
    }

    if (mesh->empty()) {
        fprintf(stderr, "[AssetManager] NIF has no renderable geometry: %s\n", key.c_str());
        m_modelCache[key] = nullptr;
        return nullptr;
    }

    Mesh* ptr = mesh.get();
    m_modelCache[key] = std::move(mesh);
    return ptr;
}
