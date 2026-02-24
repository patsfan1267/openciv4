// GLRenderer.cpp — OpenGL 3.3 map renderer + 2D UI overlay
//
// Replaces Renderer.cpp (SDL_Renderer). All drawing goes through OpenGL
// using a batched quad system: each frame builds vertex data on CPU,
// uploads to a single VBO, draws in minimal draw calls.

#include "GLRenderer.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

// Render step tracking for crash diagnostics
// 0=idle, 1=terrain, 2=3dModels, 3=HUD, 4=turnBanner, 5=messages, 6=cityPanel,
// 7=unitPanel, 8=actionBar, 9=playerPanel, 10=minimap, 11=techPicker, 12=tooltip, 13=helpOverlay
volatile int g_renderStep = 0;

// Font pointer validation helper
static bool isFontValid(TTF_Font* font) {
    if (!font) return false;
    uintptr_t addr = (uintptr_t)font;
    return (addr >= 0x10000 && addr <= 0x00007FFFFFFFFFFF);
}

// ---- GLSL shader sources (embedded) ----

static const char* VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;

void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uTexture;
uniform int uUseTexture;

out vec4 fragColor;

void main() {
    if (uUseTexture != 0) {
        fragColor = texture(uTexture, vUV) * vColor;
    } else {
        fragColor = vColor;
    }
}
)";

// ---- 3D Model shaders ----

static const char* VERT_3D_SRC = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection3D;

out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = uProjection3D * uView * worldPos;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
}
)";

static const char* FRAG_3D_SRC = R"(
#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vWorldPos;

uniform sampler2D uTexture3D;
uniform int uUseTexture3D;
uniform vec4 uDiffuseColor;
uniform vec3 uLightDir;
uniform vec3 uAmbient;
uniform int uDiffuseOverride;   // 1 = multiply by uOverrideColor (for civ-color tinting)
uniform vec3 uOverrideColor;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    float NdotL = max(dot(N, uLightDir), 0.0);
    vec3 lighting = uAmbient + vec3(0.8) * NdotL;

    vec4 baseColor = uDiffuseColor;
    if (uUseTexture3D != 0) {
        baseColor *= texture(uTexture3D, vUV);
    }

    vec3 finalColor = baseColor.rgb * lighting;
    if (uDiffuseOverride != 0) {
        finalColor *= uOverrideColor;
    }

    fragColor = vec4(finalColor, baseColor.a);
}
)";

// ---- 3D Terrain shader (M1: vertex color + lighting) ----

static const char* VERT_TERRAIN_SRC = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uMVP;

out vec3 vNormal;
out vec3 vColor;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNormal = aNormal;
    vColor = aColor;
}
)";

static const char* FRAG_TERRAIN_SRC = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uLightDir;
uniform vec3 uAmbient;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    float NdotL = max(dot(N, uLightDir), 0.0);
    vec3 lighting = uAmbient + vec3(0.8) * NdotL;
    fragColor = vec4(vColor * lighting, 1.0);
}
)";

// Simple 4x4 matrix helpers for 3D rendering (column-major for OpenGL)
namespace mat4 {
    static void identity(float* m) {
        memset(m, 0, 16 * sizeof(float));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static void ortho(float* m, float left, float right, float bottom, float top, float near, float far) {
        memset(m, 0, 16 * sizeof(float));
        m[0]  = 2.0f / (right - left);
        m[5]  = 2.0f / (top - bottom);
        m[10] = -2.0f / (far - near);
        m[12] = -(right + left) / (right - left);
        m[13] = -(top + bottom) / (top - bottom);
        m[14] = -(far + near) / (far - near);
        m[15] = 1.0f;
    }

    static void translate(float* m, float tx, float ty, float tz) {
        identity(m);
        m[12] = tx; m[13] = ty; m[14] = tz;
    }

    static void scale(float* m, float sx, float sy, float sz) {
        memset(m, 0, 16 * sizeof(float));
        m[0] = sx; m[5] = sy; m[10] = sz; m[15] = 1.0f;
    }

    static void multiply(float* out, const float* a, const float* b) {
        float tmp[16];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                tmp[j * 4 + i] = 0;
                for (int k = 0; k < 4; k++)
                    tmp[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
            }
        memcpy(out, tmp, 16 * sizeof(float));
    }

    // Rotation around X axis (angle in radians)
    static void rotateX(float* m, float angle) {
        identity(m);
        float c = cosf(angle), s = sinf(angle);
        m[5] = c;  m[6] = s;
        m[9] = -s; m[10] = c;
    }

    // Rotation around Y axis (angle in radians)
    static void rotateY(float* m, float angle) {
        identity(m);
        float c = cosf(angle), s = sinf(angle);
        m[0] = c;  m[2] = -s;
        m[8] = s;  m[10] = c;
    }

    // Perspective projection matrix
    static void perspective(float* m, float fovY, float aspect, float nearZ, float farZ) {
        memset(m, 0, 16 * sizeof(float));
        float f = 1.0f / tanf(fovY * 0.5f);
        m[0]  = f / aspect;
        m[5]  = f;
        m[10] = (farZ + nearZ) / (nearZ - farZ);
        m[11] = -1.0f;
        m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    }

    // Look-at view matrix
    static void lookAt(float* m, float eyeX, float eyeY, float eyeZ,
                       float atX, float atY, float atZ,
                       float upX, float upY, float upZ) {
        // Forward = normalize(at - eye)
        float fx = atX - eyeX, fy = atY - eyeY, fz = atZ - eyeZ;
        float flen = sqrtf(fx*fx + fy*fy + fz*fz);
        if (flen > 0) { fx /= flen; fy /= flen; fz /= flen; }
        // Right = normalize(forward x up)
        float rx = fy * upZ - fz * upY;
        float ry = fz * upX - fx * upZ;
        float rz = fx * upY - fy * upX;
        float rlen = sqrtf(rx*rx + ry*ry + rz*rz);
        if (rlen > 0) { rx /= rlen; ry /= rlen; rz /= rlen; }
        // True up = right x forward
        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;
        // Column-major
        m[0] = rx;  m[1] = ux;  m[2] = -fx; m[3] = 0;
        m[4] = ry;  m[5] = uy;  m[6] = -fy; m[7] = 0;
        m[8] = rz;  m[9] = uz;  m[10]= -fz; m[11]= 0;
        m[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
        m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
        m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
        m[15] = 1.0f;
    }

    // Invert a 4x4 matrix (general case, for unprojection)
    static bool invert(float* out, const float* m) {
        float inv[16];
        inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
               + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
               - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
               + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12]= -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
               - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
        float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
        if (fabsf(det) < 1e-12f) return false;
        inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
               - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
               + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
               - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13]= m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
               + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
        inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
               + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
        inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
               - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
        inv[10]= m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
               + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
        inv[14]= -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
               - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
        inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
               - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
        inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
               + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
        inv[11]= -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
               - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
        inv[15]= m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
               + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
        float invDet = 1.0f / det;
        for (int i = 0; i < 16; i++) out[i] = inv[i] * invDet;
        return true;
    }
}

// ---- Lifecycle ----

GLRenderer::GLRenderer()
    : m_assets(nullptr)
    , m_windowW(0), m_windowH(0)
    , m_camera()
    , m_pushCommand()
    , m_keyUp(false), m_keyDown(false), m_keyLeft(false), m_keyRight(false)
    , m_panSpeed(400.0f)
    , m_lastFrameTime(0)
    , m_showMinimap(true)
    , m_showHelp(false)
    , m_showPlayerPanel(true)
    , m_showGrid(false)
    , m_showTechPicker(false)
    , m_disable3D(false)
    , m_techScrollOffset(0)
    , m_mouseX(0), m_mouseY(0)
    , m_cameraInitialized(false)
    , m_mmX(0), m_mmY(0), m_mmW(0), m_mmH(0)
    , m_prodScrollOffset(0)
    , m_shader()
    , m_shader3D()
    , m_batchVAO(0), m_batchVBO(0)
    , m_batchVerts()
    , m_whiteTexture(0)
    , m_fontSmall(nullptr)
    , m_fontMedium(nullptr)
    , m_fontLarge(nullptr)
    , m_textCache()
    , m_textCacheFrame(0)
{
}

GLRenderer::~GLRenderer() {
    shutdown();
}

bool GLRenderer::init(int windowW, int windowH, AssetManager* assets) {
    m_windowW = windowW;
    m_windowH = windowH;
    m_assets = assets;

    initShaders();
    initBatchBuffers();
    initWhiteTexture();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    return true;
}

void GLRenderer::shutdown() {
    m_shader.destroy();
    m_shader3D.destroy();
    if (m_batchVAO) { glDeleteVertexArrays(1, &m_batchVAO); m_batchVAO = 0; }
    if (m_batchVBO) { glDeleteBuffers(1, &m_batchVBO); m_batchVBO = 0; }
    if (m_whiteTexture) { glDeleteTextures(1, &m_whiteTexture); m_whiteTexture = 0; }

    for (auto& pair : m_textCache)
        glDeleteTextures(1, &pair.second.textureID);
    m_textCache.clear();

    if (m_fontSmall)  { TTF_CloseFont(m_fontSmall);  m_fontSmall = nullptr; }
    if (m_fontMedium) { TTF_CloseFont(m_fontMedium); m_fontMedium = nullptr; }
    if (m_fontLarge)  { TTF_CloseFont(m_fontLarge);  m_fontLarge = nullptr; }
}

void GLRenderer::initShaders() {
    if (!m_shader.compile(VERT_SRC, FRAG_SRC)) {
        fprintf(stderr, "[GLRenderer] FATAL: 2D shader compile failed\n");
    }
    if (!m_shader3D.compile(VERT_3D_SRC, FRAG_3D_SRC)) {
        fprintf(stderr, "[GLRenderer] FATAL: 3D shader compile failed\n");
    }
    if (!m_shaderTerrain.compile(VERT_TERRAIN_SRC, FRAG_TERRAIN_SRC)) {
        fprintf(stderr, "[GLRenderer] FATAL: terrain shader compile failed\n");
    }
}

void GLRenderer::initBatchBuffers() {
    glGenVertexArrays(1, &m_batchVAO);
    glGenBuffers(1, &m_batchVBO);

    glBindVertexArray(m_batchVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);

    // Layout: pos(2) + uv(2) + color(4) = 8 floats = 32 bytes per vertex
    int stride = FLOATS_PER_VERT * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);                    // aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));  // aUV
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));  // aColor
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void GLRenderer::initWhiteTexture() {
    uint8_t white[] = {255, 255, 255, 255};
    glGenTextures(1, &m_whiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// ---- Batch system ----

void GLRenderer::beginBatch() {
    m_batchVerts.clear();
}

// Push a textured or colored quad. If tex==0, uses white texture (pure color).
void GLRenderer::pushQuad(float x, float y, float w, float h,
                           float r, float g, float b, float a,
                           GLuint tex, float u0, float v0, float u1, float v1) {
    // We need to flush when texture changes
    // For simplicity in M1, we flush per-texture-change and track current texture
    // But for now, we'll just push vertices — the caller manages flush/texture binding

    // 6 vertices (2 triangles) per quad
    // Top-left, bottom-left, bottom-right, top-left, bottom-right, top-right
    float verts[] = {
        x,     y,     u0, v0, r, g, b, a,
        x,     y + h, u0, v1, r, g, b, a,
        x + w, y + h, u1, v1, r, g, b, a,
        x,     y,     u0, v0, r, g, b, a,
        x + w, y + h, u1, v1, r, g, b, a,
        x + w, y,     u1, v0, r, g, b, a,
    };
    m_batchVerts.insert(m_batchVerts.end(), verts, verts + 48);
}

void GLRenderer::pushLine(float x1, float y1, float x2, float y2,
                           float thickness, float r, float g, float b, float a) {
    // Turn line into a thin quad
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;

    float verts[] = {
        x1 + nx, y1 + ny, 0, 0, r, g, b, a,
        x1 - nx, y1 - ny, 0, 0, r, g, b, a,
        x2 - nx, y2 - ny, 0, 0, r, g, b, a,
        x1 + nx, y1 + ny, 0, 0, r, g, b, a,
        x2 - nx, y2 - ny, 0, 0, r, g, b, a,
        x2 + nx, y2 + ny, 0, 0, r, g, b, a,
    };
    m_batchVerts.insert(m_batchVerts.end(), verts, verts + 48);
}

void GLRenderer::pushQuadAlphaGrad(float x, float y, float w, float h,
                                    float r, float g, float b,
                                    float aTL, float aTR, float aBL, float aBR,
                                    GLuint tex, float u0, float v0, float u1, float v1) {
    // 6 vertices (2 triangles) with per-corner alpha for gradient blending
    float verts[] = {
        x,     y,     u0, v0, r, g, b, aTL,   // top-left
        x,     y + h, u0, v1, r, g, b, aBL,   // bottom-left
        x + w, y + h, u1, v1, r, g, b, aBR,   // bottom-right
        x,     y,     u0, v0, r, g, b, aTL,   // top-left
        x + w, y + h, u1, v1, r, g, b, aBR,   // bottom-right
        x + w, y,     u1, v0, r, g, b, aTR,   // top-right
    };
    m_batchVerts.insert(m_batchVerts.end(), verts, verts + 48);
}

void GLRenderer::flushBatch() {
    if (m_batchVerts.empty()) return;

    glBindVertexArray(m_batchVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);
    glBufferData(GL_ARRAY_BUFFER, m_batchVerts.size() * sizeof(float),
                 m_batchVerts.data(), GL_STREAM_DRAW);

    int numVerts = (int)m_batchVerts.size() / FLOATS_PER_VERT;
    glDrawArrays(GL_TRIANGLES, 0, numVerts);

    m_batchVerts.clear();
}

void GLRenderer::setProjectionOrtho() {
    // Orthographic projection: (0,0) = top-left, (windowW, windowH) = bottom-right
    float L = 0, R = (float)m_windowW, T = 0, B = (float)m_windowH;
    float proj[16] = {
        2.0f/(R-L),  0,            0,  0,
        0,            2.0f/(T-B),  0,  0,
        0,            0,           -1,  0,
        -(R+L)/(R-L), -(T+B)/(T-B), 0,  1,
    };
    m_shader.use();
    m_shader.setMat4("uProjection", proj);
}

// ---- Fonts ----

bool GLRenderer::initFonts(const char* fontPath) {
    m_fontSmall = TTF_OpenFont(fontPath, 11);
    m_fontMedium = TTF_OpenFont(fontPath, 14);
    m_fontLarge = TTF_OpenFont(fontPath, 18);
    if (!m_fontSmall || !m_fontMedium) {
        fprintf(stderr, "[GLRenderer] Warning: Could not load font '%s': %s\n",
                fontPath, TTF_GetError());
        return false;
    }
    fprintf(stderr, "[GLRenderer] Loaded font: %s (11/14/18pt)\n", fontPath);
    return true;
}

GLuint GLRenderer::getTextTexture(const std::string& text, TTF_Font* font,
                                   uint8_t r, uint8_t g, uint8_t b, int& tw, int& th) {
    if (!isFontValid(font)) { tw = th = 0; return 0; }

    char key[512];
    int fontSize = TTF_FontHeight(font);
    snprintf(key, sizeof(key), "%s|%d|%d%d%d", text.c_str(), fontSize, r, g, b);
    std::string keyStr(key);

    auto it = m_textCache.find(keyStr);
    if (it != m_textCache.end()) {
        tw = it->second.w;
        th = it->second.h;
        return it->second.textureID;
    }

    SDL_Color color = {r, g, b, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) { tw = th = 0; return 0; }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!rgba) { tw = th = 0; return 0; }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    tw = rgba->w;
    th = rgba->h;
    SDL_FreeSurface(rgba);

    m_textCache[keyStr] = {texID, tw, th};
    return texID;
}

void GLRenderer::drawText(const std::string& text, int x, int y, TTF_Font* font,
                           uint8_t r, uint8_t g, uint8_t b) {
    if (!font || text.empty()) return;
    int tw, th;
    GLuint tex = getTextTexture(text, font, r, g, b, tw, th);
    if (!tex) return;

    flushBatch();
    glBindTexture(GL_TEXTURE_2D, tex);
    m_shader.setInt("uUseTexture", 1);

    beginBatch();
    pushQuad((float)x, (float)y, (float)tw, (float)th, 1, 1, 1, 1, tex);
    flushBatch();

    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
}

void GLRenderer::drawTextCentered(const std::string& text, int cx, int cy, TTF_Font* font,
                                   uint8_t r, uint8_t g, uint8_t b) {
    if (!isFontValid(font) || text.empty()) return;
    int tw = 0, th = 0;
    TTF_SizeText(font, text.c_str(), &tw, &th);
    drawText(text, cx - tw / 2, cy - th / 2, font, r, g, b);
}

// ---- Camera ----

void GLRenderer::autoFitCamera(const MapSnapshot& snapshot) {
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;
    float zoomX = (m_windowW * 0.9f) / mapPixelW;
    float zoomY = (m_windowH * 0.9f) / mapPixelH;
    m_camera.zoom = std::max(0.1f, std::min(std::min(zoomX, zoomY), 20.0f));
    float scaledW = mapPixelW * m_camera.zoom;
    float scaledH = mapPixelH * m_camera.zoom;
    m_camera.offsetX = -(m_windowW - scaledW) / (2.0f * m_camera.zoom);
    m_camera.offsetY = -(m_windowH - scaledH) / (2.0f * m_camera.zoom);
}

void GLRenderer::centerOnTile(int tileX, int tileY, int mapHeight) {
    if (m_use3DTerrain) {
        // 3D camera: set focal point to tile center in world coords
        m_cam3D.focalX = tileX + 0.5f;
        m_cam3D.focalZ = (mapHeight - 1 - tileY) + 0.5f;
    } else {
        int flippedRow = mapHeight - 1 - tileY;
        float worldX = (tileX + 0.5f) * TILE_SIZE;
        float worldY = (flippedRow + 0.5f) * TILE_SIZE;
        m_camera.offsetX = worldX - (m_windowW / m_camera.zoom) * 0.5f;
        m_camera.offsetY = worldY - (m_windowH / m_camera.zoom) * 0.5f;
    }
}

void GLRenderer::tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const {
    int flippedRow = mapHeight - 1 - row;
    tx = col * TILE_SIZE;
    ty = flippedRow * TILE_SIZE;
}

bool GLRenderer::screenToTile(int screenX, int screenY, const MapSnapshot& snapshot,
                               int& tileX, int& tileY) {
    if (m_use3DTerrain) {
        // Unproject screen point through inverse VP, intersect with Y=0 plane
        float invVP[16];
        if (!mat4::invert(invVP, m_vpMatrix)) return false;

        // NDC coordinates: map screen [0,W] x [0,H] to [-1,1]
        float ndcX = (2.0f * screenX / m_windowW) - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY / m_windowH); // flip Y

        // Near point in clip space
        float nearPt[4] = { ndcX, ndcY, -1.0f, 1.0f };
        float farPt[4]  = { ndcX, ndcY,  1.0f, 1.0f };

        // Transform to world space
        auto transformPt = [&](const float* pt, float* out) {
            for (int i = 0; i < 4; i++)
                out[i] = invVP[i]*pt[0] + invVP[4+i]*pt[1] + invVP[8+i]*pt[2] + invVP[12+i]*pt[3];
            if (fabsf(out[3]) > 1e-12f) {
                out[0] /= out[3]; out[1] /= out[3]; out[2] /= out[3];
            }
        };

        float wNear[4], wFar[4];
        transformPt(nearPt, wNear);
        transformPt(farPt, wFar);

        // Ray direction
        float dx = wFar[0] - wNear[0];
        float dy = wFar[1] - wNear[1];
        float dz = wFar[2] - wNear[2];

        // Intersect with Y=0 plane
        if (fabsf(dy) < 1e-8f) return false; // ray parallel to ground
        float t = -wNear[1] / dy;
        if (t < 0) return false; // behind camera

        float hitX = wNear[0] + t * dx;
        float hitZ = wNear[2] + t * dz;

        int col = (int)floorf(hitX);
        int flippedRow = (int)floorf(hitZ);

        if (snapshot.wrapX) col = ((col % snapshot.width) + snapshot.width) % snapshot.width;
        if (snapshot.wrapY) flippedRow = ((flippedRow % snapshot.height) + snapshot.height) % snapshot.height;

        int row = snapshot.height - 1 - flippedRow;
        if (col < 0 || col >= snapshot.width || row < 0 || row >= snapshot.height)
            return false;
        tileX = col;
        tileY = row;
        return true;
    }

    // Legacy 2D path
    float worldX = screenX / m_camera.zoom + m_camera.offsetX;
    float worldY = screenY / m_camera.zoom + m_camera.offsetY;
    int col = (int)floorf(worldX / TILE_SIZE);
    int flippedRow = (int)floorf(worldY / TILE_SIZE);

    if (snapshot.wrapX) col = ((col % snapshot.width) + snapshot.width) % snapshot.width;
    if (snapshot.wrapY) flippedRow = ((flippedRow % snapshot.height) + snapshot.height) % snapshot.height;

    int row = snapshot.height - 1 - flippedRow;
    if (col < 0 || col >= snapshot.width || row < 0 || row >= snapshot.height)
        return false;
    tileX = col;
    tileY = row;
    return true;
}

// ---- Selection highlight ----

void GLRenderer::drawSelectionHighlight(float screenTX, float screenTY, float screenTileSize) {
    Uint32 ms = SDL_GetTicks();
    float alpha = 0.6f + 0.4f * sinf(ms * 0.005f);
    float th = std::max(2.0f, screenTileSize * 0.1f);

    // Draw 4 edges as thin quads
    float s = screenTileSize;
    pushQuad(screenTX - th, screenTY - th, s + th * 2, th, 1, 1, 1, alpha); // top
    pushQuad(screenTX - th, screenTY + s, s + th * 2, th, 1, 1, 1, alpha);  // bottom
    pushQuad(screenTX - th, screenTY, th, s, 1, 1, 1, alpha);               // left
    pushQuad(screenTX + s, screenTY, th, s, 1, 1, 1, alpha);                // right
}

// ---- Main draw ----

void GLRenderer::draw(MapSnapshot& snapshot) {
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastFrameTime == 0) ? (1.0f / 60.0f) : ((now - m_lastFrameTime) / 1000.0f);
    dt = std::min(dt, 0.1f);
    m_lastFrameTime = now;

    if (m_use3DTerrain) {
        // 3D camera: pan focal point based on azimuth direction
        float panSpeed3D = m_cam3D.distance * 0.5f * dt;
        float cosAz = cosf(m_cam3D.azimuth);
        float sinAz = sinf(m_cam3D.azimuth);
        if (m_keyUp)    { m_cam3D.focalX -= sinAz * panSpeed3D; m_cam3D.focalZ -= cosAz * panSpeed3D; }
        if (m_keyDown)  { m_cam3D.focalX += sinAz * panSpeed3D; m_cam3D.focalZ += cosAz * panSpeed3D; }
        if (m_keyLeft)  { m_cam3D.focalX -= cosAz * panSpeed3D; m_cam3D.focalZ += sinAz * panSpeed3D; }
        if (m_keyRight) { m_cam3D.focalX += cosAz * panSpeed3D; m_cam3D.focalZ -= sinAz * panSpeed3D; }
    } else {
        float panDelta = m_panSpeed * dt / m_camera.zoom;
        if (m_keyUp)    m_camera.offsetY -= panDelta;
        if (m_keyDown)  m_camera.offsetY += panDelta;
        if (m_keyLeft)  m_camera.offsetX -= panDelta;
        if (m_keyRight) m_camera.offsetX += panDelta;
    }

    // Periodically clear text cache to prevent memory growth
    m_textCacheFrame++;
    if (m_textCacheFrame > 300) { // every ~5 seconds at 60fps
        for (auto& pair : m_textCache)
            glDeleteTextures(1, &pair.second.textureID);
        m_textCache.clear();
        m_textCacheFrame = 0;
    }

    glViewport(0, 0, m_windowW, m_windowH);
    glClearColor(10.0f / 255, 20.0f / 255, 40.0f / 255, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shader.use();
    setProjectionOrtho();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uTexture", 0);
    m_shader.setInt("uUseTexture", 0);

    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);

        if (snapshot.width == 0 || snapshot.height == 0) {
            // No map data yet — draw a "loading" message
            if (isFontValid(m_fontMedium)) {
                drawTextCentered("Waiting for game data...", m_windowW / 2, m_windowH / 2,
                                 m_fontMedium, 200, 200, 200);
            }
            return;
        }

        if (!m_cameraInitialized) {
            if (m_use3DTerrain) {
                autoFitCamera3D(snapshot);
            } else {
                autoFitCamera(snapshot);
            }
            m_cameraInitialized = true;
        }

        // Normalize camera for wrapping maps
        if (snapshot.wrapX) {
            float mapPixelW = snapshot.width * TILE_SIZE;
            m_camera.offsetX = fmodf(m_camera.offsetX, mapPixelW);
            if (m_camera.offsetX < 0) m_camera.offsetX += mapPixelW;
        }
        if (snapshot.wrapY) {
            float mapPixelH = snapshot.height * TILE_SIZE;
            m_camera.offsetY = fmodf(m_camera.offsetY, mapPixelH);
            if (m_camera.offsetY < 0) m_camera.offsetY += mapPixelH;
        }

        g_renderStep = 1;
        if (m_use3DTerrain) {
            drawTerrain3D(snapshot);
        } else {
            drawTerrain(snapshot);
        }

        if (!m_disable3D) {
            g_renderStep = 2;
            draw3DModels(snapshot);
        }

        // Restore 2D state after 3D rendering
        m_shader.use();
        setProjectionOrtho();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);

        // UI overlays
        g_renderStep = 3;
        drawHUD(snapshot);
        g_renderStep = 4;
        drawTurnBanner(snapshot);
        g_renderStep = 5;
        drawGameMessages(snapshot);
        g_renderStep = 6;
        if (snapshot.cityScreenOpen)
            drawCityPanel(snapshot);
        g_renderStep = 7;
        if (!snapshot.cityScreenOpen && snapshot.selectedUnitID >= 0)
            drawUnitPanel(snapshot);
        g_renderStep = 8;
        drawActionBar(snapshot);
        g_renderStep = 9;
        if (m_showPlayerPanel) drawPlayerPanel(snapshot);
        g_renderStep = 10;
        if (m_showMinimap) drawMinimap(snapshot);
        g_renderStep = 11;
        if (m_showTechPicker) drawTechPicker(snapshot);
        g_renderStep = 12;
        drawTooltip(snapshot);
        g_renderStep = 13;
        if (m_showHelp) drawHelpOverlay();
        g_renderStep = 0;
    }
}

// ---- 3D Model rendering ----

#include "Mesh.h"

void GLRenderer::draw3DModels(const MapSnapshot& snapshot) {
    // TODO: 3D model rendering disabled until lighting/texturing is improved
    return;
    if (!m_assets || !m_shader3D.id()) return;

    float screenTileSize = TILE_SIZE * m_camera.zoom;
    if (screenTileSize < 12) return;  // Don't draw 3D models when zoomed out too far

    // Set up 3D rendering state
    m_shader3D.use();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Set up orthographic projection that matches our 2D camera but with depth
    // The 3D models are rendered in screen space, positioned at tile centers
    float proj[16];
    mat4::ortho(proj, 0, (float)m_windowW, (float)m_windowH, 0, -1000.0f, 1000.0f);
    glUniformMatrix4fv(glGetUniformLocation(m_shader3D.id(), "uProjection3D"), 1, GL_FALSE, proj);

    // View matrix: identity (we're already in screen space)
    float view[16];
    mat4::identity(view);
    glUniformMatrix4fv(glGetUniformLocation(m_shader3D.id(), "uView"), 1, GL_FALSE, view);

    // Lighting
    float lightDir[3] = { 0.4f, -0.7f, 0.6f };  // top-right, slightly forward
    float len = sqrtf(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
    lightDir[0] /= len; lightDir[1] /= len; lightDir[2] /= len;
    glUniform3f(glGetUniformLocation(m_shader3D.id(), "uLightDir"), lightDir[0], lightDir[1], lightDir[2]);
    glUniform3f(glGetUniformLocation(m_shader3D.id(), "uAmbient"), 0.55f, 0.55f, 0.55f);
    glUniform1i(glGetUniformLocation(m_shader3D.id(), "uTexture3D"), 0);

    // Visible tile range
    float viewLeft = m_camera.offsetX;
    float viewRight = m_camera.offsetX + m_windowW / m_camera.zoom;
    float viewTop = m_camera.offsetY;
    float viewBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

    int colStart = (int)floorf(viewLeft / TILE_SIZE) - 1;
    int colEnd   = (int)ceilf(viewRight / TILE_SIZE);
    int rowStart = (int)floorf(viewTop / TILE_SIZE) - 1;
    int rowEnd   = (int)ceilf(viewBottom / TILE_SIZE);

    if (!snapshot.wrapX) { colStart = std::max(0, colStart); colEnd = std::min(snapshot.width - 1, colEnd); }
    if (!snapshot.wrapY) { rowStart = std::max(0, rowStart); rowEnd = std::min(snapshot.height - 1, rowEnd); }

    GLint locModel = glGetUniformLocation(m_shader3D.id(), "uModel");
    GLint locDiffuseOverride = glGetUniformLocation(m_shader3D.id(), "uDiffuseOverride");
    GLint locOverrideColor = glGetUniformLocation(m_shader3D.id(), "uOverrideColor");

    for (int vr = rowStart; vr <= rowEnd; vr++) {
        for (int vc = colStart; vc <= colEnd; vc++) {
            int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
            int fr = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
            int y = snapshot.height - 1 - fr;
            if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;

            const PlotData& plot = snapshot.getPlot(x, y);

            // Skip 3D models on unseen or fog-of-war tiles
            if (plot.visibility < 2) continue;

            // Calculate screen position (center of tile)
            float worldTX = vc * TILE_SIZE + TILE_SIZE * 0.5f;
            float worldTY = vr * TILE_SIZE + TILE_SIZE * 0.5f;
            float screenCX = (worldTX - m_camera.offsetX) * m_camera.zoom;
            float screenCY = (worldTY - m_camera.offsetY) * m_camera.zoom;

            // --- Render building on city tiles ---
            if (plot.isCity && !plot.cityBuildingNIF.empty()) {
                Mesh* mesh = m_assets->getModel(plot.cityBuildingNIF);
                if (mesh) {
                    float modelScale = screenTileSize * 0.012f * plot.cityBuildingScale;
                    float mTranslate[16], mScale[16], mRotX[16], mRotY[16], model[16], tmp[16], tmp2[16];
                    mat4::translate(mTranslate, screenCX, screenCY, 0);
                    mat4::scale(mScale, modelScale, -modelScale, modelScale);
                    mat4::rotateX(mRotX, -m_camera.tiltAngle);
                    mat4::rotateY(mRotY, m_camera.rotationAngle);
                    mat4::multiply(tmp, mRotX, mRotY);      // tilt then rotate
                    mat4::multiply(tmp2, mTranslate, tmp);   // position in screen
                    mat4::multiply(model, tmp2, mScale);     // scale model
                    glUniformMatrix4fv(locModel, 1, GL_FALSE, model);
                    if (locDiffuseOverride >= 0) glUniform1i(locDiffuseOverride, 0);
                    mesh->draw(m_shader3D.id());
                }
            }

            // --- Render unit on tiles with units ---
            if (plot.unitCount > 0 && !plot.firstUnitNIF.empty()) {
                Mesh* mesh = m_assets->getModel(plot.firstUnitNIF);
                if (mesh) {
                    // Position unit slightly offset from center if city also present
                    float unitCX = screenCX;
                    float unitCY = screenCY;
                    if (plot.isCity) {
                        unitCX += screenTileSize * 0.3f;  // offset right
                        unitCY += screenTileSize * 0.15f;
                    }

                    float modelScale = screenTileSize * 0.012f * plot.firstUnitNIFScale;
                    float mTranslate[16], mScale[16], mRotX[16], mRotY[16], model[16], tmp[16], tmp2[16];
                    mat4::translate(mTranslate, unitCX, unitCY, 0);
                    mat4::scale(mScale, modelScale, -modelScale, modelScale);
                    mat4::rotateX(mRotX, -m_camera.tiltAngle);
                    mat4::rotateY(mRotY, m_camera.rotationAngle);
                    mat4::multiply(tmp, mRotX, mRotY);
                    mat4::multiply(tmp2, mTranslate, tmp);
                    mat4::multiply(model, tmp2, mScale);
                    glUniformMatrix4fv(locModel, 1, GL_FALSE, model);

                    // Tint unit by owner color
                    if (locDiffuseOverride >= 0 && plot.firstUnitOwner >= 0) {
                        float r = plot.ownerColorR / 255.0f;
                        float g = plot.ownerColorG / 255.0f;
                        float b = plot.ownerColorB / 255.0f;
                        // Use the unit owner's color, not the plot owner's
                        int uOwner = plot.firstUnitOwner;
                        if (uOwner >= 0 && uOwner < snapshot.numPlayers) {
                            r = snapshot.players[uOwner].colorR / 255.0f;
                            g = snapshot.players[uOwner].colorG / 255.0f;
                            b = snapshot.players[uOwner].colorB / 255.0f;
                        }
                        glUniform1i(locDiffuseOverride, 1);
                        if (locOverrideColor >= 0)
                            glUniform3f(locOverrideColor, r, g, b);
                    }

                    // Selection highlight: brighter for selected unit
                    bool isSelected = (plot.firstUnitID == snapshot.selectedUnitID &&
                                       snapshot.selectedUnitID >= 0);
                    if (isSelected && locOverrideColor >= 0) {
                        // Pulse brightness for selected unit
                        float pulse = 0.7f + 0.3f * sinf(SDL_GetTicks() * 0.005f);
                        glUniform3f(locOverrideColor, pulse, pulse, pulse);
                        glUniform1i(locDiffuseOverride, 1);
                    }

                    mesh->draw(m_shader3D.id());

                    // Reset override
                    if (locDiffuseOverride >= 0) glUniform1i(locDiffuseOverride, 0);
                }
            }
        }
    }

    // Restore state
    glDisable(GL_DEPTH_TEST);
}

// ---- 3D Camera helpers ----

void GLRenderer::buildCamera3DMatrices(float* view, float* proj) const {
    // Eye position: orbit around focal point using spherical coordinates
    float cosEl = cosf(m_cam3D.elevation);
    float sinEl = sinf(m_cam3D.elevation);
    float cosAz = cosf(m_cam3D.azimuth);
    float sinAz = sinf(m_cam3D.azimuth);

    float eyeX = m_cam3D.focalX + m_cam3D.distance * sinAz * cosEl;
    float eyeY = m_cam3D.distance * sinEl;
    float eyeZ = m_cam3D.focalZ + m_cam3D.distance * cosAz * cosEl;

    mat4::lookAt(view, eyeX, eyeY, eyeZ,
                 m_cam3D.focalX, 0.0f, m_cam3D.focalZ,
                 0.0f, 1.0f, 0.0f);

    float aspect = (m_windowW > 0 && m_windowH > 0)
                   ? (float)m_windowW / (float)m_windowH : 1.0f;
    mat4::perspective(proj, m_cam3D.fovY, aspect, m_cam3D.nearZ, m_cam3D.farZ);
}

void GLRenderer::autoFitCamera3D(const MapSnapshot& snapshot) {
    // Center focal point on the map
    m_cam3D.focalX = snapshot.width * 0.5f;
    m_cam3D.focalZ = snapshot.height * 0.5f;
    // Distance so the whole map is roughly visible
    float mapDiag = sqrtf((float)(snapshot.width * snapshot.width + snapshot.height * snapshot.height));
    m_cam3D.distance = mapDiag * 0.7f;
    m_cam3D.elevation = 0.8f; // ~45 degree angle
    m_cam3D.azimuth = 0.0f;   // looking south
}

// ---- 3D Terrain rendering ----

void GLRenderer::drawTerrain3D(const MapSnapshot& snapshot) {
    // Build terrain mesh on first frame (or if map changed)
    if (!m_terrainMeshBuilt || m_terrainMesh.mapWidth() != snapshot.width ||
        m_terrainMesh.mapHeight() != snapshot.height) {
        m_terrainMesh.rebuild(snapshot);
        m_terrainMeshBuilt = true;
    }

    if (m_terrainMesh.empty()) return;

    // Build view and projection matrices
    float view[16], proj[16], mvp[16];
    buildCamera3DMatrices(view, proj);
    mat4::multiply(mvp, proj, view);

    // Cache VP for screenToTile unprojection
    memcpy(m_vpMatrix, mvp, sizeof(m_vpMatrix));

    // Set up terrain shader
    m_shaderTerrain.use();
    glUniformMatrix4fv(glGetUniformLocation(m_shaderTerrain.id(), "uMVP"), 1, GL_FALSE, mvp);

    // Lighting: sun from upper-right
    float lightDir[3] = { 0.4f, 0.8f, -0.4f };
    float len = sqrtf(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
    lightDir[0] /= len; lightDir[1] /= len; lightDir[2] /= len;
    glUniform3f(glGetUniformLocation(m_shaderTerrain.id(), "uLightDir"),
                lightDir[0], lightDir[1], lightDir[2]);
    glUniform3f(glGetUniformLocation(m_shaderTerrain.id(), "uAmbient"), 0.35f, 0.35f, 0.35f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_terrainMesh.draw();

    glDisable(GL_DEPTH_TEST);
}

// ---- Legacy 2D Terrain drawing (map tiles) ----

void GLRenderer::drawTerrain(const MapSnapshot& snapshot) {
    float screenTileSize = TILE_SIZE * m_camera.zoom;

    float viewLeft = m_camera.offsetX;
    float viewRight = m_camera.offsetX + m_windowW / m_camera.zoom;
    float viewTop = m_camera.offsetY;
    float viewBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

    int colStart = (int)floorf(viewLeft / TILE_SIZE) - 1;
    int colEnd   = (int)ceilf(viewRight / TILE_SIZE);
    int rowStart = (int)floorf(viewTop / TILE_SIZE) - 1;
    int rowEnd   = (int)ceilf(viewBottom / TILE_SIZE);

    if (!snapshot.wrapX) { colStart = std::max(0, colStart); colEnd = std::min(snapshot.width - 1, colEnd); }
    if (!snapshot.wrapY) { rowStart = std::max(0, rowStart); rowEnd = std::min(snapshot.height - 1, rowEnd); }

    // --- Pass 1: Terrain textures (Atlas-based multi-layer blending) ---
    bool haveBlend = m_assets && m_assets->hasBlendTextures();
    GLuint curBoundTex = 0;

    beginBatch();

    if (haveBlend) {
        m_shader.setInt("uUseTexture", 1);
        
        auto getTerrainKey = [](const PlotData& p) -> int {
            if (p.plotType == 0) return -1;
            if (p.plotType == 1) return -2;
            return p.terrainType;
        };

        auto layerForKey = [](int key) -> int {
            switch (key) {
                case -1: return 80;
                case -2: return 79;
                case 0: return 4;
                case 1: return 3;
                case 2: return 2;
                case 3: return 1;
                case 4: return 5;
                case 5: return 50;
                case 6: return 60;
                default: return 0;
            }
        };

        auto getNeighbor = [&](int px, int py) -> const PlotData* {
            if (snapshot.wrapX) px = ((px % snapshot.width) + snapshot.width) % snapshot.width;
            if (snapshot.wrapY) py = ((py % snapshot.height) + snapshot.height) % snapshot.height;
            if (px < 0 || px >= snapshot.width || py < 0 || py >= snapshot.height) return nullptr;
            return &snapshot.getPlot(px, py);
        };
        
        static const int blendMaskToAtlasSlot[16] = {
            0, 8, 1, 2, 6, 13, 10, 3, 5, 9, 7, 4, 12, 11, 14, 15
        };

        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                int fr = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                int y = snapshot.height - 1 - fr;
                if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;

                const PlotData& plot = snapshot.getPlot(x, y);
                if (plot.visibility == 0) continue;

                float worldTX = vc * TILE_SIZE;
                float worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;

                if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                    screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                float fogBright = (plot.visibility == 1) ? 0.45f : 1.0f;
                float landBright = fogBright * 1.15f;
                
                int myKey = getTerrainKey(plot);
                const PlotData* pN = getNeighbor(x, y + 1);
                const PlotData* pE = getNeighbor(x + 1, y);
                const PlotData* pS = getNeighbor(x, y - 1);
                const PlotData* pW = getNeighbor(x - 1, y);
                
                int keys[5];
                keys[0] = myKey;
                keys[1] = (pN && pN->visibility > 0) ? getTerrainKey(*pN) : myKey;
                keys[2] = (pE && pE->visibility > 0) ? getTerrainKey(*pE) : myKey;
                keys[3] = (pS && pS->visibility > 0) ? getTerrainKey(*pS) : myKey;
                keys[4] = (pW && pW->visibility > 0) ? getTerrainKey(*pW) : myKey;
                
                struct BlendEntry { int key; int order; };
                BlendEntry blends[5];
                int numBlends = 0;
                for (int i = 0; i < 5; i++) {
                    bool found = false;
                    for (int j = 0; j < numBlends; j++) {
                        if (blends[j].key == keys[i]) { found = true; break; }
                    }
                    if (!found) {
                        blends[numBlends++] = {keys[i], layerForKey(keys[i])};
                    }
                }
                
                for (int i = 1; i < numBlends; i++) {
                    auto tmp = blends[i];
                    int j = i - 1;
                    while (j >= 0 && blends[j].order > tmp.order) {
                        blends[j + 1] = blends[j]; j--;
                    }
                    blends[j + 1] = tmp;
                }
                
                for (int b = 0; b < numBlends; b++) {
                    int bKey = blends[b].key;
                    bool isWater = (bKey == 5 || bKey == 6);
                    
                    int mask = 0;
                    if (keys[3] == bKey) mask |= 1; // S
                    if (keys[4] == bKey) mask |= 2; // W
                    if (keys[1] == bKey) mask |= 4; // N
                    if (keys[2] == bKey) mask |= 8; // E
                    
                    if (b == 0) mask = 15;
                    
                    GLuint tex = 0;
                    float u0, v0, u1, v1;
                    
                    if (isWater && mask == 15) {
                        tex = m_assets->getWaterSurfaceGL();
                        if (!tex) tex = (bKey == 6) ? m_assets->getOceanWaterGL() : m_assets->getCoastWaterGL();
                        if (tex) {
                            float timeS = SDL_GetTicks() * 0.00004f;
                            u0 = (float)x * 0.5f + timeS;
                            v0 = (float)y * 0.5f + timeS * 0.7f;
                            u1 = u0 + 0.5f;
                            v1 = v0 + 0.5f;
                        }
                    } else {
                        tex = m_assets->getTerrainBlendGL(bKey);
                        if (!tex && isWater) tex = (bKey == 6) ? m_assets->getOceanWaterGL() : m_assets->getCoastWaterGL();
                        
                        if (tex) {
                            int slot = blendMaskToAtlasSlot[mask];
                            if (mask == 15) {
                                int hash = ((x * 73) ^ (y * 37)) % 17;
                                slot = 15 + hash;
                            }
                            
                            int col = slot % 4;
                            int row = slot / 4;
                            u0 = col * 0.25f;
                            v0 = row * 0.125f;
                            u1 = u0 + 0.25f;
                            v1 = v0 + 0.125f;
                        }
                    }
                    
                    if (!tex) continue;
                    
                    if (tex != curBoundTex) {
                        flushBatch();
                        glBindTexture(GL_TEXTURE_2D, tex);
                        curBoundTex = tex;
                    }
                    
                    float tR = landBright, tG = landBright, tB = landBright;
                    if (isWater && mask == 15) {
                        tR = ((bKey == 6) ? 0.75f : 0.85f) * fogBright;
                        tG = ((bKey == 6) ? 0.80f : 0.92f) * fogBright;
                        tB = ((bKey == 6) ? 0.95f : 1.00f) * fogBright;
                    }
                    
                    pushQuad(screenTX, screenTY, screenTileSize, screenTileSize,
                             tR, tG, tB, 1.0f, tex, u0, v0, u1, v1);
                }
            }
        }
    } else {
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        m_shader.setInt("uUseTexture", 0);
        curBoundTex = m_whiteTexture;
        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                int fr = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                int y = snapshot.height - 1 - fr;
                if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;

                const PlotData& plot = snapshot.getPlot(x, y);
                if (plot.visibility == 0) continue;

                float worldTX = vc * TILE_SIZE;
                float worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;

                if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                    screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                float fogBright = (plot.visibility == 1) ? 0.45f : 1.0f;
                TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
                pushQuad(screenTX, screenTY, screenTileSize, screenTileSize,
                         tc.r / 255.0f * fogBright, tc.g / 255.0f * fogBright,
                         tc.b / 255.0f * fogBright, 1.0f);
            }
        }
    }
    flushBatch();

    // --- Pass 1b: Feature texture overlays ---
    // Uses REAL art assets from FPK — NEVER button icons.
    // 0=Ice (icepack_1024.dds), 1=Jungle (JungleBlend.dds), 2=Oasis (oasis_water.dds),
    // 3=FloodPlains (floodplain_baseall.dds), 4=Forest (ForestBlend.dds), 5=Fallout (fallout2.dds)
    if (m_assets) {
        curBoundTex = 0;
        beginBatch();
        m_shader.setInt("uUseTexture", 1);

        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                int fr2 = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                int y = snapshot.height - 1 - fr2;
                if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;
                const PlotData& plot = snapshot.getPlot(x, y);
                if (plot.visibility == 0) continue;
                if (plot.featureType < 0) continue;

                float worldTX = vc * TILE_SIZE;
                float worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                    screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                // All feature textures come from getFeatureBlendGL (loaded from real FPK art)
                GLuint ftex = m_assets->getFeatureBlendGL(plot.featureType);
                if (!ftex) continue; // skip if no real texture available

                if (ftex != curBoundTex) {
                    flushBatch();
                    glBindTexture(GL_TEXTURE_2D, ftex);
                    curBoundTex = ftex;
                }

                // Per-feature rendering parameters
                float alpha, tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
                float u0, v0, u1, v1;

                switch (plot.featureType) {
                    case 0: // Ice: 512x512 seamless texture, tile it
                        alpha = 0.70f;
                        tintR = 0.85f; tintG = 0.90f; tintB = 1.0f;
                        u0 = (float)x * 0.5f; v0 = (float)y * 0.5f;
                        u1 = u0 + 0.5f; v1 = v0 + 0.5f;
                        break;
                    case 4: // Forest: trees_1024.dds tree canopy atlas
                        alpha = 0.85f;
                        tintR = 0.55f; tintG = 0.75f; tintB = 0.40f; // green forest tint
                        {
                          // 8x8 grid. Use columns 0-3 for variety, rows 0-7 for density/shape
                          int col = (x * 7 + y * 13) % 4;
                          int row = (x * 11 + y * 17) % 8;
                          u0 = col * 0.125f; v0 = row * 0.125f;
                          u1 = u0 + 0.125f; v1 = v0 + 0.125f;
                        }
                        break;
                    case 1: // Jungle: trees_1024.dds with darker tropical tint
                        alpha = 0.90f;
                        tintR = 0.35f; tintG = 0.60f; tintB = 0.25f; // darker jungle tint
                        {
                          // Jungle often uses columns 4-7
                          int col = 4 + ((x * 7 + y * 13) % 4);
                          int row = (x * 11 + y * 17) % 8;
                          u0 = col * 0.125f; v0 = row * 0.125f;
                          u1 = u0 + 0.125f; v1 = v0 + 0.125f;
                        }
                        break;
                    case 2: // Oasis: small 128x128 water texture, stretch to tile
                        alpha = 0.80f;
                        u0 = 0; v0 = 0; u1 = 1; v1 = 1;
                        break;
                    case 3: // Flood Plains: 512x1024 texture, tile it
                        alpha = 0.55f;
                        tintR = 0.85f; tintG = 0.95f; tintB = 0.75f;
                        u0 = (float)x * 0.5f; v0 = (float)y * 0.25f;
                        u1 = u0 + 0.5f; v1 = v0 + 0.25f;
                        break;
                    case 5: // Fallout: small 128x128 texture, tile it densely
                        alpha = 0.50f;
                        tintR = 0.75f; tintG = 0.70f; tintB = 0.55f;
                        u0 = (float)x; v0 = (float)y;
                        u1 = u0 + 1.0f; v1 = v0 + 1.0f;
                        break;
                    default:
                        alpha = 0.60f;
                        u0 = 0; v0 = 0; u1 = 1; v1 = 1;
                        break;
                }

                pushQuad(screenTX, screenTY, screenTileSize, screenTileSize,
                         tintR, tintG, tintB, alpha, ftex, u0, v0, u1, v1);
            }
        }
        flushBatch();
    }

    // --- Pass 1b2: Detail texture overlays (visible when zoomed in) ---
    // Fine-scale terrain detail that fades in at higher zoom levels
    if (m_assets && screenTileSize >= 20.0f) {
        float detailAlpha = std::min((screenTileSize - 20.0f) / 60.0f * 0.3f, 0.3f);
        if (detailAlpha > 0.01f) {
            curBoundTex = 0;
            beginBatch();
            m_shader.setInt("uUseTexture", 1);

            for (int vr = rowStart; vr <= rowEnd; vr++) {
                for (int vc = colStart; vc <= colEnd; vc++) {
                    int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                    int frd = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                    int y = snapshot.height - 1 - frd;
                    if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;
                    const PlotData& plot = snapshot.getPlot(x, y);
                    if (plot.visibility == 0) continue;
                    bool isWater = (plot.terrainType == 5 || plot.terrainType == 6);
                    if (isWater) continue; // water has its own detail via UV animation

                    GLuint dtex = m_assets->getTerrainDetailGL(plot.terrainType);
                    if (!dtex) continue;

                    float worldTX = vc * TILE_SIZE;
                    float worldTY = vr * TILE_SIZE;
                    float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                    float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                    if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                        screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                    if (dtex != curBoundTex) {
                        flushBatch();
                        glBindTexture(GL_TEXTURE_2D, dtex);
                        curBoundTex = dtex;
                    }

                    // High-frequency tiling for fine detail
                    float detailTiles = 8.0f;
                    float du0 = (float)x * detailTiles;
                    float dv0 = (float)y * detailTiles;
                    float du1 = du0 + detailTiles;
                    float dv1 = dv0 + detailTiles;

                    pushQuad(screenTX, screenTY, screenTileSize, screenTileSize,
                             1.0f, 1.0f, 1.0f, detailAlpha, dtex, du0, dv0, du1, dv1);
                }
            }
            flushBatch();
        }
    }

    // --- Pass 1c: Rivers (textured water strips) ---
    {
        GLuint riverTex = m_assets ? m_assets->getWaterSurfaceGL() : 0;
        if (riverTex && screenTileSize >= 4) {
            beginBatch();
            glBindTexture(GL_TEXTURE_2D, riverTex);
            m_shader.setInt("uUseTexture", 1);

            float timeS = SDL_GetTicks() * 0.00005f; // gentle flow animation

            for (int vr = rowStart; vr <= rowEnd; vr++) {
                for (int vc = colStart; vc <= colEnd; vc++) {
                    int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                    int fr2 = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                    int y = snapshot.height - 1 - fr2;
                    if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;
                    const PlotData& plot = snapshot.getPlot(x, y);
                    if (plot.visibility == 0) continue;
                    if (!plot.isNOfRiver && !plot.isWOfRiver) continue;

                    float worldTX = vc * TILE_SIZE;
                    float worldTY = vr * TILE_SIZE;
                    float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                    float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                    if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                        screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                    float riverThk = std::max(4.0f, screenTileSize * 0.25f);

                    if (plot.isNOfRiver) {
                        // Horizontal river strip along north edge
                        float ru0 = (float)x + timeS;
                        float rv0 = 0.0f;
                        float ru1 = ru0 + 1.0f;
                        float rv1 = 1.0f;
                        pushQuad(screenTX, screenTY - riverThk * 0.5f, screenTileSize, riverThk,
                                 0.45f, 0.65f, 0.92f, 0.9f, riverTex, ru0, rv0, ru1, rv1);
                    }
                    if (plot.isWOfRiver) {
                        // Vertical river strip along west edge
                        float ru0 = 0.0f;
                        float rv0 = (float)y + timeS;
                        float ru1 = 1.0f;
                        float rv1 = rv0 + 1.0f;
                        pushQuad(screenTX - riverThk * 0.5f, screenTY, riverThk, screenTileSize,
                                 0.45f, 0.65f, 0.92f, 0.9f, riverTex, ru0, rv0, ru1, rv1);
                    }
                }
            }
            flushBatch();
        }
    }

    // --- Pass 2: Overlays (colored, non-textured) ---
    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);

    for (int vr = rowStart; vr <= rowEnd; vr++) {
        for (int vc = colStart; vc <= colEnd; vc++) {
            int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
            int fr = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
            int y = snapshot.height - 1 - fr;
            if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;

            const PlotData& plot = snapshot.getPlot(x, y);

            // Skip overlays on unseen tiles
            if (plot.visibility == 0) continue;

            float worldTX = vc * TILE_SIZE;
            float worldTY = vr * TILE_SIZE;
            float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
            float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;

            if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

            // Territory overlay (dimmed in fog)
            if (plot.ownerID >= 0) {
                float fogAlpha = 0.18f;
                pushQuad(screenTX, screenTY, screenTileSize, screenTileSize,
                         plot.ownerColorR / 255.0f, plot.ownerColorG / 255.0f, plot.ownerColorB / 255.0f, fogAlpha);
            }

            // Hills: brownish shadow overlay for depth
            if (plot.plotType == 1) {
                pushQuad(screenTX, screenTY, screenTileSize, screenTileSize, 0.25f, 0.18f, 0.08f, 0.25f);
            }
            // Peaks: cold-tinted snow-cap brightening
            if (plot.plotType == 0) {
                pushQuad(screenTX, screenTY, screenTileSize, screenTileSize, 0.85f, 0.88f, 1.0f, 0.25f);
            }

            // Grid
            if (m_showGrid && screenTileSize >= 4) {
                float lw = 1.0f;
                pushQuad(screenTX, screenTY, screenTileSize, lw, 0.16f, 0.16f, 0.2f, 1);
                pushQuad(screenTX, screenTY, lw, screenTileSize, 0.16f, 0.16f, 0.2f, 1);
            }

            // Rivers are now rendered in Pass 1c (textured water strips)

            // Territory borders
            if (plot.ownerID >= 0 && screenTileSize >= 3) {
                float borderThk = std::max(1.0f, screenTileSize * 0.08f);
                float cr = plot.ownerColorR / 255.0f, cg = plot.ownerColorG / 255.0f, cb = plot.ownerColorB / 255.0f;
                int nDx[4] = {0, +1, 0, -1}, nDy[4] = {+1, 0, -1, 0};
                for (int e = 0; e < 4; e++) {
                    int nx = x + nDx[e], ny = y + nDy[e];
                    if (snapshot.wrapX) nx = ((nx % snapshot.width) + snapshot.width) % snapshot.width;
                    if (snapshot.wrapY) ny = ((ny % snapshot.height) + snapshot.height) % snapshot.height;
                    bool drawEdge = false;
                    if (nx < 0 || nx >= snapshot.width || ny < 0 || ny >= snapshot.height) drawEdge = true;
                    else if (snapshot.getPlot(nx, ny).ownerID != plot.ownerID) drawEdge = true;
                    if (drawEdge) {
                        switch (e) {
                            case 0: pushQuad(screenTX, screenTY, screenTileSize, borderThk, cr, cg, cb, 1); break;
                            case 1: pushQuad(screenTX + screenTileSize - borderThk, screenTY, borderThk, screenTileSize, cr, cg, cb, 1); break;
                            case 2: pushQuad(screenTX, screenTY + screenTileSize - borderThk, screenTileSize, borderThk, cr, cg, cb, 1); break;
                            case 3: pushQuad(screenTX, screenTY, borderThk, screenTileSize, cr, cg, cb, 1); break;
                        }
                    }
                }
            }

            // Feature overlays now handled in Pass 1b (textured)

            // City marker (only on currently visible tiles)
            if (plot.isCity && plot.visibility == 2 && screenTileSize >= 3) {
                float inset = screenTileSize * 0.3f;
                pushQuad(screenTX + inset, screenTY + inset, screenTileSize - inset * 2, screenTileSize - inset * 2,
                         plot.ownerColorR / 255.0f, plot.ownerColorG / 255.0f, plot.ownerColorB / 255.0f, 1.0f);
            }

            // Unit indicator (only on currently visible tiles)
            if (plot.unitCount > 0 && !plot.isCity && plot.visibility == 2 && screenTileSize >= 4) {
                float sz = std::max(2.0f, screenTileSize * 0.25f);
                float cx = screenTX + screenTileSize * 0.5f;
                float cy = screenTY + screenTileSize * 0.5f;
                float ur, ug, ub;
                if (plot.ownerID >= 0) {
                    ur = plot.ownerColorR / 255.0f; ug = plot.ownerColorG / 255.0f; ub = plot.ownerColorB / 255.0f;
                } else {
                    ur = 0.86f; ug = 0.86f; ub = 0.86f;
                }
                pushQuad(cx - sz / 2, cy - sz / 2, sz, sz, ur, ug, ub, 1);

                // Health bar
                if (plot.firstUnitHP < 100 && plot.firstUnitHP > 0 && screenTileSize >= 8) {
                    float barW = screenTileSize * 0.6f;
                    float barH = std::max(2.0f, screenTileSize * 0.08f);
                    float barX = screenTX + screenTileSize * 0.2f;
                    float barY = screenTY + screenTileSize - barH - 1;
                    pushQuad(barX, barY, barW, barH, 0.31f, 0, 0, 0.78f);
                    float hpW = (barW * plot.firstUnitHP) / 100.0f;
                    float gr = plot.firstUnitHP > 50 ? 0.0f : 0.78f;
                    float gg = plot.firstUnitHP > 50 ? 0.78f : (plot.firstUnitHP > 25 ? 0.78f : 0.0f);
                    pushQuad(barX, barY, hpW, barH, gr, gg, 0, 1);
                }
            }

            // Selection highlight
            if (x == snapshot.selectedUnitX && y == snapshot.selectedUnitY && snapshot.selectedUnitID >= 0)
                drawSelectionHighlight(screenTX, screenTY, screenTileSize);
        }
    }
    flushBatch();

    // --- Pass 2: City name labels ---
    if (isFontValid(m_fontSmall) && screenTileSize >= 8) {
        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                int x2 = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                int fr2 = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                int y2 = snapshot.height - 1 - fr2;
                if (x2 < 0 || x2 >= snapshot.width || y2 < 0 || y2 >= snapshot.height) continue;
                const PlotData& plot = snapshot.getPlot(x2, y2);
                if (!plot.isCity || plot.cityName.empty() || plot.visibility < 1) continue;

                float worldTX = vc * TILE_SIZE, worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                float screenCX = screenTX + screenTileSize * 0.5f;
                if (screenCX < -100 || screenCX > m_windowW + 100) continue;

                float labelY = screenTY + screenTileSize + 2;
                char label[128];
                snprintf(label, sizeof(label), "%s (%d)", plot.cityName.c_str(), plot.cityPopulation);
                int tw = 0, th = 0;
                TTF_SizeText(m_fontSmall, label, &tw, &th);

                // Background
                beginBatch();
                glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
                m_shader.setInt("uUseTexture", 0);
                pushQuad(screenCX - tw / 2.0f - 2, labelY - 1, (float)(tw + 4), (float)(th + 2), 0, 0, 0, 0.7f);
                flushBatch();

                drawTextCentered(label, (int)screenCX, (int)(labelY + th / 2.0f), m_fontSmall, 255, 255, 255);
            }
        }
    }
}

// ---- HUD ----

void GLRenderer::drawHUD(MapSnapshot& snapshot) {
    if (!isFontValid(m_fontMedium)) return;

    if (snapshot.humanPlayerID < 0 || snapshot.humanPlayerID >= 18)
        return;

    const PlayerInfo& pi = snapshot.players[snapshot.humanPlayerID];
    char buf[512];
    if (pi.currentResearch >= 0) {
        snprintf(buf, sizeof(buf), "Turn %d | %d AD | Gold: %d (%+d) | Research: %s (%d turns)",
                 snapshot.gameTurn, snapshot.gameYear,
                 pi.gold, pi.goldRate,
                 pi.currentResearchName.c_str(), pi.researchTurns);
    } else {
        snprintf(buf, sizeof(buf), "Turn %d | %d AD | Gold: %d (%+d) | No Research (F6)",
                 snapshot.gameTurn, snapshot.gameYear, pi.gold, pi.goldRate);
    }
    int tw = 0, th = 0;
    TTF_SizeText(m_fontMedium, buf, &tw, &th);

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad(0, 0, (float)(tw + 16), (float)(th + 8), 0, 0, 0, 0.78f);
    flushBatch();
    drawText(buf, 8, 4, m_fontMedium, 200, 220, 200);

    // Bottom-left: zoom + FPS
    float fps = (m_lastFrameTime > 0) ? (1000.0f / std::max(1u, SDL_GetTicks() - m_lastFrameTime + 1)) : 0;
    snprintf(buf, sizeof(buf), "Zoom: %.1fx | FPS: %.0f | Map %dx%d | OpenGL",
             m_camera.zoom, fps, snapshot.width, snapshot.height);
    TTF_SizeText(m_fontMedium, buf, &tw, &th);
    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad(0, (float)(m_windowH - th - 8), (float)(tw + 16), (float)(th + 8), 0, 0, 0, 0.78f);
    flushBatch();
    drawText(buf, 8, m_windowH - th - 4, m_fontMedium, 160, 180, 160);
}

// ---- Turn banner ----

void GLRenderer::drawTurnBanner(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontLarge)) return;

    const char* msg = nullptr;
    uint8_t r = 255, g = 255, b = 200;

    if (snapshot.waitingForEndTurn) {
        msg = "YOUR TURN - Press Enter to End Turn";
        r = 255; g = 255; b = 100;
    } else if (!snapshot.isHumanTurn && snapshot.gameTurn > 0) {
        msg = "AI thinking...";
        r = 180; g = 180; b = 180;
    }

    if (msg) {
        int tw = 0, th = 0;
        TTF_SizeText(m_fontLarge, msg, &tw, &th);
        int bx = (m_windowW - tw) / 2 - 12;
        int by = 28;
        beginBatch();
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        m_shader.setInt("uUseTexture", 0);
        pushQuad((float)bx, (float)by, (float)(tw + 24), (float)(th + 8), 0, 0, 0, 0.78f);
        flushBatch();
        drawTextCentered(msg, m_windowW / 2, by + th / 2 + 4, m_fontLarge, r, g, b);
    }
}

// ---- Game messages ----

void GLRenderer::drawGameMessages(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || snapshot.gameMessages.empty()) return;

    int startY = 56;
    int lineH = 16;
    int maxMsg = std::min((int)snapshot.gameMessages.size(), 6);
    int startIdx = (int)snapshot.gameMessages.size() - maxMsg;

    for (int i = startIdx; i < (int)snapshot.gameMessages.size(); i++) {
        int y = startY + (i - startIdx) * lineH;
        int tw = 0, th = 0;
        TTF_SizeText(m_fontSmall, snapshot.gameMessages[i].c_str(), &tw, &th);
        beginBatch();
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        m_shader.setInt("uUseTexture", 0);
        pushQuad((float)(m_windowW / 2 - tw / 2 - 4), (float)y, (float)(tw + 8), (float)(th + 2), 0, 0, 0, 0.63f);
        flushBatch();
        drawTextCentered(snapshot.gameMessages[i], m_windowW / 2, y + th / 2, m_fontSmall, 220, 220, 180);
    }
}

// ---- Unit panel ----

void GLRenderer::drawUnitPanel(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || snapshot.selectedUnitID < 0) return;
    if (snapshot.selectedUnitX < 0 || snapshot.selectedUnitY < 0) return;
    const PlotData& pd = snapshot.getPlot(snapshot.selectedUnitX, snapshot.selectedUnitY);

    int panelW = 220, panelH = 100;
    int panelX = m_windowW - panelW - 10;
    int panelY = 10;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)panelX, (float)panelY, (float)panelW, (float)panelH, 0.04f, 0.04f, 0.12f, 0.86f);
    // Border
    pushQuad((float)panelX, (float)panelY, (float)panelW, 1, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)panelX, (float)(panelY + panelH), (float)panelW, 1, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)panelX, (float)panelY, 1, (float)panelH, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)(panelX + panelW), (float)panelY, 1, (float)panelH, 0.31f, 0.39f, 0.31f, 1);
    flushBatch();

    int y = panelY + 6;
    drawText(pd.firstUnitName, panelX + 8, y, m_fontMedium, 255, 255, 200);
    y += 20;

    char buf[128];
    snprintf(buf, sizeof(buf), "HP: %d%%  Str: %.1f", pd.firstUnitHP, pd.firstUnitStrength / 100.0f);
    drawText(buf, panelX + 8, y, m_fontSmall, 200, 210, 200);
    y += 16;

    snprintf(buf, sizeof(buf), "Moves: %d/%d", pd.firstUnitMoves / 60, pd.firstUnitMaxMoves / 60);
    drawText(buf, panelX + 8, y, m_fontSmall, 200, 210, 200);
    y += 16;

    snprintf(buf, sizeof(buf), "At: (%d, %d)", snapshot.selectedUnitX, snapshot.selectedUnitY);
    drawText(buf, panelX + 8, y, m_fontSmall, 160, 170, 160);
    y += 16;

    std::string actions;
    if (pd.firstUnitCanFound) actions += "[B]uild City  ";
    actions += "[F]ortify  [S]leep";
    drawText(actions, panelX + 8, y, m_fontSmall, 180, 200, 180);
}

// ---- City panel ----

void GLRenderer::drawCityPanel(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || !snapshot.cityScreenOpen) return;

    const CityDetail& cd = snapshot.selectedCity;
    int panelW = 300;
    int lineH = 16;
    int headerH = 90;
    int itemCount = (int)cd.availableProduction.size();
    int maxVisible = 20;
    int panelH = headerH + std::min(itemCount, maxVisible) * lineH + 20;
    int panelX = m_windowW - panelW - 10;
    int panelY = 10;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)panelX, (float)panelY, (float)panelW, (float)panelH, 0.04f, 0.04f, 0.12f, 0.9f);
    pushQuad((float)panelX, (float)panelY, (float)panelW, 1, 0.31f, 0.39f, 0.47f, 1);
    pushQuad((float)panelX, (float)(panelY + panelH), (float)panelW, 1, 0.31f, 0.39f, 0.47f, 1);
    pushQuad((float)panelX, (float)panelY, 1, (float)panelH, 0.31f, 0.39f, 0.47f, 1);
    pushQuad((float)(panelX + panelW), (float)panelY, 1, (float)panelH, 0.31f, 0.39f, 0.47f, 1);
    flushBatch();

    int y = panelY + 6;
    drawText(cd.name + " (Pop " + std::to_string(cd.population) + ")", panelX + 8, y, m_fontMedium, 255, 255, 200);
    y += 20;

    char buf[128];
    snprintf(buf, sizeof(buf), "Food: %d/%d (%+d)", cd.foodStored, cd.foodNeeded, cd.foodRate);
    drawText(buf, panelX + 8, y, m_fontSmall, 180, 220, 180);
    y += lineH;

    snprintf(buf, sizeof(buf), "Production: %d/%d (%+d)", cd.productionStored, cd.productionNeeded, cd.productionRate);
    drawText(buf, panelX + 8, y, m_fontSmall, 220, 180, 140);
    y += lineH;

    snprintf(buf, sizeof(buf), "Building: %s (%d turns)", cd.currentProduction.c_str(), cd.productionTurns);
    drawText(buf, panelX + 8, y, m_fontSmall, 200, 200, 220);
    y += lineH + 4;

    drawText("-- Choose Production (click or 1-9) --", panelX + 8, y, m_fontSmall, 180, 180, 160);
    y += lineH + 2;

    int visStart = m_prodScrollOffset;
    int visEnd = std::min(visStart + maxVisible, itemCount);
    for (int i = visStart; i < visEnd; i++) {
        const ProductionItem& item = cd.availableProduction[i];
        char line[128];
        int num = i - visStart + 1;
        snprintf(line, sizeof(line), "%s%d. %s (%d turns) %s",
                 num <= 9 ? "" : "", num <= 9 ? num : 0,
                 item.name.c_str(), item.turns, item.isUnit ? "[U]" : "[B]");
        uint8_t cr = item.isUnit ? 200 : 180;
        uint8_t cg = item.isUnit ? 220 : 200;
        uint8_t cb = item.isUnit ? 200 : 220;
        drawText(line, panelX + 8, y, m_fontSmall, cr, cg, cb);
        y += lineH;
    }
    drawText("[Esc] Close city", panelX + 8, y + 4, m_fontSmall, 140, 140, 140);
}

// ---- Tech picker ----

void GLRenderer::drawTechPicker(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || snapshot.availableTechs.empty()) return;

    int panelW = 280;
    int lineH = 18;
    int maxVisible = 15;
    int itemCount = (int)snapshot.availableTechs.size();
    int panelH = 30 + std::min(itemCount, maxVisible) * lineH + 10;
    int panelX = (m_windowW - panelW) / 2;
    int panelY = (m_windowH - panelH) / 2;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)panelX, (float)panelY, (float)panelW, (float)panelH, 0.04f, 0.06f, 0.14f, 0.94f);
    pushQuad((float)panelX, (float)panelY, (float)panelW, 1, 0.24f, 0.31f, 0.55f, 1);
    pushQuad((float)panelX, (float)(panelY + panelH), (float)panelW, 1, 0.24f, 0.31f, 0.55f, 1);
    pushQuad((float)panelX, (float)panelY, 1, (float)panelH, 0.24f, 0.31f, 0.55f, 1);
    pushQuad((float)(panelX + panelW), (float)panelY, 1, (float)panelH, 0.24f, 0.31f, 0.55f, 1);
    flushBatch();

    int y = panelY + 6;
    drawText("Choose Research (click or 1-9, Esc to close)", panelX + 8, y, m_fontMedium, 200, 220, 255);
    y += 22;

    int visStart = m_techScrollOffset;
    int visEnd = std::min(visStart + maxVisible, itemCount);
    for (int i = visStart; i < visEnd; i++) {
        const TechItem& ti = snapshot.availableTechs[i];
        char line[128];
        int num = i - visStart + 1;
        snprintf(line, sizeof(line), "%d. %s (%d turns)", num <= 9 ? num : 0, ti.name.c_str(), ti.turnsLeft);
        drawText(line, panelX + 12, y, m_fontSmall, 200, 220, 240);
        y += lineH;
    }
}

// ---- Action bar ----

void GLRenderer::drawActionBar(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || !snapshot.isHumanTurn) return;

    int barH = 24;
    int barY = m_windowH - barH - 30;
    std::string actions = "Enter=End Turn  Tab=Next Unit  F6=Research  H=Help";
    if (snapshot.selectedUnitID >= 0) {
        actions = "Right-click=Move  B=Found  F=Fortify  S=Sleep  Space=Skip  " + actions;
    }
    int tw = 0, th = 0;
    TTF_SizeText(m_fontSmall, actions.c_str(), &tw, &th);
    int barX = (m_windowW - tw) / 2 - 8;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)barX, (float)barY, (float)(tw + 16), (float)barH, 0, 0, 0, 0.7f);
    flushBatch();
    drawTextCentered(actions, m_windowW / 2, barY + barH / 2, m_fontSmall, 180, 200, 180);
}

// ---- Minimap ----

void GLRenderer::drawMinimap(const MapSnapshot& snapshot) {
    int mmMaxW = 200, mmMaxH = 140, margin = 10;
    float scaleX = (float)mmMaxW / snapshot.width;
    float scaleY = (float)mmMaxH / snapshot.height;
    float scale = std::min(scaleX, scaleY);
    int mmW = (int)(snapshot.width * scale);
    int mmH = (int)(snapshot.height * scale);
    int mmX = m_windowW - mmW - margin;
    int mmY = m_windowH - mmH - margin;
    m_mmX = mmX; m_mmY = mmY; m_mmW = mmW; m_mmH = mmH;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);

    // Background
    pushQuad((float)(mmX - 2), (float)(mmY - 2), (float)(mmW + 4), (float)(mmH + 4), 0, 0, 0, 0.78f);

    // Draw each tile as a tiny colored quad
    for (int y = 0; y < snapshot.height; y++) {
        for (int x = 0; x < snapshot.width; x++) {
            const PlotData& plot = snapshot.getPlot(x, y);
            TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
            float r = tc.r / 255.0f, g = tc.g / 255.0f, b = tc.b / 255.0f;
            if (plot.isCity) {
                r = plot.ownerColorR / 255.0f; g = plot.ownerColorG / 255.0f; b = plot.ownerColorB / 255.0f;
            }
            int flippedY = snapshot.height - 1 - y;
            float px = mmX + x * scale, py = mmY + flippedY * scale;
            float pw = std::max(1.0f, scale), ph = std::max(1.0f, scale);
            pushQuad(px, py, pw, ph, r, g, b, 1);
        }
    }

    // Viewport rectangle (4 edge lines)
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;
    auto clamp01 = [](float v) { return std::max(0.0f, std::min(v, 1.0f)); };
    float fracLeft = clamp01(m_camera.offsetX / mapPixelW);
    float fracRight = clamp01((m_camera.offsetX + m_windowW / m_camera.zoom) / mapPixelW);
    float fracTop = clamp01(m_camera.offsetY / mapPixelH);
    float fracBot = clamp01((m_camera.offsetY + m_windowH / m_camera.zoom) / mapPixelH);

    float vpX = mmX + fracLeft * mmW, vpY = mmY + fracTop * mmH;
    float vpW = (fracRight - fracLeft) * mmW, vpH = (fracBot - fracTop) * mmH;
    if (vpW > 0 && vpH > 0) {
        pushQuad(vpX, vpY, vpW, 1, 1, 1, 1, 1);
        pushQuad(vpX, vpY + vpH, vpW, 1, 1, 1, 1, 1);
        pushQuad(vpX, vpY, 1, vpH, 1, 1, 1, 1);
        pushQuad(vpX + vpW, vpY, 1, vpH, 1, 1, 1, 1);
    }

    flushBatch();
}

// ---- Help overlay ----

void GLRenderer::drawHelpOverlay() {
    if (!isFontValid(m_fontMedium)) return;
    const char* lines[] = {
        "Controls:",
        "  WASD/Arrows    - Pan camera",
        "  Mouse wheel    - Zoom in/out",
        "  Right-drag     - Pan camera",
        "  Middle-drag    - Rotate/tilt camera",
        "  PgUp/PgDn      - Tilt camera",
        "  Q/E            - Rotate camera",
        "  C              - Center on selected unit",
        "  Left-click     - Select unit/city",
        "  Right-click    - Move unit / orders",
        "  Enter          - End turn",
        "  Tab            - Next unit needing orders",
        "  Space          - Skip current unit",
        "  B              - Found city (settler)",
        "  F              - Fortify unit",
        "  S              - Sleep unit",
        "  F6             - Tech research picker",
        "  1-9            - Pick production/tech",
        "  F / Home       - Fit map to window",
        "  P              - Toggle player panel",
        "  G              - Toggle grid lines",
        "  M              - Toggle minimap",
        "  H              - Toggle this help",
        "  Escape         - Close panel / Quit",
    };
    int numLines = sizeof(lines) / sizeof(lines[0]);
    int lineH = 20, padX = 16, padY = 12;
    int maxW = 0;
    for (int i = 0; i < numLines; i++) {
        int tw = 0, th = 0;
        TTF_SizeText(m_fontMedium, lines[i], &tw, &th);
        maxW = std::max(maxW, tw);
    }
    int boxW = maxW + padX * 2, boxH = numLines * lineH + padY * 2;
    int boxX = (m_windowW - boxW) / 2, boxY = (m_windowH - boxH) / 2;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)boxX, (float)boxY, (float)boxW, (float)boxH, 0, 0, 0, 0.86f);
    pushQuad((float)boxX, (float)boxY, (float)boxW, 1, 0.39f, 0.47f, 0.39f, 1);
    pushQuad((float)boxX, (float)(boxY + boxH), (float)boxW, 1, 0.39f, 0.47f, 0.39f, 1);
    pushQuad((float)boxX, (float)boxY, 1, (float)boxH, 0.39f, 0.47f, 0.39f, 1);
    pushQuad((float)(boxX + boxW), (float)boxY, 1, (float)boxH, 0.39f, 0.47f, 0.39f, 1);
    flushBatch();

    for (int i = 0; i < numLines; i++) {
        uint8_t r = (i == 0) ? 255 : 200, g = (i == 0) ? 255 : 220, b = (i == 0) ? 200 : 200;
        drawText(lines[i], boxX + padX, boxY + padY + i * lineH, m_fontMedium, r, g, b);
    }
}

// ---- Tooltip ----

void GLRenderer::drawTooltip(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall) || snapshot.width == 0) return;
    int tileX, tileY;
    if (!screenToTile(m_mouseX, m_mouseY, snapshot, tileX, tileY)) return;

    const PlotData& plot = snapshot.getPlot(tileX, tileY);
    static const char* terrainNames[] = {"Grass","Plains","Desert","Tundra","Snow","Coast","Ocean","Peak","Hill"};
    static const char* plotNames[] = {"Peak","Hills","Land","Ocean"};
    const char* tName = (plot.terrainType >= 0 && plot.terrainType <= 8) ? terrainNames[plot.terrainType] : "???";
    const char* pName = (plot.plotType >= 0 && plot.plotType <= 3) ? plotNames[plot.plotType] : "???";

    char line1[128], line2[128];
    snprintf(line1, sizeof(line1), "(%d, %d) %s %s", tileX, tileY, tName, pName);
    line2[0] = '\0';

    if (plot.unitCount > 0 && !plot.firstUnitName.empty()) {
        snprintf(line2, sizeof(line2), "%s (HP:%d%% Str:%.0f)",
                 plot.firstUnitName.c_str(), plot.firstUnitHP, plot.firstUnitStrength / 100.0f);
        if (plot.unitCount > 1) {
            char tmp[32]; snprintf(tmp, sizeof(tmp), " +%d more", plot.unitCount - 1);
            strncat(line2, tmp, sizeof(line2) - strlen(line2) - 1);
        }
    } else if (plot.isCity) {
        snprintf(line2, sizeof(line2), "%s (pop %d)", plot.cityName.c_str(), plot.cityPopulation);
    }

    int tipX = m_mouseX + 16, tipY = m_mouseY + 4;
    int tw1 = 0, th1 = 0, tw2 = 0, th2 = 0;
    TTF_SizeText(m_fontSmall, line1, &tw1, &th1);
    if (line2[0]) TTF_SizeText(m_fontSmall, line2, &tw2, &th2);
    int boxW = std::max(tw1, tw2) + 8;
    int boxH = th1 + (line2[0] ? th2 + 2 : 0) + 6;
    if (tipX + boxW > m_windowW) tipX = m_mouseX - boxW - 8;
    if (tipY + boxH > m_windowH) tipY = m_mouseY - boxH - 4;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)tipX, (float)tipY, (float)boxW, (float)boxH, 0.08f, 0.08f, 0.12f, 0.86f);
    pushQuad((float)tipX, (float)tipY, (float)boxW, 1, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)tipX, (float)(tipY + boxH), (float)boxW, 1, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)tipX, (float)tipY, 1, (float)boxH, 0.31f, 0.39f, 0.31f, 1);
    pushQuad((float)(tipX + boxW), (float)tipY, 1, (float)boxH, 0.31f, 0.39f, 0.31f, 1);
    flushBatch();

    drawText(line1, tipX + 4, tipY + 3, m_fontSmall, 220, 230, 200);
    if (line2[0]) drawText(line2, tipX + 4, tipY + 3 + th1 + 2, m_fontSmall, 180, 200, 180);
}

// ---- Player panel ----

void GLRenderer::drawPlayerPanel(const MapSnapshot& snapshot) {
    if (!isFontValid(m_fontSmall)) return;
    int panelW = 220, lineH = 16, padX = 8, padY = 6;
    int alive = 0;
    for (int p = 0; p < snapshot.numPlayers; p++) if (snapshot.players[p].alive) alive++;
    int panelH = padY * 2 + (alive + 1) * lineH;
    int panelX = 0, panelY = 28;

    beginBatch();
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    m_shader.setInt("uUseTexture", 0);
    pushQuad((float)panelX, (float)panelY, (float)panelW, (float)panelH, 0.04f, 0.04f, 0.08f, 0.82f);
    flushBatch();

    drawText("Player       Cities Pop Score", panelX + padX, panelY + padY, m_fontSmall, 180, 190, 160);
    int row = 1;
    for (int p = 0; p < snapshot.numPlayers; p++) {
        const PlayerInfo& pi = snapshot.players[p];
        if (!pi.alive) continue;
        int ly = panelY + padY + row * lineH;

        // Color swatch
        beginBatch();
        glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
        m_shader.setInt("uUseTexture", 0);
        pushQuad((float)(panelX + padX), (float)(ly + 2), 10, 10,
                 pi.colorR / 255.0f, pi.colorG / 255.0f, pi.colorB / 255.0f, 1);
        flushBatch();

        char buf[128];
        snprintf(buf, sizeof(buf), "%-12s %3d  %3d  %4d%s",
                 pi.civName.c_str(), pi.numCities, pi.totalPop, pi.score,
                 pi.isHuman ? " *" : "");
        drawText(buf, panelX + padX + 14, ly, m_fontSmall, 200, 210, 200);
        row++;
    }
}

// ---- Input handling (identical logic to Renderer) ----

void GLRenderer::handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot) {
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = true;    break;
        case SDLK_DOWN:  case SDLK_s:
            if (key == SDLK_s && m_pushCommand) {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.selectedUnitID >= 0) {
                    GameCommand cmd; cmd.type = GameCommand::SLEEP;
                    cmd.id = snapshot.selectedUnitID;
                    m_pushCommand(cmd);
                    return;
                }
            }
            m_keyDown = true;
            break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = true;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = true; break;
        case SDLK_f:
            if (SDL_GetModState() & KMOD_CTRL) break;
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.selectedUnitID >= 0 && m_pushCommand) {
                    GameCommand cmd; cmd.type = GameCommand::FORTIFY;
                    cmd.id = snapshot.selectedUnitID;
                    m_pushCommand(cmd);
                } else if (snapshot.width > 0) {
                    if (m_use3DTerrain) autoFitCamera3D(snapshot);
                    else autoFitCamera(snapshot);
                }
            }
            break;
        case SDLK_HOME:
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.width > 0) {
                    if (m_use3DTerrain) autoFitCamera3D(snapshot);
                    else autoFitCamera(snapshot);
                }
            }
            break;
        case SDLK_PAGEUP:
            if (m_use3DTerrain)
                m_cam3D.elevation = std::min(m_cam3D.elevation + 0.1f, 1.5f);
            else
                m_camera.tiltAngle = std::min(m_camera.tiltAngle + 0.1f, 1.3f);
            break;
        case SDLK_PAGEDOWN:
            if (m_use3DTerrain)
                m_cam3D.elevation = std::max(m_cam3D.elevation - 0.1f, 0.1f);
            else
                m_camera.tiltAngle = std::max(m_camera.tiltAngle - 0.1f, 0.0f);
            break;
        case SDLK_q:
            if (m_use3DTerrain)
                m_cam3D.azimuth -= 0.15f;
            else
                m_camera.rotationAngle -= 0.15f;
            break;
        case SDLK_e:
            if (m_use3DTerrain)
                m_cam3D.azimuth += 0.15f;
            else
                m_camera.rotationAngle += 0.15f;
            break;
        case SDLK_c:
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.selectedUnitX >= 0 && snapshot.selectedUnitY >= 0) {
                    centerOnTile(snapshot.selectedUnitX, snapshot.selectedUnitY, snapshot.height);
                }
            }
            break;
        case SDLK_b:
            if (m_pushCommand) {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.selectedUnitID >= 0) {
                    GameCommand cmd; cmd.type = GameCommand::FOUND_CITY;
                    cmd.id = snapshot.selectedUnitID;
                    m_pushCommand(cmd);
                }
            }
            break;
        case SDLK_m: m_showMinimap = !m_showMinimap; break;
        case SDLK_h: m_showHelp = !m_showHelp; break;
        case SDLK_p: m_showPlayerPanel = !m_showPlayerPanel; break;
        case SDLK_g: m_showGrid = !m_showGrid; break;
        case SDLK_F6:
            m_showTechPicker = !m_showTechPicker;
            m_techScrollOffset = 0;
            break;
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
        case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
        {
            int idx = (key - SDLK_1);
            std::lock_guard<std::mutex> lock(snapshot.mtx);
            if (m_showTechPicker && m_pushCommand) {
                int techIdx = m_techScrollOffset + idx;
                if (techIdx < (int)snapshot.availableTechs.size()) {
                    GameCommand cmd; cmd.type = GameCommand::SET_RESEARCH;
                    cmd.id = snapshot.availableTechs[techIdx].techID;
                    m_pushCommand(cmd);
                    m_showTechPicker = false;
                }
            } else if (snapshot.cityScreenOpen && m_pushCommand) {
                int prodIdx = m_prodScrollOffset + idx;
                if (prodIdx < (int)snapshot.selectedCity.availableProduction.size()) {
                    const auto& item = snapshot.selectedCity.availableProduction[prodIdx];
                    GameCommand cmd; cmd.type = GameCommand::SET_PRODUCTION;
                    cmd.id = snapshot.selectedCity.cityID;
                    cmd.param = item.type;
                    cmd.x = item.isUnit ? 1 : 0;
                    m_pushCommand(cmd);
                }
            }
            break;
        }
    }
}

void GLRenderer::handleKeyUp(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = false;    break;
        case SDLK_DOWN:               m_keyDown = false;   break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = false;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = false; break;
    }
}

void GLRenderer::handleMouseWheel(int y, int mouseX, int mouseY) {
    if (m_use3DTerrain) {
        // 3D: zoom by changing distance
        if (y > 0) m_cam3D.distance /= 1.15f;
        else if (y < 0) m_cam3D.distance *= 1.15f;
        m_cam3D.distance = std::max(2.0f, std::min(m_cam3D.distance, 150.0f));
    } else {
        float oldZoom = m_camera.zoom;
        if (y > 0) m_camera.zoom *= 1.15f;
        else if (y < 0) m_camera.zoom /= 1.15f;
        m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));
        float worldMouseX = mouseX / oldZoom + m_camera.offsetX;
        float worldMouseY = mouseY / oldZoom + m_camera.offsetY;
        m_camera.offsetX = worldMouseX - mouseX / m_camera.zoom;
        m_camera.offsetY = worldMouseY - mouseY / m_camera.zoom;
    }
}

void GLRenderer::handleMouseMotion(int dx, int dy, bool rightDrag, bool middleDrag) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    m_mouseX = mx;
    m_mouseY = my;
    if (m_use3DTerrain) {
        if (middleDrag) {
            // Middle-mouse drag: orbit camera
            m_cam3D.azimuth += dx * 0.005f;
            m_cam3D.elevation -= dy * 0.005f;
            m_cam3D.elevation = std::max(0.1f, std::min(m_cam3D.elevation, 1.5f));
        } else if (rightDrag) {
            // Right-mouse drag: pan focal point
            float panScale = m_cam3D.distance * 0.002f;
            float cosAz = cosf(m_cam3D.azimuth);
            float sinAz = sinf(m_cam3D.azimuth);
            m_cam3D.focalX -= (cosAz * dx + sinAz * dy) * panScale;
            m_cam3D.focalZ -= (-sinAz * dx + cosAz * dy) * panScale;
        }
    } else {
        if (middleDrag) {
            m_camera.rotationAngle += dx * 0.005f;
            m_camera.tiltAngle -= dy * 0.005f;
            m_camera.tiltAngle = std::max(0.0f, std::min(m_camera.tiltAngle, 1.3f));
        } else if (rightDrag) {
            m_camera.offsetX -= dx / m_camera.zoom;
            m_camera.offsetY -= dy / m_camera.zoom;
        }
    }
}

void GLRenderer::handleMouseClick(int mouseX, int mouseY, int button, MapSnapshot& snapshot) {
    // Minimap click-to-jump
    if (button == SDL_BUTTON_LEFT && m_showMinimap && m_mmW > 0 && m_mmH > 0) {
        if (mouseX >= m_mmX && mouseX < m_mmX + m_mmW &&
            mouseY >= m_mmY && mouseY < m_mmY + m_mmH) {
            std::lock_guard<std::mutex> lock(snapshot.mtx);
            if (snapshot.width > 0) {
                float fracX = (float)(mouseX - m_mmX) / m_mmW;
                float fracY = (float)(mouseY - m_mmY) / m_mmH;
                float mapPixelW = snapshot.width * TILE_SIZE;
                float mapPixelH = snapshot.height * TILE_SIZE;
                m_camera.offsetX = fracX * mapPixelW - (m_windowW / m_camera.zoom) * 0.5f;
                m_camera.offsetY = fracY * mapPixelH - (m_windowH / m_camera.zoom) * 0.5f;
            }
            return;
        }
    }

    int tileX, tileY;
    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        if (!screenToTile(mouseX, mouseY, snapshot, tileX, tileY)) return;
    }

    if (button == SDL_BUTTON_LEFT && m_pushCommand) {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        const PlotData& plot = snapshot.getPlot(tileX, tileY);

        if (plot.hasHumanUnit) {
            GameCommand cmd; cmd.type = GameCommand::SELECT_UNIT;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
            if (snapshot.cityScreenOpen) m_pushCommand({GameCommand::CLOSE_CITY});
        } else if (plot.isCity && plot.ownerID == snapshot.humanPlayerID) {
            GameCommand cmd; cmd.type = GameCommand::SELECT_CITY;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
        } else {
            m_pushCommand({GameCommand::DESELECT});
            if (snapshot.cityScreenOpen) m_pushCommand({GameCommand::CLOSE_CITY});
        }
    } else if (button == SDL_BUTTON_RIGHT && m_pushCommand) {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        if (snapshot.selectedUnitID >= 0) {
            GameCommand cmd; cmd.type = GameCommand::MOVE_UNIT;
            cmd.id = snapshot.selectedUnitID;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
        }
    }
}

void GLRenderer::handleResize(int newW, int newH) {
    m_windowW = newW;
    m_windowH = newH;
}
