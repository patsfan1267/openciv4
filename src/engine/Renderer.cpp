// Renderer.cpp — 2D square-grid map renderer with gameplay UI
//
// Phase 2: Adds unit selection highlights, unit/city info panels,
// city production picker, tech research picker, action bar,
// turn banner, and game command routing from mouse clicks.

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
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
}

bool Renderer::initFonts(const char* fontPath)
{
    m_fontSmall = TTF_OpenFont(fontPath, 11);
    m_fontMedium = TTF_OpenFont(fontPath, 14);
    m_fontLarge = TTF_OpenFont(fontPath, 18);

    if (!m_fontSmall || !m_fontMedium) {
        fprintf(stderr, "[renderer] Warning: Could not load font '%s': %s\n",
                fontPath, TTF_GetError());
        return false;
    }
    fprintf(stderr, "[renderer] Loaded font: %s (11/14/18pt)\n", fontPath);
    return true;
}

// ---------- Square tile geometry helpers ----------

void Renderer::tileTopLeft(int col, int row, int mapHeight, float& tx, float& ty) const
{
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
    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    for (int t = 0; t < thickness; t++) {
        switch (edge) {
            case 0: SDL_RenderDrawLine(m_renderer, (int)tx, (int)(ty+t), (int)(tx+size), (int)(ty+t)); break;
            case 1: SDL_RenderDrawLine(m_renderer, (int)(tx+size-t), (int)ty, (int)(tx+size-t), (int)(ty+size)); break;
            case 2: SDL_RenderDrawLine(m_renderer, (int)tx, (int)(ty+size-t), (int)(tx+size), (int)(ty+size-t)); break;
            case 3: SDL_RenderDrawLine(m_renderer, (int)(tx+t), (int)ty, (int)(tx+t), (int)(ty+size)); break;
        }
    }
}

// ---------- Feature overlays ----------

void Renderer::drawFeatureOverlay(float cx, float cy, float halfSize, int featureType)
{
    if (featureType == 0) { // Forest
        SDL_SetRenderDrawColor(m_renderer, 0, 80, 0, 255);
        float h = halfSize * 0.6f, w = halfSize * 0.35f;
        int tx = (int)cx, ty = (int)(cy - h*0.6f);
        int lx = (int)(cx-w), ly = (int)(cy+h*0.3f), rx = (int)(cx+w);
        SDL_RenderDrawLine(m_renderer, tx, ty, lx, ly);
        SDL_RenderDrawLine(m_renderer, lx, ly, rx, ly);
        SDL_RenderDrawLine(m_renderer, rx, ly, tx, ty);
        SDL_RenderDrawLine(m_renderer, (int)cx, (int)(cy+h*0.3f), (int)cx, (int)(cy+h*0.6f));
    }
    else if (featureType == 1) { // Jungle
        SDL_SetRenderDrawColor(m_renderer, 0, 100, 20, 255);
        float h = halfSize * 0.6f, w = halfSize * 0.3f, off = halfSize * 0.18f;
        for (int t = 0; t < 2; t++) {
            float ox = (t == 0) ? -off : off;
            int tx = (int)(cx+ox), ty = (int)(cy-h*0.5f);
            int lx = (int)(cx+ox-w), ly = (int)(cy+h*0.3f), rx = (int)(cx+ox+w);
            SDL_RenderDrawLine(m_renderer, tx, ty, lx, ly);
            SDL_RenderDrawLine(m_renderer, lx, ly, rx, ly);
            SDL_RenderDrawLine(m_renderer, rx, ly, tx, ty);
        }
    }
    else if (featureType == 2) { // Oasis
        SDL_SetRenderDrawColor(m_renderer, 0, 200, 200, 255);
        float r = halfSize * 0.3f;
        float prevX = cx + r, prevY = cy;
        for (int i = 1; i <= 8; i++) {
            float angle = (float)i / 8.0f * 6.283185f;
            float nx = cx + r * cosf(angle), ny = cy + r * sinf(angle);
            SDL_RenderDrawLine(m_renderer, (int)prevX, (int)prevY, (int)nx, (int)ny);
            prevX = nx; prevY = ny;
        }
    }
    else if (featureType == 6) { // Ice
        SDL_SetRenderDrawColor(m_renderer, 180, 220, 255, 255);
        float s = halfSize * 0.3f;
        SDL_RenderDrawLine(m_renderer, (int)(cx-s), (int)(cy-s), (int)(cx+s), (int)(cy+s));
        SDL_RenderDrawLine(m_renderer, (int)(cx+s), (int)(cy-s), (int)(cx-s), (int)(cy+s));
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
    drawText(text, cx - tw/2, cy - th/2, font, r, g, b);
}

// ---------- Camera ----------

void Renderer::autoFitCamera(const MapSnapshot& snapshot)
{
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

void Renderer::centerOnTile(int tileX, int tileY, int mapHeight)
{
    int flippedRow = mapHeight - 1 - tileY;
    float worldX = (tileX + 0.5f) * TILE_SIZE;
    float worldY = (flippedRow + 0.5f) * TILE_SIZE;
    m_camera.offsetX = worldX - (m_windowW / m_camera.zoom) * 0.5f;
    m_camera.offsetY = worldY - (m_windowH / m_camera.zoom) * 0.5f;
}

// ---------- Screen to tile conversion ----------

bool Renderer::screenToTile(int screenX, int screenY, const MapSnapshot& snapshot,
                            int& tileX, int& tileY)
{
    float worldX = screenX / m_camera.zoom + m_camera.offsetX;
    float worldY = screenY / m_camera.zoom + m_camera.offsetY;
    int col = (int)floorf(worldX / TILE_SIZE);
    int flippedRow = (int)floorf(worldY / TILE_SIZE);

    if (snapshot.wrapX)
        col = ((col % snapshot.width) + snapshot.width) % snapshot.width;
    if (snapshot.wrapY)
        flippedRow = ((flippedRow % snapshot.height) + snapshot.height) % snapshot.height;

    int row = snapshot.height - 1 - flippedRow;
    if (col < 0 || col >= snapshot.width || row < 0 || row >= snapshot.height)
        return false;

    tileX = col;
    tileY = row;
    return true;
}

// ---------- Selection highlight ----------

void Renderer::drawSelectionHighlight(float screenTX, float screenTY, float screenTileSize)
{
    // Animated pulsing white border
    Uint32 ms = SDL_GetTicks();
    int alpha = 150 + (int)(105.0f * sinf(ms * 0.005f));

    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, alpha);
    int th = std::max(2, (int)(screenTileSize * 0.1f));
    for (int t = 0; t < th; t++) {
        SDL_Rect r = {(int)(screenTX - t), (int)(screenTY - t),
                      (int)(screenTileSize + t*2), (int)(screenTileSize + t*2)};
        SDL_RenderDrawRect(m_renderer, &r);
    }
}

// ---------- Main draw ----------

void Renderer::draw(MapSnapshot& snapshot)
{
    Uint32 now = SDL_GetTicks();
    float dt = (m_lastFrameTime == 0) ? (1.0f/60.0f) : ((now - m_lastFrameTime) / 1000.0f);
    dt = std::min(dt, 0.1f);
    m_lastFrameTime = now;

    float panDelta = m_panSpeed * dt / m_camera.zoom;
    if (m_keyUp)    m_camera.offsetY -= panDelta;
    if (m_keyDown)  m_camera.offsetY += panDelta;
    if (m_keyLeft)  m_camera.offsetX -= panDelta;
    if (m_keyRight) m_camera.offsetX += panDelta;

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

        float screenTileSize = TILE_SIZE * m_camera.zoom;

        float viewLeft = m_camera.offsetX;
        float viewRight = m_camera.offsetX + m_windowW / m_camera.zoom;
        float viewTop = m_camera.offsetY;
        float viewBottom = m_camera.offsetY + m_windowH / m_camera.zoom;

        int colStart = (int)floorf(viewLeft / TILE_SIZE) - 1;
        int colEnd   = (int)ceilf(viewRight / TILE_SIZE);
        int rowStart = (int)floorf(viewTop / TILE_SIZE) - 1;
        int rowEnd   = (int)ceilf(viewBottom / TILE_SIZE);

        if (!snapshot.wrapX) { colStart = std::max(0, colStart); colEnd = std::min(snapshot.width-1, colEnd); }
        if (!snapshot.wrapY) { rowStart = std::max(0, rowStart); rowEnd = std::min(snapshot.height-1, rowEnd); }

        // --- Pass 1: Draw all tiles ---
        for (int vr = rowStart; vr <= rowEnd; vr++) {
            for (int vc = colStart; vc <= colEnd; vc++) {
                int x = snapshot.wrapX ? ((vc % snapshot.width) + snapshot.width) % snapshot.width : vc;
                int fr = snapshot.wrapY ? ((vr % snapshot.height) + snapshot.height) % snapshot.height : vr;
                int y = snapshot.height - 1 - fr;
                if (x < 0 || x >= snapshot.width || y < 0 || y >= snapshot.height) continue;

                const PlotData& plot = snapshot.getPlot(x, y);

                float worldTX = vc * TILE_SIZE;
                float worldTY = vr * TILE_SIZE;
                float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;

                if (screenTX + screenTileSize < 0 || screenTX > m_windowW ||
                    screenTY + screenTileSize < 0 || screenTY > m_windowH) continue;

                // Terrain fill
                bool usedTexture = false;
                if (m_assets && m_assets->hasTextures()) {
                    SDL_Texture* tex = nullptr;
                    if (plot.plotType == 0) tex = m_assets->getTerrainTexture(-1);
                    else if (plot.plotType == 1) tex = m_assets->getTerrainTexture(-2);
                    if (!tex) tex = m_assets->getTerrainTexture(plot.terrainType);
                    if (tex) {
                        drawTexturedTile(screenTX, screenTY, screenTileSize, tex);
                        usedTexture = true;
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

                // Territory overlay
                if (plot.ownerID >= 0) {
                    SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 45);
                    SDL_Rect terr = {(int)screenTX, (int)screenTY, (int)screenTileSize, (int)screenTileSize};
                    SDL_RenderFillRect(m_renderer, &terr);
                }

                // Grid
                if (m_showGrid && screenTileSize >= 4)
                    drawTileOutline(screenTX, screenTY, screenTileSize, 40, 40, 50);

                // Features
                if (plot.featureType >= 0 && screenTileSize >= 6) {
                    SDL_Texture* featTex = m_assets ? m_assets->getFeatureTexture(plot.featureType) : nullptr;
                    if (featTex) drawTexturedTile(screenTX, screenTY, screenTileSize, featTex);
                    else {
                        float cx = screenTX + screenTileSize*0.5f, cy = screenTY + screenTileSize*0.5f;
                        drawFeatureOverlay(cx, cy, screenTileSize*0.5f, plot.featureType);
                    }
                }

                // Rivers
                if (screenTileSize >= 4) {
                    int riverThk = std::max(2, (int)(screenTileSize * 0.1f));
                    if (plot.isNOfRiver) drawTileEdge(screenTX, screenTY, screenTileSize, 0, 40, 120, 220, riverThk);
                    if (plot.isWOfRiver) drawTileEdge(screenTX, screenTY, screenTileSize, 3, 40, 120, 220, riverThk);
                }

                // Territory borders
                if (plot.ownerID >= 0 && screenTileSize >= 3) {
                    int borderThk = std::max(1, (int)(screenTileSize * 0.08f));
                    int nDx[4] = {0, +1, 0, -1}, nDy[4] = {+1, 0, -1, 0};
                    for (int e = 0; e < 4; e++) {
                        int nx = x + nDx[e], ny = y + nDy[e];
                        if (snapshot.wrapX) nx = ((nx % snapshot.width) + snapshot.width) % snapshot.width;
                        if (snapshot.wrapY) ny = ((ny % snapshot.height) + snapshot.height) % snapshot.height;
                        bool drawEdge = false;
                        if (nx < 0 || nx >= snapshot.width || ny < 0 || ny >= snapshot.height) drawEdge = true;
                        else if (snapshot.getPlot(nx, ny).ownerID != plot.ownerID) drawEdge = true;
                        if (drawEdge)
                            drawTileEdge(screenTX, screenTY, screenTileSize, e,
                                         plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, borderThk);
                    }
                }

                // City marker
                if (plot.isCity && screenTileSize >= 3) {
                    float inset = screenTileSize * 0.3f;
                    SDL_Rect cityRect = {(int)(screenTX+inset), (int)(screenTY+inset),
                                         (int)(screenTileSize-inset*2), (int)(screenTileSize-inset*2)};
                    SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    SDL_RenderFillRect(m_renderer, &cityRect);
                }

                // Unit indicator
                if (plot.unitCount > 0 && !plot.isCity && screenTileSize >= 4) {
                    int sz = std::max(2, (int)(screenTileSize * 0.25f));
                    float cx = screenTX + screenTileSize * 0.5f;
                    float cy = screenTY + screenTileSize * 0.5f;
                    SDL_Rect uRect = {(int)(cx - sz/2), (int)(cy - sz/2), sz, sz};
                    if (plot.ownerID >= 0)
                        SDL_SetRenderDrawColor(m_renderer, plot.ownerColorR, plot.ownerColorG, plot.ownerColorB, 255);
                    else
                        SDL_SetRenderDrawColor(m_renderer, 220, 220, 220, 255);
                    SDL_RenderFillRect(m_renderer, &uRect);

                    // Health bar for damaged units
                    if (plot.firstUnitHP < 100 && plot.firstUnitHP > 0 && screenTileSize >= 8) {
                        int barW = (int)(screenTileSize * 0.6f);
                        int barH = std::max(2, (int)(screenTileSize * 0.08f));
                        int barX = (int)(screenTX + screenTileSize * 0.2f);
                        int barY = (int)(screenTY + screenTileSize - barH - 1);
                        SDL_Rect bgBar = {barX, barY, barW, barH};
                        SDL_SetRenderDrawColor(m_renderer, 80, 0, 0, 200);
                        SDL_RenderFillRect(m_renderer, &bgBar);
                        int hpW = (barW * plot.firstUnitHP) / 100;
                        SDL_Rect hpBar = {barX, barY, hpW, barH};
                        uint8_t gr = plot.firstUnitHP > 50 ? 0 : 200;
                        uint8_t gg = plot.firstUnitHP > 50 ? 200 : (plot.firstUnitHP > 25 ? 200 : 0);
                        SDL_SetRenderDrawColor(m_renderer, gr, gg, 0, 255);
                        SDL_RenderFillRect(m_renderer, &hpBar);
                    }
                }

                // Selection highlight
                if (x == snapshot.selectedUnitX && y == snapshot.selectedUnitY && snapshot.selectedUnitID >= 0)
                    drawSelectionHighlight(screenTX, screenTY, screenTileSize);
            }
        }

        // --- Pass 2: City name labels ---
        if (m_fontSmall && screenTileSize >= 8) {
            for (int vr2 = rowStart; vr2 <= rowEnd; vr2++) {
                for (int vc2 = colStart; vc2 <= colEnd; vc2++) {
                    int x2 = snapshot.wrapX ? ((vc2 % snapshot.width)+snapshot.width)%snapshot.width : vc2;
                    int fr2 = snapshot.wrapY ? ((vr2 % snapshot.height)+snapshot.height)%snapshot.height : vr2;
                    int y2 = snapshot.height - 1 - fr2;
                    if (x2 < 0 || x2 >= snapshot.width || y2 < 0 || y2 >= snapshot.height) continue;
                    const PlotData& plot = snapshot.getPlot(x2, y2);
                    if (!plot.isCity || plot.cityName.empty()) continue;

                    float worldTX = vc2 * TILE_SIZE, worldTY = vr2 * TILE_SIZE;
                    float screenTX = (worldTX - m_camera.offsetX) * m_camera.zoom;
                    float screenTY = (worldTY - m_camera.offsetY) * m_camera.zoom;
                    float screenCX = screenTX + screenTileSize * 0.5f;
                    if (screenCX < -100 || screenCX > m_windowW+100) continue;

                    float labelY = screenTY + screenTileSize + 2;
                    char label[128];
                    snprintf(label, sizeof(label), "%s (%d)", plot.cityName.c_str(), plot.cityPopulation);
                    int tw = 0, th = 0;
                    TTF_SizeText(m_fontSmall, label, &tw, &th);
                    SDL_Rect bg = {(int)(screenCX - tw/2 - 2), (int)(labelY - 1), tw + 4, th + 2};
                    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
                    SDL_RenderFillRect(m_renderer, &bg);
                    drawTextCentered(label, (int)screenCX, (int)(labelY + th/2), m_fontSmall, 255, 255, 255);
                }
            }
        }

        // --- UI overlays ---
        drawHUD(snapshot);
        drawTurnBanner(snapshot);
        drawGameMessages(snapshot);
        if (snapshot.cityScreenOpen)
            drawCityPanel(snapshot);
        else if (snapshot.selectedUnitID >= 0)
            drawUnitPanel(snapshot);
        drawActionBar(snapshot);
        if (m_showPlayerPanel) drawPlayerPanel(snapshot);
        if (m_showMinimap) drawMinimap(snapshot);
        if (m_showTechPicker) drawTechPicker(snapshot);
        drawTooltip(snapshot);
        if (m_showHelp) drawHelpOverlay();
    }

    SDL_RenderPresent(m_renderer);
}

// ---------- HUD ----------

void Renderer::drawHUD(MapSnapshot& snapshot)
{
    if (!m_fontMedium) return;

    // Top-left: turn, year, player info
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
    SDL_Rect hudBg = {0, 0, tw + 16, th + 8};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, &hudBg);
    drawText(buf, 8, 4, m_fontMedium, 200, 220, 200);

    // Bottom-left: zoom + FPS
    float fps = (m_lastFrameTime > 0) ? (1000.0f / std::max(1u, SDL_GetTicks() - m_lastFrameTime + 1)) : 0;
    snprintf(buf, sizeof(buf), "Zoom: %.1fx | FPS: %.0f | Map %dx%d",
             m_camera.zoom, fps, snapshot.width, snapshot.height);
    TTF_SizeText(m_fontMedium, buf, &tw, &th);
    SDL_Rect fpsBar = {0, m_windowH - th - 8, tw + 16, th + 8};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, &fpsBar);
    drawText(buf, 8, m_windowH - th - 4, m_fontMedium, 160, 180, 160);
}

// ---------- Turn banner ----------

void Renderer::drawTurnBanner(const MapSnapshot& snapshot)
{
    if (!m_fontLarge) return;

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
        SDL_Rect bg = {bx, by, tw + 24, th + 8};
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
        SDL_RenderFillRect(m_renderer, &bg);
        drawTextCentered(msg, m_windowW / 2, by + th/2 + 4, m_fontLarge, r, g, b);
    }
}

// ---------- Game messages ----------

void Renderer::drawGameMessages(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || snapshot.gameMessages.empty()) return;

    int startY = 56;
    int lineH = 16;
    int maxMsg = std::min((int)snapshot.gameMessages.size(), 6);
    int startIdx = (int)snapshot.gameMessages.size() - maxMsg;

    for (int i = startIdx; i < (int)snapshot.gameMessages.size(); i++) {
        int y = startY + (i - startIdx) * lineH;
        int tw = 0, th = 0;
        TTF_SizeText(m_fontSmall, snapshot.gameMessages[i].c_str(), &tw, &th);
        SDL_Rect bg = {m_windowW/2 - tw/2 - 4, y, tw + 8, th + 2};
        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(m_renderer, &bg);
        drawTextCentered(snapshot.gameMessages[i], m_windowW/2, y + th/2,
                         m_fontSmall, 220, 220, 180);
    }
}

// ---------- Unit info panel ----------

void Renderer::drawUnitPanel(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || snapshot.selectedUnitID < 0) return;

    // Find the selected unit's plot data
    if (snapshot.selectedUnitX < 0 || snapshot.selectedUnitY < 0) return;
    const PlotData& pd = snapshot.getPlot(snapshot.selectedUnitX, snapshot.selectedUnitY);

    int panelW = 220, panelH = 100;
    int panelX = m_windowW - panelW - 10;
    int panelY = 10;

    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 30, 220);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 80, 100, 80, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

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

    // Show available actions
    std::string actions;
    if (pd.firstUnitCanFound) actions += "[B]uild City  ";
    actions += "[F]ortify  [S]leep";
    drawText(actions, panelX + 8, y, m_fontSmall, 180, 200, 180);
}

// ---------- City panel (production picker) ----------

void Renderer::drawCityPanel(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || !snapshot.cityScreenOpen) return;

    const CityDetail& cd = snapshot.selectedCity;
    int panelW = 300;
    int lineH = 16;
    int headerH = 90;
    int itemCount = (int)cd.availableProduction.size();
    int maxVisible = 20;
    int panelH = headerH + std::min(itemCount, maxVisible) * lineH + 20;
    int panelX = m_windowW - panelW - 10;
    int panelY = 10;

    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 30, 230);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 80, 100, 120, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

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
                 item.name.c_str(), item.turns,
                 item.isUnit ? "[U]" : "[B]");
        uint8_t r = item.isUnit ? 200 : 180;
        uint8_t g = item.isUnit ? 220 : 200;
        uint8_t b = item.isUnit ? 200 : 220;
        drawText(line, panelX + 8, y, m_fontSmall, r, g, b);
        y += lineH;
    }

    drawText("[Esc] Close city", panelX + 8, y + 4, m_fontSmall, 140, 140, 140);
}

// ---------- Tech picker ----------

void Renderer::drawTechPicker(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || snapshot.availableTechs.empty()) return;

    int panelW = 280;
    int lineH = 18;
    int maxVisible = 15;
    int itemCount = (int)snapshot.availableTechs.size();
    int panelH = 30 + std::min(itemCount, maxVisible) * lineH + 10;
    int panelX = (m_windowW - panelW) / 2;
    int panelY = (m_windowH - panelH) / 2;

    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(m_renderer, 10, 15, 35, 240);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 60, 80, 140, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

    int y = panelY + 6;
    drawText("Choose Research (click or 1-9, Esc to close)", panelX + 8, y, m_fontMedium, 200, 220, 255);
    y += 22;

    int visStart = m_techScrollOffset;
    int visEnd = std::min(visStart + maxVisible, itemCount);
    for (int i = visStart; i < visEnd; i++) {
        const TechItem& ti = snapshot.availableTechs[i];
        char line[128];
        int num = i - visStart + 1;
        snprintf(line, sizeof(line), "%d. %s (%d turns)",
                 num <= 9 ? num : 0, ti.name.c_str(), ti.turnsLeft);
        drawText(line, panelX + 12, y, m_fontSmall, 200, 220, 240);
        y += lineH;
    }
}

// ---------- Action bar ----------

void Renderer::drawActionBar(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || !snapshot.isHumanTurn) return;

    // Bottom-center action bar
    int barH = 24;
    int barY = m_windowH - barH - 30; // above the zoom/FPS bar
    std::string actions = "Enter=End Turn  Tab=Next Unit  F6=Research  H=Help";
    if (snapshot.selectedUnitID >= 0) {
        actions = "Right-click=Move  B=Found  F=Fortify  S=Sleep  Space=Skip  " + actions;
    }
    int tw = 0, th = 0;
    TTF_SizeText(m_fontSmall, actions.c_str(), &tw, &th);
    int barX = (m_windowW - tw) / 2 - 8;
    SDL_Rect bg = {barX, barY, tw + 16, barH};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(m_renderer, &bg);
    drawTextCentered(actions, m_windowW/2, barY + barH/2, m_fontSmall, 180, 200, 180);
}

// ---------- Minimap ----------

void Renderer::drawMinimap(const MapSnapshot& snapshot)
{
    int mmMaxW = 200, mmMaxH = 140, margin = 10;
    float scaleX = (float)mmMaxW / snapshot.width;
    float scaleY = (float)mmMaxH / snapshot.height;
    float scale = std::min(scaleX, scaleY);
    int mmW = (int)(snapshot.width * scale);
    int mmH = (int)(snapshot.height * scale);
    int mmX = m_windowW - mmW - margin;
    int mmY = m_windowH - mmH - margin;
    m_mmX = mmX; m_mmY = mmY; m_mmW = mmW; m_mmH = mmH;

    SDL_Rect bg = {mmX-2, mmY-2, mmW+4, mmH+4};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(m_renderer, &bg);

    for (int y = 0; y < snapshot.height; y++) {
        for (int x = 0; x < snapshot.width; x++) {
            const PlotData& plot = snapshot.getPlot(x, y);
            TerrainColor tc = getTerrainColor(plot.terrainType, plot.plotType);
            uint8_t r = tc.r, g = tc.g, b = tc.b;
            if (plot.isCity) { r = plot.ownerColorR; g = plot.ownerColorG; b = plot.ownerColorB; }
            int flippedY = snapshot.height - 1 - y;
            int px = mmX + (int)(x * scale), py = mmY + (int)(flippedY * scale);
            int pw = std::max(1, (int)scale), ph = std::max(1, (int)scale);
            SDL_Rect r2 = {px, py, pw, ph};
            SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
            SDL_RenderFillRect(m_renderer, &r2);
        }
    }

    // Viewport rectangle
    float mapPixelW = snapshot.width * TILE_SIZE;
    float mapPixelH = snapshot.height * TILE_SIZE;
    SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
    float fracLeft = m_camera.offsetX / mapPixelW;
    float fracRight = (m_camera.offsetX + m_windowW / m_camera.zoom) / mapPixelW;
    float fracTop = m_camera.offsetY / mapPixelH;
    float fracBot = (m_camera.offsetY + m_windowH / m_camera.zoom) / mapPixelH;
    auto clamp01 = [](float v) { return std::max(0.0f, std::min(v, 1.0f)); };
    SDL_Rect vp = {mmX + (int)(clamp01(fracLeft)*mmW), mmY + (int)(clamp01(fracTop)*mmH),
                   (int)((clamp01(fracRight)-clamp01(fracLeft))*mmW),
                   (int)((clamp01(fracBot)-clamp01(fracTop))*mmH)};
    if (vp.w > 0 && vp.h > 0) SDL_RenderDrawRect(m_renderer, &vp);
}

// ---------- Help overlay ----------

void Renderer::drawHelpOverlay()
{
    if (!m_fontMedium) return;
    const char* lines[] = {
        "Controls:",
        "  WASD/Arrows    - Pan camera",
        "  Mouse wheel    - Zoom in/out",
        "  Middle-drag    - Pan camera",
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
    int boxW = maxW + padX*2, boxH = numLines*lineH + padY*2;
    int boxX = (m_windowW - boxW)/2, boxY = (m_windowH - boxH)/2;

    SDL_Rect bg = {boxX, boxY, boxW, boxH};
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 220);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 100, 120, 100, 255);
    SDL_RenderDrawRect(m_renderer, &bg);

    for (int i = 0; i < numLines; i++) {
        uint8_t r = (i==0)?255:200, g = (i==0)?255:220, b = (i==0)?200:200;
        drawText(lines[i], boxX+padX, boxY+padY+i*lineH, m_fontMedium, r, g, b);
    }
}

// ---------- Tooltip ----------

void Renderer::drawTooltip(const MapSnapshot& snapshot)
{
    if (!m_fontSmall || snapshot.width == 0) return;
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
            strncat(line2, tmp, sizeof(line2)-strlen(line2)-1);
        }
    } else if (plot.isCity) {
        snprintf(line2, sizeof(line2), "%s (pop %d)", plot.cityName.c_str(), plot.cityPopulation);
    }

    int tipX = m_mouseX + 16, tipY = m_mouseY + 4;
    int tw1=0, th1=0, tw2=0, th2=0;
    TTF_SizeText(m_fontSmall, line1, &tw1, &th1);
    if (line2[0]) TTF_SizeText(m_fontSmall, line2, &tw2, &th2);
    int boxW = std::max(tw1, tw2) + 8;
    int boxH = th1 + (line2[0] ? th2+2 : 0) + 6;
    if (tipX + boxW > m_windowW) tipX = m_mouseX - boxW - 8;
    if (tipY + boxH > m_windowH) tipY = m_mouseY - boxH - 4;

    SDL_Rect bg = {tipX, tipY, boxW, boxH};
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 30, 220);
    SDL_RenderFillRect(m_renderer, &bg);
    SDL_SetRenderDrawColor(m_renderer, 80, 100, 80, 255);
    SDL_RenderDrawRect(m_renderer, &bg);
    drawText(line1, tipX+4, tipY+3, m_fontSmall, 220, 230, 200);
    if (line2[0]) drawText(line2, tipX+4, tipY+3+th1+2, m_fontSmall, 180, 200, 180);
}

// ---------- Player panel ----------

void Renderer::drawPlayerPanel(const MapSnapshot& snapshot)
{
    if (!m_fontSmall) return;
    int panelW = 220, lineH = 16, padX = 8, padY = 6;
    int alive = 0;
    for (int p = 0; p < snapshot.numPlayers; p++) if (snapshot.players[p].alive) alive++;
    int panelH = padY*2 + (alive+1)*lineH;
    int panelX = 0, panelY = 28;

    SDL_Rect bg = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawColor(m_renderer, 10, 10, 20, 210);
    SDL_RenderFillRect(m_renderer, &bg);

    drawText("Player       Cities Pop Score", panelX+padX, panelY+padY, m_fontSmall, 180, 190, 160);
    int row = 1;
    for (int p = 0; p < snapshot.numPlayers; p++) {
        const PlayerInfo& pi = snapshot.players[p];
        if (!pi.alive) continue;
        int ly = panelY + padY + row*lineH;
        SDL_Rect swatch = {panelX+padX, ly+2, 10, 10};
        SDL_SetRenderDrawColor(m_renderer, pi.colorR, pi.colorG, pi.colorB, 255);
        SDL_RenderFillRect(m_renderer, &swatch);
        char buf[128];
        snprintf(buf, sizeof(buf), "%-12s %3d  %3d  %4d%s",
                 pi.civName.c_str(), pi.numCities, pi.totalPop, pi.score,
                 pi.isHuman ? " *" : "");
        drawText(buf, panelX+padX+14, ly, m_fontSmall, 200, 210, 200);
        row++;
    }
}

// ---------- Input handling ----------

void Renderer::handleKeyDown(SDL_Keycode key, MapSnapshot& snapshot)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = true;    break;
        case SDLK_DOWN:  case SDLK_s:
            // S = sleep if unit selected, else camera pan
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
            if (SDL_GetModState() & KMOD_CTRL) break; // Ctrl+F reserved
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.selectedUnitID >= 0 && m_pushCommand) {
                    // F = fortify
                    GameCommand cmd; cmd.type = GameCommand::FORTIFY;
                    cmd.id = snapshot.selectedUnitID;
                    m_pushCommand(cmd);
                } else if (snapshot.width > 0) {
                    autoFitCamera(snapshot);
                }
            }
            break;
        case SDLK_HOME:
            {
                std::lock_guard<std::mutex> lock(snapshot.mtx);
                if (snapshot.width > 0) autoFitCamera(snapshot);
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

void Renderer::handleKeyUp(SDL_Keycode key)
{
    switch (key) {
        case SDLK_UP:    case SDLK_w: m_keyUp = false;    break;
        case SDLK_DOWN:               m_keyDown = false;   break;
        case SDLK_LEFT:  case SDLK_a: m_keyLeft = false;  break;
        case SDLK_RIGHT: case SDLK_d: m_keyRight = false; break;
    }
}

void Renderer::handleMouseWheel(int y, int mouseX, int mouseY)
{
    float oldZoom = m_camera.zoom;
    if (y > 0) m_camera.zoom *= 1.15f;
    else if (y < 0) m_camera.zoom /= 1.15f;
    m_camera.zoom = std::max(0.1f, std::min(m_camera.zoom, 20.0f));
    float worldMouseX = mouseX / oldZoom + m_camera.offsetX;
    float worldMouseY = mouseY / oldZoom + m_camera.offsetY;
    m_camera.offsetX = worldMouseX - mouseX / m_camera.zoom;
    m_camera.offsetY = worldMouseY - mouseY / m_camera.zoom;
}

void Renderer::handleMouseMotion(int dx, int dy, bool anyDragButton)
{
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
    // Minimap click-to-jump (left click)
    if (button == SDL_BUTTON_LEFT && m_showMinimap && m_mmW > 0 && m_mmH > 0) {
        if (mouseX >= m_mmX && mouseX < m_mmX + m_mmW &&
            mouseY >= m_mmY && mouseY < m_mmY + m_mmH)
        {
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

    // Map clicks
    int tileX, tileY;
    {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        if (!screenToTile(mouseX, mouseY, snapshot, tileX, tileY)) return;
    }

    if (button == SDL_BUTTON_LEFT && m_pushCommand) {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        const PlotData& plot = snapshot.getPlot(tileX, tileY);

        if (plot.hasHumanUnit) {
            // Select human unit
            GameCommand cmd; cmd.type = GameCommand::SELECT_UNIT;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
            // Close city screen if open
            if (snapshot.cityScreenOpen)
                m_pushCommand({GameCommand::CLOSE_CITY});
        } else if (plot.isCity && plot.ownerID == snapshot.humanPlayerID) {
            // Select human city
            GameCommand cmd; cmd.type = GameCommand::SELECT_CITY;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
        } else {
            // Deselect
            m_pushCommand({GameCommand::DESELECT});
            if (snapshot.cityScreenOpen)
                m_pushCommand({GameCommand::CLOSE_CITY});
        }
    }
    else if (button == SDL_BUTTON_RIGHT && m_pushCommand) {
        std::lock_guard<std::mutex> lock(snapshot.mtx);
        if (snapshot.selectedUnitID >= 0) {
            GameCommand cmd; cmd.type = GameCommand::MOVE_UNIT;
            cmd.id = snapshot.selectedUnitID;
            cmd.x = tileX; cmd.y = tileY;
            m_pushCommand(cmd);
        }
    }
}

void Renderer::handleResize(int newW, int newH)
{
    m_windowW = newW;
    m_windowH = newH;
}
