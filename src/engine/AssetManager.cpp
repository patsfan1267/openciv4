// AssetManager.cpp — Loads terrain textures from Civ4's FPK archives
//
// Terrain type indices (from BTS XML, matches CvEnums.h):
//   TERRAIN_GRASS=0, PLAINS=1, DESERT=2, TUNDRA=3, SNOW=4, COAST=5, OCEAN=6
// Plot types that override terrain visuals:
//   PLOT_PEAK=0 (uses peak texture), PLOT_HILLS=1 (uses hill texture)

#include "AssetManager.h"
#include <cstdio>

AssetManager::~AssetManager() {
    for (auto& pair : m_terrainTextures)
        if (pair.second) SDL_DestroyTexture(pair.second);
    for (auto& pair : m_featureTextures)
        if (pair.second) SDL_DestroyTexture(pair.second);
}

bool AssetManager::init(SDL_Renderer* renderer, const char* btsInstallDir) {
    m_renderer = renderer;

    // Base game assets are one level up from BTS install
    std::string baseDir = std::string(btsInstallDir) + "/../Assets";
    std::string fpkPath = baseDir + "/Art0.FPK";

    fprintf(stderr, "[AssetManager] Opening FPK: %s\n", fpkPath.c_str());
    if (!m_art0.open(fpkPath.c_str())) {
        fprintf(stderr, "[AssetManager] WARNING: Could not open Art0.FPK — using solid colors\n");
        return false;
    }

    // Load terrain button icons (64x64 DDS)
    // These map terrain type index → icon path in the FPK
    struct TerrainIconMap {
        int terrainType;
        const char* fpkPath;
    };

    TerrainIconMap terrainIcons[] = {
        { 0, "art/interface/buttons/baseterrain/grassland.dds" },  // TERRAIN_GRASS
        { 1, "art/interface/buttons/baseterrain/plains.dds" },     // TERRAIN_PLAINS
        { 2, "art/interface/buttons/baseterrain/desert.dds" },     // TERRAIN_DESERT
        { 3, "art/interface/buttons/baseterrain/tundra.dds" },     // TERRAIN_TUNDRA
        { 4, "art/interface/buttons/baseterrain/ice.dds" },        // TERRAIN_SNOW
        { 5, "art/interface/buttons/baseterrain/coast.dds" },      // TERRAIN_COAST
        { 6, "art/interface/buttons/baseterrain/ocean.dds" },      // TERRAIN_OCEAN
    };

    for (auto& entry : terrainIcons) {
        SDL_Texture* tex = loadTextureFromFPK(m_art0, entry.fpkPath);
        if (tex) {
            m_terrainTextures[entry.terrainType] = tex;
            fprintf(stderr, "[AssetManager] Loaded terrain %d: %s\n", entry.terrainType, entry.fpkPath);
        }
    }

    // Load peak and hill textures (special plot type overrides)
    // Use negative indices to distinguish from terrain types:
    //   -1 = PLOT_PEAK, -2 = PLOT_HILLS
    SDL_Texture* peakTex = loadTextureFromFPK(m_art0, "art/interface/buttons/baseterrain/peak.dds");
    if (peakTex) m_terrainTextures[-1] = peakTex;
    SDL_Texture* hillTex = loadTextureFromFPK(m_art0, "art/interface/buttons/baseterrain/hill.dds");
    if (hillTex) m_terrainTextures[-2] = hillTex;

    // Load feature icons
    struct FeatureIconMap {
        int featureType;
        const char* fpkPath;
    };
    FeatureIconMap featureIcons[] = {
        { 0, "art/interface/buttons/terrainfeatures/forest.dds" },        // FEATURE_FOREST
        { 1, "art/interface/buttons/terrainfeatures/jungle.dds" },        // FEATURE_JUNGLE
        { 2, "art/interface/buttons/terrainfeatures/oasis.dds" },         // FEATURE_OASIS
        { 3, "art/interface/buttons/terrainfeatures/floodplains.dds" },   // FEATURE_FLOOD_PLAINS
        { 4, "art/interface/buttons/terrainfeatures/fallout.dds" },       // FEATURE_FALLOUT
        { 5, "art/interface/buttons/terrainfeatures/forestevergreen.dds" },       // FEATURE_FOREST_EVERGREEN (BTS addition, type varies by mod)
        { 6, "art/interface/buttons/terrainfeatures/forestsnowyevergreen.dds" },  // snowy forest
    };

    for (auto& entry : featureIcons) {
        SDL_Texture* tex = loadTextureFromFPK(m_art0, entry.fpkPath);
        if (tex) {
            m_featureTextures[entry.featureType] = tex;
        }
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
    if (!tex) {
        fprintf(stderr, "[AssetManager] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return nullptr;
    }
    SDL_UpdateTexture(tex, nullptr, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

SDL_Texture* AssetManager::loadTextureFromFPK(const FPKArchive& fpk, const std::string& path) {
    auto ddsData = fpk.readFile(path);
    if (ddsData.empty()) {
        fprintf(stderr, "[AssetManager] File not found in FPK: %s\n", path.c_str());
        return nullptr;
    }

    DDSImage img;
    if (!loadDDS(ddsData.data(), ddsData.size(), img)) {
        fprintf(stderr, "[AssetManager] DDS decode failed: %s\n", path.c_str());
        return nullptr;
    }

    return createTextureFromRGBA(img.pixels.data(), img.width, img.height);
}
