// Renderer.cpp — 2D square-grid map renderer
//
// Civ4 uses a square tile grid (hexes were introduced in Civ5).
// Renders terrain-colored squares with borders, rivers, cities, features,
// HUD, minimap, tooltip, and player panel.

#include "Renderer.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

Renderer::Renderer(SDL_Renderer* sdlRenderer, int windowW, int windowH, AssetManager* assets)
    : m_renderer(sdlRenderer)
    , m_assets(assets)
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

// ---------- Square tile geometry helpers ----------

void Renderer::tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const
{
    // BTS: Y increases northward (row 0 = south). Flip for screen (Y down).
    int flippedRow = mapHeight - 1 - row;
    tx = col * TILE_SIZE;
    ty = flippedRow * TILE_SIZE;
}

void Renderer::drawFilledTile(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b)
{
    SDL_Rect rect = {(int)tx, (int)ty, (int)size, (int)size};
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderFillRect(m_renderer, &rect);
}

void Renderer::drawTexturedTile(float tx, float ty, float size, SDL_Texture* tex)
{
    SDL_Rect dst = {(int)tx, (int)ty, (int)size, (int)size};
    SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
}

void Renderer::drawTileOutline(float tx, float ty, float size, uint8_t r, uint8_t g, uint8_t b)
{
    SDL_Rect rect = {(int)tx, (int)ty, (int)size, (int)size};
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderDrawRect(m_renderer, &rect);
}

void Renderer::drawTileEdge(float tx, float ty, float size, int edge,
                             uint8_t r, uint8_t g, uint8_t b, int thickness)
{
    // Edge indices: 0=North (top), 1=East (right), 2=South (bottom), 3=West (left)
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);

    for (int t = 0; t < thickness; t++) {
        switch (edge) {
            case 0: // North (top edge)
                SDL_RenderDrawLine(m_renderer, (int)tx, (int)(ty + t),
                                   (int)(tx + size), (int)(ty + t));
                break;
            case 1: // East (right edge)
                SDL_RenderDrawLine(m_renderer, (int)(tx + size - t), (int)ty,
                                   (int)(tx + size - t), (int)(ty + size));
                break;
            case 2: // South (bottom edge)
                SDL_RenderDrawLine(m_renderer, (int)tx, (int)(ty + size - t),
                                   (int)(tx + size), (int)(ty + size - t));
                break;
            case 3: // West (left edge)
                SDL_RenderDrawLine(m_renderer, (int)(tx + t), (int)ty,
                                   (int)(tx + t), (int)(ty + size));
                break;
        }
    }
}

// ---------- Feature overlays ----------

void Renderer::drawFeatureOverlay(float cx, float cy, float halfSize, int featureType)
{
    // Feature types (standard BTS order):
    // 0 = FEATURE_FOREST, 1 = FEATURE_JUNGLE, 2 = FEATURE_OASIS,
    // 4 = FEATURE_FLOOD_PLAINS, 5 = FEATURE_FALLOUT, 6 = FEATURE_ICE

    if (featureType == 0) {
        // Forest — draw a small triangle (tree)
        SDL_SetRenderDrawColor(m_renderer, 0, 80, 0, 255);
        float h = halfSize * 0.6f;
        float w = halfSize * 0.35f;
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
        float h = halfSize * 0.6f;
        float w = halfSize * 0.3f;
        float off = halfSize * 0.18f;
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
        // Oasis — small cyan circle
        SDL_SetRenderDrawColor(m_renderer, 0, 200, 200, 255);
        float r = halfSize * 0.3f;
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
        float s = halfSize * 0.3f;
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
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;

    float zoomX = (m_windowW * 0.9f) / mapPixelW;
    float zoomY = (m_windowH * 0.9f) / mapPixelH;
    m_camera.zoom = std::min(zoomX, zoomY);
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));

    float scaledW = mapPixelW * m_camera.zoom;
    float scaledH = mapPixelH * m_camera.zoom;
    m_camera.offsetX = -(m_windowW - scaledW) / (2.0f * m_camera.zoom);
    m_camera.offsetY = -(m_windowH - scaledH) / (2.0f * m_camera.zoom);

    fprintf(stderr, "[renderer] Auto-fit: map=%dx%d zoom=%.2f offset=(%.0f,%.0f)\n",
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
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

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

        // Normalize camera for wrapping maps (keep offset in [0, mapPixel) range)
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

        float screenTileSize = TILE_SIZE * m_camera.zoom;

        // Calculate visible tile range using virtual coords (supports wrapping)
        float viewLeft = m_camera.offsetX;
        float viewRight = m_camera.offsetX + m_windowW / m_camera.zoom;
        float viewTop = m_camera.offsetY;
        float viewBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

        int colStart = (int)floorf(viewLeft / TILE_SIZE) - 1;
        int colEnd   = (int)ceilf(viewRight / TILE_SIZE);
        int rowStart = (int)floorf(viewTop / TILE_SIZE) - 1;
        int rowEnd   = (int)ceilf(viewBottom / TILE_SIZE);

        // For non-wrapping axes, clamp to map bounds
        if (!snapshot.wrapX) {
            colStart = std::max(0, colStart);
            colEnd   = std::min(snapshot.width - 1, colEnd);
        }
        if (!snapshot.wrapY) {
            rowStart = std::max(0, rowStart);
            rowEnd   = std::min(snapshot.height - 1, rowEnd);
        }

        // --- Pass 1: Draw all tiles ---
        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                // Map virtual coords to actual game coords (with wrapping)
                int x = snapshot.wrapX
                    ? ((vc % snapshot.width) + snapshot.width) % snapshot.width
                    : vc;
                int fr = snapshot.wrapY
                    ? ((vr % snapshot.height) + snapshot.height) % snapshot.height
                    : vr;
                int y = snapshot.height - 1 - fr;

                if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height)
                    continue;

                const PlotData& plot = snapshot.getPlot(x, y);

                // Screen position uses virtual coords for seamless wrapping
                float worldTX = vc * TILE_SIZE;
                float worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;

                // Frustum culling
                if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                    screenTY + screenTileSize < 0 || screenTY > m_windowH)
                    continue;

                // Terrain fill: use texture if available, else solid color
                bool usedTexture = false;
                if (m_assets && m_assets->hasTextures()) {
                    // Check for plot-type override textures first (peak=-1, hill=-2)
                    SDL_Texture* tex = nullptr;
                    if (plot.plotType == 0) // PLOT_PEAK
                        tex = m_assets->getTerrainTexture(-1);
                    else if (plot.plotType == 1) // PLOT_HILLS
                        tex = m_assets->getTerrainTexture(-2);
                    if (!tex)
                        tex = m_assets->getTerrainTexture(plot.terrainType);
                    if (tex) {
                        drawTexturedTile(screenTX, screenTY, screenTileSize, tex);
                        usedTexture = true;
                        // Darken hills slightly with a semi-transparent overlay
                        if (plot.plotType == 1) {
                            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 40);
                            SDL_Rect r = {(int)screenTX, (int)screenTY, (int)screenTileSize, (int)screenTileSize};
                            SDL_RenderFillRect(m_renderer, &r);
                        }
                    }
                }
                if (!usedTexture) {
                    TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
                    drawFilledTile(screenTX, screenTY, screenTileSize, tc.r, tc.g, tc.b);
                }

                // Grid outline
                if (m_showGrid && screenTileSize >= 4) {
                    drawTileOutline(screenTX, screenTY, screenTileSize, 40, 40, 50);
                }

                // Feature overlay: texture if available, else geometric symbols
                if (plot.featureType >= 0 && screenTileSize >= 6) {
                    SDL_Texture* featTex = m_assets ? m_assets->getFeatureTexture(plot.featureType) : nullptr;
                    if (featTex) {
                        drawTexturedTile(screenTX, screenTY, screenTileSize, featTex);
                    } else {
                        float cx = screenTX + screenTileSize * 0.5f;
                        float cy = screenTY + screenTileSize * 0.5f;
                        drawFeatureOverlay(cx, cy, screenTileSize * 0.5f, plot.featureType);
                    }
                }

                // River edges — thick blue lines on tile edges
                // isNOfRiver: river along north edge = top edge (edge 0)
                // isWOfRiver: river along west edge = left edge (edge 3)
                if (screenTileSize >= 4) {
                    int riverThickness = std::max(2, (int)(screenTileSize * 0.1f));
                    if (plot.isNOfRiver) {
                        drawTileEdge(screenTX, screenTY, screenTileSize, 0,
                                     40, 120, 220, riverThickness);
                    }
                    if (plot.isWOfRiver) {
                        drawTileEdge(screenTX, screenTY, screenTileSize, 3,
                                     40, 120, 220, riverThickness);
                    }
                }

                // Territory border — colored edges where neighbor has different owner
                // 4-cardinal neighbors: 0=N, 1=E, 2=S, 3=W
                if (plot.ownerID >= 0 && screenTileSize >= 3) {
                    int borderThk = std::max(1, (int)(screenTileSize * 0.08f));
                    // Neighbor offsets: {dx, dy} for N, E, S, W
                    int nDx[4] = {  0, +1,  0, -1 };
                    int nDy[4] = { +1,  0, -1,  0 };

                    for (int e = 0; e < 4; e++) {
                        int nx = x + nDx[e];
                        int ny = y + nDy[e];

                        // Handle map wrapping for neighbor lookup
                        if (snapshot.wrapX)
                            nx = ((nx % snapshot.width) + snapshot.width) % snapshot.width;
                        if (snapshot.wrapY)
                            ny = ((ny % snapshot.height) + snapshot.height) % snapshot.height;

                        bool drawEdge = false;
                        if (nx < 0 || nx >= snapshot.width || ny < 0 || ny >= snapshot.height) {
                            drawEdge = true; // non-wrapping map edge = border
                        } else if (snapshot.getPlot(nx, ny).ownerID != plot.ownerID) {
                            drawEdge = true;
                        }
                        if (drawEdge) {
                            drawTileEdge(screenTX, screenTY, screenTileSize, e,
                                         plot.ownerColorR, plot.ownerColorG, plot.ownerColorB,
                                         borderThk);
                        }
                    }
                }

                // City marker (filled rectangle in owner's color)
                if (plot.isCity && screenTileSize >= 3) {
                    float inset = screenTileSize * 0.3f;
                    SDL_Rect cityRect = {
                        (int)(screenTX + inset), (int)(screenTY + inset),
                        (int)(screenTileSize - inset * 2), (int)(screenTileSize - inset * 2)
                    };
                    SDL_SetRenderDrawColor(m_renderer,
                        plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    SDL_RenderFillRect(m_renderer, &cityRect);
                }

                // Unit indicator (small filled square, offset from center)
                if (plot.unitCount > 0 && !plot.isCity && screenTileSize >= 4) {
                    int sz = std::max(2, (int)(screenTileSize * 0.25f));
                    float cx = screenTX + screenTileSize * 0.5f;
                    float cy = screenTY + screenTileSize * 0.5f;
                    SDL_Rect uRect = {(int)(cx - sz / 2), (int)(cy - sz / 2), sz, sz};
                    if (plot.ownerID >= 0) {
                        SDL_SetRenderDrawColor(m_renderer,
                            plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    } else {
                        SDL_SetRenderDrawColor(m_renderer, 220, 220, 220, 255);
                    }
                    SDL_RenderFillRect(m_renderer, &uRect);
                }
            }
        }

        // --- Pass 2: Draw city name labels (on top of everything) ---
        if (m_fontSmall && screenTileSize >= 8) {
            for (int vr2 = rowStart; vr2 <= rowEnd; vr2++) {
                for (int vc2 = colStart; vc2 <= colEnd; vc2++) {
                    int x2 = snapshot.wrapX
                        ? ((vc2 % snapshot.width) + snapshot.width) % snapshot.width
                        : vc2;
                    int fr2 = snapshot.wrapY
                        ? ((vr2 % snapshot.height) + snapshot.height) % snapshot.height
                        : vr2;
                    int y2 = snapshot.height - 1 - fr2;

                    if (x2 < 0 || x2 >= snapshot.width || y2 < 0 || y2 >= snapshot.height)
                        continue;

                    const PlotData& plot = snapshot.getPlot(x2, y2);
                    if (!plot.isCity || plot.cityName.empty()) continue;

                    float worldTX = vc2 * TILE_SIZE;
                    float worldTY = vr2 * TILE_SIZE;
                    float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                    float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                    float screenCX = screenTX + screenTileSize * 0.5f;

                    if (screenCX < -100 || screenCX > m_windowW + 100 ||
                        screenTY < -50 || screenTY > m_windowH + 50)
                        continue;

                    // Draw city name + population below the tile
                    float labelY = screenTY + screenTileSize + 2;

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

        // --- HUD, panels, minimap, tooltip ---
        drawHUD(snapshot);
        if (m_showPlayerPanel)
            drawPlayerPanel(snapshot);
        if (m_showMinimap)
            drawMinimap(snapshot);
        drawTooltip(snapshot);
        if (m_showHelp)
            drawHelpOverlay();
    }

    SDL_RenderPresent(m_renderer);
}

// ---------- HUD ----------

void Renderer::drawHUD(MapSnapshot& snapshot)
{
    if (m_fontMedium) {
        // Top-left: turn & year & pause status
        char buf[256];
        if (snapshot.paused) {
            snprintf(buf, sizeof(buf), "Turn %d  |  Year %d  |  Map %dx%d  |  PAUSED",
                     snapshot.gameTurn, snapshot.gameYear, snapshot.width, snapshot.height);
        } else {
            int delay = snapshot.turnDelayMs;
            if (delay > 0) {
                snprintf(buf, sizeof(buf), "Turn %d  |  Year %d  |  Map %dx%d  |  Delay %dms",
                         snapshot.gameTurn, snapshot.gameYear, snapshot.width, snapshot.height, delay);
            } else {
                snprintf(buf, sizeof(buf), "Turn %d  |  Year %d  |  Map %dx%d  |  Max Speed",
                         snapshot.gameTurn, snapshot.gameYear, snapshot.width, snapshot.height);
            }
        }
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
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;

    float worldLeft = m_camera.offsetX;
    float worldTop = m_camera.offsetY;
    float worldRight = m_camera.offsetX + m_windowW / m_camera.zoom;
    float worldBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);

    // Convert to fractional map coordinates
    float fracLeft  = worldLeft / mapPixelW;
    float fracRight = worldRight / mapPixelW;
    float fracTop   = worldTop / mapPixelH;
    float fracBot   = worldBottom / mapPixelH;

    // Helper: draw a clamped viewport rect on the minimap
    auto drawVPRect = [&](float fL, float fR, float fT, float fB) {
        float vL = mmX + std::max(0.0f, std::min(fL, 1.0f)) * mmW;
        float vR = mmX + std::max(0.0f, std::min(fR, 1.0f)) * mmW;
        float vT = mmY + std::max(0.0f, std::min(fT, 1.0f)) * mmH;
        float vB = mmY + std::max(0.0f, std::min(fB, 1.0f)) * mmH;
        if (vR > vL && vB > vT) {
            SDL_Rect vp = {(int)vL, (int)vT, (int)(vR - vL), (int)(vB - vT)};
            SDL_RenderDrawRect(m_renderer, &vp);
        }
    };

    // For wrapping maps, viewport may span the map edge — draw split rectangles
    bool splitX = snapshot.wrapX && fracRight > 1.0f;
    bool splitY = snapshot.wrapY && fracBot > 1.0f;

    if (splitX && splitY) {
        drawVPRect(fracLeft, 1.0f, fracTop, 1.0f);
        drawVPRect(0.0f, fracRight - 1.0f, fracTop, 1.0f);
        drawVPRect(fracLeft, 1.0f, 0.0f, fracBot - 1.0f);
        drawVPRect(0.0f, fracRight - 1.0f, 0.0f, fracBot - 1.0f);
    } else if (splitX) {
        drawVPRect(fracLeft, 1.0f, fracTop, fracBot);
        drawVPRect(0.0f, fracRight - 1.0f, fracTop, fracBot);
    } else if (splitY) {
        drawVPRect(fracLeft, fracRight, fracTop, 1.0f);
        drawVPRect(fracLeft, fracRight, 0.0f, fracBot - 1.0f);
    } else {
        drawVPRect(fracLeft, fracRight, fracTop, fracBot);
    }
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
        "  Space          - Pause / unpause game",
        "  +/-            - Slower / faster turns",
        "  P              - Toggle player panel",
        "  G              - Toggle grid lines",
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

// ---------- Tooltip ----------

void Renderer::drawTooltip(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || snapshot.width == 0) return;

    // Convert screen coords to world coords
    float worldX = m_mouseX / m_camera.zoom + m_camera.offsetX;
    float worldY = m_mouseY / m_camera.zoom + m_camera.offsetY;

    // Simple grid division for square tiles
    int col = (int)floorf(worldX / TILE_SIZE);
    int flippedRow = (int)floorf(worldY / TILE_SIZE);

    // Handle wrapping for tooltip coordinates
    if (snapshot.wrapX)
        col = ((col % snapshot.width) + snapshot.width) % snapshot.width;
    if (snapshot.wrapY)
        flippedRow = ((flippedRow % snapshot.height) + snapshot.height) % snapshot.height;

    int row = snapshot.height - 1 - flippedRow;

    // Bounds check
    if (col < 0 || col >= snapshot.width || row < 0 || row >= snapshot.height) return;

    const PlotData& plot = snapshot.getPlot(col, row);

    // Build tooltip text
    static const char* terrainNames[] = {
        "Grass", "Plains", "Desert", "Tundra", "Snow", "Coast", "Ocean", "Peak", "Hill"
    };
    static const char* plotNames[] = { "Peak", "Hills", "Land", "Ocean" };

    const char* tName = (plot.terrainType >= 0 && plot.terrainType <= 8)
                        ? terrainNames[plot.terrainType] : "???";
    const char* pName = (plot.plotType >= 0 && plot.plotType <= 3)
                        ? plotNames[plot.plotType] : "???";

    char line1[128], line2[128];
    snprintf(line1, sizeof(line1), "(%d, %d) %s %s", col, row, tName, pName);

    // Second line: features, river, owner
    line2[0] = '\0';
    static const char* featureNames[] = {
        "Forest", "Jungle", "Oasis", "FP?", "FloodPlains", "Fallout", "Ice"
    };
    if (plot.featureType >= 0 && plot.featureType <= 6) {
        snprintf(line2, sizeof(line2), "%s", featureNames[plot.featureType]);
    }
    if (plot.isRiver) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%sRiver", line2[0] ? ", " : "");
        strncat(line2, tmp, sizeof(line2) - strlen(line2) - 1);
    }
    if (plot.unitCount > 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s%d unit%s", line2[0] ? ", " : "",
                 plot.unitCount, plot.unitCount > 1 ? "s" : "");
        strncat(line2, tmp, sizeof(line2) - strlen(line2) - 1);
    }

    // Draw tooltip near mouse cursor
    int tipX = m_mouseX + 16;
    int tipY = m_mouseY + 4;

    int tw1 = 0, th1 = 0, tw2 = 0, th2 = 0;
    TTF_SizeText(m_fontSmall, line1, &tw1, &th1);
    if (line2[0])
        TTF_SizeText(m_fontSmall, line2, &tw2, &th2);

    int boxW = std::max(tw1, tw2) + 8;
    int boxH = th1 + (line2[0] ? th2 + 2 : 0) + 6;

    // Keep on screen
    if (tipX + boxW > m_windowW) tipX = m_mouseX - boxW - 8;
    if (tipY + boxH > m_windowH) tipY = m_mouseY - boxH - 4;

    SDL_Rect bg = {tipX, tipY, boxW, boxH};
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 30, 220);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 80, 100, 80, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

    drawText(line1, tipX + 4, tipY + 3, m_fontSmall, 220, 230, 200);
    if (line2[0])
        drawText(line2, tipX + 4, tipY + 3 + th1 + 2, m_fontSmall, 180, 200, 180);
}

// ---------- Player info panel ----------

void Renderer::drawPlayerPanel(const MapSnapshot& snapshot)
{
    if (!m_fontSmall) return;

    int panelW = 220;
    int lineH = 16;
    int padX = 8, padY = 6;

    // Count alive players
    int alive = 0;
    for (int p = 0; p < snapshot.numPlayers; p++)
        if (snapshot.players[p].alive) alive++;

    int panelH = padY * 2 + (alive + 1) * lineH; // +1 for header
    int panelX = 0;
    int panelY = 28; // below the top HUD bar

    // Background
    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 20, 210);
    SDL_RenderFillRect(m_renderer, &bg);

    // Header
    drawText("Player       Cities Pop Score", panelX + padX, panelY + padY,
             m_fontSmall, 180, 190, 160);

    int row = 1;
    for (int p = 0; p < snapshot.numPlayers; p++) {
        const PlayerInfo& pi = snapshot.players[p];
        if (!pi.alive) continue;

        int ly = panelY + padY + row * lineH;

        // Color swatch
        SDL_Rect swatch = {panelX + padX, ly + 2, 10, 10};
        SDL_SetRenderDrawColor(m_renderer, pi.colorR, pi.colorG, pi.colorB, 255);
        SDL_RenderFillRect(m_renderer, &swatch);

        // Player info
        char buf[128];
        snprintf(buf, sizeof(buf), "%-12s %3d  %3d  %4d",
                 pi.civName.c_str(), pi.numCities, pi.totalPop, pi.score);
        drawText(buf, panelX + padX + 14, ly, m_fontSmall, 200, 210, 200);

        row++;
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
        case SDLK_p:
            m_showPlayerPanel = !m_showPlayerPanel;
            break;
        case SDLK_g:
            m_showGrid = !m_showGrid;
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
    // Track mouse position for tooltip
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    m_mouseX = mx;
    m_mouseY = my;

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

                float mapPixelW = snapshot.width * TILE_SIZE;
                float mapPixelH = snapshot.height * TILE_SIZE;

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
