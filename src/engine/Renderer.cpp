// Renderer.cpp — 2D hex-grid map renderer
//
// Milestone 3-5: Hex grid, text labels, feature overlays, minimap, HUD.

#include "Renderer.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

// Precomputed cos/sin for flat-top hex vertices at 0°,60°,120°,180°,240°,300°
static const float HEX_COS[6] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f };
static const float HEX_SIN[6] = { 0.0f, 0.866025404f, 0.866025404f, 0.0f, -0.866025404f, -0.866025404f };

Renderer::Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH)
    : m_renderer(sdlRenderer)
    , m_windowW(windowW)
    , m_windowH(windowH)
{
}

Renderer::~Renderer()
{
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontMedium) TTF_CloseFont(m_fontMedium);
}

bool Renderer::initFonts(const char* fontPath)
{
    m_fontSmall = TTF_OpenFont(fontPath, 11);
    m_fontMedium = TTF_OpenFont(fontPath, 14);

    if (!m_fontSmall || !m_fontMedium) {
        fprintf(stderr, "[renderer] Warning: Could not load font '%s': %s\n",
                fontPath, TTF_GetError());
        // Non-fatal — renderer works without fonts, just no text
        return false;
    }

    fprintf(stderr, "[renderer] Loaded font: %s (11pt + 14pt)\n", fontPath);
    return true;
}

// ---------- Hex geometry helpers ----------

void Renderer::hexCenter(int col, int row, int mapHeight, float& cx, float& cy) const
{
    cx = col * COL_SPACING + HEX_RADIUS;

    int flippedRow = mapHeight - 1 - row;
    cy = flippedRow * ROW_SPACING + HEX_HEIGHT / 2.0f;

    if (col % 2 != 0)
        cy += ROW_SPACING / 2.0f;
}

void Renderer::drawFilledHex(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    SDL_Vertex verts[7];

    verts[0].position.x = cx;
    verts[0].position.y = cy;
    verts[0].color.r = r;
    verts[0].color.g = g;
    verts[0].color.b = b;
    verts[0].color.a = 255;
    verts[0].tex_coord.x = 0;
    verts[0].tex_coord.y = 0;

    for (int i = 0; i < 6; i++) {
        verts[i + 1].position.x = cx + radius * HEX_COS[i];
        verts[i + 1].position.y = cy + radius * HEX_SIN[i];
        verts[i + 1].color = verts[0].color;
        verts[i + 1].tex_coord.x = 0;
        verts[i + 1].tex_coord.y = 0;
    }

    int indices[18];
    for (int i = 0; i < 6; i++) {
        indices[i * 3]     = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % 6 + 1;
    }

    SDL_RenderGeometry(m_renderer, nullptr, verts, 7, indices, 18);
}

void Renderer::drawHexOutline(float cx, float cy, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);

    for (int i = 0; i < 6; i++) {
        int j = (i + 1) % 6;
        SDL_RenderDrawLine(m_renderer,
            (int)(cx + radius * HEX_COS[i]), (int)(cy + radius * HEX_SIN[i]),
            (int)(cx + radius * HEX_COS[j]), (int)(cy + radius * HEX_SIN[j]));
    }
}

void Renderer::drawHexEdge(float cx, float cy, float radius, int edgeIndex,
                           uint8_t r, uint8_t g, uint8_t b, int thickness)
{
    // edgeIndex: 0=right, 1=bottom-right, 2=bottom-left, 3=left, 4=top-left, 5=top-right
    // For flat-top hex: vertex i connects to vertex (i+1)%6
    int i = edgeIndex;
    int j = (i + 1) % 6;

    float x1 = cx + radius * HEX_COS[i];
    float y1 = cy + radius * HEX_SIN[i];
    float x2 = cx + radius * HEX_COS[j];
    float y2 = cy + radius * HEX_SIN[j];

    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderDrawLine(m_renderer, (int)x1, (int)y1, (int)x2, (int)y2);

    // Draw thicker by offsetting
    for (int t = 1; t < thickness; t++) {
        // Offset perpendicular to the edge (inward)
        float mx = (x1 + x2) * 0.5f;
        float my = (y1 + y2) * 0.5f;
        float dx = cx - mx;
        float dy = cy - my;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.01f) continue;
        float nx = dx / len * t;
        float ny = dy / len * t;
        SDL_RenderDrawLine(m_renderer,
            (int)(x1 + nx), (int)(y1 + ny),
            (int)(x2 + nx), (int)(y2 + ny));
    }
}

// ---------- Feature overlays ----------

void Renderer::drawFeatureOverlay(float cx, float cy, float radius, int featureType)
{
    // Feature types (standard BTS order):
    // 0 = FEATURE_FOREST, 1 = FEATURE_JUNGLE, 2 = FEATURE_OASIS,
    // 4 = FEATURE_FLOOD_PLAINS, 5 = FEATURE_FALLOUT, 6 = FEATURE_ICE

    if (featureType == 0) {
        // Forest — draw a small triangle (tree)
        SDL_SetRenderDrawColor(m_renderer, 0, 80, 0, 255);
        float h = radius * 0.5f;
        float w = radius * 0.3f;
        // Triangle top
        int tx = (int)cx, ty = (int)(cy - h * 0.6f);
        int lx = (int)(cx - w), ly = (int)(cy + h * 0.3f);
        int rx = (int)(cx + w), ry = ly;
        SDL_RenderDrawLine(m_renderer, tx, ty, lx, ly);
        SDL_RenderDrawLine(m_renderer, lx, ly, rx, ry);
        SDL_RenderDrawLine(m_renderer, rx, ry, tx, ty);
        // Trunk
        SDL_RenderDrawLine(m_renderer, (int)cx, (int)(cy + h * 0.3f),
                                       (int)cx, (int)(cy + h * 0.6f));
    }
    else if (featureType == 1) {
        // Jungle — two overlapping triangles
        SDL_SetRenderDrawColor(m_renderer, 0, 100, 20, 255);
        float h = radius * 0.5f;
        float w = radius * 0.25f;
        float off = radius * 0.15f;
        for (int t = 0; t < 2; t++) {
            float ox = (t == 0) ? -off : off;
            int tx = (int)(cx + ox), ty = (int)(cy - h * 0.5f);
            int lx = (int)(cx + ox - w), ly = (int)(cy + h * 0.3f);
            int rx = (int)(cx + ox + w), ry = ly;
            SDL_RenderDrawLine(m_renderer, tx, ty, lx, ly);
            SDL_RenderDrawLine(m_renderer, lx, ly, rx, ry);
            SDL_RenderDrawLine(m_renderer, rx, ry, tx, ty);
        }
    }
    else if (featureType == 2) {
        // Oasis — small cyan circle (4 lines approximation)
        SDL_SetRenderDrawColor(m_renderer, 0, 200, 200, 255);
        float r = radius * 0.25f;
        int segments = 8;
        float prevX = cx + r, prevY = cy;
        for (int i = 1; i <= segments; i++) {
            float angle = (float)i / segments * 6.283185f;
            float nx = cx + r * cosf(angle);
            float ny = cy + r * sinf(angle);
            SDL_RenderDrawLine(m_renderer, (int)prevX, (int)prevY, (int)nx, (int)ny);
            prevX = nx;
            prevY = ny;
        }
    }
    else if (featureType == 6) {
        // Ice — light blue X
        SDL_SetRenderDrawColor(m_renderer, 180, 220, 255, 255);
        float s = radius * 0.25f;
        SDL_RenderDrawLine(m_renderer, (int)(cx - s), (int)(cy - s),
                                       (int)(cx + s), (int)(cy + s));
        SDL_RenderDrawLine(m_renderer, (int)(cx + s), (int)(cy - s),
                                       (int)(cx - s), (int)(cy + s));
    }
}

// ---------- Text rendering ----------

void Renderer::drawText(const std::string& text, int x, int y, TTF_Font* font,
                        uint8_t r, uint8_t g, uint8_t b)
{
    if (!font || text.empty()) return;

    SDL_Color color = {r, g, b, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(m_renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void Renderer::drawTextCentered(const std::string& text, int cx, int cy, TTF_Font* font,
                                uint8_t r, uint8_t g, uint8_t b)
{
    if (!font || text.empty()) return;

    int tw = 0, th = 0;
    TTF_SizeText(font, text.c_str(), &tw, &th);
    drawText(text, cx - tw / 2, cy - th / 2, font, r, g, b);
}

// ---------- Camera ----------

void Renderer::autoFitCamera(const MapSnapshot& snapshot)
{
    float mapPixelW = (snapshot.width - 1) * COL_SPACING + HEX_WIDTH;
    float mapPixelH = (snapshot.height - 1) * ROW_SPACING + HEX_HEIGHT + ROW_SPACING / 2.0f;

    float zoomX = (m_windowW * 0.9f) / mapPixelW;
    float zoomY = (m_windowH * 0.9f) / mapPixelH;
    m_camera.zoom = std::min(zoomX, zoomY);
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    float scaledW = mapPixelW * m_camera.zoom;
    float scaledH = mapPixelH * m_camera.zoom;
    m_camera.offsetX = -(m_windowW - scaledW) / (2.0f * m_camera.zoom);
    m_camera.offsetY = -(m_windowH - scaledH) / (2.0f * m_camera.zoom);

    fprintf(stderr, "[renderer] Auto-fit hex: map=%dx%d zoom=%.2f offset=(%.0f,%.0f)\n",
            snapshot.width, snapshot.height, m_camera.zoom,
            m_camera.offsetX, m_camera.offsetY);
}

// ---------- Main draw ----------

void Renderer::draw(MapSnapshot& snapshot)
{
    // Compute real delta-time
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastFrameTime == 0) ? (1.0f / 60.0f) : ((now - m_lastFrameTime) / 1000.0f);
    dt = std::min(dt, 0.1f);  // clamp to 100ms to avoid huge jumps
    m_lastFrameTime = now;

    // Update camera position from held keys
    float panDelta = m_panSpeed * dt / m_camera.zoom;
    if (m_keyUp)    m_camera.offsetY -= panDelta;
    if (m_keyDown)  m_camera.offsetY += panDelta;
    if (m_keyLeft)  m_camera.offsetX -= panDelta;
    if (m_keyRight) m_camera.offsetX += panDelta;

    // Clear to dark ocean background
    SDL_SetRenderDrawColor(m_renderer, 10, 20, 40, 255);
    SDL_RenderClear(m_renderer);

    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);

        if (snapshot.width == 0 || snapshot.height == 0) {
            SDL_RenderPresent(m_renderer);
            return;
        }

        if (!m_cameraInitialized) {
            autoFitCamera(snapshot);
            m_cameraInitialized = true;
        }

        float screenRadius = HEX_RADIUS * m_camera.zoom;

        // --- Pass 1: Draw all hex tiles ---
        for (int y = 0; y < snapshot.height; y++) {
            for (int x = 0; x < snapshot.width; x++) {
                const PlotData& plot = snapshot.getPlot(x, y);

                float worldCX, worldCY;
                hexCenter(x, y, snapshot.height, worldCX, worldCY);

                float screenCX = (worldCX - m_camera.offsetX) * m_camera.zoom;
                float screenCY = (worldCY - m_camera.offsetY) * m_camera.zoom;

                if (screenCX + screenRadius < 0 || screenCX - screenRadius > m_windowW ||
                    screenCY + screenRadius < 0 || screenCY - screenRadius > m_windowH)
                    continue;

                // Terrain fill
                TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
                drawFilledHex(screenCX, screenCY, screenRadius, tc.r, tc.g, tc.b);

                // Hill indicator (small triangle inside hex)
                if (plot.plotType == 1 && screenRadius >= 4) { // PLOT_HILLS
                    SDL_SetRenderDrawColor(m_renderer,
                        (uint8_t)std::min(255, tc.r + 30),
                        (uint8_t)std::min(255, tc.g + 30),
                        (uint8_t)std::min(255, tc.b + 30), 255);
                    float h = screenRadius * 0.3f;
                    float w = screenRadius * 0.25f;
                    SDL_RenderDrawLine(m_renderer,
                        (int)screenCX, (int)(screenCY - h),
                        (int)(screenCX - w), (int)(screenCY + h * 0.3f));
                    SDL_RenderDrawLine(m_renderer,
                        (int)(screenCX - w), (int)(screenCY + h * 0.3f),
                        (int)(screenCX + w), (int)(screenCY + h * 0.3f));
                    SDL_RenderDrawLine(m_renderer,
                        (int)(screenCX + w), (int)(screenCY + h * 0.3f),
                        (int)screenCX, (int)(screenCY - h));
                }

                // Feature overlay (forest, jungle, etc.)
                if (plot.featureType >= 0 && screenRadius >= 4) {
                    drawFeatureOverlay(screenCX, screenCY, screenRadius, plot.featureType);
                }

                // River edges — draw thick blue lines on specific hex edges
                // For flat-top hex vertices: 0=right, 1=bottom-right, 2=bottom-left,
                //                            3=left, 4=top-left, 5=top-right
                // Edge N connects vertex N to vertex (N+1)%6
                // isNOfRiver: river along the top edge = edge 4 (top-left to top-right)
                // isWOfRiver: river along the left edge = edge 3 (left to top-left)
                if (screenRadius >= 3) {
                    int riverThickness = std::max(2, (int)(screenRadius * 0.15f));
                    if (plot.isNOfRiver) {
                        drawHexEdge(screenCX, screenCY, screenRadius, 4,
                                    40, 120, 220, riverThickness);
                    }
                    if (plot.isWOfRiver) {
                        drawHexEdge(screenCX, screenCY, screenRadius, 3,
                                    40, 120, 220, riverThickness);
                    }
                }

                // Territory border
                if (plot.ownerID >= 0 && screenRadius >= 2) {
                    bool needBorder = false;

                    if (x + 1 < snapshot.width && snapshot.getPlot(x + 1, y).ownerID != plot.ownerID)
                        needBorder = true;
                    if (x - 1 >= 0 && snapshot.getPlot(x - 1, y).ownerID != plot.ownerID)
                        needBorder = true;
                    if (y + 1 < snapshot.height && snapshot.getPlot(x, y + 1).ownerID != plot.ownerID)
                        needBorder = true;
                    if (y - 1 >= 0 && snapshot.getPlot(x, y - 1).ownerID != plot.ownerID)
                        needBorder = true;

                    if (x == 0 || x == snapshot.width - 1 || y == 0 || y == snapshot.height - 1)
                        needBorder = true;

                    if (needBorder) {
                        drawHexOutline(screenCX, screenCY, screenRadius,
                                       plot.ownerColorR, plot.ownerColorG, plot.ownerColorB);
                    }
                }

                // City marker (smaller hex in owner's color)
                if (plot.isCity && screenRadius >= 2) {
                    drawFilledHex(screenCX, screenCY, screenRadius * 0.35f,
                                  plot.ownerColorR, plot.ownerColorG, plot.ownerColorB);
                }

                // Unit indicator (small filled square, offset from center)
                if (plot.unitCount > 0 && !plot.isCity && screenRadius >= 3) {
                    int sz = std::max(2, (int)(screenRadius * 0.3f));
                    SDL_Rect uRect = {(int)(screenCX - sz / 2), (int)(screenCY - sz / 2), sz, sz};
                    // Use owner color if owned, otherwise white
                    if (plot.ownerID >= 0) {
                        SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    } else {
                        SDL_SetRenderDrawColor(m_renderer, 220, 220, 220, 255);
                    }
                    SDL_RenderFillRect(m_renderer, &uRect);
                }
            }
        }

        // --- Pass 2: Draw city name labels (on top of everything) ---
        if (m_fontSmall && screenRadius >= 5) {
            for (int y = 0; y < snapshot.height; y++) {
                for (int x = 0; x < snapshot.width; x++) {
                    const PlotData& plot = snapshot.getPlot(x, y);
                    if (!plot.isCity || plot.cityName.empty()) continue;

                    float worldCX, worldCY;
                    hexCenter(x, y, snapshot.height, worldCX, worldCY);

                    float screenCX = (worldCX - m_camera.offsetX) * m_camera.zoom;
                    float screenCY = (worldCY - m_camera.offsetY) * m_camera.zoom;

                    if (screenCX < -100 || screenCX > m_windowW + 100 ||
                        screenCY < -50 || screenCY > m_windowH + 50)
                        continue;

                    // Draw city name + population below the hex
                    float labelY = screenCY + screenRadius * 0.9f;

                    char label[128];
                    snprintf(label, sizeof(label), "%s (%d)",
                             plot.cityName.c_str(), plot.cityPopulation);

                    // Dark background behind text for readability
                    int tw = 0, th = 0;
                    TTF_SizeText(m_fontSmall, label, &tw, &th);
                    SDL_Rect bg = {(int)(screenCX - tw / 2 - 2), (int)(labelY - 1),
                                   tw + 4, th + 2};
                    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
                    SDL_RenderFillRect(m_renderer, &bg);

                    drawTextCentered(label, (int)screenCX, (int)(labelY + th / 2),
                                     m_fontSmall, 255, 255, 255);
                }
            }
        }

        // --- HUD and minimap ---
        drawHUD(snapshot);
        if (m_showMinimap)
            drawMinimap(snapshot);
        if (m_showHelp)
            drawHelpOverlay();
    }

    SDL_RenderPresent(m_renderer);
}

// ---------- HUD ----------

void Renderer::drawHUD(MapSnapshot& snapshot)
{
    if (m_fontMedium) {
        // Top-left: turn & year
        char buf[128];
        snprintf(buf, sizeof(buf), "Turn %d  |  Year %d  |  Map %dx%d",
                 snapshot.gameTurn, snapshot.gameYear, snapshot.width, snapshot.height);
        int tw = 0, th = 0;
        TTF_SizeText(m_fontMedium, buf, &tw, &th);

        SDL_Rect hudBg = {0, 0, tw + 16, th + 8};
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(m_renderer, &hudBg);
        drawText(buf, 8, 4, m_fontMedium, 200, 220, 200);

        // Bottom-left: zoom level and FPS
        float fps = (m_lastFrameTime > 0) ? (1000.0f / std::max(1u, SDL_GetTicks() - m_lastFrameTime + 1)) : 0;
        snprintf(buf, sizeof(buf), "Zoom: %.1fx  |  FPS: %.0f", m_camera.zoom, fps);
        TTF_SizeText(m_fontMedium, buf, &tw, &th);

        SDL_Rect fpsBar = {0, m_windowH - th - 8, tw + 16, th + 8};
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(m_renderer, &fpsBar);
        drawText(buf, 8, m_windowH - th - 4, m_fontMedium, 160, 180, 160);
    } else {
        // Fallback: dot-based turn counter (no font loaded)
        SDL_Rect hudBg = {0, 0, 200, 24};
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(m_renderer, &hudBg);

        int dots = snapshot.gameTurn / 10;
        for (int i = 0; i < std::min(dots, 20); i++) {
            SDL_Rect dot = {4 + i * 9, 8, 7, 7};
            SDL_SetRenderDrawColor(m_renderer, 0, 200, 0, 255);
            SDL_RenderFillRect(m_renderer, &dot);
        }
    }
}

// ---------- Minimap ----------

void Renderer::drawMinimap(const MapSnapshot& snapshot)
{
    // Draw in bottom-right corner
    int mmMaxW = 200;
    int mmMaxH = 140;
    int margin = 10;

    // Calculate pixel-per-plot to fit the map in the minimap area
    float scaleX = (float)mmMaxW / snapshot.width;
    float scaleY = (float)mmMaxH / snapshot.height;
    float scale = std::min(scaleX, scaleY);

    int mmW = (int)(snapshot.width * scale);
    int mmH = (int)(snapshot.height * scale);
    int mmX = m_windowW - mmW - margin;
    int mmY = m_windowH - mmH - margin;

    // Store bounds for click-to-jump
    m_mmX = mmX; m_mmY = mmY; m_mmW = mmW; m_mmH = mmH;

    // Dark background
    SDL_Rect bg = {mmX - 2, mmY - 2, mmW + 4, mmH + 4};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, &bg);

    // Draw each plot as a small colored rectangle
    for (int y = 0; y < snapshot.height; y++) {
        for (int x = 0; x < snapshot.width; x++) {
            const PlotData& plot = snapshot.getPlot(x, y);
            TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);

            // Override color for cities/owned territory
            uint8_t r = tc.r, g = tc.g, b = tc.b;
            if (plot.isCity) {
                r = plot.ownerColorR;
                g = plot.ownerColorG;
                b = plot.ownerColorB;
            }

            int flippedY = snapshot.height - 1 - y;
            int px = mmX + (int)(x * scale);
            int py = mmY + (int)(flippedY * scale);
            int pw = std::max(1, (int)scale);
            int ph = std::max(1, (int)scale);

            SDL_Rect r2 = {px, py, pw, ph};
            SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
            SDL_RenderFillRect(m_renderer, &r2);
        }
    }

    // Draw viewport rectangle (white outline showing current camera view)
    // Convert screen corners to world coords, then to minimap coords
    float worldLeft = m_camera.offsetX;
    float worldTop = m_camera.offsetY;
    float worldRight = m_camera.offsetX + m_windowW / m_camera.zoom;
    float worldBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

    // Convert world coords to map grid coords (approximate)
    float mapPixelW = (snapshot.width - 1) * COL_SPACING + HEX_WIDTH;
    float mapPixelH = (snapshot.height - 1) * ROW_SPACING + HEX_HEIGHT + ROW_SPACING / 2.0f;

    float vpLeft   = mmX + (worldLeft / mapPixelW) * mmW;
    float vpTop    = mmY + (worldTop / mapPixelH) * mmH;
    float vpRight  = mmX + (worldRight / mapPixelW) * mmW;
    float vpBottom = mmY + (worldBottom / mapPixelH) * mmH;

    // Clamp to minimap bounds
    vpLeft   = std::max((float)mmX, std::min(vpLeft, (float)(mmX + mmW)));
    vpTop    = std::max((float)mmY, std::min(vpTop, (float)(mmY + mmH)));
    vpRight  = std::max((float)mmX, std::min(vpRight, (float)(mmX + mmW)));
    vpBottom = std::max((float)mmY, std::min(vpBottom, (float)(mmY + mmH)));

    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    SDL_Rect vp = {(int)vpLeft, (int)vpTop,
                   (int)(vpRight - vpLeft), (int)(vpBottom - vpTop)};
    SDL_RenderDrawRect(m_renderer, &vp);
}

// ---------- Help overlay ----------

void Renderer::drawHelpOverlay()
{
    if (!m_fontMedium) return;

    const char* lines[] = {
        "Controls:",
        "  WASD / Arrows  - Pan camera",
        "  Mouse wheel    - Zoom in/out",
        "  Right-drag     - Pan camera",
        "  F / Home       - Fit map to window",
        "  M              - Toggle minimap",
        "  H              - Toggle this help",
        "  Escape         - Quit",
    };
    int numLines = sizeof(lines) / sizeof(lines[0]);

    int lineH = 20;
    int padX = 16, padY = 12;
    int maxW = 0;
    for (int i = 0; i < numLines; i++) {
        int tw = 0, th = 0;
        TTF_SizeText(m_fontMedium, lines[i], &tw, &th);
        maxW = std::max(maxW, tw);
    }

    int boxW = maxW + padX * 2;
    int boxH = numLines * lineH + padY * 2;
    int boxX = (m_windowW - boxW) / 2;
    int boxY = (m_windowH - boxH) / 2;

    SDL_Rect bg = {boxX, boxY, boxW, boxH};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 220);
    SDL_RenderFillRect(m_renderer, &bg);

    // Border
    SDL_SetRenderDrawColor(m_renderer, 100, 120, 100, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

    for (int i = 0; i < numLines; i++) {
        uint8_t r = (i == 0) ? 255 : 200;
        uint8_t g = (i == 0) ? 255 : 220;
        uint8_t b = (i == 0) ? 200 : 200;
        drawText(lines[i], boxX + padX, boxY + padY + i * lineH, m_fontMedium, r, g, b);
    }
}

// ---------- Input handling ----------

void Renderer::handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = true;    break;
        case SDLK_DOWN:  case SDLK_s: m_keyDown = true;  break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = true;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = true; break;
        case SDLK_f: case SDLK_HOME:
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.width > 0 && snapshot.height > 0)
                    autoFitCamera(snapshot);
            }
            break;
        case SDLK_m:
            m_showMinimap = !m_showMinimap;
            break;
        case SDLK_h:
            m_showHelp = !m_showHelp;
            break;
    }
}

void Renderer::handleKeyUp(SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = false;    break;
        case SDLK_DOWN:  case SDLK_s: m_keyDown = false;  break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = false;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = false; break;
    }
}

void Renderer::handleMouseWheel(int y, int mouseX, int mouseY)
{
    float oldZoom = m_camera.zoom;

    if (y > 0)
        m_camera.zoom *= 1.15f;
    else if (y < 0)
        m_camera.zoom /= 1.15f;

    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    float worldMouseX = mouseX / oldZoom + m_camera.offsetX;
    float worldMouseY = mouseY / oldZoom + m_camera.offsetY;
    m_camera.offsetX = worldMouseX - mouseX / m_camera.zoom;
    m_camera.offsetY = worldMouseY - mouseY / m_camera.zoom;
}

void Renderer::handleMouseMotion(int dx, int dy, bool anyDragButton)
{
    if (anyDragButton) {
        m_camera.offsetX -= dx / m_camera.zoom;
        m_camera.offsetY -= dy / m_camera.zoom;
    }
}

void Renderer::handleMouseClick(int mouseX, int mouseY, int button, MapSnapshot& snapshot)
{
    // Left click on minimap: jump camera to that position
    if (button == SDL_BUTTON_LEFT && m_showMinimap && m_mmW > 0 && m_mmH > 0) {
        if (mouseX >= m_mmX && mouseX < m_mmX + m_mmW &&
            mouseY >= m_mmY && mouseY < m_mmY + m_mmH)
        {
            std::lock_guard<std::mutex> lock(snapshot.mtx);
            if (snapshot.width > 0 && snapshot.height > 0) {
                // Convert minimap click to world coordinates
                float fracX = (float)(mouseX - m_mmX) / m_mmW;
                float fracY = (float)(mouseY - m_mmY) / m_mmH;

                float mapPixelW = (snapshot.width - 1) * COL_SPACING + HEX_WIDTH;
                float mapPixelH = (snapshot.height - 1) * ROW_SPACING + HEX_HEIGHT + ROW_SPACING / 2.0f;

                // Center the viewport on the clicked world position
                float worldX = fracX * mapPixelW;
                float worldY = fracY * mapPixelH;
                m_camera.offsetX = worldX - (m_windowW / m_camera.zoom) * 0.5f;
                m_camera.offsetY = worldY - (m_windowH / m_camera.zoom) * 0.5f;
            }
        }
    }
}

void Renderer::handleResize(int newW, int newH)
{
    m_windowW = newW;
    m_windowH = newH;
}
