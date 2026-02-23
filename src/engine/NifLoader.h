#pragma once
// NifLoader — Gamebryo NIF binary model parser for Civilization IV: Beyond the Sword
//
// Parses NIF version 20.0.0.4 (0x14000004), user_version=0.
// Extracts scene graph, mesh geometry, materials, and texture paths.
//
// Key format facts for this version:
//   - NO user_version_2 field (only if user_version >= 10)
//   - NO export info strings (only if user_version >= 10)
//   - NO block_size array (only version >= 20.2.0.7)
//   - NO string table (only version >= 20.1.0.3)
//   - Strings are inline: uint32 length + ASCII chars
//   - Bools are 1 byte (uint8) since version >= 4.1.0.1
//   - Flags are uint16 (2 bytes)
//   - Refs are int32 (4 bytes), -1 = none
//   - Must parse blocks sequentially (no block sizes)

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace nif {

// --- Math types ---

struct Vec2 {
    float x = 0, y = 0;
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
};

struct Matrix33 {
    float m[9] = {};  // row-major
};

struct Color3 {
    float r = 0, g = 0, b = 0;
};

struct Color4 {
    float r = 0, g = 0, b = 0, a = 1.0f;
};

struct Triangle {
    uint16_t v0 = 0, v1 = 0, v2 = 0;
};

// --- Block reference type ---
using Ref = int32_t;
static constexpr Ref NIF_NONE = -1;

// --- Block types ---

enum class BlockType {
    Unknown = 0,
    NiNode,
    NiTriShape,
    NiTriStrips,
    NiTriShapeData,
    NiTriStripsData,
    NiTexturingProperty,
    NiSourceTexture,
    NiMaterialProperty,
    NiAlphaProperty,
    NiZBufferProperty,
    NiVertexColorProperty,
    NiStencilProperty,
    NiSpecularProperty,
    NiStringExtraData,
    NiIntegerExtraData,
    NiPointLight,
    NiDirectionalLight,
    NiAmbientLight,
    NiTransformController,
    NiTransformInterpolator,
    NiTransformData,
    NiTextureTransformController,
    NiFloatInterpolator,
    NiFloatData,
    NiBillboardNode,
    NiCamera,
    NiMultiTargetTransformController,
    NiSkinInstance,
    NiSkinData,
    NiSkinPartition,
    NiVisController,
    NiBoolInterpolator,
    NiBoolData,
    NiAlphaController,
    NiMaterialColorController,
    NiFlipController,
    NiControllerSequence,
    NiTextKeyExtraData,
    NiControllerManager,
    NiSpotLight,
    NiObjectNETGeneric,
    NiBooleanExtraData,
    NiFloatExtraData,
    NiColorExtraData,
};

// --- Base block ---

struct NiBlock {
    BlockType type = BlockType::Unknown;
    int blockIndex = -1;
    virtual ~NiBlock() = default;
};

// --- NiObjectNET fields ---

struct NiObjectNETData {
    std::string name;
    std::vector<Ref> extraDataRefs;
    Ref controllerRef = NIF_NONE;
};

// --- NiAVObject fields ---

struct NiAVObjectData : NiObjectNETData {
    uint16_t flags = 0;
    Vec3 translation;
    Matrix33 rotation;
    float scale = 1.0f;
    std::vector<Ref> propertyRefs;
    Ref collisionRef = NIF_NONE;
};

// --- NiNode ---

struct NiNodeBlock : NiBlock, NiAVObjectData {
    std::vector<Ref> childRefs;
    std::vector<Ref> effectRefs;
    NiNodeBlock() { type = BlockType::NiNode; }
};

// --- NiBillboardNode (extends NiNode) ---

struct NiBillboardNodeBlock : NiNodeBlock {
    uint16_t billboardMode = 0;
    NiBillboardNodeBlock() { type = BlockType::NiBillboardNode; }
};

// --- NiGeometry (NiTriShape / NiTriStrips) ---

struct NiGeometryBlock : NiBlock, NiAVObjectData {
    Ref dataRef = NIF_NONE;
    Ref skinInstanceRef = NIF_NONE;
    bool hasShader = false;
    std::string shaderName;
    int32_t shaderUnknown = 0;
};

struct NiTriShapeBlock : NiGeometryBlock {
    NiTriShapeBlock() { type = BlockType::NiTriShape; }
};

struct NiTriStripsBlock : NiGeometryBlock {
    NiTriStripsBlock() { type = BlockType::NiTriStrips; }
};

// --- Geometry Data ---

struct NiGeometryDataCommon {
    int32_t groupId = 0;
    uint16_t numVertices = 0;
    uint8_t keepFlags = 0;
    uint8_t compressFlags = 0;
    bool hasVertices = false;
    std::vector<Vec3> vertices;
    uint8_t numUVSetsByte = 0;   // bits 0-5 = count
    uint8_t extraVectorsFlags = 0; // bit 4 = has tangents/bitangents
    bool hasNormals = false;
    std::vector<Vec3> normals;
    std::vector<Vec3> tangents;
    std::vector<Vec3> bitangents;
    Vec3 center;
    float radius = 0;
    bool hasVertexColors = false;
    std::vector<Color4> vertexColors;
    std::vector<std::vector<Vec2>> uvSets;
    uint16_t consistencyFlags = 0;
    Ref additionalDataRef = NIF_NONE;

    int uvSetCount() const { return numUVSetsByte & 0x3F; }
    bool hasTangentSpace() const { return (extraVectorsFlags & 0x10) != 0; }
};

struct NiTriShapeDataBlock : NiBlock, NiGeometryDataCommon {
    uint16_t numTriangles = 0;
    uint32_t numTrianglePoints = 0;
    bool hasTriangles = false;
    std::vector<Triangle> triangles;
    // Match groups (typically unused by renderers)
    uint16_t numMatchGroups = 0;

    NiTriShapeDataBlock() { type = BlockType::NiTriShapeData; }
};

struct NiTriStripsDataBlock : NiBlock, NiGeometryDataCommon {
    uint16_t numTriangles = 0;
    uint16_t numStrips = 0;
    std::vector<uint16_t> stripLengths;
    bool hasPoints = false;
    std::vector<std::vector<uint16_t>> strips;

    NiTriStripsDataBlock() { type = BlockType::NiTriStripsData; }

    // Convert triangle strips to triangle list for uniform rendering
    std::vector<Triangle> toTriangleList() const;
};

// --- TexDesc (texture descriptor compound) ---

struct TexDesc {
    Ref sourceRef = NIF_NONE;
    uint32_t clampMode = 3;     // WRAP_S_WRAP_T
    uint32_t filterMode = 2;    // TRILERP
    uint32_t uvSet = 0;
    bool hasTextureTransform = false;
    Vec2 translation;
    Vec2 tiling = {1.0f, 1.0f};
    float wRotation = 0;
    uint32_t transformType = 0;
    Vec2 centerOffset;
};

// --- NiTexturingProperty ---

struct NiTexturingPropertyBlock : NiBlock, NiObjectNETData {
    uint32_t applyMode = 2;   // MODULATE
    uint32_t textureCount = 0;

    // Standard slots: 0=Base, 1=Dark, 2=Detail, 3=Gloss, 4=Glow, 5=Bump, 6=Decal0, 7=Decal1
    bool hasTexture[8] = {};
    TexDesc textures[8];

    // Bump map extras (only if slot 5 is present)
    float bumpLumaScale = 0;
    float bumpLumaOffset = 0;
    float bumpMatrix[4] = {};

    // Shader textures
    struct ShaderTex {
        bool isUsed = false;
        TexDesc texData;
        uint32_t mapIndex = 0;
    };
    std::vector<ShaderTex> shaderTextures;

    NiTexturingPropertyBlock() { type = BlockType::NiTexturingProperty; }
};

// --- NiSourceTexture ---

struct NiSourceTextureBlock : NiBlock, NiObjectNETData {
    uint8_t useExternal = 1;
    std::string fileName;
    Ref unknownLink = NIF_NONE;  // for external
    Ref pixelDataRef = NIF_NONE; // for internal
    uint32_t pixelLayout = 6;    // DEFAULT
    uint32_t useMipmaps = 2;     // DEFAULT
    uint32_t alphaFormat = 3;    // DEFAULT
    uint8_t isStatic = 1;
    bool directRender = true;

    NiSourceTextureBlock() { type = BlockType::NiSourceTexture; }
};

// --- NiMaterialProperty ---

struct NiMaterialPropertyBlock : NiBlock, NiObjectNETData {
    // NO Flags for version 20.0.0.4
    Color3 ambient;
    Color3 diffuse;
    Color3 specular;
    Color3 emissive;
    float glossiness = 0;
    float alpha = 1.0f;

    NiMaterialPropertyBlock() { type = BlockType::NiMaterialProperty; }
};

// --- NiAlphaProperty ---

struct NiAlphaPropertyBlock : NiBlock, NiObjectNETData {
    uint16_t flags = 0;
    uint8_t threshold = 0;

    bool blendEnabled() const { return (flags & 1) != 0; }
    int srcBlend() const { return (flags >> 1) & 0xF; }
    int dstBlend() const { return (flags >> 5) & 0xF; }
    bool testEnabled() const { return (flags & (1 << 9)) != 0; }
    int testFunc() const { return (flags >> 10) & 0x7; }

    NiAlphaPropertyBlock() { type = BlockType::NiAlphaProperty; }
};

// --- NiZBufferProperty ---

struct NiZBufferPropertyBlock : NiBlock, NiObjectNETData {
    uint16_t flags = 0;
    uint32_t function = 0;

    bool zTestEnabled() const { return (flags & 1) != 0; }
    bool zWriteEnabled() const { return (flags & 2) != 0; }

    NiZBufferPropertyBlock() { type = BlockType::NiZBufferProperty; }
};

// --- NiVertexColorProperty ---

struct NiVertexColorPropertyBlock : NiBlock, NiObjectNETData {
    uint16_t flags = 0;
    uint32_t vertexMode = 0;
    uint32_t lightingMode = 0;

    NiVertexColorPropertyBlock() { type = BlockType::NiVertexColorProperty; }
};

// --- NiStencilProperty ---

struct NiStencilPropertyBlock : NiBlock, NiObjectNETData {
    // NO Flags for version 20.0.0.4
    uint8_t stencilEnabled = 0;
    uint32_t stencilFunction = 0;
    uint32_t stencilRef = 0;
    uint32_t stencilMask = 0;
    uint32_t failAction = 0;
    uint32_t zFailAction = 0;
    uint32_t passAction = 0;
    uint32_t drawMode = 0;   // 0=CCW_OR_BOTH, 1=CCW, 2=CW, 3=BOTH

    NiStencilPropertyBlock() { type = BlockType::NiStencilProperty; }
};

// --- NiSpecularProperty ---

struct NiSpecularPropertyBlock : NiBlock, NiObjectNETData {
    uint16_t flags = 0;
    NiSpecularPropertyBlock() { type = BlockType::NiSpecularProperty; }
};

// --- Extra data blocks ---

struct NiStringExtraDataBlock : NiBlock {
    std::string name;
    std::string value;
    NiStringExtraDataBlock() { type = BlockType::NiStringExtraData; }
};

struct NiIntegerExtraDataBlock : NiBlock {
    std::string name;
    uint32_t value = 0;
    NiIntegerExtraDataBlock() { type = BlockType::NiIntegerExtraData; }
};

// --- Light blocks ---

struct NiDynamicEffectData : NiAVObjectData {
    bool switchState = false;
    std::vector<Ref> affectedNodeRefs;
};

struct NiLightData : NiDynamicEffectData {
    float dimmer = 0;
    Color3 ambientColor;
    Color3 diffuseColor;
    Color3 specularColor;
};

struct NiPointLightBlock : NiBlock, NiLightData {
    float constantAttenuation = 0;
    float linearAttenuation = 0;
    float quadraticAttenuation = 0;
    NiPointLightBlock() { type = BlockType::NiPointLight; }
};

struct NiDirectionalLightBlock : NiBlock, NiLightData {
    NiDirectionalLightBlock() { type = BlockType::NiDirectionalLight; }
};

struct NiAmbientLightBlock : NiBlock, NiLightData {
    NiAmbientLightBlock() { type = BlockType::NiAmbientLight; }
};

// --- Controller blocks ---

struct NiTimeControllerData {
    Ref nextControllerRef = NIF_NONE;
    uint16_t flags = 0;
    float frequency = 1.0f;
    float phase = 0;
    float startTime = 0;
    float stopTime = 0;
    Ref targetRef = NIF_NONE;
};

struct NiSingleInterpControllerData : NiTimeControllerData {
    Ref interpolatorRef = NIF_NONE;
};

struct NiTransformControllerBlock : NiBlock, NiSingleInterpControllerData {
    NiTransformControllerBlock() { type = BlockType::NiTransformController; }
};

struct NiTextureTransformControllerBlock : NiBlock, NiSingleInterpControllerData {
    uint8_t shaderMap = 0;       // (version >= 10.0.1.0)
    uint32_t textureSlot = 0;    // TexType enum
    uint32_t operation = 0;      // TexTransform enum
    NiTextureTransformControllerBlock() { type = BlockType::NiTextureTransformController; }
};

// --- Interpolator blocks ---

struct NiTransformInterpolatorBlock : NiBlock {
    Vec3 translation;
    Vec4 rotation;  // quaternion (w, x, y, z)
    float scale = 1.0f;
    Ref dataRef = NIF_NONE;
    NiTransformInterpolatorBlock() { type = BlockType::NiTransformInterpolator; }
};

struct NiFloatInterpolatorBlock : NiBlock {
    float value = 0;
    Ref dataRef = NIF_NONE;
    NiFloatInterpolatorBlock() { type = BlockType::NiFloatInterpolator; }
};

// --- Keyframe data blocks ---

struct NiTransformDataBlock : NiBlock {
    // We skip reading individual keys since the C++ renderer
    // does not need animation data for static rendering.
    // The parser still consumes the correct number of bytes.
    uint32_t numRotKeys = 0;
    uint32_t numTransKeys = 0;
    uint32_t numScaleKeys = 0;
    NiTransformDataBlock() { type = BlockType::NiTransformData; }
};

struct NiFloatDataBlock : NiBlock {
    uint32_t numKeys = 0;
    NiFloatDataBlock() { type = BlockType::NiFloatData; }
};

// --- NiCamera ---

struct NiCameraBlock : NiBlock, NiAVObjectData {
    uint16_t unknownShort = 0;
    float frustumLeft = 0, frustumRight = 0, frustumTop = 0, frustumBottom = 0;
    float frustumNear = 0, frustumFar = 0;
    bool useOrthographic = false;
    float viewportLeft = 0, viewportRight = 0, viewportTop = 0, viewportBottom = 0;
    float lodAdjust = 0;
    Ref unknownLink = NIF_NONE;
    uint32_t unknownInt = 0;
    uint32_t unknownInt2 = 0;

    NiCameraBlock() { type = BlockType::NiCamera; }
};

// --- NiMultiTargetTransformController ---

struct NiMultiTargetTransformControllerBlock : NiBlock, NiTimeControllerData {
    uint16_t numExtraTargets = 0;
    std::vector<Ref> extraTargets;
    NiMultiTargetTransformControllerBlock() { type = BlockType::NiMultiTargetTransformController; }
};

// --- NiSkinInstance ---

struct NiSkinInstanceBlock : NiBlock {
    Ref dataRef = NIF_NONE;        // -> NiSkinData
    Ref skinPartitionRef = NIF_NONE; // -> NiSkinPartition (version >= 10.2.0.0)
    Ref skeletonRoot = NIF_NONE;   // -> NiNode
    uint32_t numBones = 0;
    std::vector<Ref> boneRefs;     // -> NiNode[]
    NiSkinInstanceBlock() { type = BlockType::NiSkinInstance; }
};

// --- NiSkinData ---

struct SkinWeight {
    uint16_t index = 0;
    float weight = 0;
};

struct SkinBoneData {
    Matrix33 rotation;
    Vec3 translation;
    float scale = 1.0f;
    Vec3 boundingSphereOffset;
    float boundingSphereRadius = 0;
    std::vector<SkinWeight> vertexWeights;
};

struct NiSkinDataBlock : NiBlock {
    Matrix33 skinRotation;
    Vec3 skinTranslation;
    float skinScale = 1.0f;
    uint32_t numBones = 0;
    uint8_t hasVertexWeights = 1;
    std::vector<SkinBoneData> bones;
    NiSkinDataBlock() { type = BlockType::NiSkinData; }
};

// --- NiSkinPartition (parsed but contents skipped for now) ---

struct NiSkinPartitionBlock : NiBlock {
    uint32_t numPartitions = 0;
    // Partition data is complex; we skip it for static rendering
    NiSkinPartitionBlock() { type = BlockType::NiSkinPartition; }
};

// --- NiVisController ---

struct NiVisControllerBlock : NiBlock, NiSingleInterpControllerData {
    NiVisControllerBlock() { type = BlockType::NiVisController; }
};

// --- NiBoolInterpolator ---

struct NiBoolInterpolatorBlock : NiBlock {
    bool value = false;
    Ref dataRef = NIF_NONE;
    NiBoolInterpolatorBlock() { type = BlockType::NiBoolInterpolator; }
};

// --- NiBoolData ---

struct NiBoolDataBlock : NiBlock {
    uint32_t numKeys = 0;
    NiBoolDataBlock() { type = BlockType::NiBoolData; }
};

// --- NiSpotLight ---

struct NiSpotLightBlock : NiBlock, NiLightData {
    float constantAttenuation = 0;
    float linearAttenuation = 0;
    float quadraticAttenuation = 0;
    float cutoffAngle = 0;
    float exponent = 0;
    NiSpotLightBlock() { type = BlockType::NiSpotLight; }
};

// --- NiBooleanExtraData ---

struct NiBooleanExtraDataBlock : NiBlock {
    std::string name;
    bool value = false;
    NiBooleanExtraDataBlock() { type = BlockType::NiBooleanExtraData; }
};

// --- NiFloatExtraData ---

struct NiFloatExtraDataBlock : NiBlock {
    std::string name;
    float value = 0;
    NiFloatExtraDataBlock() { type = BlockType::NiFloatExtraData; }
};

// --- NiColorExtraData ---

struct NiColorExtraDataBlock : NiBlock {
    std::string name;
    Color4 value;
    NiColorExtraDataBlock() { type = BlockType::NiColorExtraData; }
};

// --- NiAlphaController (= NiSingleInterpController, no extra fields for v20.0.0.4) ---

struct NiAlphaControllerBlock : NiBlock, NiSingleInterpControllerData {
    NiAlphaControllerBlock() { type = BlockType::NiAlphaController; }
};

// --- NiMaterialColorController (= NiSingleInterpController + targetColor uint16 for ver >= 10.1.0.0) ---

struct NiMaterialColorControllerBlock : NiBlock, NiSingleInterpControllerData {
    uint16_t targetColor = 0; // MaterialColor enum
    NiMaterialColorControllerBlock() { type = BlockType::NiMaterialColorController; }
};

// --- NiFlipController ---

struct NiFlipControllerBlock : NiBlock, NiSingleInterpControllerData {
    uint32_t textureSlot = 0;  // TexType (version >= 10.2.0.0)
    uint32_t numSources = 0;
    std::vector<Ref> sourceRefs;
    NiFlipControllerBlock() { type = BlockType::NiFlipController; }
};

// --- NiTextKeyExtraData ---

struct TextKey {
    float time = 0;
    std::string value;
};

struct NiTextKeyExtraDataBlock : NiBlock {
    std::string name;
    uint32_t numTextKeys = 0;
    std::vector<TextKey> textKeys;
    NiTextKeyExtraDataBlock() { type = BlockType::NiTextKeyExtraData; }
};

// --- NiControllerSequence ---

struct ControllerLink {
    std::string targetName;
    Ref controllerRef = NIF_NONE;
    Ref interpolatorRef = NIF_NONE;
    Ref controller2Ref = NIF_NONE;
    uint16_t priority = 0;
    std::string nodeName;
    std::string propertyType;
    std::string controllerType;
    std::string variable1;
    std::string variable2;
};

struct NiControllerSequenceBlock : NiBlock {
    std::string name;
    uint32_t numControlledBlocks = 0;
    uint32_t arrayGrowBy = 0;
    std::vector<ControllerLink> controlledBlocks;
    float weight = 1.0f;
    Ref textKeysRef = NIF_NONE;
    uint32_t cycleType = 0;
    float frequency = 1.0f;
    float startTime = 0;
    float stopTime = 0;
    Ref managerRef = NIF_NONE;
    std::string targetName;
    NiControllerSequenceBlock() { type = BlockType::NiControllerSequence; }
};

// --- NiControllerManager ---

struct NiControllerManagerBlock : NiBlock, NiTimeControllerData {
    bool cumulative = false;
    uint32_t numControllerSequences = 0;
    std::vector<Ref> controllerSequenceRefs;
    Ref objectPaletteRef = NIF_NONE;
    NiControllerManagerBlock() { type = BlockType::NiControllerManager; }
};

// --- NIF File ---

struct NifHeader {
    std::string headerString;
    uint32_t version = 0;
    uint8_t endian = 1;
    uint32_t userVersion = 0;
    uint32_t numBlocks = 0;
    uint16_t numBlockTypes = 0;
    std::vector<std::string> blockTypeNames;
    std::vector<uint16_t> blockTypeIndices;
    uint32_t unknownInt2 = 0;
};

struct NifFile {
    NifHeader header;
    std::vector<std::unique_ptr<NiBlock>> blocks;
    std::vector<Ref> rootRefs;  // from footer

    // Convenience accessors (return nullptr if index invalid or wrong type)
    template<typename T>
    T* getBlock(Ref ref) const {
        if (ref < 0 || ref >= (Ref)blocks.size()) return nullptr;
        return dynamic_cast<T*>(blocks[ref].get());
    }

    NiBlock* getBlock(Ref ref) const {
        if (ref < 0 || ref >= (Ref)blocks.size()) return nullptr;
        return blocks[ref].get();
    }

    const std::string& blockTypeName(int blockIndex) const {
        uint16_t typeIdx = header.blockTypeIndices[blockIndex];
        return header.blockTypeNames[typeIdx];
    }

    // Find the first block of a given type
    template<typename T>
    T* findFirst() const {
        for (auto& b : blocks) {
            T* p = dynamic_cast<T*>(b.get());
            if (p) return p;
        }
        return nullptr;
    }
};

// --- Parser ---

// Load a NIF file from a memory buffer. Returns null on failure.
std::unique_ptr<NifFile> loadNif(const uint8_t* data, size_t size);

// Load a NIF file from disk. Returns null on failure.
std::unique_ptr<NifFile> loadNifFromFile(const char* path);

// Load a NIF file from FPK archive data. Just calls loadNif().
inline std::unique_ptr<NifFile> loadNifFromMemory(const std::vector<uint8_t>& data) {
    return loadNif(data.data(), data.size());
}

} // namespace nif
