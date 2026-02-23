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

// Pull in the gamecore's global singleton and types
#include "CvGameCoreDLL.h"
#include "CvXMLLoadUtility.h"
#include "CvArtFileMgr.h"
#include "CvInitCore.h"
#include "CvGameAI.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "CvMap.h"
#include "CvMapGenerator.h"

// Our stub implementations of all 13 interface classes
#include "StubInterfaces.h"

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

int main(int argc, char* argv[])
{
    fprintf(stderr, "=== OpenCiv4 Engine ===\n");
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

        // Close remaining slots
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
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (GC.getInitCore().getSlotStatus((PlayerTypes)i) == SS_COMPUTER)
        {
            GET_PLAYER((PlayerTypes)i).init((PlayerTypes)i);
            fprintf(stderr, "[main]   Initialized player %d: alive=%s\n",
                    i, GET_PLAYER((PlayerTypes)i).isAlive() ? "yes" : "no");
        }
    }

    // Set initial items: free techs, starting plots, starting units
    fprintf(stderr, "[main] Setting initial items...\n");
    GC.getGameINLINE().setInitialItems();
    fprintf(stderr, "[main] Initial items set.\n\n");

    // Recalculate score maximums now that the map is generated
    // (CvGame::init called initScoreCalculation before the map existed)
    GC.getGameINLINE().initScoreCalculation();

    // Mark game as ready
    GC.getGameINLINE().setFinalInitialized(true);

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

    // ---- Step 7: Run headless game loop ----
    // Simulates the BTS sequential turn flow:
    //   For each player in order:
    //     1. doTurnUnits()    — refreshes AI found values, resets unit movement points
    //     2. AI_unitUpdate()  — units execute AI (move, settle, attack)
    //     3. doTurn()         — cities process production, AI chooses next build
    //   Then CvGame::doTurn() advances the global game state.
    //
    // This matches the real BTS flow in setTurnActive(true)/setTurnActive(false):
    //   setTurnActive(true)  → doTurnUnits()  (line 9818 in CvPlayer.cpp)
    //   updateMoves()        → AI_unitUpdate()
    //   setTurnActive(false) → doTurn()       (line 9880 in CvPlayer.cpp)
    const int MAX_TURNS = 300;
    fprintf(stderr, "=== Starting headless game loop (max %d turns) ===\n", MAX_TURNS);

    for (int iTurn = 0; iTurn < MAX_TURNS; iTurn++)
    {
        // Process each alive player's full turn (BTS sequential order)
        for (int p = 0; p < MAX_PLAYERS; p++)
        {
            CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)p);
            if (kPlayer.isAlive())
            {
                GC.getGameINLINE().setActivePlayer((PlayerTypes)p);

                // Refresh AI found values before production decisions so the AI
                // knows about valid city sites (needed for settler training).
                ((CvPlayerAI&)kPlayer).AI_updateFoundValues();

                // Turn processing: doTurn (cities produce), doTurnUnits (refresh
                // movement), AI_unitUpdate (units act — settlers found, warriors move).
                kPlayer.doTurn();
                kPlayer.doTurnUnits();
                kPlayer.AI_unitUpdate();
            }
        }
        GC.getGameINLINE().setActivePlayer((PlayerTypes)0);

        // Advance the game turn (team turns, map turn, barbarians, etc.)
        GC.getGameINLINE().doTurn();

        // Progress report every 10 turns
        if (iTurn % 10 == 0)
        {
            fprintf(stderr, "  Turn %3d:", iTurn);
            for (int p = 0; p < MAX_CIV_PLAYERS; p++)
            {
                CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
                if (kPlayer.isAlive())
                {
                    fprintf(stderr, "  P%d[c=%d u=%d pop=%d sc=%d]", p,
                            kPlayer.getNumCities(), kPlayer.getNumUnits(),
                            kPlayer.getTotalPopulation(),
                            GC.getGameINLINE().getPlayerScore((PlayerTypes)p));
                }
            }
            fprintf(stderr, "\n");
        }

        // Check if game ended
        if (GC.getGameINLINE().getGameState() == GAMESTATE_OVER)
        {
            fprintf(stderr, "[main] Game over at turn %d!\n",
                    GC.getGameINLINE().getGameTurn());
            break;
        }
    }

    fprintf(stderr, "\n=== Headless game loop finished ===\n");
    fprintf(stderr, "Final game turn: %d\n", GC.getGameINLINE().getGameTurn());

    // Print final stats
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)i);
        if (kPlayer.isAlive())
        {
            // Count known techs
            int numTechs = 0;
            CvTeam& kTeam = GET_TEAM(kPlayer.getTeam());
            for (int t = 0; t < GC.getNumTechInfos(); t++)
            {
                if (kTeam.isHasTech((TechTypes)t))
                    numTechs++;
            }
            const char* civType = GC.getCivilizationInfo(kPlayer.getCivilizationType()).getType();
            fprintf(stderr, "  Player %d (%s): cities=%d units=%d pop=%d techs=%d score=%d\n",
                    i, civType ? civType : "???",
                    kPlayer.getNumCities(), kPlayer.getNumUnits(),
                    kPlayer.getTotalPopulation(), numTechs,
                    GC.getGameINLINE().getPlayerScore((PlayerTypes)i));
        }
    }

    // ---- Cleanup ----
    fprintf(stderr, "\n[main] Calling GC.uninit()...\n");
    CvGlobals::getInstance().uninit();
    fprintf(stderr, "[main] Shutdown complete.\n");

    return 0;
}
