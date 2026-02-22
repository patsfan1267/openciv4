#pragma once

// =============================================================================
// StubInterfaces.h  --  Headless stub implementations of all CvDLL*IFaceBase
//                        interfaces for the OpenCiv4 64-bit engine replacement.
//
// Every method body is a no-op that returns a sensible default value.
// Memory management methods delegate to the C runtime (malloc/free/realloc).
// Important lifecycle calls (init, save, load) log to stderr via fprintf.
//
// The master stub -- StubUtilityIFace -- owns instances of every sub-interface
// and returns pointers to them from the get*IFace() accessors.
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// Pull in the original Firaxis interface base classes.
// These are found via the include path pointing at the gamecore SDK headers.
#include "CvDLLUtilityIFaceBase.h"
#include "CvDLLEngineIFaceBase.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLEntityIFaceBase.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvDLLFlagEntityIFaceBase.h"
#include "CvDLLIniParserIFaceBase.h"
#include "CvDLLPlotBuilderIFaceBase.h"
#include "CvDLLSymbolIFaceBase.h"
#include "CvDLLXMLIFaceBase.h"
#include "CvDLLEventReporterIFaceBase.h"

namespace OpenCiv4 {

// =============================================================================
//  StubEngineIFace
// =============================================================================
class StubEngineIFace : public CvDLLEngineIFaceBase
{
public:
    void cameraLookAt(NiPoint3 lookingPoint) override {} // STUB
    bool isCameraLocked() override { return false; } // STUB

    void SetObeyEntityVisibleFlags(bool bObeyHide) override {} // STUB
    void AutoSave(bool bInitial) override { fprintf(stderr, "[StubEngine] AutoSave(initial=%d)\n", bInitial); } // STUB
    void SaveReplay(PlayerTypes ePlayer) override { fprintf(stderr, "[StubEngine] SaveReplay(player=%d)\n", (int)ePlayer); } // STUB
    void SaveGame(CvString& szFilename, SaveGameTypes eType) override { fprintf(stderr, "[StubEngine] SaveGame(%s)\n", szFilename.c_str()); } // STUB
    void DoTurn() override {} // STUB
    void ClearMinimap() override {} // STUB
    byte GetLandscapePlotTerrainData(uint uiX, uint uiY, uint uiPointX, uint uiPointY) override { return 0; } // STUB
    byte GetLandscapePlotHeightData(uint uiX, uint uiY, uint uiPointX, uint uiPointY) override { return 0; } // STUB
    LoadType getLoadType() override { return (LoadType)0; } // STUB
    void ClampToWorldCoords(NiPoint3* pPt3, float fOffset) override {} // STUB
    void SetCameraZoom(float zoom) override {} // STUB
    float GetUpdateRate() override { return 0.0f; } // STUB
    bool SetUpdateRate(float fUpdateRate) override { return false; } // STUB
    void toggleGlobeview() override {} // STUB
    bool isGlobeviewUp() override { return false; } // STUB
    void toggleResourceLayer() override {} // STUB
    void toggleUnitLayer() override {} // STUB
    void setResourceLayer(bool bOn) override {} // STUB

    void MoveBaseTurnRight(float increment) override {} // STUB
    void MoveBaseTurnLeft(float increment) override {} // STUB
    void SetFlying(bool value) override {} // STUB
    void CycleFlyingMode(int displacement) override {} // STUB
    void SetMouseFlying(bool value) override {} // STUB
    void SetSatelliteMode(bool value) override {} // STUB
    void SetOrthoCamera(bool value) override {} // STUB
    bool GetFlying() override { return false; } // STUB
    bool GetMouseFlying() override { return false; } // STUB
    bool GetSatelliteMode() override { return false; } // STUB
    bool GetOrthoCamera() override { return false; } // STUB

    int InitGraphics() override { fprintf(stderr, "[StubEngine] InitGraphics()\n"); return 0; } // STUB
    void GetLandscapeDimensions(float& fWidth, float& fHeight) override { fWidth = 0.0f; fHeight = 0.0f; } // STUB
    void GetLandscapeGameDimensions(float& fWidth, float& fHeight) override { fWidth = 0.0f; fHeight = 0.0f; } // STUB
    uint GetGameCellSizeX() override { return 0; } // STUB
    uint GetGameCellSizeY() override { return 0; } // STUB
    float GetPointZSpacing() override { return 0.0f; } // STUB
    float GetPointXYSpacing() override { return 0.0f; } // STUB
    float GetPointXSpacing() override { return 0.0f; } // STUB
    float GetPointYSpacing() override { return 0.0f; } // STUB
    float GetHeightmapZ(const NiPoint3& pt3, bool bClampAboveWater) override { return 0.0f; } // STUB
    void LightenVisibility(uint) override {} // STUB
    void DarkenVisibility(uint) override {} // STUB
    void BlackenVisibility(uint) override {} // STUB
    void RebuildAllPlots() override {} // STUB
    void RebuildPlot(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures) override {} // STUB
    void RebuildRiverPlotTile(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures) override {} // STUB
    void RebuildTileArt(int plotX, int plotY) override {} // STUB
    void ForceTreeOffsets(int plotX, int plotY) override {} // STUB

    bool GetGridMode() override { return false; } // STUB
    void SetGridMode(bool bVal) override {} // STUB

    void addColoredPlot(int plotX, int plotY, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer) override {} // STUB
    void clearColoredPlots(PlotLandscapeLayers layer) override {} // STUB
    void fillAreaBorderPlot(int plotX, int plotY, const NiColorA& color, AreaBorderLayers layer) override {} // STUB
    void clearAreaBorderPlots(AreaBorderLayers layer) override {} // STUB
    void updateFoundingBorder() override {} // STUB
    void addLandmark(CvPlot* plot, const wchar* caption) override {} // STUB

    void TriggerEffect(int iEffect, NiPoint3 pt3Point, float rotation) override {} // STUB
    void printProfileText() override {} // STUB

    void clearSigns() override {} // STUB
    CvPlot* pickPlot(int x, int y, NiPoint3& worldPoint) override { return nullptr; } // STUB

    void SetDirty(EngineDirtyBits eBit, bool bNewValue) override {} // STUB
    bool IsDirty(EngineDirtyBits eBit) override { return false; } // STUB
    void PushFogOfWar(FogOfWarModeTypes eNewMode) override {} // STUB
    FogOfWarModeTypes PopFogOfWar() override { return (FogOfWarModeTypes)0; } // STUB
    void setFogOfWarFromStack() override {} // STUB
    void MarkBridgesDirty() override {} // STUB
    void AddLaunch(PlayerTypes playerType) override {} // STUB
    void AddGreatWall(CvCity* city) override {} // STUB
    void RemoveGreatWall(CvCity* city) override {} // STUB
    void MarkPlotTextureAsDirty(int plotX, int plotY) override {} // STUB
};

// =============================================================================
//  StubInterfaceIFace
// =============================================================================
class StubInterfaceIFace : public CvDLLInterfaceIFaceBase
{
public:
    void lookAtSelectionPlot(bool bRelease) override {} // STUB

    bool canHandleAction(int iAction, CvPlot* pPlot, bool bTestVisible) override { return false; } // STUB
    bool canDoInterfaceMode(InterfaceModeTypes eInterfaceMode, CvSelectionGroup* pSelectionGroup) override { return false; } // STUB

    CvPlot* getLookAtPlot() override { return nullptr; } // STUB
    CvPlot* getSelectionPlot() override { return nullptr; } // STUB
    CvUnit* getInterfacePlotUnit(const CvPlot* pPlot, int iIndex) override { return nullptr; } // STUB
    CvUnit* getSelectionUnit(int iIndex) override { return nullptr; } // STUB
    CvUnit* getHeadSelectedUnit() override { return nullptr; } // STUB
    void selectUnit(CvUnit* pUnit, bool bClear, bool bToggle, bool bSound) override {} // STUB
    void selectGroup(CvUnit* pUnit, bool bShift, bool bCtrl, bool bAlt) override {} // STUB
    void selectAll(CvPlot* pPlot) override {} // STUB

    bool removeFromSelectionList(CvUnit* pUnit) override { return false; } // STUB
    void makeSelectionListDirty() override {} // STUB
    bool mirrorsSelectionGroup() override { return false; } // STUB
    bool canSelectionListFound() override { return false; } // STUB

    void bringToTop(CvPopup* pPopup) override {} // STUB
    bool isPopupUp() override { return false; } // STUB
    bool isPopupQueued() override { return false; } // STUB
    bool isDiploOrPopupWaiting() override { return false; } // STUB

    CvUnit* getLastSelectedUnit() override { return nullptr; } // STUB
    void setLastSelectedUnit(CvUnit* pUnit) override {} // STUB
    void changePlotListColumn(int iChange) override {} // STUB
    CvPlot* getGotoPlot() override { return nullptr; } // STUB
    CvPlot* getSingleMoveGotoPlot() override { return nullptr; } // STUB
    CvPlot* getOriginalPlot() override { return nullptr; } // STUB

    void playGeneralSound(LPCTSTR pszSound, NiPoint3 vPos) override {} // STUB
    void playGeneralSound(int iSoundId, int iSoundType, NiPoint3 vPos) override {} // STUB
    void clearQueuedPopups() override {} // STUB

    CvSelectionGroup* getSelectionList() override { return nullptr; } // STUB
    void clearSelectionList() override {} // STUB
    void insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange) override {} // STUB
    void selectionListPostChange() override {} // STUB
    void selectionListPreChange() override {} // STUB
    int getSymbolID(int iSymbol) override { return 0; } // STUB
    CLLNode<IDInfo>* deleteSelectionListNode(CLLNode<IDInfo>* pNode) override { return nullptr; } // STUB
    CLLNode<IDInfo>* nextSelectionListNode(CLLNode<IDInfo>* pNode) override { return nullptr; } // STUB
    int getLengthSelectionList() override { return 0; } // STUB
    CLLNode<IDInfo>* headSelectionListNode() override { return nullptr; } // STUB

    void selectCity(CvCity* pNewValue, bool bTestProduction) override {} // STUB
    void selectLookAtCity(bool bAdd) override {} // STUB
    void addSelectedCity(CvCity* pNewValue, bool bToggle) override {} // STUB
    void clearSelectedCities() override {} // STUB
    bool isCitySelected(CvCity* pCity) override { return false; } // STUB
    CvCity* getHeadSelectedCity() override { return nullptr; } // STUB
    bool isCitySelection() override { return false; } // STUB
    CLLNode<IDInfo>* nextSelectedCitiesNode(CLLNode<IDInfo>* pNode) override { return nullptr; } // STUB
    CLLNode<IDInfo>* headSelectedCitiesNode() override { return nullptr; } // STUB

    void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, LPCTSTR pszSound,
        InterfaceMessageTypes eType, LPCSTR pszIcon, ColorTypes eFlashColor,
        int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows) override {} // STUB
    void addCombatMessage(PlayerTypes ePlayer, CvWString szString) override {} // STUB
    void addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId) override {} // STUB
    void showMessage(CvTalkingHeadMessage& msg) override {} // STUB
    void flushTalkingHeadMessages() override {} // STUB
    void clearEventMessages() override {} // STUB
    void addPopup(CvPopupInfo* pInfo, PlayerTypes ePlayer, bool bImmediate, bool bFront) override {} // STUB
    void getDisplayedButtonPopups(CvPopupQueue& infos) override {} // STUB

    int getCycleSelectionCounter() override { return 0; } // STUB
    void setCycleSelectionCounter(int iNewValue) override {} // STUB
    void changeCycleSelectionCounter(int iChange) override {} // STUB

    int getEndTurnCounter() override { return 0; } // STUB
    void setEndTurnCounter(int iNewValue) override {} // STUB
    void changeEndTurnCounter(int iChange) override {} // STUB

    bool isCombatFocus() override { return false; } // STUB
    void setCombatFocus(bool bNewValue) override {} // STUB
    void setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override {} // STUB

    bool isDirty(InterfaceDirtyBits eDirtyItem) override { return false; } // STUB
    void setDirty(InterfaceDirtyBits eDirtyItem, bool bNewValue) override {} // STUB
    void makeInterfaceDirty() override {} // STUB
    bool updateCursorType() override { return false; } // STUB
    void updatePythonScreens() override {} // STUB

    void lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection) override {} // STUB
    void centerCamera(CvUnit*) override {} // STUB
    void releaseLockedCamera() override {} // STUB
    bool isFocusedWidget() override { return false; } // STUB
    bool isFocused() override { return false; } // STUB
    bool isBareMapMode() override { return false; } // STUB
    void toggleBareMapMode() override {} // STUB
    bool isShowYields() override { return false; } // STUB
    void toggleYieldVisibleMode() override {} // STUB
    bool isScoresVisible() override { return false; } // STUB
    void toggleScoresVisible() override {} // STUB
    bool isScoresMinimized() override { return false; } // STUB
    void toggleScoresMinimized() override {} // STUB
    bool isNetStatsVisible() override { return false; } // STUB

    int getOriginalPlotCount() override { return 0; } // STUB
    bool isCityScreenUp() override { return false; } // STUB
    bool isEndTurnMessage() override { return false; } // STUB
    void setInterfaceMode(InterfaceModeTypes eNewValue) override {} // STUB
    InterfaceModeTypes getInterfaceMode() override { return (InterfaceModeTypes)0; } // STUB
    InterfaceVisibility getShowInterface() override { return (InterfaceVisibility)0; } // STUB
    CvPlot* getMouseOverPlot() override { return nullptr; } // STUB
    void setFlashing(PlayerTypes eWho, bool bFlashing) override {} // STUB
    bool isFlashing(PlayerTypes eWho) override { return false; } // STUB
    void setDiplomacyLocked(bool bLocked) override {} // STUB
    bool isDiplomacyLocked() override { return false; } // STUB

    void setMinimapColor(MinimapModeTypes eMinimapMode, int iX, int iY, ColorTypes eColor, float fAlpha) override {} // STUB
    unsigned char* getMinimapBaseTexture() const override { return nullptr; } // STUB
    void setEndTurnMessage(bool bNewValue) override {} // STUB

    bool isHasMovedUnit() override { return false; } // STUB
    void setHasMovedUnit(bool bNewValue) override {} // STUB

    bool isForcePopup() override { return false; } // STUB
    void setForcePopup(bool bNewValue) override {} // STUB

    void lookAtCityOffset(int iCity) override {} // STUB

    void toggleTurnLog() override {} // STUB
    void showTurnLog(ChatTargetTypes eTarget) override {} // STUB
    void dirtyTurnLog(PlayerTypes ePlayer) override {} // STUB

    int getPlotListColumn() override { return 0; } // STUB
    void verifyPlotListColumn() override {} // STUB
    int getPlotListOffset() override { return 0; } // STUB

    void unlockPopupHelp() override {} // STUB

    void showDetails(bool bPasswordOnly) override {} // STUB
    void showAdminDetails() override {} // STUB

    void toggleClockAlarm(bool bValue, int iHour, int iMin) override {} // STUB
    bool isClockAlarmOn() override { return false; } // STUB

    void setScreenDying(int iPythonFileID, bool bDying) override {} // STUB
    bool isExitingToMainMenu() override { return false; } // STUB
    void exitingToMainMenu(const char* szLoadFile) override {} // STUB
    void setWorldBuilder(bool bTurnOn) override {} // STUB

    int getFontLeftJustify() override { return DLL_FONT_LEFT_JUSTIFY; } // STUB
    int getFontRightJustify() override { return DLL_FONT_RIGHT_JUSTIFY; } // STUB
    int getFontCenterJustify() override { return DLL_FONT_CENTER_JUSTIFY; } // STUB
    int getFontCenterVertically() override { return 0; } // STUB
    int getFontAdditive() override { return 0; } // STUB

    void popupSetHeaderString(CvPopup* pPopup, CvWString szText, uint uiFlags) override {} // STUB
    void popupSetBodyString(CvPopup* pPopup, CvWString szText, uint uiFlags, char* szName, CvWString szHelpText) override {} // STUB
    void popupLaunch(CvPopup* pPopup, bool bCreateOkButton, PopupStates bState, int iNumPixelScroll) override {} // STUB
    void popupSetPopupType(CvPopup* pPopup, PopupEventTypes ePopupType, LPCTSTR szArtFileName) override {} // STUB
    void popupSetStyle(CvPopup* pPopup, const char* styleId) override {} // STUB

    void popupAddDDS(CvPopup* pPopup, const char* szIconFilename, int iWidth, int iHeight, CvWString szHelpText) override {} // STUB

    void popupAddSeparator(CvPopup* pPopup, int iSpace) override {} // STUB

    void popupAddGenericButton(CvPopup* pPopup, CvWString szText, const char* szIcon, int iButtonId, WidgetTypes eWidgetType, int iData1, int iData2,
        bool bOption, PopupControlLayout ctrlLayout, unsigned int textJustifcation) override {} // STUB

    void popupCreateEditBox(CvPopup* pPopup, CvWString szDefaultString, WidgetTypes eWidgetType, CvWString szHelpText, int iGroup,
        PopupControlLayout ctrlLayout, unsigned int preferredCharWidth, unsigned int maxCharCount) override {} // STUB
    void popupEnableEditBox(CvPopup* pPopup, int iGroup, bool bEnable) override {} // STUB

    void popupCreateRadioButtons(CvPopup* pPopup, int iNumButtons, int iGroup, WidgetTypes eWidgetType, PopupControlLayout ctrlLayout) override {} // STUB
    void popupSetRadioButtonText(CvPopup* pPopup, int iRadioButtonID, CvWString szText, int iGroup, CvWString szHelpText) override {} // STUB

    void popupCreateCheckBoxes(CvPopup* pPopup, int iNumBoxes, int iGroup, WidgetTypes eWidgetType, PopupControlLayout ctrlLayout) override {} // STUB
    void popupSetCheckBoxText(CvPopup* pPopup, int iCheckBoxID, CvWString szText, int iGroup, CvWString szHelpText) override {} // STUB
    void popupSetCheckBoxState(CvPopup* pPopup, int iCheckBoxID, bool bChecked, int iGroup) override {} // STUB

    void popupSetAsCancelled(CvPopup* pPopup) override {} // STUB
    bool popupIsDying(CvPopup* pPopup) override { return false; } // STUB
    void setCityTabSelectionRow(CityTabTypes eTabType) override {} // STUB

    bool noTechSplash() override { return true; } // STUB

    bool isInAdvancedStart() const override { return false; } // STUB
    void setInAdvancedStart(bool bAdvancedStart) override {} // STUB

    bool isSpaceshipScreenUp() const override { return false; } // STUB
    bool isDebugMenuCreated() const override { return false; } // STUB

    void setBusy(bool bBusy) override {} // STUB

    void getInterfaceScreenIdsForInput(std::vector<int>& aIds) override {} // STUB
    void doPing(int iX, int iY, PlayerTypes ePlayer) override {} // STUB
};

// =============================================================================
//  StubEntityIFace  (base class already has default implementations)
// =============================================================================
class StubEntityIFace : public CvDLLEntityIFaceBase
{
public:
    // All methods are already implemented in the base class with FAssertMsg stubs.
    // We override them here to silence the asserts in headless mode.
    void removeEntity(CvEntity*) override {} // STUB
    void addEntity(CvEntity*, uint uiEntAddFlags) override {} // STUB
    void setup(CvEntity*) override {} // STUB
    void setVisible(CvEntity*, bool) override {} // STUB
    void createCityEntity(CvCity*) override {} // STUB
    void createUnitEntity(CvUnit*) override {} // STUB
    void destroyEntity(CvEntity*&, bool bSafeDelete) override {} // STUB
    void updatePosition(CvEntity* gameEntity) override {} // STUB
    void setupFloodPlains(CvRiver* river) override {} // STUB

    bool IsSelected(const CvEntity*) const override { return false; } // STUB
    void PlayAnimation(CvEntity*, AnimationTypes eAnim, float fSpeed, bool bQueue, int iLayer, float fStartPct, float fEndPct) override {} // STUB
    void StopAnimation(CvEntity*, AnimationTypes eAnim) override {} // STUB
    void StopAnimation(CvEntity*) override {} // STUB
    void NotifyEntity(CvUnitEntity*, MissionTypes eMission) override {} // STUB
    void MoveTo(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void QueueMove(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void ExecuteMove(CvUnitEntity*, float fTimeToExecute, bool bCombat) override {} // STUB
    void SetPosition(CvUnitEntity* pEntity, const CvPlot* pkPlot) override {} // STUB
    void AddMission(const CvMissionDefinition* pDefinition) override {} // STUB
    void RemoveUnitFromBattle(CvUnit* pUnit) override {} // STUB
    void showPromotionGlow(CvUnitEntity* pEntity, bool show) override {} // STUB
    void updateEnemyGlow(CvUnitEntity* pEntity) override {} // STUB
    void updatePromotionLayers(CvUnitEntity* pEntity) override {} // STUB
    void updateGraphicEra(CvUnitEntity* pEntity, EraTypes eOldEra) override {} // STUB
    void SetSiegeTower(CvUnitEntity* pEntity, bool show) override {} // STUB
    bool GetSiegeTower(CvUnitEntity* pEntity) override { return false; } // STUB
};

// =============================================================================
//  StubPythonIFace
// =============================================================================
class StubPythonIFace : public CvDLLPythonIFaceBase
{
public:
    bool isInitialized() override { return false; } // STUB

    const char* getMapScriptModule() override { return ""; } // STUB

    PyObject* MakeFunctionArgs(void** args, int argc) override { return nullptr; } // STUB

    bool moduleExists(const char* moduleName, bool bLoadIfNecessary) override { return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg) override { return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, long* result) override { if (result) *result = 0; return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, CvString* result) override { if (result) *result = ""; return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, CvWString* result) override { if (result) *result = L""; return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<byte>* pList) override { return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<int>* pIntList) override { return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, int* pIntList, int* iListSize) override { if (iListSize) *iListSize = 0; return false; } // STUB
    bool callFunction(const char* moduleName, const char* fxnName, void* fxnArg, std::vector<float>* pFloatList) override { return false; } // STUB
    bool callPythonFunction(const char* szModName, const char* szFxnName, int iArg, long* result) override { if (result) *result = 0; return false; } // STUB

    bool pythonUsingDefaultImpl() override { return true; } // STUB
};

// =============================================================================
//  RealFAStarIFace — actual A* implementation (not a stub)
// =============================================================================
class StubFAStarIFace : public CvDLLFAStarIFaceBase
{
public:
    FAStar* create() override;
    void destroy(FAStar*& ptr, bool bSafeDelete) override;
    bool GeneratePath(FAStar* pFAStar, int iXstart, int iYstart, int iXdest, int iYdest, bool bCardinalOnly, int iInfo, bool bReuse) override;
    void Initialize(FAStar* pFAStar, int iColumns, int iRows, bool bWrapX, bool bWrapY, FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc, FAStarFunc CostFunc, FAStarFunc ValidFunc, FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc, void* pData) override;
    void SetData(FAStar* pFAStar, const void* pData) override;
    FAStarNode* GetLastNode(FAStar* pFAStar) override;
    bool IsPathStart(FAStar* pFAStar, int iX, int iY) override;
    bool IsPathDest(FAStar* pFAStar, int iX, int iY) override;
    int GetStartX(FAStar*) override { return 0; }
    int GetStartY(FAStar*) override { return 0; }
    int GetDestX(FAStar* pFAStar) override;
    int GetDestY(FAStar* pFAStar) override;
    int GetInfo(FAStar* pFAStar) override;
    void ForceReset(FAStar* pFAStar) override;
};

// =============================================================================
//  StubFlagEntityIFace
// =============================================================================
class StubFlagEntityIFace : public CvDLLFlagEntityIFaceBase
{
public:
    // CvDLLEntityIFaceBase overrides (silence the base-class asserts)
    void removeEntity(CvEntity*) override {} // STUB
    void addEntity(CvEntity*, uint uiEntAddFlags) override {} // STUB
    void setup(CvEntity*) override {} // STUB
    void setVisible(CvEntity*, bool) override {} // STUB
    void createCityEntity(CvCity*) override {} // STUB
    void createUnitEntity(CvUnit*) override {} // STUB
    void destroyEntity(CvEntity*&, bool bSafeDelete) override {} // STUB
    void updatePosition(CvEntity* gameEntity) override {} // STUB
    void setupFloodPlains(CvRiver* river) override {} // STUB
    bool IsSelected(const CvEntity*) const override { return false; } // STUB
    void PlayAnimation(CvEntity*, AnimationTypes eAnim, float fSpeed, bool bQueue, int iLayer, float fStartPct, float fEndPct) override {} // STUB
    void StopAnimation(CvEntity*, AnimationTypes eAnim) override {} // STUB
    void StopAnimation(CvEntity*) override {} // STUB
    void NotifyEntity(CvUnitEntity*, MissionTypes eMission) override {} // STUB
    void MoveTo(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void QueueMove(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void ExecuteMove(CvUnitEntity*, float fTimeToExecute, bool bCombat) override {} // STUB
    void SetPosition(CvUnitEntity* pEntity, const CvPlot* pkPlot) override {} // STUB
    void AddMission(const CvMissionDefinition* pDefinition) override {} // STUB
    void RemoveUnitFromBattle(CvUnit* pUnit) override {} // STUB
    void showPromotionGlow(CvUnitEntity* pEntity, bool show) override {} // STUB
    void updateEnemyGlow(CvUnitEntity* pEntity) override {} // STUB
    void updatePromotionLayers(CvUnitEntity* pEntity) override {} // STUB
    void updateGraphicEra(CvUnitEntity* pEntity, EraTypes eOldEra) override {} // STUB
    void SetSiegeTower(CvUnitEntity* pEntity, bool show) override {} // STUB
    bool GetSiegeTower(CvUnitEntity* pEntity) override { return false; } // STUB

    // CvDLLFlagEntityIFaceBase pure virtuals
    CvFlagEntity* create(PlayerTypes ePlayer) override { return nullptr; } // STUB
    PlayerTypes getPlayer(CvFlagEntity* pkFlag) const override { return NO_PLAYER; } // STUB
    CvPlot* getPlot(CvFlagEntity* pkFlag) const override { return nullptr; } // STUB
    void setPlot(CvFlagEntity* pkFlag, CvPlot* pkPlot, bool bOffset) override {} // STUB
    void updateUnitInfo(CvFlagEntity* pkFlag, const CvPlot* pkPlot, bool bOffset) override {} // STUB
    void updateGraphicEra(CvFlagEntity* pkFlag) override {} // STUB
    void setVisible(CvFlagEntity* pEnt, bool bVis) override {} // STUB
    void destroy(CvFlagEntity*& pImp, bool bSafeDelete) override { pImp = nullptr; } // STUB
};

// =============================================================================
//  StubIniParserIFace
// =============================================================================
class StubIniParserIFace : public CvDLLIniParserIFaceBase
{
public:
    FIniParser* create(const char* szFile) override { return nullptr; } // STUB
    void destroy(FIniParser*& pParser, bool bSafeDelete) override { pParser = nullptr; } // STUB
    bool SetGroupKey(FIniParser* pParser, const LPCTSTR pGroupKey) override { return false; } // STUB
    bool GetKeyValue(FIniParser* pParser, const LPCTSTR szKey, bool* iValue) override { return false; } // STUB
    bool GetKeyValue(FIniParser* pParser, const LPCTSTR szKey, short* iValue) override { return false; } // STUB
    bool GetKeyValue(FIniParser* pParser, const LPCTSTR szKey, int* iValue) override { return false; } // STUB
    bool GetKeyValue(FIniParser* pParser, const LPCTSTR szKey, float* fValue) override { return false; } // STUB
    bool GetKeyValue(FIniParser* pParser, const LPCTSTR szKey, LPTSTR szValue) override { return false; } // STUB
};

// =============================================================================
//  StubPlotBuilderIFace
// =============================================================================
class StubPlotBuilderIFace : public CvDLLPlotBuilderIFaceBase
{
public:
    // CvDLLEntityIFaceBase overrides
    void removeEntity(CvEntity*) override {} // STUB
    void addEntity(CvEntity*, uint uiEntAddFlags) override {} // STUB
    void setup(CvEntity*) override {} // STUB
    void setVisible(CvEntity*, bool) override {} // STUB
    void createCityEntity(CvCity*) override {} // STUB
    void createUnitEntity(CvUnit*) override {} // STUB
    void destroyEntity(CvEntity*&, bool bSafeDelete) override {} // STUB
    void updatePosition(CvEntity* gameEntity) override {} // STUB
    void setupFloodPlains(CvRiver* river) override {} // STUB
    bool IsSelected(const CvEntity*) const override { return false; } // STUB
    void PlayAnimation(CvEntity*, AnimationTypes eAnim, float fSpeed, bool bQueue, int iLayer, float fStartPct, float fEndPct) override {} // STUB
    void StopAnimation(CvEntity*, AnimationTypes eAnim) override {} // STUB
    void StopAnimation(CvEntity*) override {} // STUB
    void NotifyEntity(CvUnitEntity*, MissionTypes eMission) override {} // STUB
    void MoveTo(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void QueueMove(CvUnitEntity*, const CvPlot* pkPlot) override {} // STUB
    void ExecuteMove(CvUnitEntity*, float fTimeToExecute, bool bCombat) override {} // STUB
    void SetPosition(CvUnitEntity* pEntity, const CvPlot* pkPlot) override {} // STUB
    void AddMission(const CvMissionDefinition* pDefinition) override {} // STUB
    void RemoveUnitFromBattle(CvUnit* pUnit) override {} // STUB
    void showPromotionGlow(CvUnitEntity* pEntity, bool show) override {} // STUB
    void updateEnemyGlow(CvUnitEntity* pEntity) override {} // STUB
    void updatePromotionLayers(CvUnitEntity* pEntity) override {} // STUB
    void updateGraphicEra(CvUnitEntity* pEntity, EraTypes eOldEra) override {} // STUB
    void SetSiegeTower(CvUnitEntity* pEntity, bool show) override {} // STUB
    bool GetSiegeTower(CvUnitEntity* pEntity) override { return false; } // STUB

    // CvDLLPlotBuilderIFaceBase pure virtuals
    void init(CvPlotBuilder*, CvPlot*) override {} // STUB
    CvPlotBuilder* create() override { return nullptr; } // STUB
};

// =============================================================================
//  StubSymbolIFace
// =============================================================================
class StubSymbolIFace : public CvDLLSymbolIFaceBase
{
public:
    void init(CvSymbol*, int iID, int iOffset, int iType, CvPlot* pPlot) override {} // STUB
    CvSymbol* createSymbol() override { return nullptr; } // STUB
    void destroy(CvSymbol*&, bool bSafeDelete) override {} // STUB
    void setAlpha(CvSymbol*, float fAlpha) override {} // STUB
    void setScale(CvSymbol*, float fScale) override {} // STUB
    void Hide(CvSymbol*, bool bHide) override {} // STUB
    bool IsHidden(CvSymbol*) override { return true; } // STUB
    void updatePosition(CvSymbol*) override {} // STUB
    int getID(CvSymbol*) override { return 0; } // STUB
    SymbolTypes getSymbol(CvSymbol* pSym) override { return (SymbolTypes)0; } // STUB
    void setTypeYield(CvSymbol*, int iType, int count) override {} // STUB
};

// =============================================================================
//  StubFeatureIFace
// =============================================================================
class StubFeatureIFace : public CvDLLFeatureIFaceBase
{
public:
    CvFeature* createFeature() override { return nullptr; } // STUB
    void init(CvFeature*, int iID, int iOffset, int iType, CvPlot* pPlot) override {} // STUB
    FeatureTypes getFeature(CvFeature* pObj) override { return NO_FEATURE; } // STUB
    void setDummyVisibility(CvFeature* feature, const char* dummyTag, bool show) override {} // STUB
    void addDummyModel(CvFeature* feature, const char* dummyTag, const char* modelTag) override {} // STUB
    void setDummyTexture(CvFeature* feature, const char* dummyTag, const char* textureTag) override {} // STUB
    CvString pickDummyTag(CvFeature* feature, int mouseX, int mouseY) override { return ""; } // STUB
    void resetModel(CvFeature* feature) override {} // STUB
};

// =============================================================================
//  StubRouteIFace
// =============================================================================
class StubRouteIFace : public CvDLLRouteIFaceBase
{
public:
    CvRoute* createRoute() override { return nullptr; } // STUB
    void init(CvRoute*, int iID, int iOffset, int iType, CvPlot* pPlot) override {} // STUB
    RouteTypes getRoute(CvRoute* pObj) override { return NO_ROUTE; } // STUB
    int getConnectionMask(CvRoute* pObj) override { return 0; } // STUB
    void updateGraphicEra(CvRoute* pObj) override {} // STUB
};

// =============================================================================
//  StubRiverIFace
// =============================================================================
class StubRiverIFace : public CvDLLRiverIFaceBase
{
public:
    CvRiver* createRiver() override { return nullptr; } // STUB
    void init(CvRiver*, int iID, int iOffset, int iType, CvPlot* pPlot) override {} // STUB
};

// =============================================================================
//  XML IFace — uses real pugixml parser (defined in xml/XmlParser.cpp)
// =============================================================================
} // temporarily close namespace OpenCiv4
// Factory function implemented in XmlParser.cpp — returns a PugiXmlIFace instance
extern CvDLLXmlIFaceBase* OpenCiv4_CreateXmlParser();
namespace OpenCiv4 { // reopen

// =============================================================================
//  StubEventReporterIFace
// =============================================================================
class StubEventReporterIFace : public CvDLLEventReporterIFaceBase
{
public:
    void genericEvent(const char* szEventName, void* pythonArgs) override {} // STUB

    void mouseEvent(int evt, const POINT& ptCursor) override {} // STUB
    void kbdEvent(int evt, int key) override {} // STUB

    void gameEnd() override { fprintf(stderr, "[StubEventReporter] gameEnd()\n"); } // STUB

    void beginGameTurn(int iGameTurn) override {} // STUB
    void endGameTurn(int iGameTurn) override {} // STUB

    void beginPlayerTurn(int iGameTurn, PlayerTypes ePlayer) override {} // STUB
    void endPlayerTurn(int iGameTurn, PlayerTypes ePlayer) override {} // STUB

    void firstContact(TeamTypes eTeamID1, TeamTypes eTeamID2) override {} // STUB
    void combatResult(CvUnit* pWinner, CvUnit* pLoser) override {} // STUB
    void improvementBuilt(int iImprovementType, int iX, int iY) override {} // STUB
    void improvementDestroyed(int iImprovementType, int iPlayer, int iX, int iY) override {} // STUB
    void routeBuilt(int RouteType, int iX, int iY) override {} // STUB

    void plotRevealed(CvPlot* pPlot, TeamTypes eTeam) override {} // STUB
    void plotFeatureRemoved(CvPlot* pPlot, FeatureTypes eFeature, CvCity* pCity) override {} // STUB
    void plotPicked(CvPlot* pPlot) override {} // STUB
    void nukeExplosion(CvPlot* pPlot, CvUnit* pNukeUnit) override {} // STUB
    void gotoPlotSet(CvPlot* pPlot, PlayerTypes ePlayer) override {} // STUB

    void cityBuilt(CvCity* pCity) override {} // STUB
    void cityRazed(CvCity* pCity, PlayerTypes ePlayer) override {} // STUB
    void cityAcquired(PlayerTypes eOldOwner, PlayerTypes ePlayer, CvCity* pCity, bool bConquest, bool bTrade) override {} // STUB
    void cityAcquiredAndKept(PlayerTypes ePlayer, CvCity* pCity) override {} // STUB
    void cityLost(CvCity* pCity) override {} // STUB
    void cultureExpansion(CvCity* pCity, PlayerTypes ePlayer) override {} // STUB
    void cityGrowth(CvCity* pCity, PlayerTypes ePlayer) override {} // STUB
    void cityDoTurn(CvCity* pCity, PlayerTypes ePlayer) override {} // STUB
    void cityBuildingUnit(CvCity* pCity, UnitTypes eUnitType) override {} // STUB
    void cityBuildingBuilding(CvCity* pCity, BuildingTypes eBuildingType) override {} // STUB
    void cityRename(CvCity* pCity) override {} // STUB
    void cityHurry(CvCity* pCity, HurryTypes eHurry) override {} // STUB

    void selectionGroupPushMission(CvSelectionGroup* pSelectionGroup, MissionTypes eMission) override {} // STUB

    void unitMove(CvPlot* pPlot, CvUnit* pUnit, CvPlot* pOldPlot) override {} // STUB
    void unitSetXY(CvPlot* pPlot, CvUnit* pUnit) override {} // STUB
    void unitCreated(CvUnit* pUnit) override {} // STUB
    void unitBuilt(CvCity* pCity, CvUnit* pUnit) override {} // STUB
    void unitKilled(CvUnit* pUnit, PlayerTypes eAttacker) override {} // STUB
    void unitLost(CvUnit* pUnit) override {} // STUB
    void unitPromoted(CvUnit* pUnit, PromotionTypes ePromotion) override {} // STUB
    void unitSelected(CvUnit* pUnit) override {} // STUB
    void unitRename(CvUnit* pUnit) override {} // STUB
    void unitPillage(CvUnit* pUnit, ImprovementTypes eImprovement, RouteTypes eRoute, PlayerTypes ePlayer) override {} // STUB
    void unitSpreadReligionAttempt(CvUnit* pUnit, ReligionTypes eReligion, bool bSuccess) override {} // STUB
    void unitGifted(CvUnit* pUnit, PlayerTypes eGiftingPlayer, CvPlot* pPlotLocation) override {} // STUB
    void unitBuildImprovement(CvUnit* pUnit, BuildTypes eBuild, bool bFinished) override {} // STUB

    void goodyReceived(PlayerTypes ePlayer, CvPlot* pGoodyPlot, CvUnit* pGoodyUnit, GoodyTypes eGoodyType) override {} // STUB

    void greatPersonBorn(CvUnit* pUnit, PlayerTypes ePlayer, CvCity* pCity) override {} // STUB

    void buildingBuilt(CvCity* pCity, BuildingTypes eBuilding) override {} // STUB
    void projectBuilt(CvCity* pCity, ProjectTypes eProject) override {} // STUB

    void techAcquired(TechTypes eType, TeamTypes eTeam, PlayerTypes ePlayer, bool bAnnounce) override {} // STUB
    void techSelected(TechTypes eTech, PlayerTypes ePlayer) override {} // STUB

    void religionFounded(ReligionTypes eType, PlayerTypes ePlayer) override {} // STUB
    void religionSpread(ReligionTypes eType, PlayerTypes ePlayer, CvCity* pSpreadCity) override {} // STUB
    void religionRemove(ReligionTypes eType, PlayerTypes ePlayer, CvCity* pSpreadCity) override {} // STUB

    void corporationFounded(CorporationTypes eType, PlayerTypes ePlayer) override {} // STUB
    void corporationSpread(CorporationTypes eType, PlayerTypes ePlayer, CvCity* pSpreadCity) override {} // STUB
    void corporationRemove(CorporationTypes eType, PlayerTypes ePlayer, CvCity* pSpreadCity) override {} // STUB

    void goldenAge(PlayerTypes ePlayer) override {} // STUB
    void endGoldenAge(PlayerTypes ePlayer) override {} // STUB
    void changeWar(bool bWar, TeamTypes eTeam, TeamTypes eOtherTeam) override {} // STUB

    void setPlayerAlive(PlayerTypes ePlayerID, bool bNewValue) override {} // STUB
    void playerChangeStateReligion(PlayerTypes ePlayerID, ReligionTypes eNewReligion, ReligionTypes eOldReligion) override {} // STUB
    void playerGoldTrade(PlayerTypes eFromPlayer, PlayerTypes eToPlayer, int iAmount) override {} // STUB

    void chat(char* szString) override {} // STUB

    void victory(TeamTypes eNewWinner, VictoryTypes eNewVictory) override { fprintf(stderr, "[StubEventReporter] victory(team=%d, victory=%d)\n", (int)eNewWinner, (int)eNewVictory); } // STUB

    void vassalState(TeamTypes eMaster, TeamTypes eVassal, bool bVassal) override {} // STUB
};

// =============================================================================
//  StubUtilityIFace  --  The master interface that owns all sub-interfaces
// =============================================================================
class StubUtilityIFace : public CvDLLUtilityIFaceBase
{
public:
    StubUtilityIFace() : m_pXmlIFace(OpenCiv4_CreateXmlParser()) {}
    ~StubUtilityIFace() { delete m_pXmlIFace; }
    // ---- Sub-interface instances ----
    StubEntityIFace       m_entityIFace;
    StubInterfaceIFace    m_interfaceIFace;
    StubEngineIFace       m_engineIFace;
    StubIniParserIFace    m_iniParserIFace;
    StubSymbolIFace       m_symbolIFace;
    StubFeatureIFace      m_featureIFace;
    StubRouteIFace        m_routeIFace;
    StubPlotBuilderIFace  m_plotBuilderIFace;
    StubRiverIFace        m_riverIFace;
    StubFAStarIFace       m_faStarIFace;
    CvDLLXmlIFaceBase*    m_pXmlIFace;
    StubFlagEntityIFace   m_flagEntityIFace;
    StubPythonIFace       m_pythonIFace;
    StubEventReporterIFace m_eventReporterIFace;

    // ---- Sub-interface accessors ----
    CvDLLEntityIFaceBase* getEntityIFace() override { return &m_entityIFace; } // STUB
    CvDLLInterfaceIFaceBase* getInterfaceIFace() override { return &m_interfaceIFace; } // STUB
    CvDLLEngineIFaceBase* getEngineIFace() override { return &m_engineIFace; } // STUB
    CvDLLIniParserIFaceBase* getIniParserIFace() override { return &m_iniParserIFace; } // STUB
    CvDLLSymbolIFaceBase* getSymbolIFace() override { return &m_symbolIFace; } // STUB
    CvDLLFeatureIFaceBase* getFeatureIFace() override { return &m_featureIFace; } // STUB
    CvDLLRouteIFaceBase* getRouteIFace() override { return &m_routeIFace; } // STUB
    CvDLLPlotBuilderIFaceBase* getPlotBuilderIFace() override { return &m_plotBuilderIFace; } // STUB
    CvDLLRiverIFaceBase* getRiverIFace() override { return &m_riverIFace; } // STUB
    CvDLLFAStarIFaceBase* getFAStarIFace() override { return &m_faStarIFace; } // STUB
    CvDLLXmlIFaceBase* getXMLIFace() override { return m_pXmlIFace; }
    CvDLLFlagEntityIFaceBase* getFlagEntityIFace() override { return &m_flagEntityIFace; } // STUB
    CvDLLPythonIFaceBase* getPythonIFace() override { return &m_pythonIFace; } // STUB

    // ---- Memory management: delegate to C runtime ----
    void delMem(void* p) override { free(p); } // STUB
    void* newMem(size_t size) override { return malloc(size); } // STUB

    void delMem(void* p, const char* pcFile, int iLine) override { free(p); } // STUB
    void* newMem(size_t size, const char* pcFile, int iLine) override { return malloc(size); } // STUB

    void delMemArray(void* p, const char* pcFile, int iLine) override { free(p); } // STUB
    void* newMemArray(size_t size, const char* pcFile, int iLine) override { return malloc(size); } // STUB

    void* reallocMem(void* a, unsigned int uiBytes, const char* pcFile, int iLine) override { return realloc(a, uiBytes); } // STUB
    unsigned int memSize(void* a) override { return 0; } // STUB

    void clearVector(std::vector<int>& vec) override { vec.clear(); } // STUB
    void clearVector(std::vector<byte>& vec) override { vec.clear(); } // STUB
    void clearVector(std::vector<float>& vec) override { vec.clear(); } // STUB

    // ---- Networking stubs ----
    int getAssignedNetworkID(int iPlayerID) override { return 0; } // STUB
    bool isConnected(int iNetID) override { return false; } // STUB
    bool isGameActive() override { return true; } // STUB
    int GetLocalNetworkID() override { return 0; } // STUB
    int GetSyncOOS(int iNetID) override { return 0; } // STUB
    int GetOptionsOOS(int iNetID) override { return 0; } // STUB
    int GetLastPing(int iNetID) override { return 0; } // STUB

    bool IsModem() override { return false; } // STUB
    void SetModem(bool bModem) override {} // STUB

    void AcceptBuddy(const char* szName, int iRequestID) override {} // STUB
    void RejectBuddy(const char* szName, int iRequestID) override {} // STUB

    // ---- Misc utility ----
    void messageControlLog(char* s) override {} // STUB
    int getChtLvl() override { return 0; } // STUB
    void setChtLvl(int iLevel) override {} // STUB
    bool GetWorldBuilderMode() override { return false; } // STUB
    int getCurrentLanguage() const override { return 0; } // STUB
    void setCurrentLanguage(int iNewLanguage) override {} // STUB
    bool isModularXMLLoading() const override { return false; } // STUB

    bool IsPitbossHost() const override { return false; } // STUB
    CvString GetPitbossSmtpHost() const override { return ""; } // STUB
    CvWString GetPitbossSmtpLogin() const override { return L""; } // STUB
    CvWString GetPitbossSmtpPassword() const override { return L""; } // STUB
    CvString GetPitbossEmail() const override { return ""; } // STUB

    // ---- Network send stubs (all no-ops) ----
    void sendMessageData(CvMessageData* pData) override {} // STUB
    void sendPlayerInfo(PlayerTypes eActivePlayer) override {} // STUB
    void sendGameInfo(const CvWString& szGameName, const CvWString& szAdminPassword) override {} // STUB
    void sendPlayerOption(PlayerOptionTypes eOption, bool bValue) override {} // STUB
    void sendChat(const CvWString& szChatString, ChatTargetTypes eTarget) override {} // STUB
    void sendPause(int iPauseID) override {} // STUB
    void sendMPRetire() override {} // STUB
    void sendToggleTradeMessage(PlayerTypes eWho, TradeableItems eItemType, int iData, int iOtherWho, bool bAIOffer, bool bSendToAll) override {} // STUB
    void sendClearTableMessage(PlayerTypes eWhoTradingWith) override {} // STUB
    void sendImplementDealMessage(PlayerTypes eOtherWho, CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirList) override {} // STUB
    void sendContactCiv(NetContactTypes eContactType, PlayerTypes eWho) override {} // STUB
    void sendOffer() override {} // STUB
    void sendDiploEvent(PlayerTypes eWhoTradingWith, DiploEventTypes eDiploEvent, int iData1, int iData2) override {} // STUB
    void sendRenegotiate(PlayerTypes eWhoTradingWith) override {} // STUB
    void sendRenegotiateThisItem(PlayerTypes ePlayer2, TradeableItems eItemType, int iData) override {} // STUB
    void sendExitTrade() override {} // STUB
    void sendKillDeal(int iDealID, bool bFromDiplomacy) override {} // STUB
    void sendDiplomacy(PlayerTypes ePlayer, CvDiploParameters* pParams) override {} // STUB
    void sendPopup(PlayerTypes ePlayer, CvPopupInfo* pInfo) override {} // STUB

    // ---- Timing ----
    int getMillisecsPerTurn() override { return 0; } // STUB
    float getSecsPerTurn() override { return 0.0f; } // STUB
    int getTurnsPerSecond() override { return 0; } // STUB
    int getTurnsPerMinute() override { return 0; } // STUB

    // ---- Slot management ----
    void openSlot(PlayerTypes eID) override {} // STUB
    void closeSlot(PlayerTypes eID) override {} // STUB

    // ---- Map ----
    CvWString getMapScriptName() override { return L""; } // STUB
    bool getTransferredMap() override { return false; } // STUB
    bool isDescFileName(const char* szFileName) override { return false; } // STUB
    bool isWBMapScript() override { return false; } // STUB
    bool isWBMapNoPlayers() override { return false; } // STUB
    bool pythonMapExists(const char* szMapName) override { return false; } // STUB

    void stripSpecialCharacters(CvWString& szName) override {} // STUB

    // ---- Global init/uninit ----
    void initGlobals() override;
    void uninitGlobals() override;

    void callUpdater() override {} // STUB

    // ---- Compression ----
    bool Uncompress(byte** bufIn, unsigned long* bufLenIn, unsigned long maxBufLenOut, int offset) override { return false; } // STUB
    bool Compress(byte** bufIn, unsigned long* bufLenIn, int offset) override { return false; } // STUB

    // ---- UI / system ----
    void NiTextOut(const TCHAR* szText) override {} // STUB
    void MessageBox(const TCHAR* szText, const TCHAR* szCaption) override
    {
        static int sMsgCount = 0;
        if (sMsgCount < 20)
        {
            fprintf(stderr, "[StubUtility] MessageBox: %s -- %s\n", szCaption ? szCaption : "(null)", szText ? szText : "(null)");
            ++sMsgCount;
            if (sMsgCount == 20)
                fprintf(stderr, "[StubUtility] (further MessageBox output suppressed)\n");
        }
    } // STUB
    void SetDone(bool bDone) override { fprintf(stderr, "[StubUtility] SetDone(%d)\n", bDone); } // STUB
    bool GetDone() override { return false; } // STUB
    bool GetAutorun() override { return false; } // STUB

    // ---- Diplomacy ----
    void beginDiplomacy(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override {} // STUB
    void endDiplomacy() override {} // STUB
    bool isDiplomacy() override { return false; } // STUB
    int getDiplomacyPlayer() override { return -1; } // STUB
    void updateDiplomacyAttitude(bool bForce) override {} // STUB
    bool isMPDiplomacy() override { return false; } // STUB
    bool isMPDiplomacyScreenUp() override { return false; } // STUB
    int getMPDiplomacyPlayer() override { return -1; } // STUB
    void beginMPDiplomacy(PlayerTypes eWhoTalkingTo, bool bRenegotiate, bool bSimultaneous) override {} // STUB
    void endMPDiplomacy() override {} // STUB

    // ---- Audio ----
    bool getAudioDisabled() override { return true; } // STUB
    int getAudioTagIndex(const TCHAR* szTag, int iScriptType) override { return -1; } // STUB
    void DoSound(int iScriptId) override {} // STUB
    void Do3DSound(int iScriptId, NiPoint3 vPosition) override {} // STUB

    // ---- Data streams ----
    FDataStreamBase* createFileStream() override { return nullptr; } // STUB
    void destroyDataStream(FDataStreamBase*& stream) override { stream = nullptr; } // STUB

    // ---- Cache objects ----
    CvCacheObject* createGlobalTextCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createGlobalDefinesCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createTechInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createBuildingInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createUnitInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createLeaderHeadInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createCivilizationInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createPromotionInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createDiplomacyInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createEventInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createEventTriggerInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createCivicInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createHandicapInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createBonusInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB
    CvCacheObject* createImprovementInfoCacheObject(const TCHAR* szCacheFileName) override { return nullptr; } // STUB

    bool cacheRead(CvCacheObject* pCache, const TCHAR* szSourceFileName) override { return false; } // STUB
    bool cacheWrite(CvCacheObject* pCache) override { return false; } // STUB
    void destroyCache(CvCacheObject*& pCache) override { pCache = nullptr; } // STUB

    bool fileManagerEnabled() override { return false; } // STUB

    // ---- Logging ----
    void logMsg(const TCHAR* pLogFileName, const TCHAR* pBuf, bool bWriteToConsole, bool bTimeStamp) override
    {
        // Suppress repetitive "info type NONE not found" messages
        if (pBuf && strstr(pBuf, "info type") && strstr(pBuf, "not found"))
        {
            static int sInfoTypeCount = 0;
            if (++sInfoTypeCount > 5) return;
        }
        fprintf(stderr, "[LOG %s] %s\n", pLogFileName ? pLogFileName : "(null)", pBuf ? pBuf : "(null)");
    } // STUB
    void logMemState(const char* msg) override {} // STUB

    // ---- Symbol ID ----
    int getSymbolID(int iID) override { return 0; } // STUB
    void setSymbolID(int iID, int iValue) override {} // STUB

    // ---- Text / localization ----
    CvWString getText(CvWString szIDTag, ...) override { return szIDTag; } // Return raw key
    CvWString getObjectText(CvWString szIDTag, uint uiForm, bool bNoSubs) override { return szIDTag; } // Return raw key
    void addText(const TCHAR* szIDTag, const wchar* szString, const wchar* szGender, const wchar* szPlural) override {} // STUB
    uint getNumForms(CvWString szIDTag) override { return 0; } // STUB

    // ---- World / frame ----
    WorldSizeTypes getWorldSize() override { return (WorldSizeTypes)0; } // STUB
    uint getFrameCounter() const override { return 0; } // STUB

    // ---- Input state ----
    bool altKey() override { return false; } // STUB
    bool shiftKey() override { return false; } // STUB
    bool ctrlKey() override { return false; } // STUB
    bool scrollLock() override { return false; } // STUB
    bool capsLock() override { return false; } // STUB
    bool numLock() override { return false; } // STUB

    // ---- Profiler ----
    void ProfilerBegin() override {} // STUB
    void ProfilerEnd() override {} // STUB
    void BeginSample(ProfileSample* pSample) override {} // STUB
    void EndSample(ProfileSample* pSample) override {} // STUB
    bool isGameInitializing() override { return false; } // STUB

    // ---- File enumeration ----
    void enumerateFiles(std::vector<CvString>& files, const char* szPattern) override {} // STUB
    void enumerateModuleFiles(std::vector<CvString>& aszFiles, const CvString& refcstrRootDirectory, const CvString& refcstrModularDirectory, const CvString& refcstrExtension, bool bSearchSubdirectories) override {} // STUB

    // ---- Save / Load ----
    void SaveGame(SaveGameTypes eSaveGame) override { fprintf(stderr, "[StubUtility] SaveGame(type=%d)\n", (int)eSaveGame); } // STUB
    void LoadGame() override { fprintf(stderr, "[StubUtility] LoadGame()\n"); } // STUB
    int loadReplays(std::vector<CvReplayInfo*>& listReplays) override { return 0; } // STUB
    void QuickSave() override { fprintf(stderr, "[StubUtility] QuickSave()\n"); } // STUB
    void QuickLoad() override { fprintf(stderr, "[StubUtility] QuickLoad()\n"); } // STUB
    void sendPbemTurn(PlayerTypes ePlayer) override {} // STUB
    void getPassword(PlayerTypes ePlayer) override {} // STUB

    // ---- Options ----
    bool getGraphicOption(GraphicOptionTypes eGraphicOption) override { return false; } // STUB
    bool getPlayerOption(PlayerOptionTypes ePlayerOption) override { return false; } // STUB
    int getMainMenu() override { return 0; } // STUB

    // ---- FMP / connection state ----
    bool isFMPMgrHost() override { return false; } // STUB
    bool isFMPMgrPublic() override { return false; } // STUB
    void handleRetirement(PlayerTypes ePlayer) override {} // STUB
    PlayerTypes getFirstBadConnection() override { return NO_PLAYER; } // STUB
    int getConnState(PlayerTypes ePlayer) override { return 0; } // STUB

    // ---- INI ----
    bool ChangeINIKeyValue(const char* szGroupKey, const char* szKeyValue, const char* szOut) override { return false; } // STUB

    // ---- Hashing ----
    char* md5String(char* szString) override { return nullptr; } // STUB

    // ---- Mod info ----
    const char* getModName(bool bFullPath) const override { return ""; } // STUB
    bool hasSkippedSaveChecksum() const override { return false; } // STUB
    void reportStatistics() override {} // STUB
};

} // namespace OpenCiv4
