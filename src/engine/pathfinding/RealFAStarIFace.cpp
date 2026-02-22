// OpenCiv4 — Real FAStar interface implementation
// Bridges the CvDLLFAStarIFaceBase abstract interface to our FAStar class.

#include "CvGameCoreDLL.h"
#include "StubInterfaces.h"
#include "FAStar.h"

FAStar* OpenCiv4::StubFAStarIFace::create()
{
    return new FAStar();
}

void OpenCiv4::StubFAStarIFace::destroy(FAStar*& ptr, bool bSafeDelete)
{
    delete ptr;
    ptr = nullptr;
}

bool OpenCiv4::StubFAStarIFace::GeneratePath(FAStar* pFAStar, int iXstart, int iYstart,
                                              int iXdest, int iYdest, bool bCardinalOnly,
                                              int iInfo, bool bReuse)
{
    if (!pFAStar) return false;
    return pFAStar->generatePath(iXstart, iYstart, iXdest, iYdest, bCardinalOnly, iInfo, bReuse);
}

void OpenCiv4::StubFAStarIFace::Initialize(FAStar* pFAStar, int iColumns, int iRows,
                                           bool bWrapX, bool bWrapY,
                                           FAPointFunc DestValidFunc, FAHeuristic HeuristicFunc,
                                           FAStarFunc CostFunc, FAStarFunc ValidFunc,
                                           FAStarFunc NotifyChildFunc, FAStarFunc NotifyListFunc,
                                           void* pData)
{
    if (!pFAStar) return;
    pFAStar->initialize(iColumns, iRows, bWrapX, bWrapY,
                        DestValidFunc, HeuristicFunc, CostFunc, ValidFunc,
                        NotifyChildFunc, NotifyListFunc, pData);
}

void OpenCiv4::StubFAStarIFace::SetData(FAStar* pFAStar, const void* pData)
{
    if (!pFAStar) return;
    pFAStar->setData(pData);
}

FAStarNode* OpenCiv4::StubFAStarIFace::GetLastNode(FAStar* pFAStar)
{
    if (!pFAStar) return nullptr;
    return pFAStar->getLastNode();
}

bool OpenCiv4::StubFAStarIFace::IsPathStart(FAStar* pFAStar, int iX, int iY)
{
    // Not tracked in our impl — return false
    return false;
}

bool OpenCiv4::StubFAStarIFace::IsPathDest(FAStar* pFAStar, int iX, int iY)
{
    if (!pFAStar) return false;
    return (iX == pFAStar->getXdest() && iY == pFAStar->getYdest());
}

int OpenCiv4::StubFAStarIFace::GetDestX(FAStar* pFAStar)
{
    if (!pFAStar) return 0;
    return pFAStar->getXdest();
}

int OpenCiv4::StubFAStarIFace::GetDestY(FAStar* pFAStar)
{
    if (!pFAStar) return 0;
    return pFAStar->getYdest();
}

int OpenCiv4::StubFAStarIFace::GetInfo(FAStar* pFAStar)
{
    if (!pFAStar) return 0;
    return pFAStar->getInfo();
}

void OpenCiv4::StubFAStarIFace::ForceReset(FAStar* pFAStar)
{
    // Nothing to do in our implementation — reset happens in generatePath
}
