// OpenCiv4 — Clean-room 64-bit engine replacement for Civilization IV
// Phase 0: Headless mode — run AI games with no graphics
//
// This is the host executable entry point. It:
// 1. Creates stub interface implementations (no graphics, no audio, no Python)
// 2. Wires them into CvGlobals via setDLLIFace()
// 3. Calls GC.init() to bring up the gamecore
// 4. Loads XML data (units, techs, buildings, civs, etc.)
// 5. Initializes a game with AI players
// 6. Runs the game loop

#include <cstdio>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <windows.h>

#include <SDL.h>

// Crash handler to print which turn/phase we were in when crashing
static volatile int g_crashTurn = -1;
static volatile int g_crashPlayer = -1;
static volatile int g_crashPhase = -1; // 0=doTurn,1=doTurnUnits,2=AI_unitUpdate,3=game.doTurn
volatile int g_crashSubPhase = -1; // Sub-phase within AI_unitUpdate (set from gamecore)

static void crashHandler(int sig) {
    fprintf(stderr, "\n!!! CRASH (signal %d) at turn=%d player=%d phase=%d sub=%d !!!\n",
            sig, g_crashTurn, g_crashPlayer, g_crashPhase, g_crashSubPhase);
    fflush(stderr);
    _exit(139);
}

// Windows Vectored Exception Handler - captures faulting address for access violations
static LONG WINAPI vehHandler(EXCEPTION_POINTERS* pExInfo) {
    if (pExInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR readWrite = pExInfo->ExceptionRecord->ExceptionInformation[0]; // 0=read, 1=write
        ULONG_PTR faultAddr = pExInfo->ExceptionRecord->ExceptionInformation[1];
        ULONG_PTR instrAddr = (ULONG_PTR)pExInfo->ExceptionRecord->ExceptionAddress;
        fprintf(stderr, "\n!!! ACCESS VIOLATION: %s at address 0x%llX (instruction at 0x%llX)\n",
                readWrite == 0 ? "READ" : "WRITE",
                (unsigned long long)faultAddr, (unsigned long long)instrAddr);
        fprintf(stderr, "    turn=%d player=%d phase=%d sub=%d\n",
                g_crashTurn, g_crashPlayer, g_crashPhase, g_crashSubPhase);
        // Print RIP, RSP, and a few registers for context
        CONTEXT* ctx = pExInfo->ContextRecord;
        fprintf(stderr, "    RIP=0x%llX RSP=0x%llX RBP=0x%llX\n",
                (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp, (unsigned long long)ctx->Rbp);
        fprintf(stderr, "    RAX=0x%llX RBX=0x%llX RCX=0x%llX RDX=0x%llX\n",
                (unsigned long long)ctx->Rax, (unsigned long long)ctx->Rbx,
                (unsigned long long)ctx->Rcx, (unsigned long long)ctx->Rdx);
        fflush(stderr);
        _exit(139);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


// Pull in the gamecore's global singleton and types
#include "CvGameCoreDLL.h"
#include "CvXMLLoadUtility.h"
#include "CvArtFileMgr.h"
#include "CvInitCore.h"
#include "CvGameAI.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "AI_Defines.h"
#include "CvMap.h"
#include "CvMapGenerator.h"

// Our stub implementations of all 13 interface classes
#include "StubInterfaces.h"

// Phase 1: Map viewer
#include "MapSnapshot.h"
#include "Renderer.h"

// Global map snapshot shared between game thread and render thread
static MapSnapshot g_mapSnapshot;
static std::atomic<bool> g_gameRunning{true};
static std::atomic<bool> g_gameThreadDone{false};

// ---- Helper: find valid civ/leader pairs from loaded XML data ----
struct CivLeaderPair {
    CivilizationTypes eCiv;
    LeaderHeadTypes eLeader;
    const wchar_t* szCivDesc;
};

static int findCivLeaderPairs(CivLeaderPair* pairs, int maxPairs)
{
    int count = 0;
    for (int iCiv = 0; iCiv < GC.getNumCivilizationInfos() && count < maxPairs; iCiv++)
    {
        CvCivilizationInfo& civInfo = GC.getCivilizationInfo((CivilizationTypes)iCiv);

        // Skip barbarian/minor civs (they have isPlayable() == false)
        if (!civInfo.isPlayable())
            continue;

        // Find a valid leader for this civ
        for (int iLeader = 0; iLeader < GC.getNumLeaderHeadInfos(); iLeader++)
        {
            if (civInfo.isLeaders((LeaderHeadTypes)iLeader))
            {
                pairs[count].eCiv = (CivilizationTypes)iCiv;
                pairs[count].eLeader = (LeaderHeadTypes)iLeader;
                pairs[count].szCivDesc = civInfo.getDescription();
                count++;
                break; // One leader per civ is enough
            }
        }
    }
    return count;
}

// ---- Populate MapSnapshot from game state ----
static void updateMapSnapshot()
{
    CvMap& map = GC.getMapINLINE();
    int w = map.getGridWidth();
    int h = map.getGridHeight();

    // Build a local copy first, then swap under lock
    std::vector<PlotData> newPlots(w * h);
    for (int i = 0; i < w * h; i++) {
        CvPlot* pPlot = map.plotByIndexINLINE(i);
        PlotData& pd = newPlots[i];

        pd.terrainType = (int)pPlot->getTerrainType();
        pd.plotType = (int)pPlot->getPlotType();
        pd.featureType = (int)pPlot->getFeatureType();
        pd.ownerID = (int)pPlot->getOwnerINLINE();
        pd.isRiver = pPlot->isRiver();
        pd.isCity = pPlot->isCity();
        pd.unitCount = pPlot->getNumUnits();

        // Get owner's primary color
        pd.ownerColorR = pd.ownerColorG = pd.ownerColorB = 255; // default white
        if (pd.ownerID >= 0 && pd.ownerID < MAX_PLAYERS) {
            CvPlayer& kOwner = GET_PLAYER((PlayerTypes)pd.ownerID);
            PlayerColorTypes eColor = kOwner.getPlayerColor();
            if (eColor >= 0 && eColor < GC.getNumPlayerColorInfos()) {
                CvPlayerColorInfo& colorInfo = GC.getPlayerColorInfo(eColor);
                ColorTypes ePrimary = (ColorTypes)colorInfo.getColorTypePrimary();
                if (ePrimary >= 0 && ePrimary < GC.getNumColorInfos()) {
                    const NiColorA& c = GC.getColorInfo(ePrimary).getColor();
                    pd.ownerColorR = (uint8_t)(c.r * 255.0f);
                    pd.ownerColorG = (uint8_t)(c.g * 255.0f);
                    pd.ownerColorB = (uint8_t)(c.b * 255.0f);
                }
            }
        }

        // City name
        if (pd.isCity) {
            CvCity* pCity = pPlot->getPlotCity();
            if (pCity) {
                const wchar_t* wName = pCity->getName().GetCString();
                // Convert wchar_t to char (ASCII subset)
                if (wName) {
                    std::string name;
                    for (int c = 0; wName[c] && c < 64; c++)
                        name += (char)(wName[c] < 128 ? wName[c] : '?');
                    pd.cityName = name;
                }
            }
        }
    }

    // Swap into the shared snapshot under lock
    {
        std::lock_guard<std::mutex> lock(g_mapSnapshot.mtx);
        g_mapSnapshot.width = w;
        g_mapSnapshot.height = h;
        g_mapSnapshot.gameTurn = GC.getGameINLINE().getGameTurn();
        g_mapSnapshot.gameYear = GC.getGameINLINE().getGameTurnYear();
        g_mapSnapshot.wrapX = map.isWrapX();
        g_mapSnapshot.wrapY = map.isWrapY();
        g_mapSnapshot.plots = std::move(newPlots);
    }
}

// ---- Game thread function ----
static void gameThreadFunc()
{
    const int MAX_TURNS = 300;
    fprintf(stderr, "=== Starting headless game loop (max %d turns) ===\n", MAX_TURNS);

    // Take initial snapshot before any turns
    updateMapSnapshot();

    for (int iTurn = 0; iTurn < MAX_TURNS && g_gameRunning; iTurn++)
    {
        // Process each alive player's full turn
        for (int p = 0; p < MAX_PLAYERS && g_gameRunning; p++)
        {
            CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)p);
            if (kPlayer.isAlive())
            {
                GC.getGameINLINE().setActivePlayer((PlayerTypes)p);
                ((CvPlayerAI&)kPlayer).AI_updateFoundValues();

                g_crashTurn = iTurn; g_crashPlayer = p;
                g_crashPhase = 0;
                kPlayer.doTurn();

                // Force research if AI didn't pick one
                if (kPlayer.getCurrentResearch() == NO_TECH && kPlayer.isResearch()
                    && !kPlayer.isBarbarian())
                {
                    for (int t = 0; t < GC.getNumTechInfos(); t++)
                    {
                        if (kPlayer.canResearch((TechTypes)t))
                        {
                            kPlayer.pushResearch((TechTypes)t, true);
                            break;
                        }
                    }
                }

                g_crashPhase = 1;
                kPlayer.doTurnUnits();
                g_crashPhase = 2;
                kPlayer.AI_unitUpdate();
            }
        }
        GC.getGameINLINE().setActivePlayer((PlayerTypes)0);

        g_crashPhase = 3;
        GC.getGameINLINE().doTurn();

        // Update map snapshot for the renderer
        updateMapSnapshot();

        // Progress report every 50 turns
        if (iTurn % 50 == 0)
        {
            fprintf(stderr, "--- Turn %d ---\n", iTurn);
            for (int p = 0; p < MAX_CIV_PLAYERS; p++)
            {
                CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
                if (kPlayer.isAlive())
                {
                    int numTechs = 0;
                    for (int tt = 0; tt < GC.getNumTechInfos(); tt++)
                        if (GET_TEAM(kPlayer.getTeam()).isHasTech((TechTypes)tt)) numTechs++;
                    fprintf(stderr, "  P%d: cities=%d units=%d pop=%d techs=%d score=%d\n", p,
                            kPlayer.getNumCities(), kPlayer.getNumUnits(),
                            kPlayer.getTotalPopulation(), numTechs,
                            GC.getGameINLINE().getPlayerScore((PlayerTypes)p));
                }
            }
        }

        if (GC.getGameINLINE().getGameState() == GAMESTATE_OVER)
        {
            fprintf(stderr, "[game] Game over at turn %d!\n",
                    GC.getGameINLINE().getGameTurn());
            break;
        }
    }

    fprintf(stderr, "\n=== Game loop finished ===\n");
    fprintf(stderr, "Final game turn: %d\n", GC.getGameINLINE().getGameTurn());

    // Print final stats
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)i);
        if (kPlayer.isAlive())
        {
            int numTechs = 0;
            CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
            for (int t = 0; t < GC.getNumTechInfos(); t++)
                if (kTeam.isHasTech((TechTypes)t)) numTechs++;
            const char* civType = GC.getCivilizationInfo(kPlayer.getCivilizationType()).getType();
            fprintf(stderr, "  Player %d (%s): cities=%d units=%d pop=%d techs=%d score=%d\n",
                    i, civType ? civType : "???",
                    kPlayer.getNumCities(), kPlayer.getNumUnits(),
                    kPlayer.getTotalPopulation(), numTechs,
                    GC.getGameINLINE().getPlayerScore((PlayerTypes)i));
        }
    }

    g_gameThreadDone = true;
}

int main(int argc, char* argv[])
{
    // Register VEH first to catch access violations with full detail
    AddVectoredExceptionHandler(1, vehHandler);
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    HMODULE hModule = GetModuleHandle(NULL);
    fprintf(stderr, "=== OpenCiv4 Engine ===\n");
    fprintf(stderr, "Module base: 0x%llX\n", (unsigned long long)hModule);
    fprintf(stderr, "Phase 0: Headless Mode\n");
    fprintf(stderr, "Build: 64-bit (%zu-byte pointers)\n\n", sizeof(void*));
    fprintf(stderr, "BTS Install: %s\n\n", BTS_INSTALL_DIR);

    // ---- Step 1: Create our stub DLL interface ----
    OpenCiv4::StubUtilityIFace stubDLL;
    fprintf(stderr, "[main] Stub interfaces created.\n");

    // ---- Step 2: Wire up gDLL ----
    CvGlobals::getInstance().setDLLIFace(&stubDLL);
    fprintf(stderr, "[main] gDLL wired up.\n");

    // ---- Step 3: Initialize the gamecore ----
    fprintf(stderr, "[main] Calling GC.init()...\n");
    CvGlobals::getInstance().init();
    fprintf(stderr, "[main] GC.init() completed successfully!\n\n");

    // ---- Phase 0 status ----
    fprintf(stderr, "=== Gamecore initialized ===\n");
    fprintf(stderr, "CvGameAI: %s\n", GC.getGamePointer() ? "allocated" : "NULL");
    fprintf(stderr, "CvMap:    %s\n", &GC.getMap() ? "allocated" : "NULL");
    fprintf(stderr, "\n");

    // ---- Step 4: Load XML data ----
    fprintf(stderr, "[main] Loading XML data...\n");
    {
        CvXMLLoadUtility xml;

        fprintf(stderr, "[main]   SetGlobalDefines...\n");
        if (!xml.SetGlobalDefines())
            fprintf(stderr, "[main]   WARNING: SetGlobalDefines failed\n");

        fprintf(stderr, "[main]   SetGlobalTypes...\n");
        if (!xml.SetGlobalTypes())
            fprintf(stderr, "[main]   WARNING: SetGlobalTypes failed\n");

        fprintf(stderr, "[main]   LoadBasicInfos...\n");
        if (!xml.LoadBasicInfos())
            fprintf(stderr, "[main]   WARNING: LoadBasicInfos failed\n");

        fprintf(stderr, "[main]   ARTFILEMGR.Init()...\n");
        ARTFILEMGR.Init();

        fprintf(stderr, "[main]   SetGlobalArtDefines...\n");
        if (!xml.SetGlobalArtDefines())
            fprintf(stderr, "[main]   WARNING: SetGlobalArtDefines failed\n");

        fprintf(stderr, "[main]   buildArtFileInfoMaps...\n");
        ARTFILEMGR.buildArtFileInfoMaps();

        fprintf(stderr, "[main]   LoadPreMenuGlobals...\n");
        if (!xml.LoadPreMenuGlobals())
            fprintf(stderr, "[main]   WARNING: LoadPreMenuGlobals failed\n");

        fprintf(stderr, "[main]   SetPostGlobalsGlobalDefines...\n");
        if (!xml.SetPostGlobalsGlobalDefines())
            fprintf(stderr, "[main]   WARNING: SetPostGlobalsGlobalDefines failed\n");

        fprintf(stderr, "[main]   LoadPostMenuGlobals...\n");
        if (!xml.LoadPostMenuGlobals())
            fprintf(stderr, "[main]   WARNING: LoadPostMenuGlobals failed\n");
    }
    fprintf(stderr, "[main] XML loading complete.\n\n");

    // ---- Step 5: Configure game settings via CvInitCore ----
    fprintf(stderr, "[main] Configuring game...\n");
    {
        CvInitCore& initCore = GC.getInitCore();

        // Game type: single player new game
        initCore.setType(GAME_SP_NEW);

        // World size: STANDARD (index 3 in standard BTS: 0=Duel,1=Tiny,2=Small,3=Standard,4=Large,5=Huge)
        // Tiny (13x8=104 plots) is too small for 4 players — MIN_CITY_RANGE blocks nearly
        // all potential second-city sites. Standard (21x13=273) gives room for expansion.
        initCore.setWorldSize((WorldSizeTypes)3);

        // Climate: TEMPERATE (index 1 in standard BTS)
        initCore.setClimate((ClimateTypes)1);

        // Sea level: MEDIUM (index 1 in standard BTS)
        initCore.setSeaLevel((SeaLevelTypes)1);

        // Game speed: NORMAL (index 2 in standard BTS: 0=Marathon,1=Epic,2=Normal,3=Quick)
        initCore.setGameSpeed((GameSpeedTypes)2);

        // Era: ANCIENT (index 0)
        initCore.setEra((EraTypes)0);

        // Find playable civs
        const int MAX_AI_PLAYERS = 4; // Keep it manageable
        CivLeaderPair pairs[32];
        int numPairs = findCivLeaderPairs(pairs, 32);
        int numPlayers = (numPairs < MAX_AI_PLAYERS) ? numPairs : MAX_AI_PLAYERS;

        fprintf(stderr, "[main]   Found %d playable civ/leader pairs, using %d\n",
                numPairs, numPlayers);

        // Handicap: use index 4 which is typically "Noble" (middle difficulty)
        // Noble = fair AI, good for testing
        HandicapTypes eHandicap = (HandicapTypes)4;
        if (GC.getNumHandicapInfos() <= 4)
            eHandicap = (HandicapTypes)(GC.getNumHandicapInfos() / 2);

        // Configure each AI player slot
        for (int i = 0; i < numPlayers; i++)
        {
            PlayerTypes ePlayer = (PlayerTypes)i;

            initCore.setSlotStatus(ePlayer, SS_COMPUTER);
            initCore.setCiv(ePlayer, pairs[i].eCiv);
            initCore.setLeader(ePlayer, pairs[i].eLeader);
            initCore.setTeam(ePlayer, (TeamTypes)i);  // Each player on own team
            initCore.setHandicap(ePlayer, eHandicap);
            initCore.setColor(ePlayer, (PlayerColorTypes)i);

            fprintf(stderr, "[main]   Player %d: %ls (AI)\n", i, pairs[i].szCivDesc);
        }

        // Close remaining slots (CvInitCore defaults each team to player index)
        for (int i = numPlayers; i < MAX_CIV_PLAYERS; i++)
        {
            initCore.setSlotStatus((PlayerTypes)i, SS_CLOSED);
        }

        // Set game handicap (global difficulty)
        initCore.setHandicap(BARBARIAN_PLAYER, eHandicap);

        // ---- Initialize barbarian player (index 18) ----
        // Without this, CvGame::doTurn() -> createBarbarianCities()/createBarbarianUnits()
        // accesses GET_PLAYER(BARBARIAN_PLAYER) which has NO_CIVILIZATION, causing
        // getCivilizationInfo(-1) -> out-of-bounds array access -> SEGFAULT.
        {
            CivilizationTypes eBarbCiv = (CivilizationTypes)GC.getDefineINT("BARBARIAN_CIVILIZATION");
            LeaderHeadTypes eBarbLeader = (LeaderHeadTypes)GC.getDefineINT("BARBARIAN_LEADER");

            if (eBarbCiv != NO_CIVILIZATION && eBarbLeader != NO_LEADER)
            {
                initCore.setSlotStatus(BARBARIAN_PLAYER, SS_COMPUTER);
                initCore.setCiv(BARBARIAN_PLAYER, eBarbCiv);
                initCore.setLeader(BARBARIAN_PLAYER, eBarbLeader);
                initCore.setTeam(BARBARIAN_PLAYER, BARBARIAN_TEAM);
                // Use a color that doesn't conflict with regular players.
                // PlayerColorTypes count is usually > MAX_CIV_PLAYERS, pick the last one.
                initCore.setColor(BARBARIAN_PLAYER, (PlayerColorTypes)(GC.getNumPlayerColorInfos() - 1));

                fprintf(stderr, "[main]   Barbarian player (slot %d): civ=%d leader=%d team=%d\n",
                        (int)BARBARIAN_PLAYER, (int)eBarbCiv, (int)eBarbLeader, (int)BARBARIAN_TEAM);
            }
            else
            {
                fprintf(stderr, "[main]   WARNING: Could not find barbarian civ (%d) or leader (%d), "
                        "disabling barbarians\n", (int)eBarbCiv, (int)eBarbLeader);
                initCore.setOption(GAMEOPTION_NO_BARBARIANS, true);
            }
        }
    }
    fprintf(stderr, "[main] Game configured.\n\n");

    // ---- Step 6: Initialize game and map ----
    fprintf(stderr, "[main] Initializing game...\n");

    // CvGame::init() sets up game state, RNG seeds, turn counters
    HandicapTypes eHandicap = (HandicapTypes)4;
    if (GC.getNumHandicapInfos() <= 4)
        eHandicap = (HandicapTypes)(GC.getNumHandicapInfos() / 2);

    GC.getGameINLINE().init(eHandicap);
    fprintf(stderr, "[main] CvGame::init() complete.\n");

    // CvMap::init() creates the plot grid, initializes pathfinders, calculates areas
    fprintf(stderr, "[main] Initializing map...\n");
    GC.getMapINLINE().init();
    fprintf(stderr, "[main] CvMap::init() complete. Grid: %dx%d (%d plots)\n",
            GC.getMapINLINE().getGridWidth(), GC.getMapINLINE().getGridHeight(),
            GC.getMapINLINE().numPlots());

    // Generate terrain, features, bonuses using default C++ map generator
    fprintf(stderr, "[main] Generating random map...\n");
    CvMapGenerator::GetInstance().generateRandomMap();
    fprintf(stderr, "[main] Map generation complete.\n");

    // Add game elements (rivers, lakes, features, bonuses, goodies)
    fprintf(stderr, "[main] Adding game elements...\n");
    CvMapGenerator::GetInstance().addGameElements();
    fprintf(stderr, "[main] Game elements added.\n");

    // Update cached plot yields — CvPlot::getYield() returns m_aiYield[] which
    // starts at 0. updateYield() calculates the real value from terrain, features,
    // bonuses, etc. and caches it. Without this, AI_foundValue() sees all-zero
    // yields and never identifies valid city sites → no settlers ever built.
    fprintf(stderr, "[main] Updating plot yields...\n");
    GC.getMapINLINE().updateYield();
    fprintf(stderr, "[main] Plot yields updated.\n");

    // Initialize teams and players
    fprintf(stderr, "[main] Initializing teams and players...\n");
    for (int i = 0; i < MAX_TEAMS; i++)
    {
        if (GC.getInitCore().getSlotStatus((PlayerTypes)i) == SS_COMPUTER)
        {
            GET_TEAM((TeamTypes)i).init((TeamTypes)i);
        }
    }
    // Init ALL player slots — even closed ones need valid team/state
    // (BTS exe initializes all slots; game code assumes getTeam() is valid)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        GET_PLAYER((PlayerTypes)i).init((PlayerTypes)i);
    }

    // Set initial items: free techs, starting plots, starting units
    fprintf(stderr, "[main] Setting initial items...\n");
    GC.getGameINLINE().setInitialItems();
    fprintf(stderr, "[main] Initial items set.\n\n");

    // Recalculate score maximums now that the map is generated
    // (CvGame::init called initScoreCalculation before the map existed)
    GC.getGameINLINE().initScoreCalculation();

    // Set active player so getActivePlayer() doesn't return NO_PLAYER
    GC.getInitCore().setActivePlayer((PlayerTypes)0);
    fprintf(stderr, "[main] Active player set to 0.\n");

    // Reveal all plots for all teams.  In a real BTS game, fog of war is
    // peeled away incrementally as units explore.  Our headless engine
    // doesn't fully propagate visibility from units/cities, so the AI
    // never reveals tiles beyond its starting area.  AI_foundValue()
    // returns 0 for unrevealed plots → settlers are never trained.
    // Revealing everything is equivalent to running with "debug mode" on.
    fprintf(stderr, "[main] Revealing all plots for every team...\n");
    for (int i = 0; i < GC.getMapINLINE().numPlotsINLINE(); i++)
    {
        CvPlot* pPlot = GC.getMapINLINE().plotByIndexINLINE(i);
        for (int t = 0; t < MAX_TEAMS; t++)
        {
            if (GET_TEAM((TeamTypes)t).isAlive())
            {
                pPlot->setRevealed((TeamTypes)t, true, false, NO_TEAM, false);
            }
        }
    }
    fprintf(stderr, "[main] All plots revealed.\n");

    // Mark game as ready
    GC.getGameINLINE().setFinalInitialized(true);

    // Establish diplomatic contact between all alive teams.
    // In real BTS, teams "meet" when their units encounter each other.
    // Without this, AI_doWar() checks isHasMet() and never declares war.
    // Use setHasMetDirect() to set the flag without side effects.
    fprintf(stderr, "[main] Establishing contact between all teams...\n");
    for (int i = 0; i < MAX_TEAMS; i++)
    {
        if (!GET_TEAM((TeamTypes)i).isAlive())
            continue;
        for (int j = i + 1; j < MAX_TEAMS; j++)
        {
            if (!GET_TEAM((TeamTypes)j).isAlive())
                continue;
            GET_TEAM((TeamTypes)i).setHasMetDirect((TeamTypes)j);
            GET_TEAM((TeamTypes)j).setHasMetDirect((TeamTypes)i);
        }
    }
    fprintf(stderr, "[main] All teams have met.\n");

    // Print starting positions
    for (int p = 0; p < MAX_CIV_PLAYERS; p++)
    {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
        if (kPlayer.isAlive())
        {
            CvPlot* pStart = kPlayer.getStartingPlot();
            fprintf(stderr, "[main] P%d: start=(%d,%d) units=%d\n",
                    p,
                    pStart ? pStart->getX_INLINE() : -1,
                    pStart ? pStart->getY_INLINE() : -1,
                    kPlayer.getNumUnits());
        }
    }

    // Seed AI found values so the first production decision has valid city site data
    for (int p = 0; p < MAX_CIV_PLAYERS; p++)
    {
        CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)p);
        if (kPlayer.isAlive())
        {
            kPlayer.AI_updateFoundValues();
            int iSites = kPlayer.AI_getNumCitySites();
            fprintf(stderr, "[main] P%d: %d city sites found after initial AI_updateFoundValues\n", p, iSites);
        }
    }

    // ---- Step 7: Check for --headless flag ----
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
            break;
        }
    }

    if (headless) {
        // ---- Headless mode: run game loop on main thread (Phase 0 behavior) ----
        fprintf(stderr, "[main] Running in HEADLESS mode (no window).\n");
        gameThreadFunc();
    } else {
        // ---- Windowed mode: SDL2 window + game on background thread ----
        fprintf(stderr, "[main] Initializing SDL2...\n");
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "[main] SDL_Init failed: %s\n", SDL_GetError());
            fprintf(stderr, "[main] Falling back to headless mode.\n");
            gameThreadFunc();
        } else {
            int winW = 1280, winH = 720;
            SDL_Window* window = SDL_CreateWindow(
                "OpenCiv4 — Phase 1 Map Viewer",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                winW, winH,
                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
            );
            if (!window) {
                fprintf(stderr, "[main] SDL_CreateWindow failed: %s\n", SDL_GetError());
                SDL_Quit();
                gameThreadFunc();
            } else {
                SDL_Renderer* sdlRenderer = SDL_CreateRenderer(
                    window, -1,
                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
                );
                if (!sdlRenderer) {
                    fprintf(stderr, "[main] SDL_CreateRenderer failed: %s\n", SDL_GetError());
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    gameThreadFunc();
                } else {
                    fprintf(stderr, "[main] SDL2 window created (%dx%d).\n", winW, winH);

                    // Create renderer
                    Renderer renderer(sdlRenderer, winW, winH);

                    // Start game on background thread
                    fprintf(stderr, "[main] Starting game thread...\n");
                    std::thread gameThread(gameThreadFunc);

                    // ---- Main render loop ----
                    bool running = true;
                    while (running) {
                        SDL_Event event;
                        while (SDL_PollEvent(&event)) {
                            switch (event.type) {
                                case SDL_QUIT:
                                    running = false;
                                    break;
                                case SDL_KEYDOWN:
                                    if (event.key.keysym.sym == SDLK_ESCAPE)
                                        running = false;
                                    else
                                        renderer.handleKeyDown(event.key.keysym.sym, g_mapSnapshot);
                                    break;
                                case SDL_KEYUP:
                                    renderer.handleKeyUp(event.key.keysym.sym);
                                    break;
                                case SDL_MOUSEWHEEL:
                                    {
                                        int mx, my;
                                        SDL_GetMouseState(&mx, &my);
                                        renderer.handleMouseWheel(event.wheel.y, mx, my);
                                    }
                                    break;
                                case SDL_MOUSEMOTION:
                                    renderer.handleMouseMotion(
                                        event.motion.xrel, event.motion.yrel,
                                        (event.motion.state & (SDL_BUTTON_MMASK | SDL_BUTTON_RMASK)) != 0
                                    );
                                    break;
                                case SDL_WINDOWEVENT:
                                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                                        renderer.handleResize(event.window.data1, event.window.data2);
                                    }
                                    break;
                            }
                        }

                        // Draw the map
                        renderer.draw(g_mapSnapshot);

                        // If game thread is done and we're still running, keep showing the final state
                        // User can close window with X or ESC
                    }

                    // Signal game thread to stop and wait for it
                    g_gameRunning = false;
                    if (gameThread.joinable())
                        gameThread.join();

                    SDL_DestroyRenderer(sdlRenderer);
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    fprintf(stderr, "[main] SDL2 shutdown complete.\n");
                }
            }
        }
    }

    // ---- Cleanup ----
    fprintf(stderr, "\n[main] Calling GC.uninit()...\n");
    CvGlobals::getInstance().uninit();
    fprintf(stderr, "[main] Shutdown complete.\n");

    return 0;
}
