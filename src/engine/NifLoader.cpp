// NifLoader.cpp — Gamebryo NIF binary model parser for Civilization IV
//
// Parses NIF version 20.0.0.4 (0x14000004), user_version=0.
// Format details derived from niftools nif.xml and verified against real files.
//
// All block types needed for static mesh rendering are implemented:
//   NiNode, NiTriShape, NiTriStrips, NiTriShapeData, NiTriStripsData,
//   NiTexturingProperty, NiSourceTexture, NiMaterialProperty,
//   NiAlphaProperty, NiZBufferProperty, NiVertexColorProperty,
//   NiStencilProperty, NiSpecularProperty, plus lights and controllers.

#include "NifLoader.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace nif {

// ============================================================================
// Binary Reader — reads from a memory buffer
// ============================================================================

class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size), m_pos(0) {}

    size_t tell() const { return m_pos; }
    size_t remaining() const { return m_size - m_pos; }
    bool eof() const { return m_pos >= m_size; }

    void seek(size_t pos) {
        if (pos > m_size) pos = m_size;
        m_pos = pos;
    }

    bool canRead(size_t n) const { return m_pos + n <= m_size; }

    const uint8_t* readBytes(size_t n) {
        if (m_pos + n > m_size) {
            fprintf(stderr, "[NifLoader] Read past end: wanted %zu at offset %zu (size %zu)\n",
                    n, m_pos, m_size);
            return nullptr;
        }
        const uint8_t* ptr = m_data + m_pos;
        m_pos += n;
        return ptr;
    }

    uint8_t readUint8() {
        auto p = readBytes(1);
        return p ? *p : 0;
    }

    int8_t readInt8() {
        return static_cast<int8_t>(readUint8());
    }

    uint16_t readUint16() {
        auto p = readBytes(2);
        if (!p) return 0;
        uint16_t v;
        memcpy(&v, p, 2);
        return v;
    }

    int16_t readInt16() {
        return static_cast<int16_t>(readUint16());
    }

    uint32_t readUint32() {
        auto p = readBytes(4);
        if (!p) return 0;
        uint32_t v;
        memcpy(&v, p, 4);
        return v;
    }

    int32_t readInt32() {
        return static_cast<int32_t>(readUint32());
    }

    float readFloat() {
        auto p = readBytes(4);
        if (!p) return 0;
        float v;
        memcpy(&v, p, 4);
        return v;
    }

    bool readBool() {
        // NIF bool: 1 byte for version >= 4.1.0.1
        return readUint8() != 0;
    }

    Ref readRef() {
        return readInt32();
    }

    uint16_t readFlags() {
        return readUint16();
    }

    std::string readSizedString() {
        uint32_t len = readUint32();
        if (len == 0) return "";
        if (len > 100000) {
            fprintf(stderr, "[NifLoader] String too long: %u at offset %zu\n", len, m_pos - 4);
            return "";
        }
        auto p = readBytes(len);
        if (!p) return "";
        return std::string(reinterpret_cast<const char*>(p), len);
    }

    Vec2 readVec2() {
        Vec2 v;
        v.x = readFloat();
        v.y = readFloat();
        return v;
    }

    Vec3 readVec3() {
        Vec3 v;
        v.x = readFloat();
        v.y = readFloat();
        v.z = readFloat();
        return v;
    }

    Vec4 readVec4() {
        Vec4 v;
        v.x = readFloat();
        v.y = readFloat();
        v.z = readFloat();
        v.w = readFloat();
        return v;
    }

    Color3 readColor3() {
        Color3 c;
        c.r = readFloat();
        c.g = readFloat();
        c.b = readFloat();
        return c;
    }

    Color4 readColor4() {
        Color4 c;
        c.r = readFloat();
        c.g = readFloat();
        c.b = readFloat();
        c.a = readFloat();
        return c;
    }

    Matrix33 readMatrix33() {
        Matrix33 m;
        for (int i = 0; i < 9; i++)
            m.m[i] = readFloat();
        return m;
    }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
};

// ============================================================================
// Header parser
// ============================================================================

static bool parseHeader(BinaryReader& br, NifHeader& hdr) {
    // Read header string (terminated by 0x0A)
    std::string headerStr;
    while (!br.eof()) {
        uint8_t b = br.readUint8();
        if (b == 0x0A) break;
        headerStr += static_cast<char>(b);
    }
    hdr.headerString = headerStr;

    hdr.version = br.readUint32();
    if (hdr.version != 0x14000004) {
        fprintf(stderr, "[NifLoader] Unsupported NIF version: 0x%08X (expected 0x14000004)\n", hdr.version);
        return false;
    }

    hdr.endian = br.readUint8();
    hdr.userVersion = br.readUint32();
    hdr.numBlocks = br.readUint32();

    // For version 20.0.0.4, user_version=0:
    // NO user_version_2, NO export info

    hdr.numBlockTypes = br.readUint16();
    hdr.blockTypeNames.resize(hdr.numBlockTypes);
    for (uint16_t i = 0; i < hdr.numBlockTypes; i++) {
        hdr.blockTypeNames[i] = br.readSizedString();
    }

    hdr.blockTypeIndices.resize(hdr.numBlocks);
    for (uint32_t i = 0; i < hdr.numBlocks; i++) {
        hdr.blockTypeIndices[i] = br.readUint16();
    }

    // Unknown Int 2 (present for version >= 10.0.1.0)
    hdr.unknownInt2 = br.readUint32();

    return true;
}

// ============================================================================
// Common field parsers
// ============================================================================

static void parseNiObjectNET(BinaryReader& br, NiObjectNETData& data) {
    data.name = br.readSizedString();
    uint32_t numExtra = br.readUint32();
    data.extraDataRefs.resize(numExtra);
    for (uint32_t i = 0; i < numExtra; i++)
        data.extraDataRefs[i] = br.readRef();
    data.controllerRef = br.readRef();
}

static void parseNiAVObject(BinaryReader& br, NiAVObjectData& data) {
    parseNiObjectNET(br, data);
    data.flags = br.readFlags();
    data.translation = br.readVec3();
    data.rotation = br.readMatrix33();
    data.scale = br.readFloat();
    uint32_t numProps = br.readUint32();
    data.propertyRefs.resize(numProps);
    for (uint32_t i = 0; i < numProps; i++)
        data.propertyRefs[i] = br.readRef();
    data.collisionRef = br.readRef();
}

static void parseNiGeometryDataCommon(BinaryReader& br, NiGeometryDataCommon& data) {
    data.groupId = br.readInt32();
    data.numVertices = br.readUint16();
    uint16_t nv = data.numVertices;

    data.keepFlags = br.readUint8();
    data.compressFlags = br.readUint8();

    data.hasVertices = br.readBool();
    if (data.hasVertices) {
        data.vertices.resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.vertices[i] = br.readVec3();
    }

    data.numUVSetsByte = br.readUint8();
    data.extraVectorsFlags = br.readUint8();

    data.hasNormals = br.readBool();
    if (data.hasNormals) {
        data.normals.resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.normals[i] = br.readVec3();
    }

    // Tangents/bitangents if has_normals AND bit 4 of extra_vectors_flags
    if (data.hasNormals && data.hasTangentSpace()) {
        data.tangents.resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.tangents[i] = br.readVec3();
        data.bitangents.resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.bitangents[i] = br.readVec3();
    }

    data.center = br.readVec3();
    data.radius = br.readFloat();

    data.hasVertexColors = br.readBool();
    if (data.hasVertexColors) {
        data.vertexColors.resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.vertexColors[i] = br.readColor4();
    }

    int uvCount = data.uvSetCount();
    data.uvSets.resize(uvCount);
    for (int s = 0; s < uvCount; s++) {
        data.uvSets[s].resize(nv);
        for (uint16_t i = 0; i < nv; i++)
            data.uvSets[s][i] = br.readVec2();
    }

    data.consistencyFlags = br.readUint16();
    data.additionalDataRef = br.readRef();
}

static TexDesc parseTexDesc(BinaryReader& br) {
    TexDesc td;
    td.sourceRef = br.readRef();
    td.clampMode = br.readUint32();   // TexClampMode: storage=uint (4 bytes)
    td.filterMode = br.readUint32();  // TexFilterMode: storage=uint (4 bytes)
    td.uvSet = br.readUint32();
    td.hasTextureTransform = br.readBool();
    if (td.hasTextureTransform) {
        td.translation = br.readVec2();
        td.tiling = br.readVec2();
        td.wRotation = br.readFloat();
        td.transformType = br.readUint32();
        td.centerOffset = br.readVec2();
    }
    return td;
}

static void parseNiDynamicEffect(BinaryReader& br, NiDynamicEffectData& data) {
    parseNiAVObject(br, data);
    data.switchState = br.readBool();
    uint32_t numAffected = br.readUint32();
    data.affectedNodeRefs.resize(numAffected);
    for (uint32_t i = 0; i < numAffected; i++)
        data.affectedNodeRefs[i] = br.readRef();
}

static void parseNiLight(BinaryReader& br, NiLightData& data) {
    parseNiDynamicEffect(br, data);
    data.dimmer = br.readFloat();
    data.ambientColor = br.readColor3();
    data.diffuseColor = br.readColor3();
    data.specularColor = br.readColor3();
}

static void parseNiTimeController(BinaryReader& br, NiTimeControllerData& data) {
    data.nextControllerRef = br.readRef();
    data.flags = br.readFlags();
    data.frequency = br.readFloat();
    data.phase = br.readFloat();
    data.startTime = br.readFloat();
    data.stopTime = br.readFloat();
    data.targetRef = br.readRef();
}

static void parseNiSingleInterpController(BinaryReader& br, NiSingleInterpControllerData& data) {
    parseNiTimeController(br, data);
    data.interpolatorRef = br.readRef();
}

// ============================================================================
// Block parsers
// ============================================================================

static std::unique_ptr<NiBlock> parseNiNode(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiNodeBlock>();
    block->blockIndex = idx;
    parseNiAVObject(br, *block);

    uint32_t numCh = br.readUint32();
    block->childRefs.resize(numCh);
    for (uint32_t i = 0; i < numCh; i++)
        block->childRefs[i] = br.readRef();

    uint32_t numEff = br.readUint32();
    block->effectRefs.resize(numEff);
    for (uint32_t i = 0; i < numEff; i++)
        block->effectRefs[i] = br.readRef();

    return block;
}

static std::unique_ptr<NiBlock> parseNiBillboardNode(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiBillboardNodeBlock>();
    block->blockIndex = idx;
    parseNiAVObject(br, *block);

    uint32_t numCh = br.readUint32();
    block->childRefs.resize(numCh);
    for (uint32_t i = 0; i < numCh; i++)
        block->childRefs[i] = br.readRef();

    uint32_t numEff = br.readUint32();
    block->effectRefs.resize(numEff);
    for (uint32_t i = 0; i < numEff; i++)
        block->effectRefs[i] = br.readRef();

    // Billboard mode (version >= 10.1.0.0)
    block->billboardMode = br.readUint16();

    return block;
}

static std::unique_ptr<NiBlock> parseNiTriShape(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTriShapeBlock>();
    block->blockIndex = idx;
    parseNiAVObject(br, *block);

    block->dataRef = br.readRef();
    block->skinInstanceRef = br.readRef();

    // NiGeometry shader fields (version 10.0.1.0 to 20.1.0.3)
    block->hasShader = br.readBool();
    if (block->hasShader) {
        block->shaderName = br.readSizedString();
        block->shaderUnknown = br.readInt32();
    }

    return block;
}

static std::unique_ptr<NiBlock> parseNiTriStrips(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTriStripsBlock>();
    block->blockIndex = idx;
    parseNiAVObject(br, *block);

    block->dataRef = br.readRef();
    block->skinInstanceRef = br.readRef();

    block->hasShader = br.readBool();
    if (block->hasShader) {
        block->shaderName = br.readSizedString();
        block->shaderUnknown = br.readInt32();
    }

    return block;
}

static std::unique_ptr<NiBlock> parseNiTriShapeData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTriShapeDataBlock>();
    block->blockIndex = idx;
    parseNiGeometryDataCommon(br, *block);

    // NiTriBasedGeomData
    block->numTriangles = br.readUint16();

    // NiTriShapeData-specific
    block->numTrianglePoints = br.readUint32();
    block->hasTriangles = br.readBool();
    if (block->hasTriangles) {
        block->triangles.resize(block->numTriangles);
        for (uint16_t i = 0; i < block->numTriangles; i++) {
            block->triangles[i].v0 = br.readUint16();
            block->triangles[i].v1 = br.readUint16();
            block->triangles[i].v2 = br.readUint16();
        }
    }

    block->numMatchGroups = br.readUint16();
    for (uint16_t i = 0; i < block->numMatchGroups; i++) {
        uint16_t count = br.readUint16();
        for (uint16_t j = 0; j < count; j++)
            br.readUint16(); // skip match group indices
    }

    return block;
}

static std::unique_ptr<NiBlock> parseNiTriStripsData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTriStripsDataBlock>();
    block->blockIndex = idx;
    parseNiGeometryDataCommon(br, *block);

    // NiTriBasedGeomData
    block->numTriangles = br.readUint16();

    // NiTriStripsData-specific
    block->numStrips = br.readUint16();
    block->stripLengths.resize(block->numStrips);
    for (uint16_t i = 0; i < block->numStrips; i++)
        block->stripLengths[i] = br.readUint16();

    block->hasPoints = br.readBool();
    if (block->hasPoints) {
        block->strips.resize(block->numStrips);
        for (uint16_t i = 0; i < block->numStrips; i++) {
            block->strips[i].resize(block->stripLengths[i]);
            for (uint16_t j = 0; j < block->stripLengths[i]; j++)
                block->strips[i][j] = br.readUint16();
        }
    }

    return block;
}

static std::unique_ptr<NiBlock> parseNiTexturingProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTexturingPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);

    // Apply mode (version <= 20.0.0.5)
    block->applyMode = br.readUint32();
    block->textureCount = br.readUint32();

    // Standard texture slots 0-4 (Base, Dark, Detail, Gloss, Glow)
    for (int slot = 0; slot < (int)std::min(block->textureCount, 5u); slot++) {
        block->hasTexture[slot] = br.readBool();
        if (block->hasTexture[slot]) {
            block->textures[slot] = parseTexDesc(br);
        }
    }

    // Slot 5: BumpMap (extra fields after TexDesc)
    if (block->textureCount > 5) {
        block->hasTexture[5] = br.readBool();
        if (block->hasTexture[5]) {
            block->textures[5] = parseTexDesc(br);
            block->bumpLumaScale = br.readFloat();
            block->bumpLumaOffset = br.readFloat();
            for (int i = 0; i < 4; i++)
                block->bumpMatrix[i] = br.readFloat();
        }
    }

    // Slot 6: Decal0 (if textureCount >= 7)
    if (block->textureCount >= 7) {
        block->hasTexture[6] = br.readBool();
        if (block->hasTexture[6]) {
            block->textures[6] = parseTexDesc(br);
        }
    }

    // Slot 7: Decal1 (if textureCount >= 8, version <= 20.1.0.3)
    if (block->textureCount >= 8) {
        block->hasTexture[7] = br.readBool();
        if (block->hasTexture[7]) {
            block->textures[7] = parseTexDesc(br);
        }
    }

    // Shader textures (version >= 10.0.1.0)
    uint32_t numShaderTex = br.readUint32();
    block->shaderTextures.resize(numShaderTex);
    for (uint32_t i = 0; i < numShaderTex; i++) {
        block->shaderTextures[i].isUsed = br.readBool();
        if (block->shaderTextures[i].isUsed) {
            block->shaderTextures[i].texData = parseTexDesc(br);
            block->shaderTextures[i].mapIndex = br.readUint32();
        }
    }

    return block;
}

static std::unique_ptr<NiBlock> parseNiSourceTexture(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSourceTextureBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);

    block->useExternal = br.readUint8();
    if (block->useExternal) {
        block->fileName = br.readSizedString();
        block->unknownLink = br.readRef();
    } else {
        block->fileName = br.readSizedString();
        block->pixelDataRef = br.readRef();
    }

    block->pixelLayout = br.readUint32();
    block->useMipmaps = br.readUint32();
    block->alphaFormat = br.readUint32();
    block->isStatic = br.readUint8();
    block->directRender = br.readBool();

    return block;
}

static std::unique_ptr<NiBlock> parseNiMaterialProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiMaterialPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);

    // NO Flags for version 20.0.0.4 (flags only for ver 3.0 to 10.0.1.2)
    block->ambient = br.readColor3();
    block->diffuse = br.readColor3();
    block->specular = br.readColor3();
    block->emissive = br.readColor3();
    block->glossiness = br.readFloat();
    block->alpha = br.readFloat();

    return block;
}

static std::unique_ptr<NiBlock> parseNiAlphaProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiAlphaPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);
    block->flags = br.readFlags();
    block->threshold = br.readUint8();
    return block;
}

static std::unique_ptr<NiBlock> parseNiZBufferProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiZBufferPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);
    block->flags = br.readFlags();
    block->function = br.readUint32();
    return block;
}

static std::unique_ptr<NiBlock> parseNiVertexColorProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiVertexColorPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);
    block->flags = br.readFlags();
    block->vertexMode = br.readUint32();
    block->lightingMode = br.readUint32();
    return block;
}

static std::unique_ptr<NiBlock> parseNiStencilProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiStencilPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);
    // NO Flags for version 20.0.0.4
    block->stencilEnabled = br.readUint8();
    block->stencilFunction = br.readUint32();
    block->stencilRef = br.readUint32();
    block->stencilMask = br.readUint32();
    block->failAction = br.readUint32();
    block->zFailAction = br.readUint32();
    block->passAction = br.readUint32();
    block->drawMode = br.readUint32();
    return block;
}

static std::unique_ptr<NiBlock> parseNiSpecularProperty(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSpecularPropertyBlock>();
    block->blockIndex = idx;
    parseNiObjectNET(br, *block);
    block->flags = br.readFlags();
    return block;
}

static std::unique_ptr<NiBlock> parseNiStringExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiStringExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->value = br.readSizedString();
    return block;
}

static std::unique_ptr<NiBlock> parseNiIntegerExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiIntegerExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->value = br.readUint32();
    return block;
}

static std::unique_ptr<NiBlock> parseNiPointLight(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiPointLightBlock>();
    block->blockIndex = idx;
    parseNiLight(br, *block);
    block->constantAttenuation = br.readFloat();
    block->linearAttenuation = br.readFloat();
    block->quadraticAttenuation = br.readFloat();
    return block;
}

static std::unique_ptr<NiBlock> parseNiDirectionalLight(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiDirectionalLightBlock>();
    block->blockIndex = idx;
    parseNiLight(br, *block);
    return block;
}

static std::unique_ptr<NiBlock> parseNiAmbientLight(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiAmbientLightBlock>();
    block->blockIndex = idx;
    parseNiLight(br, *block);
    return block;
}

static std::unique_ptr<NiBlock> parseNiTransformController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTransformControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    return block;
}

static std::unique_ptr<NiBlock> parseNiTextureTransformController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTextureTransformControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    // Extra fields for NiTextureTransformController (extends NiFloatInterpController)
    block->shaderMap = br.readUint8();       // version >= 10.0.1.0
    block->textureSlot = br.readUint32();    // TexType
    block->operation = br.readUint32();      // TexTransform
    return block;
}

static std::unique_ptr<NiBlock> parseNiTransformInterpolator(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTransformInterpolatorBlock>();
    block->blockIndex = idx;
    block->translation = br.readVec3();
    block->rotation = br.readVec4(); // quaternion (w, x, y, z)
    block->scale = br.readFloat();
    block->dataRef = br.readRef();
    return block;
}

static std::unique_ptr<NiBlock> parseNiFloatInterpolator(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiFloatInterpolatorBlock>();
    block->blockIndex = idx;
    block->value = br.readFloat();
    block->dataRef = br.readRef();
    return block;
}

// Skip keyframe keys (KeyGroup) for a float type
static void skipKeyGroupFloat(BinaryReader& br) {
    uint32_t numKeys = br.readUint32();
    if (numKeys == 0) return;
    uint32_t keyType = br.readUint32();
    for (uint32_t i = 0; i < numKeys; i++) {
        br.readFloat(); // time
        br.readFloat(); // value
        if (keyType == 2) {
            br.readFloat(); // forward
            br.readFloat(); // backward
        } else if (keyType == 3) {
            br.readFloat(); // t
            br.readFloat(); // b
            br.readFloat(); // c
        }
    }
}

// Skip keyframe keys (KeyGroup) for a Vec3 type
static void skipKeyGroupVec3(BinaryReader& br) {
    uint32_t numKeys = br.readUint32();
    if (numKeys == 0) return;
    uint32_t keyType = br.readUint32();
    for (uint32_t i = 0; i < numKeys; i++) {
        br.readFloat(); // time
        br.readFloat(); br.readFloat(); br.readFloat(); // value
        if (keyType == 2) {
            br.readFloat(); br.readFloat(); br.readFloat(); // forward
            br.readFloat(); br.readFloat(); br.readFloat(); // backward
        } else if (keyType == 3) {
            br.readFloat(); br.readFloat(); br.readFloat(); // TBC
        }
    }
}

static std::unique_ptr<NiBlock> parseNiTransformData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTransformDataBlock>();
    block->blockIndex = idx;

    // Rotation keys
    block->numRotKeys = br.readUint32();
    if (block->numRotKeys > 0) {
        uint32_t rotType = br.readUint32();
        if (rotType == 4) {
            // XYZ rotation: 3 separate per-axis key arrays
            for (int axis = 0; axis < 3; axis++) {
                uint32_t nAxisKeys = br.readUint32();
                if (nAxisKeys > 0) {
                    uint32_t axisType = br.readUint32();
                    for (uint32_t k = 0; k < nAxisKeys; k++) {
                        br.readFloat(); // time
                        br.readFloat(); // value
                        if (axisType == 2) {
                            br.readFloat(); // forward
                            br.readFloat(); // backward
                        } else if (axisType == 3) {
                            br.readFloat(); br.readFloat(); br.readFloat(); // TBC
                        }
                    }
                }
            }
        } else {
            for (uint32_t k = 0; k < block->numRotKeys; k++) {
                br.readFloat(); // time
                // quaternion: w, x, y, z
                br.readFloat(); br.readFloat(); br.readFloat(); br.readFloat();
                if (rotType == 3) {
                    br.readFloat(); br.readFloat(); br.readFloat(); // TBC
                }
            }
        }
    }

    // Translation keys
    skipKeyGroupVec3(br);
    block->numTransKeys = 0; // We read them but don't store

    // Scale keys
    skipKeyGroupFloat(br);
    block->numScaleKeys = 0;

    return block;
}

static std::unique_ptr<NiBlock> parseNiFloatData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiFloatDataBlock>();
    block->blockIndex = idx;

    // KeyGroup<float>
    block->numKeys = br.readUint32();
    if (block->numKeys > 0) {
        uint32_t keyType = br.readUint32();
        for (uint32_t i = 0; i < block->numKeys; i++) {
            br.readFloat(); // time
            br.readFloat(); // value
            if (keyType == 2) {
                br.readFloat(); // forward
                br.readFloat(); // backward
            } else if (keyType == 3) {
                br.readFloat(); br.readFloat(); br.readFloat(); // TBC
            }
        }
    }

    return block;
}

// --- NiCamera ---

static std::unique_ptr<NiBlock> parseNiCamera(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiCameraBlock>();
    block->blockIndex = idx;
    parseNiAVObject(br, *block);

    block->unknownShort = br.readUint16();  // version >= 10.1.0.0
    block->frustumLeft = br.readFloat();
    block->frustumRight = br.readFloat();
    block->frustumTop = br.readFloat();
    block->frustumBottom = br.readFloat();
    block->frustumNear = br.readFloat();
    block->frustumFar = br.readFloat();
    block->useOrthographic = br.readBool(); // version >= 10.1.0.0
    block->viewportLeft = br.readFloat();
    block->viewportRight = br.readFloat();
    block->viewportTop = br.readFloat();
    block->viewportBottom = br.readFloat();
    block->lodAdjust = br.readFloat();
    block->unknownLink = br.readRef();
    block->unknownInt = br.readUint32();
    block->unknownInt2 = br.readUint32();   // version >= 4.2.1.0

    return block;
}

// --- NiMultiTargetTransformController ---

static std::unique_ptr<NiBlock> parseNiMultiTargetTransformController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiMultiTargetTransformControllerBlock>();
    block->blockIndex = idx;

    // NiInterpController inherits NiTimeController (no extra fields)
    parseNiTimeController(br, *block);

    block->numExtraTargets = br.readUint16();
    block->extraTargets.resize(block->numExtraTargets);
    for (uint16_t i = 0; i < block->numExtraTargets; i++)
        block->extraTargets[i] = br.readRef();  // Ptr = int32 (same as Ref on disk)

    return block;
}

// --- NiSkinInstance ---

static std::unique_ptr<NiBlock> parseNiSkinInstance(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSkinInstanceBlock>();
    block->blockIndex = idx;

    block->dataRef = br.readRef();           // -> NiSkinData
    block->skinPartitionRef = br.readRef();  // -> NiSkinPartition (version >= 10.2.0.0)
    block->skeletonRoot = br.readRef();      // -> NiNode (Ptr = int32 on disk)
    block->numBones = br.readUint32();
    block->boneRefs.resize(block->numBones);
    for (uint32_t i = 0; i < block->numBones; i++)
        block->boneRefs[i] = br.readRef();   // Ptr = int32 on disk

    return block;
}

// --- NiSkinData ---

static void readSkinTransform(BinaryReader& br, Matrix33& rot, Vec3& trans, float& scale) {
    rot = br.readMatrix33();    // SkinTransform has rotation FIRST (unlike MTransform)
    trans = br.readVec3();
    scale = br.readFloat();
}

static std::unique_ptr<NiBlock> parseNiSkinData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSkinDataBlock>();
    block->blockIndex = idx;

    readSkinTransform(br, block->skinRotation, block->skinTranslation, block->skinScale);
    block->numBones = br.readUint32();
    // Skin Partition ref: only for version 4.0.0.2 to 10.1.0.0 — NOT present for 20.0.0.4

    // Has Vertex Weights (byte) — version >= 4.2.1.0
    block->hasVertexWeights = br.readUint8();

    block->bones.resize(block->numBones);
    for (uint32_t b = 0; b < block->numBones; b++) {
        SkinBoneData& bone = block->bones[b];
        readSkinTransform(br, bone.rotation, bone.translation, bone.scale);
        bone.boundingSphereOffset = br.readVec3();
        bone.boundingSphereRadius = br.readFloat();
        uint16_t numVerts = br.readUint16();
        // Vertex weights: condition is ARG != 0 (ARG = hasVertexWeights) for version >= 4.2.2.0
        if (block->hasVertexWeights) {
            bone.vertexWeights.resize(numVerts);
            for (uint16_t v = 0; v < numVerts; v++) {
                bone.vertexWeights[v].index = br.readUint16();
                bone.vertexWeights[v].weight = br.readFloat();
            }
        }
    }

    return block;
}

// --- NiSkinPartition (complex — parse header, skip partition data) ---

static std::unique_ptr<NiBlock> parseNiSkinPartition(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSkinPartitionBlock>();
    block->blockIndex = idx;

    block->numPartitions = br.readUint32();

    // Each SkinPartition block has complex variable-length data.
    // Parse each partition to consume the correct number of bytes.
    for (uint32_t p = 0; p < block->numPartitions; p++) {
        uint16_t numVertices = br.readUint16();
        uint16_t numTriangles = br.readUint16();
        uint16_t numBones = br.readUint16();
        uint16_t numStrips = br.readUint16();
        uint16_t numWeightsPerVertex = br.readUint16();

        // Bones array
        for (uint16_t i = 0; i < numBones; i++)
            br.readUint16();

        // Has Vertex Map (version >= 10.1.0.0)
        bool hasVertexMap = br.readBool();
        if (hasVertexMap) {
            for (uint16_t i = 0; i < numVertices; i++)
                br.readUint16();
        }

        // Has Vertex Weights (version >= 10.1.0.0)
        bool hasVertexWeights = br.readBool();
        if (hasVertexWeights) {
            for (uint16_t i = 0; i < numVertices; i++)
                for (uint16_t j = 0; j < numWeightsPerVertex; j++)
                    br.readFloat();
        }

        // Strip lengths
        std::vector<uint16_t> stripLengths(numStrips);
        for (uint16_t i = 0; i < numStrips; i++)
            stripLengths[i] = br.readUint16();

        // Has Faces (version >= 10.1.0.0)
        bool hasFaces = br.readBool();
        if (hasFaces) {
            if (numStrips > 0) {
                for (uint16_t s = 0; s < numStrips; s++)
                    for (uint16_t i = 0; i < stripLengths[s]; i++)
                        br.readUint16();
            } else {
                for (uint16_t i = 0; i < numTriangles; i++) {
                    br.readUint16(); br.readUint16(); br.readUint16();
                }
            }
        }

        // Has Bone Indices
        bool hasBoneIndices = br.readBool();
        if (hasBoneIndices) {
            for (uint16_t i = 0; i < numVertices; i++)
                for (uint16_t j = 0; j < numWeightsPerVertex; j++)
                    br.readUint8();
        }
    }

    return block;
}

// --- NiVisController (NiBoolInterpController -> NiSingleInterpController) ---

static std::unique_ptr<NiBlock> parseNiVisController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiVisControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    return block;
}

// --- NiBoolInterpolator ---

static std::unique_ptr<NiBlock> parseNiBoolInterpolator(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiBoolInterpolatorBlock>();
    block->blockIndex = idx;
    block->value = br.readBool();
    block->dataRef = br.readRef();
    return block;
}

// --- NiBoolData ---

static std::unique_ptr<NiBlock> parseNiBoolData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiBoolDataBlock>();
    block->blockIndex = idx;

    // KeyGroup<byte>
    block->numKeys = br.readUint32();
    if (block->numKeys > 0) {
        uint32_t keyType = br.readUint32();
        for (uint32_t i = 0; i < block->numKeys; i++) {
            br.readFloat(); // time
            br.readUint8(); // value (byte key)
            if (keyType == 2) {
                br.readUint8(); // forward
                br.readUint8(); // backward
            } else if (keyType == 3) {
                br.readUint8(); br.readUint8(); br.readUint8(); // TBC
            }
        }
    }

    return block;
}

// --- NiSpotLight ---

static std::unique_ptr<NiBlock> parseNiSpotLight(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiSpotLightBlock>();
    block->blockIndex = idx;
    parseNiLight(br, *block);
    block->constantAttenuation = br.readFloat();
    block->linearAttenuation = br.readFloat();
    block->quadraticAttenuation = br.readFloat();
    block->cutoffAngle = br.readFloat();
    // Unknown Float: only for version >= 20.2.0.7, NOT present for us
    block->exponent = br.readFloat();
    return block;
}

// --- NiBooleanExtraData ---

static std::unique_ptr<NiBlock> parseNiBooleanExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiBooleanExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->value = br.readBool();
    return block;
}

// --- NiFloatExtraData ---

static std::unique_ptr<NiBlock> parseNiFloatExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiFloatExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->value = br.readFloat();
    return block;
}

// --- NiColorExtraData ---

static std::unique_ptr<NiBlock> parseNiColorExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiColorExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->value = br.readColor4();
    return block;
}

// --- NiAlphaController (= NiSingleInterpController, no extra fields for v20.0.0.4) ---

static std::unique_ptr<NiBlock> parseNiAlphaController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiAlphaControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    return block;
}

// --- NiMaterialColorController ---

static std::unique_ptr<NiBlock> parseNiMaterialColorController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiMaterialColorControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    // Target Color (version >= 10.1.0.0): MaterialColor enum as uint16
    block->targetColor = br.readUint16();
    return block;
}

// --- NiFlipController ---

static std::unique_ptr<NiBlock> parseNiFlipController(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiFlipControllerBlock>();
    block->blockIndex = idx;
    parseNiSingleInterpController(br, *block);
    // Texture Slot (version >= 10.2.0.0)
    block->textureSlot = br.readUint32();
    block->numSources = br.readUint32();
    block->sourceRefs.resize(block->numSources);
    for (uint32_t i = 0; i < block->numSources; i++)
        block->sourceRefs[i] = br.readRef();
    return block;
}

// --- NiTextKeyExtraData ---

static std::unique_ptr<NiBlock> parseNiTextKeyExtraData(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiTextKeyExtraDataBlock>();
    block->blockIndex = idx;
    block->name = br.readSizedString();
    block->numTextKeys = br.readUint32();
    block->textKeys.resize(block->numTextKeys);
    for (uint32_t i = 0; i < block->numTextKeys; i++) {
        block->textKeys[i].time = br.readFloat();
        block->textKeys[i].value = br.readSizedString();
    }
    return block;
}

// --- NiControllerSequence ---

static ControllerLink parseControllerLink(BinaryReader& br) {
    ControllerLink cl;
    cl.targetName = br.readSizedString();
    cl.controllerRef = br.readRef();
    cl.interpolatorRef = br.readRef();
    cl.controller2Ref = br.readRef();
    cl.priority = br.readUint16();
    cl.nodeName = br.readSizedString();
    cl.propertyType = br.readSizedString();
    cl.controllerType = br.readSizedString();
    cl.variable1 = br.readSizedString();
    cl.variable2 = br.readSizedString();
    return cl;
}

static std::unique_ptr<NiBlock> parseNiControllerSequence(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiControllerSequenceBlock>();
    block->blockIndex = idx;

    block->name = br.readSizedString();
    block->numControlledBlocks = br.readUint32();
    block->arrayGrowBy = br.readUint32();
    block->controlledBlocks.resize(block->numControlledBlocks);
    for (uint32_t i = 0; i < block->numControlledBlocks; i++) {
        block->controlledBlocks[i] = parseControllerLink(br);
    }
    block->weight = br.readFloat();
    block->textKeysRef = br.readRef();
    block->cycleType = br.readUint32();
    block->frequency = br.readFloat();
    block->startTime = br.readFloat();
    block->stopTime = br.readFloat();
    block->managerRef = br.readRef();
    block->targetName = br.readSizedString();

    return block;
}

// --- NiControllerManager ---

static std::unique_ptr<NiBlock> parseNiControllerManager(BinaryReader& br, int idx) {
    auto block = std::make_unique<NiControllerManagerBlock>();
    block->blockIndex = idx;
    parseNiTimeController(br, *block);
    block->cumulative = br.readBool();
    block->numControllerSequences = br.readUint32();
    block->controllerSequenceRefs.resize(block->numControllerSequences);
    for (uint32_t i = 0; i < block->numControllerSequences; i++)
        block->controllerSequenceRefs[i] = br.readRef();
    block->objectPaletteRef = br.readRef();
    return block;
}

// ============================================================================
// Parser dispatch table
// ============================================================================

using BlockParser = std::unique_ptr<NiBlock> (*)(BinaryReader&, int);

static const std::unordered_map<std::string, BlockParser> g_parsers = {
    {"NiNode",                         parseNiNode},
    {"NiBillboardNode",                parseNiBillboardNode},
    {"NiTriShape",                     parseNiTriShape},
    {"NiTriStrips",                    parseNiTriStrips},
    {"NiTriShapeData",                 parseNiTriShapeData},
    {"NiTriStripsData",                parseNiTriStripsData},
    {"NiTexturingProperty",            parseNiTexturingProperty},
    {"NiSourceTexture",                parseNiSourceTexture},
    {"NiMaterialProperty",             parseNiMaterialProperty},
    {"NiAlphaProperty",                parseNiAlphaProperty},
    {"NiZBufferProperty",              parseNiZBufferProperty},
    {"NiVertexColorProperty",          parseNiVertexColorProperty},
    {"NiStencilProperty",              parseNiStencilProperty},
    {"NiSpecularProperty",             parseNiSpecularProperty},
    {"NiStringExtraData",              parseNiStringExtraData},
    {"NiIntegerExtraData",             parseNiIntegerExtraData},
    {"NiPointLight",                   parseNiPointLight},
    {"NiDirectionalLight",             parseNiDirectionalLight},
    {"NiAmbientLight",                 parseNiAmbientLight},
    {"NiTransformController",          parseNiTransformController},
    {"NiTextureTransformController",   parseNiTextureTransformController},
    {"NiTransformInterpolator",        parseNiTransformInterpolator},
    {"NiFloatInterpolator",            parseNiFloatInterpolator},
    {"NiTransformData",                parseNiTransformData},
    {"NiKeyframeData",                 parseNiTransformData},  // same layout
    {"NiFloatData",                    parseNiFloatData},
    {"NiCamera",                       parseNiCamera},
    {"NiMultiTargetTransformController", parseNiMultiTargetTransformController},
    {"NiSkinInstance",                 parseNiSkinInstance},
    {"NiSkinData",                     parseNiSkinData},
    {"NiSkinPartition",                parseNiSkinPartition},
    {"NiVisController",                parseNiVisController},
    {"NiBoolInterpController",         parseNiVisController},  // same layout
    {"NiBoolInterpolator",             parseNiBoolInterpolator},
    {"NiBoolData",                     parseNiBoolData},
    {"NiAlphaController",              parseNiAlphaController},
    {"NiMaterialColorController",      parseNiMaterialColorController},
    {"NiFlipController",               parseNiFlipController},
    {"NiTextKeyExtraData",             parseNiTextKeyExtraData},
    {"NiControllerSequence",           parseNiControllerSequence},
    {"NiControllerManager",            parseNiControllerManager},
    {"NiSpotLight",                    parseNiSpotLight},
    {"NiBooleanExtraData",             parseNiBooleanExtraData},
    {"NiFloatExtraData",               parseNiFloatExtraData},
    {"NiColorExtraData",               parseNiColorExtraData},
};

// ============================================================================
// NiTriStripsData triangle conversion
// ============================================================================

std::vector<Triangle> NiTriStripsDataBlock::toTriangleList() const {
    std::vector<Triangle> tris;
    for (const auto& strip : strips) {
        if (strip.size() < 3) continue;
        for (size_t i = 2; i < strip.size(); i++) {
            Triangle t;
            if (i % 2 == 0) {
                t.v0 = strip[i - 2];
                t.v1 = strip[i - 1];
                t.v2 = strip[i];
            } else {
                // Reverse winding for odd triangles
                t.v0 = strip[i - 1];
                t.v1 = strip[i - 2];
                t.v2 = strip[i];
            }
            // Skip degenerate triangles
            if (t.v0 != t.v1 && t.v1 != t.v2 && t.v0 != t.v2) {
                tris.push_back(t);
            }
        }
    }
    return tris;
}

// ============================================================================
// Public API
// ============================================================================

std::unique_ptr<NifFile> loadNif(const uint8_t* data, size_t size) {
    if (!data || size < 64) {
        fprintf(stderr, "[NifLoader] Data too small (%zu bytes)\n", size);
        return nullptr;
    }

    BinaryReader br(data, size);
    auto nif = std::make_unique<NifFile>();

    // Parse header
    if (!parseHeader(br, nif->header)) {
        return nullptr;
    }

    fprintf(stderr, "[NifLoader] NIF v%u.%u.%u.%u, %u blocks, %u types\n",
            (nif->header.version >> 24) & 0xFF,
            (nif->header.version >> 16) & 0xFF,
            (nif->header.version >> 8) & 0xFF,
            nif->header.version & 0xFF,
            nif->header.numBlocks,
            nif->header.numBlockTypes);

    // Parse blocks sequentially
    nif->blocks.resize(nif->header.numBlocks);
    int blocksParsed = 0;

    for (uint32_t i = 0; i < nif->header.numBlocks; i++) {
        uint16_t typeIdx = nif->header.blockTypeIndices[i];
        const std::string& typeName = nif->header.blockTypeNames[typeIdx];

        auto it = g_parsers.find(typeName);
        if (it != g_parsers.end()) {
            try {
                nif->blocks[i] = it->second(br, (int)i);
                blocksParsed++;
            } catch (const std::exception& e) {
                fprintf(stderr, "[NifLoader] Error parsing %s [block %u]: %s\n",
                        typeName.c_str(), i, e.what());
                // Cannot recover without block sizes — fill remaining with nullptr
                for (uint32_t j = i; j < nif->header.numBlocks; j++)
                    nif->blocks[j] = nullptr;
                break;
            }
        } else {
            fprintf(stderr, "[NifLoader] Unknown block type: %s [block %u] at offset %zu — stopping\n",
                    typeName.c_str(), i, br.tell());
            // Cannot skip unknown blocks without block_size array
            for (uint32_t j = i; j < nif->header.numBlocks; j++)
                nif->blocks[j] = nullptr;
            break;
        }
    }

    // Parse footer (num_roots + root refs)
    if (br.remaining() >= 4) {
        uint32_t numRoots = br.readUint32();
        nif->rootRefs.resize(numRoots);
        for (uint32_t i = 0; i < numRoots; i++) {
            if (br.remaining() >= 4)
                nif->rootRefs[i] = br.readRef();
        }
    }

    fprintf(stderr, "[NifLoader] Parsed %d / %u blocks, %zu bytes remaining\n",
            blocksParsed, nif->header.numBlocks, br.remaining());

    return nif;
}

std::unique_ptr<NifFile> loadNifFromFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[NifLoader] Cannot open file: %s\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(f);
        return nullptr;
    }

    std::vector<uint8_t> data(fileSize);
    size_t bytesRead = fread(data.data(), 1, fileSize, f);
    fclose(f);

    if (bytesRead != (size_t)fileSize) {
        fprintf(stderr, "[NifLoader] Short read: got %zu of %ld bytes\n", bytesRead, fileSize);
        return nullptr;
    }

    fprintf(stderr, "[NifLoader] Loading %s (%ld bytes)\n", path, fileSize);
    return loadNif(data.data(), data.size());
}

} // namespace nif
