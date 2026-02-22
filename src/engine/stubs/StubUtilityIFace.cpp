// StubUtilityIFace.cpp — Implementation for methods that can't be inline
// Most stubs are header-only, but these need access to engine headers.

#include "CvGameCoreDLL.h"
#include "CvGlobals.h"
#include "StubInterfaces.h"
#include "FAStar.h"

void OpenCiv4::StubUtilityIFace::initGlobals()
{
    fprintf(stderr, "[StubUtility] initGlobals() — allocating FAStar pathfinders\n");

    // Allocate the 7 FAStar instances that CvGlobals stores as pointers.
    // In the real game, the engine exe allocates these; here we do it ourselves.
    GC.setPathFinder(new FAStar());
    GC.setInterfacePathFinder(new FAStar());
    GC.setStepFinder(new FAStar());
    GC.setRouteFinder(new FAStar());
    GC.setBorderFinder(new FAStar());
    GC.setAreaFinder(new FAStar());
    GC.setPlotGroupFinder(new FAStar());
}

void OpenCiv4::StubUtilityIFace::uninitGlobals()
{
    fprintf(stderr, "[StubUtility] uninitGlobals() — freeing FAStar pathfinders\n");

    // Free FAStar instances
    auto cleanup = [](FAStar*& p) { delete p; p = nullptr; };

    FAStar* pTemp;

    pTemp = &GC.getPathFinder(); cleanup(pTemp);
    pTemp = &GC.getInterfacePathFinder(); cleanup(pTemp);
    pTemp = &GC.getStepFinder(); cleanup(pTemp);
    pTemp = &GC.getRouteFinder(); cleanup(pTemp);
    pTemp = &GC.getBorderFinder(); cleanup(pTemp);
    pTemp = &GC.getAreaFinder(); cleanup(pTemp);
    pTemp = &GC.getPlotGroupFinder(); cleanup(pTemp);

    GC.setPathFinder(nullptr);
    GC.setInterfacePathFinder(nullptr);
    GC.setStepFinder(nullptr);
    GC.setRouteFinder(nullptr);
    GC.setBorderFinder(nullptr);
    GC.setAreaFinder(nullptr);
    GC.setPlotGroupFinder(nullptr);
}
