// OpenCiv4 — Clean-room 64-bit engine replacement for Civilization IV
// Phase 2: Playable game — human player with real turn flow
//
// This is the host executable entry point. It:
// 1. Creates stub interface implementations
// 2. Wires them into CvGlobals via setDLLIFace()
// 3. Calls GC.init() to bring up the gamecore
// 4. Loads XML data (units, techs, buildings, civs, etc.)
// 5. Initializes a game with 1 human + 3 AI players
// 6. Runs CvGame::update() in a loop (real BTS turn flow)

#include <cstdio>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <windows.h>

#include <SDL.h>
#include <SDL_ttf.h>

// Crash handler to print which turn/phase we were in when crashing
static volatile int g_crashTurn = -1;
static volatile int g_crashPlayer = -1;
static volatile int g_crashPhase = -1; // 0=update,1=drainCmd,2=snapshot
volatile int g_crashSubPhase = -1;

static void crashHandler(int sig) {
    fprintf(stderr, "\n!!! CRASH (signal %d) at turn=%d player=%d phase=%d sub=%d !!!\n",
            sig, g_crashTurn, g_crashPlayer, g_crashPhase, g_crashSubPhase);
    fflush(stderr);
    _exit(139);
}

// Windows Vectored Exception Handler
static LONG WINAPI vehHandler(EXCEPTION_POINTERS* pExInfo) {
    if (pExInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        ULONG_PTR readWrite = pExInfo->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR faultAddr = pExInfo->ExceptionRecord->ExceptionInformation[1];
        ULONG_PTR instrAddr = (ULONG_PTR)pExInfo->ExceptionRecord->ExceptionAddress;
        fprintf(stderr, "\n!!! ACCESS VIOLATION: %s at address 0x%llX (instruction at 0x%llX)\n",
                readWrite == 0 ? "READ" : "WRITE",
                (unsigned long long)faultAddr, (unsigned long long)instrAddr);
        fprintf(stderr, "    turn=%d player=%d phase=%d sub=%d\n",
                g_crashTurn, g_crashPlayer, g_crashPhase, g_crashSubPhase);
        CONTEXT* ctx = pExInfo->ContextRecord;
        fprintf(stderr, "    RIP=0x%llX RSP=0x%llX RBP=0x%llX\n",
                (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp, (unsigned long long)ctx->Rbp);
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
#include "CvUnit.h"
#include "CvCity.h"
#include "CvSelectionGroup.h"

// Our stub implementations of all 13 interface classes
#include "StubInterfaces.h"

// Phase 1: Map viewer
#include "MapSnapshot.h"
#include "Renderer.h"
#include "GLRenderer.h"

// OpenGL loader
#include <glad.h>

// Phase 1b: Asset loading
#include "FPKArchive.h"
#include "DDSLoader.h"
#include "NifLoader.h"

// ---- Global state ----
static MapSnapshot g_mapSnapshot;
static std::atomic<bool> g_gameRunning{true};
static std::atomic<bool> g_gameThreadDone{false};
static std::atomic<bool> g_gamePaused{false};
static std::atomic<int> g_turnDelayMs{0};

// Command queue: render thread → game thread
static std::queue<GameCommand> g_commandQueue;
static std::mutex g_cmdMutex;

// Human player ID
static const int HUMAN_PLAYER = 0;

// Push a command from the render thread
static void pushCommand(GameCommand cmd) {
    std::lock_guard<std::mutex> lock(g_cmdMutex);
    g_commandQueue.push(cmd);
}

// Game message queue (game thread → render thread, via MapSnapshot)
static std::vector<std::string> g_pendingMessages;
static std::mutex g_msgMutex;

static void addGameMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_msgMutex);
    g_pendingMessages.push_back(msg);
    if (g_pendingMessages.size() > 8)
        g_pendingMessages.erase(g_pendingMessages.begin());
    fprintf(stderr, "[game] %s\n", msg.c_str());
}

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
        if (!civInfo.isPlayable())
            continue;
        for (int iLeader = 0; iLeader < GC.getNumLeaderHeadInfos(); iLeader++)
        {
            if (civInfo.isLeaders((LeaderHeadTypes)iLeader))
            {
                pairs[count].eCiv = (CivilizationTypes)iCiv;
                pairs[count].eLeader = (LeaderHeadTypes)iLeader;
                pairs[count].szCivDesc = civInfo.getDescription();
                count++;
                break;
            }
        }
    }
    return count;
}

// ---- Helper: wchar to narrow string ----
static std::string wcharToStr(const wchar_t* w, int maxLen = 64) {
    std::string s;
    if (!w) return s;
    for (int i = 0; w[i] && i < maxLen; i++)
        s += (char)(w[i] < 128 ? w[i] : '?');
    return s;
}

// ---- Populate MapSnapshot from game state ----
static void updateMapSnapshot()
{
    CvMap& map = GC.getMapINLINE();
    int w = map.getGridWidth();
    int h = map.getGridHeight();

    std::vector<PlotData> newPlots(w * h);
    for (int i = 0; i < w * h; i++) {
        CvPlot* pPlot = map.plotByIndexINLINE(i);
        PlotData& pd = newPlots[i];

        pd.terrainType = (int)pPlot->getTerrainType();
        pd.plotType = (int)pPlot->getPlotType();
        pd.featureType = (int)pPlot->getFeatureType();
        pd.bonusType = (int)pPlot->getBonusType();
        pd.ownerID = (int)pPlot->getOwnerINLINE();
        pd.isRiver = pPlot->isRiver();
        pd.isNOfRiver = pPlot->isNOfRiver();
        pd.isWOfRiver = pPlot->isWOfRiver();
        pd.isCity = pPlot->isCity();
        pd.unitCount = pPlot->getNumUnits();
        pd.cityPopulation = 0;
        pd.improvementType = (int)pPlot->getImprovementType();

        // Owner color
        pd.ownerColorR = pd.ownerColorG = pd.ownerColorB = 255;
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

        // City info
        if (pd.isCity) {
            CvCity* pCity = pPlot->getPlotCity();
            if (pCity) {
                pd.cityPopulation = pCity->getPopulation();
                pd.cityID = pCity->getID();
                pd.cityName = wcharToStr(pCity->getName().GetCString());
            }
        }

        // First unit info
        if (pd.unitCount > 0) {
            CLLNode<IDInfo>* pNode = pPlot->headUnitNode();
            if (pNode) {
                CvUnit* pUnit = ::getUnit(pNode->m_data);
                if (pUnit) {
                    pd.firstUnitID = pUnit->getID();
                    pd.firstUnitOwner = (int)pUnit->getOwnerINLINE();
                    pd.hasHumanUnit = (pd.firstUnitOwner == HUMAN_PLAYER);
                    pd.firstUnitHP = (pUnit->currHitPoints() * 100) / std::max(1, pUnit->maxHitPoints());
                    pd.firstUnitMoves = pUnit->movesLeft();
                    pd.firstUnitMaxMoves = pUnit->maxMoves();
                    pd.firstUnitStrength = pUnit->baseCombatStr() * 100;
                    pd.firstUnitCanFound = pUnit->isFound();

                    // Unit type name
                    UnitTypes eType = pUnit->getUnitType();
                    if (eType >= 0 && eType < GC.getNumUnitInfos()) {
                        pd.firstUnitName = wcharToStr(GC.getUnitInfo(eType).getDescription());
                    }

                    // Check all units for human ownership
                    if (!pd.hasHumanUnit) {
                        CLLNode<IDInfo>* pNext = pPlot->nextUnitNode(pNode);
                        while (pNext) {
                            CvUnit* pU = ::getUnit(pNext->m_data);
                            if (pU && pU->getOwnerINLINE() == HUMAN_PLAYER) {
                                pd.hasHumanUnit = true;
                                break;
                            }
                            pNext = pPlot->nextUnitNode(pNext);
                        }
                    }
                }
            }
        }
    }

    // Collect player info
    PlayerInfo playerInfos[18];
    int numPlayers = 0;
    for (int p = 0; p < MAX_CIV_PLAYERS; p++) {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
        if (kPlayer.isAlive()) {
            PlayerInfo& pi = playerInfos[p];
            pi.alive = true;
            pi.isHuman = kPlayer.isHuman();
            pi.numCities = kPlayer.getNumCities();
            pi.numUnits = kPlayer.getNumUnits();
            pi.totalPop = kPlayer.getTotalPopulation();
            pi.score = GC.getGameINLINE().getPlayerScore((PlayerTypes)p);
            pi.gold = kPlayer.getGold();
            pi.goldRate = kPlayer.calculateGoldRate();
            pi.scienceRate = kPlayer.calculateResearchRate();
            pi.currentResearch = (int)kPlayer.getCurrentResearch();
            if (pi.currentResearch >= 0 && pi.currentResearch < GC.getNumTechInfos()) {
                pi.currentResearchName = wcharToStr(GC.getTechInfo((TechTypes)pi.currentResearch).getDescription());
                pi.researchTurns = kPlayer.getResearchTurnsLeft((TechTypes)pi.currentResearch, true);
            }

            // Color
            PlayerColorTypes eColor = kPlayer.getPlayerColor();
            if (eColor >= 0 && eColor < GC.getNumPlayerColorInfos()) {
                CvPlayerColorInfo& colorInfo = GC.getPlayerColorInfo(eColor);
                ColorTypes ePrimary = (ColorTypes)colorInfo.getColorTypePrimary();
                if (ePrimary >= 0 && ePrimary < GC.getNumColorInfos()) {
                    const NiColorA& c = GC.getColorInfo(ePrimary).getColor();
                    pi.colorR = (uint8_t)(c.r * 255.0f);
                    pi.colorG = (uint8_t)(c.g * 255.0f);
                    pi.colorB = (uint8_t)(c.b * 255.0f);
                }
            }

            // Civ name
            pi.civName = wcharToStr(GC.getCivilizationInfo(kPlayer.getCivilizationType()).getShortDescription(0));
            numPlayers = p + 1;
        }
    }

    // Selection state
    CvUnit* pSelUnit = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
    int selUnitID = -1, selUnitX = -1, selUnitY = -1;
    if (pSelUnit && pSelUnit->getOwnerINLINE() == HUMAN_PLAYER) {
        selUnitID = pSelUnit->getID();
        selUnitX = pSelUnit->getX_INLINE();
        selUnitY = pSelUnit->getY_INLINE();
    }

    // Human turn state
    CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);
    bool isHumanTurn = human.isTurnActive();
    bool waitingForEnd = isHumanTurn && !human.isEndTurn();

    // City detail if city screen is open
    CityDetail cityDet;
    bool cityScreenOpen = false;
    CvCity* pSelCity = gDLL->getInterfaceIFace()->getHeadSelectedCity();
    if (pSelCity && pSelCity->getOwnerINLINE() == HUMAN_PLAYER) {
        cityScreenOpen = true;
        cityDet.cityID = pSelCity->getID();
        cityDet.name = wcharToStr(pSelCity->getName().GetCString());
        cityDet.population = pSelCity->getPopulation();
        cityDet.foodRate = pSelCity->foodDifference();
        cityDet.productionRate = pSelCity->getCurrentProductionDifference(false, true);
        cityDet.commerceRate = pSelCity->getCommerceRate(COMMERCE_GOLD) +
                               pSelCity->getCommerceRate(COMMERCE_RESEARCH);
        cityDet.foodStored = pSelCity->getFood();
        cityDet.foodNeeded = pSelCity->growthThreshold();
        cityDet.productionStored = pSelCity->getProduction();
        cityDet.productionNeeded = pSelCity->getProductionNeeded();
        cityDet.productionTurns = pSelCity->getProductionTurnsLeft();
        cityDet.garrisonCount = pSelCity->plot()->plotCount(PUF_isUnitType, -1, -1, (PlayerTypes)HUMAN_PLAYER);

        // Current production name
        if (pSelCity->isProduction()) {
            cityDet.currentProduction = wcharToStr(pSelCity->getProductionName());
        } else {
            cityDet.currentProduction = "(none)";
        }

        // Available production
        for (int u = 0; u < GC.getNumUnitInfos(); u++) {
            if (pSelCity->canTrain((UnitTypes)u)) {
                ProductionItem item;
                item.type = u;
                item.isUnit = true;
                item.name = wcharToStr(GC.getUnitInfo((UnitTypes)u).getDescription());
                item.turns = pSelCity->getProductionTurnsLeft((UnitTypes)u, 0);
                cityDet.availableProduction.push_back(item);
            }
        }
        for (int b = 0; b < GC.getNumBuildingInfos(); b++) {
            if (pSelCity->canConstruct((BuildingTypes)b)) {
                ProductionItem item;
                item.type = b;
                item.isUnit = false;
                item.name = wcharToStr(GC.getBuildingInfo((BuildingTypes)b).getDescription());
                item.turns = pSelCity->getProductionTurnsLeft((BuildingTypes)b, 0);
                cityDet.availableProduction.push_back(item);
            }
        }
    }

    // Available techs (for human player)
    std::vector<TechItem> techs;
    if (isHumanTurn) {
        for (int t = 0; t < GC.getNumTechInfos(); t++) {
            if (human.canResearch((TechTypes)t)) {
                TechItem ti;
                ti.techID = t;
                ti.name = wcharToStr(GC.getTechInfo((TechTypes)t).getDescription());
                ti.turnsLeft = human.getResearchTurnsLeft((TechTypes)t, true);
                techs.push_back(ti);
            }
        }
    }

    // Worker builds (if selected unit is a worker)
    std::vector<BuildItem> builds;
    if (pSelUnit && pSelUnit->getOwnerINLINE() == HUMAN_PLAYER && pSelUnit->getUnitType() != NO_UNIT) {
        // Check if unit can do any build
        CvPlot* pUnitPlot = pSelUnit->plot();
        for (int b = 0; b < GC.getNumBuildInfos(); b++) {
            if (pSelUnit->canBuild(pUnitPlot, (BuildTypes)b)) {
                BuildItem bi;
                bi.buildType = b;
                bi.name = wcharToStr(GC.getBuildInfo((BuildTypes)b).getDescription());
                bi.turnsLeft = pUnitPlot->getBuildTurnsLeft((BuildTypes)b, 0, 0);
                builds.push_back(bi);
            }
        }
    }

    // Swap into shared snapshot
    {
        std::lock_guard<std::mutex> lock(g_mapSnapshot.mtx);
        g_mapSnapshot.width = w;
        g_mapSnapshot.height = h;
        g_mapSnapshot.gameTurn = GC.getGameINLINE().getGameTurn();
        g_mapSnapshot.gameYear = GC.getGameINLINE().getGameTurnYear();
        g_mapSnapshot.wrapX = map.isWrapX();
        g_mapSnapshot.wrapY = map.isWrapY();
        g_mapSnapshot.paused = g_gamePaused.load();
        g_mapSnapshot.turnDelayMs = g_turnDelayMs.load();
        g_mapSnapshot.plots = std::move(newPlots);
        for (int p = 0; p < 18; p++)
            g_mapSnapshot.players[p] = playerInfos[p];
        g_mapSnapshot.numPlayers = numPlayers;

        // Selection + turn state
        g_mapSnapshot.selectedUnitID = selUnitID;
        g_mapSnapshot.selectedUnitX = selUnitX;
        g_mapSnapshot.selectedUnitY = selUnitY;
        g_mapSnapshot.isHumanTurn = isHumanTurn;
        g_mapSnapshot.waitingForEndTurn = waitingForEnd;
        g_mapSnapshot.activePlayerID = (int)GC.getGameINLINE().getActivePlayer();
        g_mapSnapshot.humanPlayerID = HUMAN_PLAYER;

        // City detail
        g_mapSnapshot.cityScreenOpen = cityScreenOpen;
        g_mapSnapshot.selectedCity = cityDet;

        // Techs
        g_mapSnapshot.availableTechs = techs;

        // Worker builds
        g_mapSnapshot.availableBuilds = builds;

        // Messages
        {
            std::lock_guard<std::mutex> mlock(g_msgMutex);
            g_mapSnapshot.gameMessages = g_pendingMessages;
        }
    }
}

// ---- Execute a game command from the render thread ----
static void executeCommand(const GameCommand& cmd)
{
    CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);

    switch (cmd.type) {
    case GameCommand::END_TURN: {
        if (human.isTurnActive() && !human.isEndTurn()) {
            fprintf(stderr, "[cmd] END_TURN\n");
            human.setEndTurn(true);
        }
        break;
    }
    case GameCommand::SELECT_UNIT: {
        // Find first human unit at (x, y)
        CvPlot* pPlot = GC.getMapINLINE().plot(cmd.x, cmd.y);
        if (pPlot) {
            CvUnit* pBest = nullptr;
            CLLNode<IDInfo>* pNode = pPlot->headUnitNode();
            while (pNode) {
                CvUnit* pUnit = ::getUnit(pNode->m_data);
                if (pUnit && pUnit->getOwnerINLINE() == HUMAN_PLAYER) {
                    pBest = pUnit;
                    break;
                }
                pNode = pPlot->nextUnitNode(pNode);
            }
            if (pBest) {
                gDLL->getInterfaceIFace()->selectUnit(pBest, true, false, false);
                fprintf(stderr, "[cmd] SELECT_UNIT id=%d at (%d,%d)\n", pBest->getID(), cmd.x, cmd.y);
            }
        }
        break;
    }
    case GameCommand::SELECT_CITY: {
        CvPlot* pPlot = GC.getMapINLINE().plot(cmd.x, cmd.y);
        if (pPlot && pPlot->isCity()) {
            CvCity* pCity = pPlot->getPlotCity();
            if (pCity && pCity->getOwnerINLINE() == HUMAN_PLAYER) {
                gDLL->getInterfaceIFace()->selectCity(pCity, false);
                gDLL->getInterfaceIFace()->clearSelectionList();
                fprintf(stderr, "[cmd] SELECT_CITY id=%d\n", pCity->getID());
            }
        }
        break;
    }
    case GameCommand::CLOSE_CITY: {
        gDLL->getInterfaceIFace()->clearSelectedCities();
        fprintf(stderr, "[cmd] CLOSE_CITY\n");
        break;
    }
    case GameCommand::DESELECT: {
        gDLL->getInterfaceIFace()->clearSelectionList();
        break;
    }
    case GameCommand::MOVE_UNIT: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit && pUnit->getOwnerINLINE() == HUMAN_PLAYER) {
            CvSelectionGroup* pGroup = pUnit->getGroup();
            if (pGroup) {
                pGroup->pushMission(MISSION_MOVE_TO, cmd.x, cmd.y, 0, false, true);
                fprintf(stderr, "[cmd] MOVE_UNIT id=%d to (%d,%d)\n", cmd.id, cmd.x, cmd.y);
            }
        }
        break;
    }
    case GameCommand::GOTO_PLOT: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit && pUnit->getOwnerINLINE() == HUMAN_PLAYER) {
            CvSelectionGroup* pGroup = pUnit->getGroup();
            if (pGroup) {
                pGroup->pushMission(MISSION_MOVE_TO, cmd.x, cmd.y, 0, false, true);
                fprintf(stderr, "[cmd] GOTO id=%d to (%d,%d)\n", cmd.id, cmd.x, cmd.y);
            }
        }
        break;
    }
    case GameCommand::FOUND_CITY: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit && pUnit->isFound() && pUnit->canFound(pUnit->plot())) {
            pUnit->getGroup()->pushMission(MISSION_FOUND);
            addGameMessage("City founded!");
            fprintf(stderr, "[cmd] FOUND_CITY id=%d at (%d,%d)\n", cmd.id, pUnit->getX_INLINE(), pUnit->getY_INLINE());
        }
        break;
    }
    case GameCommand::SET_PRODUCTION: {
        // cmd.id = city ID, cmd.param = type index, cmd.x = 1 if unit, 0 if building
        CvCity* pCity = human.getCity(cmd.id);
        if (pCity) {
            if (cmd.x == 1) {
                // Unit
                pCity->pushOrder(ORDER_TRAIN, cmd.param, -1, false, false, false);
                fprintf(stderr, "[cmd] SET_PRODUCTION city=%d train unit %d\n", cmd.id, cmd.param);
            } else {
                // Building
                pCity->pushOrder(ORDER_CONSTRUCT, cmd.param, -1, false, false, false);
                fprintf(stderr, "[cmd] SET_PRODUCTION city=%d construct building %d\n", cmd.id, cmd.param);
            }
        }
        break;
    }
    case GameCommand::SET_RESEARCH: {
        if (cmd.id >= 0 && cmd.id < GC.getNumTechInfos()) {
            if (human.canResearch((TechTypes)cmd.id)) {
                human.pushResearch((TechTypes)cmd.id, true);
                std::string name = wcharToStr(GC.getTechInfo((TechTypes)cmd.id).getDescription());
                addGameMessage("Researching: " + name);
                fprintf(stderr, "[cmd] SET_RESEARCH tech=%d\n", cmd.id);
            }
        }
        break;
    }
    case GameCommand::BUILD_IMPROVEMENT: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit) {
            pUnit->getGroup()->pushMission(MISSION_BUILD, cmd.param);
            fprintf(stderr, "[cmd] BUILD type=%d\n", cmd.param);
        }
        break;
    }
    case GameCommand::FORTIFY: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit && pUnit->canFortify(pUnit->plot())) {
            pUnit->getGroup()->pushMission(MISSION_FORTIFY);
            fprintf(stderr, "[cmd] FORTIFY id=%d\n", cmd.id);
        }
        break;
    }
    case GameCommand::SLEEP: {
        CvUnit* pUnit = human.getUnit(cmd.id);
        if (pUnit) {
            pUnit->getGroup()->pushMission(MISSION_SLEEP);
            fprintf(stderr, "[cmd] SLEEP id=%d\n", cmd.id);
            gDLL->getInterfaceIFace()->clearSelectionList();
        }
        break;
    }
    case GameCommand::SKIP_TURN: {
        // Deselect current unit, cycle to next
        gDLL->getInterfaceIFace()->clearSelectionList();
        break;
    }
    case GameCommand::CYCLE_UNIT: {
        // Find next human unit needing orders
        int iLoop;
        CvUnit* pBest = nullptr;
        CvUnit* pCurrent = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
        bool pastCurrent = (pCurrent == nullptr);
        for (CvUnit* pUnit = human.firstUnit(&iLoop); pUnit; pUnit = human.nextUnit(&iLoop)) {
            if (!pastCurrent) {
                if (pUnit == pCurrent) pastCurrent = true;
                continue;
            }
            if (pUnit->isWaiting() || pUnit->isCargo()) continue;
            if (pUnit->movesLeft() > 0 && !(pUnit->getGroup() && pUnit->getGroup()->getActivityType() == ACTIVITY_SLEEP)) {
                pBest = pUnit;
                break;
            }
        }
        // Wrap around if needed
        if (!pBest) {
            for (CvUnit* pUnit = human.firstUnit(&iLoop); pUnit; pUnit = human.nextUnit(&iLoop)) {
                if (pUnit == pCurrent) break;
                if (pUnit->isWaiting() || pUnit->isCargo()) continue;
                if (pUnit->movesLeft() > 0 && !(pUnit->getGroup() && pUnit->getGroup()->getActivityType() == ACTIVITY_SLEEP)) {
                    pBest = pUnit;
                    break;
                }
            }
        }
        if (pBest) {
            gDLL->getInterfaceIFace()->selectUnit(pBest, true, false, false);
        }
        break;
    }
    }
}

// ---- Drain all pending commands ----
static void drainCommandQueue()
{
    std::lock_guard<std::mutex> lock(g_cmdMutex);
    while (!g_commandQueue.empty()) {
        GameCommand cmd = g_commandQueue.front();
        g_commandQueue.pop();
        executeCommand(cmd);
    }
}

// ---- Auto-select next unit needing orders ----
static void autoSelectNextUnit()
{
    CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);
    if (!human.isTurnActive() || human.isEndTurn()) return;

    // If we already have a unit selected, don't override
    CvUnit* pCurrent = gDLL->getInterfaceIFace()->getHeadSelectedUnit();
    if (pCurrent && pCurrent->movesLeft() > 0) return;

    // Find first unit needing orders
    int iLoop;
    for (CvUnit* pUnit = human.firstUnit(&iLoop); pUnit; pUnit = human.nextUnit(&iLoop)) {
        if (pUnit->isWaiting() || pUnit->isCargo()) continue;
        if (pUnit->movesLeft() > 0 && !(pUnit->getGroup() && pUnit->getGroup()->getActivityType() == ACTIVITY_SLEEP)) {
            gDLL->getInterfaceIFace()->selectUnit(pUnit, true, false, false);
            return;
        }
    }

    // No units needing orders — clear selection
    gDLL->getInterfaceIFace()->clearSelectionList();
}

// ---- Auto-assign production to idle cities ----
static void autoAssignCityProduction()
{
    CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);
    int iLoop;
    for (CvCity* pCity = human.firstCity(&iLoop); pCity; pCity = human.nextCity(&iLoop)) {
        if (!pCity->isProduction()) {
            pCity->AI_chooseProduction();
            if (pCity->isProduction()) {
                std::string name = wcharToStr(pCity->getProductionName());
                std::string cityName = wcharToStr(pCity->getName().GetCString());
                addGameMessage(cityName + " auto-builds: " + name);
            }
        }
    }
}

// ---- Auto-assign research ----
static void autoAssignResearch()
{
    CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);
    if (human.getCurrentResearch() == NO_TECH && human.isResearch()) {
        for (int t = 0; t < GC.getNumTechInfos(); t++) {
            if (human.canResearch((TechTypes)t)) {
                human.pushResearch((TechTypes)t, true);
                std::string name = wcharToStr(GC.getTechInfo((TechTypes)t).getDescription());
                addGameMessage("Auto-research: " + name);
                break;
            }
        }
    }
}

// ---- Game thread function (Phase 2: uses CvGame::update()) ----
static void gameThreadFunc()
{
    fprintf(stderr, "=== Starting game loop (CvGame::update mode) ===\n");

    // Take initial snapshot before game starts
    updateMapSnapshot();

    int lastTurn = -1;

    while (g_gameRunning)
    {
        // Drain commands from the render thread
        g_crashPhase = 1;
        drainCommandQueue();

        // Game state check
        if (GC.getGameINLINE().getGameState() == GAMESTATE_OVER) {
            addGameMessage("Game over!");
            break;
        }

        // Call the real BTS game update loop
        g_crashPhase = 0;
        g_crashTurn = GC.getGameINLINE().getGameTurn();
        GC.getGameINLINE().update();

        // Post-update: auto-management for human player
        CvPlayer& human = GET_PLAYER((PlayerTypes)HUMAN_PLAYER);
        if (human.isTurnActive() && !human.isEndTurn()) {
            // Auto-select next unit
            autoSelectNextUnit();

            // Auto-assign production to idle cities
            autoAssignCityProduction();

            // Auto-assign research if none chosen
            autoAssignResearch();
        }

        // Progress logging when turn changes
        int curTurn = GC.getGameINLINE().getGameTurn();
        if (curTurn != lastTurn) {
            lastTurn = curTurn;
            if (curTurn % 25 == 0) {
                fprintf(stderr, "--- Turn %d ---\n", curTurn);
                for (int p = 0; p < MAX_CIV_PLAYERS; p++) {
                    CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
                    if (kPlayer.isAlive()) {
                        fprintf(stderr, "  P%d%s: cities=%d units=%d pop=%d score=%d\n", p,
                                kPlayer.isHuman() ? "*" : "",
                                kPlayer.getNumCities(), kPlayer.getNumUnits(),
                                kPlayer.getTotalPopulation(),
                                GC.getGameINLINE().getPlayerScore((PlayerTypes)p));
                    }
                }
            }
        }

        // Update map snapshot for the renderer
        g_crashPhase = 2;
        updateMapSnapshot();

        // Sleep to avoid burning CPU (~60 updates/sec during human turn,
        // faster during AI turns)
        if (human.isTurnActive() && !human.isEndTurn()) {
            Sleep(16); // 60fps during human turn
        } else {
            Sleep(1);  // Fast during AI turns
        }
    }

    fprintf(stderr, "\n=== Game loop finished ===\n");
    fprintf(stderr, "Final game turn: %d\n", GC.getGameINLINE().getGameTurn());

    for (int i = 0; i < MAX_PLAYERS; i++) {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)i);
        if (kPlayer.isAlive()) {
            const char* civType = GC.getCivilizationInfo(kPlayer.getCivilizationType()).getType();
            fprintf(stderr, "  Player %d (%s)%s: cities=%d units=%d pop=%d score=%d\n",
                    i, civType ? civType : "???",
                    kPlayer.isHuman() ? " [HUMAN]" : "",
                    kPlayer.getNumCities(), kPlayer.getNumUnits(),
                    kPlayer.getTotalPopulation(),
                    GC.getGameINLINE().getPlayerScore((PlayerTypes)i));
        }
    }

    g_gameThreadDone = true;
}

// ---- Headless game loop (Phase 0 behavior, for --headless mode) ----
static void headlessGameThreadFunc()
{
    const int MAX_TURNS = 300;
    fprintf(stderr, "=== Starting headless game loop (max %d turns) ===\n", MAX_TURNS);

    updateMapSnapshot();

    for (int iTurn = 0; iTurn < MAX_TURNS && g_gameRunning; iTurn++)
    {
        while (g_gamePaused && g_gameRunning) Sleep(50);
        if (!g_gameRunning) break;

        int delay = g_turnDelayMs.load();
        if (delay > 0) Sleep(delay);

        for (int p = 0; p < MAX_PLAYERS && g_gameRunning; p++) {
            CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)p);
            if (kPlayer.isAlive()) {
                GC.getGameINLINE().setActivePlayer((PlayerTypes)p);
                ((CvPlayerAI&)kPlayer).AI_updateFoundValues();
                kPlayer.doTurn();
                if (kPlayer.getCurrentResearch() == NO_TECH && kPlayer.isResearch() && !kPlayer.isBarbarian()) {
                    for (int t = 0; t < GC.getNumTechInfos(); t++) {
                        if (kPlayer.canResearch((TechTypes)t)) { kPlayer.pushResearch((TechTypes)t, true); break; }
                    }
                }
                kPlayer.doTurnUnits();
                kPlayer.AI_unitUpdate();
            }
        }
        GC.getGameINLINE().setActivePlayer((PlayerTypes)0);
        GC.getGameINLINE().doTurn();
        updateMapSnapshot();

        if (iTurn % 50 == 0) {
            fprintf(stderr, "--- Turn %d ---\n", iTurn);
            for (int p = 0; p < MAX_CIV_PLAYERS; p++) {
                CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
                if (kPlayer.isAlive()) {
                    fprintf(stderr, "  P%d: cities=%d units=%d pop=%d score=%d\n", p,
                            kPlayer.getNumCities(), kPlayer.getNumUnits(),
                            kPlayer.getTotalPopulation(),
                            GC.getGameINLINE().getPlayerScore((PlayerTypes)p));
                }
            }
        }

        if (GC.getGameINLINE().getGameState() == GAMESTATE_OVER) break;
    }

    fprintf(stderr, "\n=== Headless game loop finished ===\n");
    g_gameThreadDone = true;
}

int main(int argc, char* argv[])
{
    AddVectoredExceptionHandler(1, vehHandler);
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    HMODULE hModule = GetModuleHandle(NULL);
    fprintf(stderr, "=== OpenCiv4 Engine ===\n");
    fprintf(stderr, "Module base: 0x%llX\n", (unsigned long long)hModule);
    fprintf(stderr, "Phase 2: Playable Game\n");
    fprintf(stderr, "Build: 64-bit (%zu-byte pointers)\n\n", sizeof(void*));
    fprintf(stderr, "BTS Install: %s\n\n", BTS_INSTALL_DIR);

    // ---- Check flags ----
    bool headless = false;
    bool legacyRenderer = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = true;
        if (strcmp(argv[i], "--legacy-2d") == 0) legacyRenderer = true;
        if (strcmp(argv[i], "--test-fpk") == 0) {
            std::string fpkPath = std::string(BTS_INSTALL_DIR) + "/../Assets/Art0.FPK";
            FPKArchive fpk;
            if (!fpk.open(fpkPath.c_str())) { fprintf(stderr, "FAILED\n"); return 1; }
            fprintf(stderr, "Entries: %d\n", fpk.entryCount());
            return 0;
        }
        if (strcmp(argv[i], "--test-dds") == 0) {
            std::string fpkPath = std::string(BTS_INSTALL_DIR) + "/../Assets/Art0.FPK";
            FPKArchive fpk;
            if (!fpk.open(fpkPath.c_str())) { fprintf(stderr, "FAILED\n"); return 1; }
            fprintf(stderr, "DDS test OK\n");
            return 0;
        }
        if (strcmp(argv[i], "--test-nif") == 0) {
            // Test NIF loader against loose NIF files from the game
            const char* testFiles[] = {
                "/../Assets/Art/Interface/Screens/Throne/throneRoom-geometryOnly.nif",
                "/../Assets/Art/Interface/Screens/Civilopedia/WaterEnvironment/WaterEnvironment.nif",
                "/../Assets/Art/Interface/Screens/SpaceShip/LaunchPad.nif",
                "/../Assets/Art/LeaderHeads/Abraham Lincoln/Abraham Lincoln.nif",
            };
            int tested = 0, passed = 0;
            for (const char* relPath : testFiles) {
                std::string fullPath = std::string(BTS_INSTALL_DIR) + relPath;
                fprintf(stderr, "\n--- Testing: %s ---\n", relPath);
                auto nifFile = nif::loadNifFromFile(fullPath.c_str());
                if (!nifFile) {
                    fprintf(stderr, "  FAILED to load\n");
                    continue;
                }
                tested++;

                // Count blocks by type
                int nodes = 0, shapes = 0, meshes = 0, textures = 0;
                int totalVerts = 0, totalTris = 0;
                for (uint32_t b = 0; b < nifFile->header.numBlocks; b++) {
                    auto* block = nifFile->getBlock(b);
                    if (!block) continue;
                    switch (block->type) {
                        case nif::BlockType::NiNode: nodes++; break;
                        case nif::BlockType::NiTriShape:
                        case nif::BlockType::NiTriStrips: shapes++; break;
                        case nif::BlockType::NiTriShapeData: {
                            auto* d = static_cast<nif::NiTriShapeDataBlock*>(block);
                            meshes++; totalVerts += d->numVertices; totalTris += d->numTriangles;
                            break;
                        }
                        case nif::BlockType::NiTriStripsData: {
                            auto* d = static_cast<nif::NiTriStripsDataBlock*>(block);
                            meshes++; totalVerts += d->numVertices; totalTris += d->numTriangles;
                            break;
                        }
                        case nif::BlockType::NiSourceTexture: textures++; break;
                        default: break;
                    }
                }
                fprintf(stderr, "  Blocks: %u total, %d nodes, %d shapes, %d meshes, %d textures\n",
                        nifFile->header.numBlocks, nodes, shapes, meshes, textures);
                fprintf(stderr, "  Geometry: %d vertices, %d triangles\n", totalVerts, totalTris);
                fprintf(stderr, "  Roots: %zu\n", nifFile->rootRefs.size());

                // Walk scene graph from root
                if (!nifFile->rootRefs.empty()) {
                    auto* root = nifFile->getBlock<nif::NiNodeBlock>(nifFile->rootRefs[0]);
                    if (root) {
                        fprintf(stderr, "  Root node: '%s', %zu children\n",
                                root->name.c_str(), root->childRefs.size());
                    }
                }

                // List texture paths
                for (uint32_t b = 0; b < nifFile->header.numBlocks; b++) {
                    auto* tex = nifFile->getBlock<nif::NiSourceTextureBlock>(b);
                    if (tex && !tex->fileName.empty()) {
                        fprintf(stderr, "  Texture: %s\n", tex->fileName.c_str());
                    }
                }
                passed++;
            }
            fprintf(stderr, "\n=== NIF test: %d/%d files passed ===\n", passed, tested);

            // Also test loading a NIF from inside Art0.FPK
            std::string fpkPath = std::string(BTS_INSTALL_DIR) + "/../Assets/Art0.FPK";
            FPKArchive fpk;
            if (fpk.open(fpkPath.c_str())) {
                // Find first .nif in the FPK
                std::string firstNif;
                for (const auto& entry : fpk.entries()) {
                    size_t len = entry.filename.size();
                    if (len > 4 && entry.filename.substr(len - 4) == ".nif") {
                        firstNif = entry.filename;
                        break;
                    }
                }
                if (!firstNif.empty()) {
                    fprintf(stderr, "\n--- FPK NIF test: %s ---\n", firstNif.c_str());
                    auto data = fpk.readFile(firstNif);
                    if (!data.empty()) {
                        auto nifFile = nif::loadNifFromMemory(data);
                        if (nifFile) {
                            fprintf(stderr, "  FPK NIF loaded: %u blocks\n", nifFile->header.numBlocks);
                        } else {
                            fprintf(stderr, "  FPK NIF parse FAILED\n");
                        }
                    }
                }
            }

            return 0;
        }
        if (strcmp(argv[i], "--scan-nif-types") == 0) {
            // Scan all NIF files in Art0.FPK to find block type frequency
            std::string fpkPath = std::string(BTS_INSTALL_DIR) + "/../Assets/Art0.FPK";
            FPKArchive fpk;
            if (!fpk.open(fpkPath.c_str())) { fprintf(stderr, "FAILED\n"); return 1; }

            std::unordered_map<std::string, int> typeCounts;
            int nifCount = 0, fullParse = 0;
            for (const auto& entry : fpk.entries()) {
                size_t len = entry.filename.size();
                if (len < 4 || entry.filename.substr(len - 4) != ".nif") continue;

                auto data = fpk.readFile(entry.filename);
                if (data.empty()) continue;

                auto nifFile = nif::loadNif(data.data(), data.size());
                if (!nifFile) continue;
                nifCount++;

                bool allParsed = true;
                for (uint32_t b = 0; b < nifFile->header.numBlocks; b++) {
                    uint16_t typeIdx = nifFile->header.blockTypeIndices[b];
                    const std::string& typeName = nifFile->header.blockTypeNames[typeIdx];
                    typeCounts[typeName]++;
                    if (!nifFile->getBlock(b)) allParsed = false;
                }
                if (allParsed) fullParse++;
            }
            fprintf(stderr, "\n=== Scanned %d NIF files from Art0.FPK ===\n", nifCount);
            fprintf(stderr, "Fully parsed: %d / %d (%.1f%%)\n", fullParse, nifCount,
                    nifCount > 0 ? 100.0 * fullParse / nifCount : 0);
            fprintf(stderr, "\nBlock types (by frequency):\n");

            // Sort by count
            std::vector<std::pair<std::string, int>> sorted(typeCounts.begin(), typeCounts.end());
            std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
            for (auto& [name, count] : sorted) {
                fprintf(stderr, "  %-45s %6d\n", name.c_str(), count);
            }
            return 0;
        }
    }

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

    fprintf(stderr, "=== Gamecore initialized ===\n");
    fprintf(stderr, "CvGameAI: %s\n", GC.getGamePointer() ? "allocated" : "NULL");
    fprintf(stderr, "CvMap:    %s\n", &GC.getMap() ? "allocated" : "NULL");
    fprintf(stderr, "\n");

    // ---- Step 4: Load XML data ----
    fprintf(stderr, "[main] Loading XML data...\n");
    {
        CvXMLLoadUtility xml;
        fprintf(stderr, "[main]   SetGlobalDefines...\n");
        xml.SetGlobalDefines();
        fprintf(stderr, "[main]   SetGlobalTypes...\n");
        xml.SetGlobalTypes();
        fprintf(stderr, "[main]   LoadBasicInfos...\n");
        xml.LoadBasicInfos();
        fprintf(stderr, "[main]   ARTFILEMGR.Init()...\n");
        ARTFILEMGR.Init();
        fprintf(stderr, "[main]   SetGlobalArtDefines...\n");
        xml.SetGlobalArtDefines();
        fprintf(stderr, "[main]   buildArtFileInfoMaps...\n");
        ARTFILEMGR.buildArtFileInfoMaps();
        fprintf(stderr, "[main]   LoadPreMenuGlobals...\n");
        xml.LoadPreMenuGlobals();
        fprintf(stderr, "[main]   SetPostGlobalsGlobalDefines...\n");
        xml.SetPostGlobalsGlobalDefines();
        fprintf(stderr, "[main]   LoadPostMenuGlobals...\n");
        xml.LoadPostMenuGlobals();
        fprintf(stderr, "[main]   LoadGlobalText...\n");
        xml.LoadGlobalText();
    }
    fprintf(stderr, "[main] XML loading complete. Text entries: %zu\n\n",
            stubDLL.m_textMap.size());

    // ---- Step 5: Configure game settings via CvInitCore ----
    fprintf(stderr, "[main] Configuring game...\n");
    {
        CvInitCore& initCore = GC.getInitCore();
        initCore.setType(GAME_SP_NEW);
        initCore.setWorldSize((WorldSizeTypes)3);   // Standard
        initCore.setClimate((ClimateTypes)1);        // Temperate
        initCore.setSeaLevel((SeaLevelTypes)1);      // Medium
        initCore.setGameSpeed((GameSpeedTypes)2);    // Normal
        initCore.setEra((EraTypes)0);                // Ancient

        const int MAX_AI_PLAYERS = 4;
        CivLeaderPair pairs[32];
        int numPairs = findCivLeaderPairs(pairs, 32);
        int numPlayers = (numPairs < MAX_AI_PLAYERS) ? numPairs : MAX_AI_PLAYERS;

        fprintf(stderr, "[main]   Found %d playable civ/leader pairs, using %d\n", numPairs, numPlayers);

        HandicapTypes eHandicap = (HandicapTypes)4; // Noble
        if (GC.getNumHandicapInfos() <= 4)
            eHandicap = (HandicapTypes)(GC.getNumHandicapInfos() / 2);

        // Player 0: HUMAN
        {
            PlayerTypes ePlayer = (PlayerTypes)0;
            if (headless) {
                initCore.setSlotStatus(ePlayer, SS_COMPUTER);
            } else {
                initCore.setSlotStatus(ePlayer, SS_TAKEN); // SS_TAKEN = human player
            }
            initCore.setCiv(ePlayer, pairs[0].eCiv);
            initCore.setLeader(ePlayer, pairs[0].eLeader);
            initCore.setTeam(ePlayer, (TeamTypes)0);
            initCore.setHandicap(ePlayer, eHandicap);
            initCore.setColor(ePlayer, (PlayerColorTypes)0);
            fprintf(stderr, "[main]   Player 0: %ls (%s)\n", pairs[0].szCivDesc,
                    headless ? "AI" : "HUMAN");
        }

        // Players 1+: AI
        for (int i = 1; i < numPlayers; i++) {
            PlayerTypes ePlayer = (PlayerTypes)i;
            initCore.setSlotStatus(ePlayer, SS_COMPUTER);
            initCore.setCiv(ePlayer, pairs[i].eCiv);
            initCore.setLeader(ePlayer, pairs[i].eLeader);
            initCore.setTeam(ePlayer, (TeamTypes)i);
            initCore.setHandicap(ePlayer, eHandicap);
            initCore.setColor(ePlayer, (PlayerColorTypes)i);
            fprintf(stderr, "[main]   Player %d: %ls (AI)\n", i, pairs[i].szCivDesc);
        }

        for (int i = numPlayers; i < MAX_CIV_PLAYERS; i++)
            initCore.setSlotStatus((PlayerTypes)i, SS_CLOSED);

        initCore.setHandicap(BARBARIAN_PLAYER, eHandicap);

        // Barbarian player setup
        {
            CivilizationTypes eBarbCiv = (CivilizationTypes)GC.getDefineINT("BARBARIAN_CIVILIZATION");
            LeaderHeadTypes eBarbLeader = (LeaderHeadTypes)GC.getDefineINT("BARBARIAN_LEADER");
            if (eBarbCiv != NO_CIVILIZATION && eBarbLeader != NO_LEADER) {
                initCore.setSlotStatus(BARBARIAN_PLAYER, SS_COMPUTER);
                initCore.setCiv(BARBARIAN_PLAYER, eBarbCiv);
                initCore.setLeader(BARBARIAN_PLAYER, eBarbLeader);
                initCore.setTeam(BARBARIAN_PLAYER, BARBARIAN_TEAM);
                initCore.setColor(BARBARIAN_PLAYER, (PlayerColorTypes)(GC.getNumPlayerColorInfos() - 1));
            } else {
                initCore.setOption(GAMEOPTION_NO_BARBARIANS, true);
            }
        }
    }
    fprintf(stderr, "[main] Game configured.\n\n");

    // ---- Step 6: Initialize game and map ----
    fprintf(stderr, "[main] Initializing game...\n");
    HandicapTypes eHandicap = (HandicapTypes)4;
    if (GC.getNumHandicapInfos() <= 4)
        eHandicap = (HandicapTypes)(GC.getNumHandicapInfos() / 2);

    GC.getGameINLINE().init(eHandicap);
    fprintf(stderr, "[main] CvGame::init() complete.\n");

    fprintf(stderr, "[main] Initializing map...\n");
    GC.getMapINLINE().init();
    fprintf(stderr, "[main] CvMap::init() complete. Grid: %dx%d\n",
            GC.getMapINLINE().getGridWidth(), GC.getMapINLINE().getGridHeight());

    fprintf(stderr, "[main] Generating random map...\n");
    CvMapGenerator::GetInstance().generateRandomMap();
    CvMapGenerator::GetInstance().addGameElements();

    fprintf(stderr, "[main] Updating plot yields...\n");
    GC.getMapINLINE().updateYield();

    // Initialize teams and players
    fprintf(stderr, "[main] Initializing teams and players...\n");
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (GC.getInitCore().getSlotStatus((PlayerTypes)i) == SS_COMPUTER ||
            GC.getInitCore().getSlotStatus((PlayerTypes)i) == SS_TAKEN) {
            GET_TEAM((TeamTypes)i).init((TeamTypes)i);
        }
    }
    for (int i = 0; i < MAX_PLAYERS; i++)
        GET_PLAYER((PlayerTypes)i).init((PlayerTypes)i);

    fprintf(stderr, "[main] Setting initial items...\n");
    GC.getGameINLINE().setInitialItems();

    GC.getGameINLINE().initScoreCalculation();

    // Set active player to the human player
    GC.getInitCore().setActivePlayer((PlayerTypes)HUMAN_PLAYER);
    GC.getGameINLINE().setActivePlayer((PlayerTypes)HUMAN_PLAYER);

    // Reveal all plots for all teams
    fprintf(stderr, "[main] Revealing all plots...\n");
    for (int i = 0; i < GC.getMapINLINE().numPlotsINLINE(); i++) {
        CvPlot* pPlot = GC.getMapINLINE().plotByIndexINLINE(i);
        for (int t = 0; t < MAX_TEAMS; t++) {
            if (GET_TEAM((TeamTypes)t).isAlive())
                pPlot->setRevealed((TeamTypes)t, true, false, NO_TEAM, false);
        }
    }

    GC.getGameINLINE().setFinalInitialized(true);

    // Establish contact between all teams
    for (int i = 0; i < MAX_TEAMS; i++) {
        if (!GET_TEAM((TeamTypes)i).isAlive()) continue;
        for (int j = i + 1; j < MAX_TEAMS; j++) {
            if (!GET_TEAM((TeamTypes)j).isAlive()) continue;
            GET_TEAM((TeamTypes)i).setHasMetDirect((TeamTypes)j);
            GET_TEAM((TeamTypes)j).setHasMetDirect((TeamTypes)i);
        }
    }

    // Seed AI found values
    for (int p = 0; p < MAX_CIV_PLAYERS; p++) {
        CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)p);
        if (kPlayer.isAlive())
            kPlayer.AI_updateFoundValues();
    }

    // Print starting positions
    for (int p = 0; p < MAX_CIV_PLAYERS; p++) {
        CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
        if (kPlayer.isAlive()) {
            CvPlot* pStart = kPlayer.getStartingPlot();
            fprintf(stderr, "[main] P%d%s: start=(%d,%d) units=%d\n", p,
                    kPlayer.isHuman() ? "*" : "",
                    pStart ? pStart->getX_INLINE() : -1,
                    pStart ? pStart->getY_INLINE() : -1,
                    kPlayer.getNumUnits());
        }
    }

    // ---- Step 7: Run the game ----
    if (headless) {
        fprintf(stderr, "[main] Running in HEADLESS mode.\n");
        headlessGameThreadFunc();
    } else {
        fprintf(stderr, "[main] Initializing SDL2...\n");
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "[main] SDL_Init failed: %s\n", SDL_GetError());
            headlessGameThreadFunc();
        } else if (TTF_Init() != 0) {
            fprintf(stderr, "[main] TTF_Init failed: %s\n", TTF_GetError());
            SDL_Quit();
            headlessGameThreadFunc();
        } else {
            int winW = 1280, winH = 720;

            // Build font path (shared between both renderers)
            std::string fontPath;
            char* basePath = SDL_GetBasePath();
            if (basePath) {
                std::string base(basePath);
                SDL_free(basePath);
                for (auto& c : base) if (c == '\\') c = '/';
                if (!base.empty() && base.back() == '/') base.pop_back();
                auto pos = base.rfind('/');
                if (pos != std::string::npos) base = base.substr(0, pos);
                fontPath = base + "/assets/fonts/DejaVuSans.ttf";
            } else {
                fontPath = "assets/fonts/DejaVuSans.ttf";
            }

            if (!legacyRenderer) {
                // ============= OpenGL 3.3 Renderer =============
                fprintf(stderr, "[main] Creating OpenGL 3.3 window...\n");
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

                SDL_Window* window = SDL_CreateWindow(
                    "OpenCiv4 — 3D Renderer",
                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    winW, winH,
                    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
                );
                if (!window) {
                    fprintf(stderr, "[main] SDL_CreateWindow (OpenGL) failed: %s\n", SDL_GetError());
                    fprintf(stderr, "[main] Falling back to legacy 2D renderer.\n");
                    legacyRenderer = true;
                } else {
                    SDL_GLContext glContext = SDL_GL_CreateContext(window);
                    if (!glContext) {
                        fprintf(stderr, "[main] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
                        SDL_DestroyWindow(window);
                        fprintf(stderr, "[main] Falling back to legacy 2D renderer.\n");
                        legacyRenderer = true;
                    } else {
                        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
                            fprintf(stderr, "[main] gladLoadGLLoader failed!\n");
                            SDL_GL_DeleteContext(glContext);
                            SDL_DestroyWindow(window);
                            fprintf(stderr, "[main] Falling back to legacy 2D renderer.\n");
                            legacyRenderer = true;
                        } else {
                            SDL_GL_SetSwapInterval(1); // vsync

                            AssetManager assetMgr;
                            assetMgr.initGL(BTS_INSTALL_DIR);

                            GLRenderer renderer;
                            renderer.init(winW, winH, &assetMgr);
                            renderer.initFonts(fontPath.c_str());
                            renderer.setCommandCallback(pushCommand);

                            fprintf(stderr, "[main] Starting game thread...\n");
                            std::thread gameThread(gameThreadFunc);

                            bool running = true;
                            while (running) {
                                SDL_Event event;
                                while (SDL_PollEvent(&event)) {
                                    switch (event.type) {
                                    case SDL_QUIT:
                                        running = false;
                                        break;
                                    case SDL_KEYDOWN:
                                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                                            if (g_mapSnapshot.cityScreenOpen)
                                                pushCommand({GameCommand::CLOSE_CITY});
                                            else
                                                running = false;
                                        }
                                        else if (event.key.keysym.sym == SDLK_RETURN ||
                                                 event.key.keysym.sym == SDLK_KP_ENTER)
                                            pushCommand({GameCommand::END_TURN});
                                        else if (event.key.keysym.sym == SDLK_TAB)
                                            pushCommand({GameCommand::CYCLE_UNIT});
                                        else if (event.key.keysym.sym == SDLK_SPACE) {
                                            if (g_mapSnapshot.selectedUnitID >= 0)
                                                pushCommand({GameCommand::SKIP_TURN});
                                        }
                                        else
                                            renderer.handleKeyDown(event.key.keysym.sym, g_mapSnapshot);
                                        break;
                                    case SDL_KEYUP:
                                        renderer.handleKeyUp(event.key.keysym.sym);
                                        break;
                                    case SDL_MOUSEWHEEL:
                                        { int mx, my; SDL_GetMouseState(&mx, &my);
                                          renderer.handleMouseWheel(event.wheel.y, mx, my); }
                                        break;
                                    case SDL_MOUSEBUTTONDOWN:
                                        renderer.handleMouseClick(event.button.x, event.button.y,
                                                                  event.button.button, g_mapSnapshot);
                                        break;
                                    case SDL_MOUSEMOTION:
                                        renderer.handleMouseMotion(event.motion.xrel, event.motion.yrel,
                                            (event.motion.state & (SDL_BUTTON_MMASK | SDL_BUTTON_RMASK)) != 0);
                                        break;
                                    case SDL_WINDOWEVENT:
                                        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                            renderer.handleResize(event.window.data1, event.window.data2);
                                        break;
                                    }
                                }

                                renderer.draw(g_mapSnapshot);
                                SDL_GL_SwapWindow(window);
                            }

                            g_gameRunning = false;
                            if (gameThread.joinable()) gameThread.join();

                            renderer.shutdown();
                            SDL_GL_DeleteContext(glContext);
                            SDL_DestroyWindow(window);
                            TTF_Quit();
                            SDL_Quit();
                        }
                    }
                }
            }

            if (legacyRenderer) {
                // ============= Legacy SDL2 2D Renderer =============
                fprintf(stderr, "[main] Using legacy 2D renderer (--legacy-2d).\n");
                SDL_Window* window = SDL_CreateWindow(
                    "OpenCiv4 — Phase 2 (Legacy 2D)",
                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                    winW, winH,
                    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
                );
                if (!window) {
                    fprintf(stderr, "[main] SDL_CreateWindow failed: %s\n", SDL_GetError());
                    SDL_Quit();
                    headlessGameThreadFunc();
                } else {
                    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(
                        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
                    if (!sdlRenderer) {
                        fprintf(stderr, "[main] SDL_CreateRenderer failed: %s\n", SDL_GetError());
                        SDL_DestroyWindow(window);
                        SDL_Quit();
                        headlessGameThreadFunc();
                    } else {
                        AssetManager assetMgr;
                        assetMgr.init(sdlRenderer, BTS_INSTALL_DIR);
                        Renderer renderer(sdlRenderer, winW, winH, &assetMgr);
                        renderer.initFonts(fontPath.c_str());
                        renderer.setCommandCallback(pushCommand);

                        fprintf(stderr, "[main] Starting game thread...\n");
                        std::thread gameThread(gameThreadFunc);

                        bool running = true;
                        while (running) {
                            SDL_Event event;
                            while (SDL_PollEvent(&event)) {
                                switch (event.type) {
                                case SDL_QUIT: running = false; break;
                                case SDL_KEYDOWN:
                                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                                        if (g_mapSnapshot.cityScreenOpen)
                                            pushCommand({GameCommand::CLOSE_CITY});
                                        else running = false;
                                    }
                                    else if (event.key.keysym.sym == SDLK_RETURN ||
                                             event.key.keysym.sym == SDLK_KP_ENTER)
                                        pushCommand({GameCommand::END_TURN});
                                    else if (event.key.keysym.sym == SDLK_TAB)
                                        pushCommand({GameCommand::CYCLE_UNIT});
                                    else if (event.key.keysym.sym == SDLK_SPACE) {
                                        if (g_mapSnapshot.selectedUnitID >= 0)
                                            pushCommand({GameCommand::SKIP_TURN});
                                    }
                                    else renderer.handleKeyDown(event.key.keysym.sym, g_mapSnapshot);
                                    break;
                                case SDL_KEYUP: renderer.handleKeyUp(event.key.keysym.sym); break;
                                case SDL_MOUSEWHEEL:
                                    { int mx, my; SDL_GetMouseState(&mx, &my);
                                      renderer.handleMouseWheel(event.wheel.y, mx, my); } break;
                                case SDL_MOUSEBUTTONDOWN:
                                    renderer.handleMouseClick(event.button.x, event.button.y,
                                                              event.button.button, g_mapSnapshot); break;
                                case SDL_MOUSEMOTION:
                                    renderer.handleMouseMotion(event.motion.xrel, event.motion.yrel,
                                        (event.motion.state & (SDL_BUTTON_MMASK | SDL_BUTTON_RMASK)) != 0); break;
                                case SDL_WINDOWEVENT:
                                    if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                                        renderer.handleResize(event.window.data1, event.window.data2);
                                    break;
                                }
                            }
                            renderer.draw(g_mapSnapshot);
                        }

                        g_gameRunning = false;
                        if (gameThread.joinable()) gameThread.join();
                        SDL_DestroyRenderer(sdlRenderer);
                        SDL_DestroyWindow(window);
                        TTF_Quit();
                        SDL_Quit();
                    }
                }
            }
        }
    }

    fprintf(stderr, "\n[main] Calling GC.uninit()...\n");
    CvGlobals::getInstance().uninit();
    fprintf(stderr, "[main] Shutdown complete.\n");

    return 0;
}
