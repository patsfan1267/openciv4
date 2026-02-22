// StubLinkFixes.cpp — No-op stubs for symbols from source files excluded from the build.
//
// Excluded files that create unresolved externals:
//   CvEventReporter.cpp    — Python event callbacks (all no-ops headless)
//   CvDllPythonEvents.cpp  — Python event dispatch (all no-ops headless)
//   CvGameInterface.cpp    — UI interaction callbacks (not needed headless)
//   CvGameCoreDLL.cpp      — DLL entry point + profiling (not a DLL, no profiling)
//   Cy*.cpp                — Python binding class implementations (not needed headless)
//
// This file is part of the OpenCiv4 engine, NOT part of the gamecore library.
// It lives in src/engine/stubs/ and is compiled as part of the engine static lib.

#include "CvGameCoreDLL.h"
#include "CvEventReporter.h"
#include "CvDllPythonEvents.h"
#include "CvStatistics.h"
#include "CvGame.h"
#include "CyCity.h"
#include "CyUnit.h"
#include "CyPlot.h"
#include "CySelectionGroup.h"
#include "CyArgsList.h"
#include "CvDLLPythonIFaceBase.h"

// ============================================================================
// CvEventReporter — singleton that fires Python callbacks for game events.
// In headless mode, all events are silently ignored.
// ============================================================================

CvEventReporter& CvEventReporter::getInstance()
{
    static CvEventReporter s_instance;
    return s_instance;
}

void CvEventReporter::resetStatistics() {}
bool CvEventReporter::mouseEvent(int, int, int, bool) { return false; }
bool CvEventReporter::kbdEvent(int, int, int, int) { return false; }
void CvEventReporter::genericEvent(const char*, void*) {}
void CvEventReporter::newGame() {}
void CvEventReporter::newPlayer(PlayerTypes) {}
void CvEventReporter::reportModNetMessage(int, int, int, int, int) {}
void CvEventReporter::init() {}
void CvEventReporter::update(float) {}
void CvEventReporter::unInit() {}
void CvEventReporter::gameStart() {}
void CvEventReporter::gameEnd() {}
void CvEventReporter::windowActivation(bool) {}
void CvEventReporter::beginGameTurn(int) {}
void CvEventReporter::endGameTurn(int) {}
void CvEventReporter::beginPlayerTurn(int, PlayerTypes) {}
void CvEventReporter::endPlayerTurn(int, PlayerTypes) {}
void CvEventReporter::firstContact(TeamTypes, TeamTypes) {}
void CvEventReporter::combatResult(CvUnit*, CvUnit*) {}
void CvEventReporter::improvementBuilt(int, int, int) {}
void CvEventReporter::improvementDestroyed(int, int, int, int) {}
void CvEventReporter::routeBuilt(int, int, int) {}
void CvEventReporter::plotRevealed(CvPlot*, TeamTypes) {}
void CvEventReporter::plotFeatureRemoved(CvPlot*, FeatureTypes, CvCity*) {}
void CvEventReporter::plotPicked(CvPlot*) {}
void CvEventReporter::nukeExplosion(CvPlot*, CvUnit*) {}
void CvEventReporter::gotoPlotSet(CvPlot*, PlayerTypes) {}
void CvEventReporter::cityBuilt(CvCity*) {}
void CvEventReporter::cityRazed(CvCity*, PlayerTypes) {}
void CvEventReporter::cityAcquired(PlayerTypes, PlayerTypes, CvCity*, bool, bool) {}
void CvEventReporter::cityAcquiredAndKept(PlayerTypes, CvCity*) {}
void CvEventReporter::cityLost(CvCity*) {}
void CvEventReporter::cultureExpansion(CvCity*, PlayerTypes) {}
void CvEventReporter::cityGrowth(CvCity*, PlayerTypes) {}
void CvEventReporter::cityDoTurn(CvCity*, PlayerTypes) {}
void CvEventReporter::cityBuildingUnit(CvCity*, UnitTypes) {}
void CvEventReporter::cityBuildingBuilding(CvCity*, BuildingTypes) {}
void CvEventReporter::cityRename(CvCity*) {}
void CvEventReporter::cityHurry(CvCity*, HurryTypes) {}
void CvEventReporter::selectionGroupPushMission(CvSelectionGroup*, MissionTypes) {}
void CvEventReporter::unitMove(CvPlot*, CvUnit*, CvPlot*) {}
void CvEventReporter::unitSetXY(CvPlot*, CvUnit*) {}
void CvEventReporter::unitCreated(CvUnit*) {}
void CvEventReporter::unitBuilt(CvCity*, CvUnit*) {}
void CvEventReporter::unitKilled(CvUnit*, PlayerTypes) {}
void CvEventReporter::unitLost(CvUnit*) {}
void CvEventReporter::unitPromoted(CvUnit*, PromotionTypes) {}
void CvEventReporter::unitSelected(CvUnit*) {}
void CvEventReporter::unitRename(CvUnit*) {}
void CvEventReporter::unitPillage(CvUnit*, ImprovementTypes, RouteTypes, PlayerTypes) {}
void CvEventReporter::unitSpreadReligionAttempt(CvUnit*, ReligionTypes, bool) {}
void CvEventReporter::unitGifted(CvUnit*, PlayerTypes, CvPlot*) {}
void CvEventReporter::unitBuildImprovement(CvUnit*, BuildTypes, bool) {}
void CvEventReporter::goodyReceived(PlayerTypes, CvPlot*, CvUnit*, GoodyTypes) {}
void CvEventReporter::greatPersonBorn(CvUnit*, PlayerTypes, CvCity*) {}
void CvEventReporter::buildingBuilt(CvCity*, BuildingTypes) {}
void CvEventReporter::projectBuilt(CvCity*, ProjectTypes) {}
void CvEventReporter::techAcquired(TechTypes, TeamTypes, PlayerTypes, bool) {}
void CvEventReporter::techSelected(TechTypes, PlayerTypes) {}
void CvEventReporter::religionFounded(ReligionTypes, PlayerTypes) {}
void CvEventReporter::religionSpread(ReligionTypes, PlayerTypes, CvCity*) {}
void CvEventReporter::religionRemove(ReligionTypes, PlayerTypes, CvCity*) {}
void CvEventReporter::corporationFounded(CorporationTypes, PlayerTypes) {}
void CvEventReporter::corporationSpread(CorporationTypes, PlayerTypes, CvCity*) {}
void CvEventReporter::corporationRemove(CorporationTypes, PlayerTypes, CvCity*) {}
void CvEventReporter::goldenAge(PlayerTypes) {}
void CvEventReporter::endGoldenAge(PlayerTypes) {}
void CvEventReporter::changeWar(bool, TeamTypes, TeamTypes) {}
void CvEventReporter::setPlayerAlive(PlayerTypes, bool) {}
void CvEventReporter::playerChangeStateReligion(PlayerTypes, ReligionTypes, ReligionTypes) {}
void CvEventReporter::playerGoldTrade(PlayerTypes, PlayerTypes, int) {}
void CvEventReporter::chat(CvWString) {}
void CvEventReporter::victory(TeamTypes, VictoryTypes) {}
void CvEventReporter::vassalState(TeamTypes, TeamTypes, bool) {}
void CvEventReporter::preSave() {}
void CvEventReporter::getGameStatistics(std::vector<CvStatBase*>&) {}
void CvEventReporter::getPlayerStatistics(PlayerTypes, std::vector<CvStatBase*>&) {}
void CvEventReporter::readStatistics(FDataStreamBase*) {}
void CvEventReporter::writeStatistics(FDataStreamBase*) {}

// ============================================================================
// CvDllPythonEvents — dispatches game events to Python scripts.
// All no-ops in headless mode.
// ============================================================================

void CvDllPythonEvents::reportGenericEvent(const char*, void*) {}
bool CvDllPythonEvents::reportKbdEvent(int, int, int, int) { return false; }
bool CvDllPythonEvents::reportMouseEvent(int, int, int, bool) { return false; }
void CvDllPythonEvents::reportModNetMessage(int, int, int, int, int) {}
void CvDllPythonEvents::reportInit() {}
void CvDllPythonEvents::reportUpdate(float) {}
void CvDllPythonEvents::reportUnInit() {}
void CvDllPythonEvents::reportGameStart() {}
void CvDllPythonEvents::reportGameEnd() {}
void CvDllPythonEvents::reportWindowActivation(bool) {}
void CvDllPythonEvents::reportBeginGameTurn(int) {}
void CvDllPythonEvents::reportEndGameTurn(int) {}
void CvDllPythonEvents::reportBeginPlayerTurn(int, PlayerTypes) {}
void CvDllPythonEvents::reportEndPlayerTurn(int, PlayerTypes) {}
void CvDllPythonEvents::reportFirstContact(TeamTypes, TeamTypes) {}
void CvDllPythonEvents::reportCombatResult(CvUnit*, CvUnit*) {}
void CvDllPythonEvents::reportImprovementBuilt(int, int, int) {}
void CvDllPythonEvents::reportImprovementDestroyed(int, int, int, int) {}
void CvDllPythonEvents::reportRouteBuilt(int, int, int) {}
void CvDllPythonEvents::reportPlotRevealed(CvPlot*, TeamTypes) {}
void CvDllPythonEvents::reportPlotFeatureRemoved(CvPlot*, FeatureTypes, CvCity*) {}
void CvDllPythonEvents::reportPlotPicked(CvPlot*) {}
void CvDllPythonEvents::reportNukeExplosion(CvPlot*, CvUnit*) {}
void CvDllPythonEvents::reportGotoPlotSet(CvPlot*, PlayerTypes) {}
void CvDllPythonEvents::reportCityBuilt(CvCity*) {}
void CvDllPythonEvents::reportCityRazed(CvCity*, PlayerTypes) {}
void CvDllPythonEvents::reportCityAcquired(PlayerTypes, PlayerTypes, CvCity*, bool, bool) {}
void CvDllPythonEvents::reportCityAcquiredAndKept(PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportCityLost(CvCity*) {}
void CvDllPythonEvents::reportCultureExpansion(CvCity*, PlayerTypes) {}
void CvDllPythonEvents::reportCityGrowth(CvCity*, PlayerTypes) {}
void CvDllPythonEvents::reportCityProduction(CvCity*, PlayerTypes) {}
void CvDllPythonEvents::reportCityBuildingUnit(CvCity*, UnitTypes) {}
void CvDllPythonEvents::reportCityBuildingBuilding(CvCity*, BuildingTypes) {}
void CvDllPythonEvents::reportCityRename(CvCity*) {}
void CvDllPythonEvents::reportCityHurry(CvCity*, HurryTypes) {}
void CvDllPythonEvents::reportSelectionGroupPushMission(CvSelectionGroup*, MissionTypes) {}
void CvDllPythonEvents::reportUnitMove(CvPlot*, CvUnit*, CvPlot*) {}
void CvDllPythonEvents::reportUnitSetXY(CvPlot*, CvUnit*) {}
void CvDllPythonEvents::reportUnitCreated(CvUnit*) {}
void CvDllPythonEvents::reportUnitBuilt(CvCity*, CvUnit*) {}
void CvDllPythonEvents::reportUnitKilled(CvUnit*, PlayerTypes) {}
void CvDllPythonEvents::reportUnitLost(CvUnit*) {}
void CvDllPythonEvents::reportUnitPromoted(CvUnit*, PromotionTypes) {}
void CvDllPythonEvents::reportUnitSelected(CvUnit*) {}
void CvDllPythonEvents::reportUnitRename(CvUnit*) {}
void CvDllPythonEvents::reportUnitPillage(CvUnit*, ImprovementTypes, RouteTypes, PlayerTypes) {}
void CvDllPythonEvents::reportUnitSpreadReligionAttempt(CvUnit*, ReligionTypes, bool) {}
void CvDllPythonEvents::reportUnitGifted(CvUnit*, PlayerTypes, CvPlot*) {}
void CvDllPythonEvents::reportUnitBuildImprovement(CvUnit*, BuildTypes, bool) {}
void CvDllPythonEvents::reportGoodyReceived(PlayerTypes, CvPlot*, CvUnit*, GoodyTypes) {}
void CvDllPythonEvents::reportGreatPersonBorn(CvUnit*, PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportBuildingBuilt(CvCity*, BuildingTypes) {}
void CvDllPythonEvents::reportProjectBuilt(CvCity*, ProjectTypes) {}
void CvDllPythonEvents::reportTechAcquired(TechTypes, TeamTypes, PlayerTypes, bool) {}
void CvDllPythonEvents::reportTechSelected(TechTypes, PlayerTypes) {}
void CvDllPythonEvents::reportReligionFounded(ReligionTypes, PlayerTypes) {}
void CvDllPythonEvents::reportReligionSpread(ReligionTypes, PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportReligionRemove(ReligionTypes, PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportCorporationFounded(CorporationTypes, PlayerTypes) {}
void CvDllPythonEvents::reportCorporationSpread(CorporationTypes, PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportCorporationRemove(CorporationTypes, PlayerTypes, CvCity*) {}
void CvDllPythonEvents::reportGoldenAge(PlayerTypes) {}
void CvDllPythonEvents::reportEndGoldenAge(PlayerTypes) {}
void CvDllPythonEvents::reportChangeWar(bool, TeamTypes, TeamTypes) {}
void CvDllPythonEvents::reportChat(CvWString) {}
void CvDllPythonEvents::reportVictory(TeamTypes, VictoryTypes) {}
void CvDllPythonEvents::reportVassalState(TeamTypes, TeamTypes, bool) {}
void CvDllPythonEvents::reportSetPlayerAlive(PlayerTypes, bool) {}
void CvDllPythonEvents::reportPlayerChangeStateReligion(PlayerTypes, ReligionTypes, ReligionTypes) {}
void CvDllPythonEvents::reportPlayerGoldTrade(PlayerTypes, PlayerTypes, int) {}
void CvDllPythonEvents::preSave() {}
bool CvDllPythonEvents::preEvent() { return false; }
bool CvDllPythonEvents::postEvent(CyArgsList&) { return false; }

// ============================================================================
// Cy* Python wrapper class constructors
// These classes wrap C++ game objects for Python script access.
// In headless mode, they just store the pointer for internal use.
// ============================================================================

CyCity::CyCity() : m_pCity(NULL) {}
CyCity::CyCity(CvCity* pCity) : m_pCity(pCity) {}

CyUnit::CyUnit() : m_pUnit(NULL) {}
CyUnit::CyUnit(CvUnit* pUnit) : m_pUnit(pUnit) {}

CyPlot::CyPlot() : m_pPlot(NULL) {}
CyPlot::CyPlot(CvPlot* pPlot) : m_pPlot(pPlot) {}

CySelectionGroup::CySelectionGroup() : m_pSelectionGroup(NULL) {}
CySelectionGroup::CySelectionGroup(CvSelectionGroup* pSelectionGroup) : m_pSelectionGroup(pSelectionGroup) {}

// ============================================================================
// CyArgsList — builds argument lists for Python function calls.
// In headless mode, just track the pointers (no actual Python objects created).
// ============================================================================

void CyArgsList::add(int i)                          { push_back(reinterpret_cast<void*>(static_cast<intptr_t>(i))); }
void CyArgsList::add(float)                          {}
void CyArgsList::add(const char* s)                  { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(const wchar* s)                 { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(const char* s, int)             { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(const byte* s, int)             { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(const int* s, int)              { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(const float* s, int)            { push_back(const_cast<void*>(static_cast<const void*>(s))); }
void CyArgsList::add(void* p)                        { push_back(p); }
void* CyArgsList::makeFunctionArgs()                 { return nullptr; }

// ============================================================================
// CvGame methods from CvGameInterface.cpp — UI interaction callbacks.
// Not needed in headless mode.
// ============================================================================

void CvGame::updateColoredPlots()  {}
void CvGame::updateSelectionList() {}
VictoryTypes CvGame::getSpaceVictory() const { return NO_VICTORY; }
void CvGame::getPlotUnits(const CvPlot*, std::vector<CvUnit*>& plotUnits) const { plotUnits.clear(); }

// ============================================================================
// Profiling hooks from CvGameCoreDLL.cpp
// Not needed — no profiling infrastructure in OpenCiv4 yet.
// ============================================================================

void startProfilingDLL() {}
void stopProfilingDLL()  {}
