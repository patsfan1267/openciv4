// OpenCiv4 — Clean-room 64-bit engine replacement for Civilization IV
// Phase 0: Headless mode — run AI games with no graphics
//
// This is the host executable entry point. It:
// 1. Creates stub interface implementations (no graphics, no audio, no Python)
// 2. Wires them into CvGlobals via setDLLIFace()
// 3. Calls GC.init() to bring up the gamecore
// 4. (Future) Loads XML, creates a game, runs AI turns

#include <cstdio>
#include <cstdint>

// Pull in the gamecore's global singleton and types
#include "CvGameCoreDLL.h"
#include "CvXMLLoadUtility.h"
#include "CvArtFileMgr.h"

// Our stub implementations of all 13 interface classes
#include "StubInterfaces.h"

int main(int argc, char* argv[])
{
    fprintf(stderr, "=== OpenCiv4 Engine ===\n");
    fprintf(stderr, "Phase 0: Headless Mode\n");
    fprintf(stderr, "Build: 64-bit (%zu-byte pointers)\n\n", sizeof(void*));
    fprintf(stderr, "BTS Install: %s\n\n", BTS_INSTALL_DIR);

    // ---- Step 1: Create our stub DLL interface ----
    // StubUtilityIFace is the "master" stub that owns all sub-interfaces
    // (engine, entity, python, XML, pathfinding, etc.)
    OpenCiv4::StubUtilityIFace stubDLL;
    fprintf(stderr, "[main] Stub interfaces created.\n");

    // ---- Step 2: Wire up gDLL ----
    // This sets the global pointer that all gamecore code uses via the gDLL macro.
    // Must happen BEFORE GC.init().
    CvGlobals::getInstance().setDLLIFace(&stubDLL);
    fprintf(stderr, "[main] gDLL wired up.\n");

    // ---- Step 3: Initialize the gamecore ----
    // GC.init() allocates core objects (CvGameAI, CvMap, CvInitCore, etc.)
    // and calls gDLL->initGlobals() for host-side allocations.
    fprintf(stderr, "[main] Calling GC.init()...\n");
    CvGlobals::getInstance().init();
    fprintf(stderr, "[main] GC.init() completed successfully!\n\n");

    // ---- Phase 0 status ----
    fprintf(stderr, "=== Phase 0 Milestone: Gamecore initialized ===\n");
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

        // Art defines must be loaded before LoadPreMenuGlobals because
        // CvUnitInfo::read() calls updateArtDefineButton() which looks up
        // unit art info by tag name in the ARTFILEMGR maps.
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

    // TODO Phase 0 next steps:
    // 5. Initialize game with AI players
    // 6. Run turn loop

    // ---- Cleanup ----
    fprintf(stderr, "[main] Calling GC.uninit()...\n");
    CvGlobals::getInstance().uninit();
    fprintf(stderr, "[main] Shutdown complete.\n");

    return 0;
}
