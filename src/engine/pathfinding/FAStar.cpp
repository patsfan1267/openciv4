// OpenCiv4 — FAStar A* pathfinding implementation
//
// This is a clean-room implementation of the A* pathfinding engine that
// Civ4's proprietary exe provided. The SDK defined FAStarNode and the
// callback signatures; we implement the actual search algorithm.
//
// Key design: The FAStar object owns a 2D grid of FAStarNodes. The caller
// provides callback functions for validation, cost, and heuristic. The
// GeneratePath() method runs standard A* using those callbacks.

#include "CvGameCoreDLL.h"
#include "FAStar.h"
#include <cstdlib>
#include <cstring>
#include <climits>

// Direction offsets: N, NE, E, SE, S, SW, W, NW
static const int s_iDirX[8] = { 0,  1, 1, 1, 0, -1, -1, -1 };
static const int s_iDirY[8] = { 1,  1, 0, -1, -1, -1, 0, 1 };

FAStar::FAStar()
    : m_pGrid(nullptr)
    , m_iColumns(0)
    , m_iRows(0)
    , m_bWrapX(false)
    , m_bWrapY(false)
    , m_destValidFunc(nullptr)
    , m_heuristicFunc(nullptr)
    , m_costFunc(nullptr)
    , m_validFunc(nullptr)
    , m_notifyChildFunc(nullptr)
    , m_notifyListFunc(nullptr)
    , m_pData(nullptr)
    , m_iInfo(0)
    , m_iXdest(0)
    , m_iYdest(0)
    , m_pOpen(nullptr)
    , m_pBest(nullptr)
{
}

FAStar::~FAStar()
{
    delete[] m_pGrid;
}

void FAStar::initialize(int iColumns, int iRows, bool bWrapX, bool bWrapY,
                        FAPointFunc destValidFunc, FAHeuristic heuristicFunc,
                        FAStarFunc costFunc, FAStarFunc validFunc,
                        FAStarFunc notifyChildFunc, FAStarFunc notifyListFunc,
                        void* pData)
{
    delete[] m_pGrid;
    m_pGrid = nullptr;

    m_iColumns = iColumns;
    m_iRows = iRows;
    m_bWrapX = bWrapX;
    m_bWrapY = bWrapY;

    m_destValidFunc = destValidFunc;
    m_heuristicFunc = heuristicFunc;
    m_costFunc = costFunc;
    m_validFunc = validFunc;
    m_notifyChildFunc = notifyChildFunc;
    m_notifyListFunc = notifyListFunc;
    m_pData = pData;

    if (iColumns > 0 && iRows > 0)
    {
        m_pGrid = new FAStarNode[iColumns * iRows];
        for (int y = 0; y < iRows; y++)
        {
            for (int x = 0; x < iColumns; x++)
            {
                FAStarNode& node = m_pGrid[y * iColumns + x];
                node.m_iX = x;
                node.m_iY = y;
            }
        }
    }
}

void FAStar::reset()
{
    m_pOpen = nullptr;
    m_pBest = nullptr;

    if (m_pGrid)
    {
        int iTotal = m_iColumns * m_iRows;
        for (int i = 0; i < iTotal; i++)
        {
            int x = m_pGrid[i].m_iX;
            int y = m_pGrid[i].m_iY;
            m_pGrid[i].clear();
            m_pGrid[i].m_iX = x;
            m_pGrid[i].m_iY = y;
        }
    }
}

int FAStar::wrapX(int iX) const
{
    if (m_bWrapX && m_iColumns > 0)
    {
        if (iX < 0) return iX + m_iColumns * ((-iX / m_iColumns) + 1);
        if (iX >= m_iColumns) return iX % m_iColumns;
    }
    return iX;
}

int FAStar::wrapY(int iY) const
{
    if (m_bWrapY && m_iRows > 0)
    {
        if (iY < 0) return iY + m_iRows * ((-iY / m_iRows) + 1);
        if (iY >= m_iRows) return iY % m_iRows;
    }
    return iY;
}

bool FAStar::isValid(int iX, int iY) const
{
    return (iX >= 0 && iX < m_iColumns && iY >= 0 && iY < m_iRows);
}

FAStarNode* FAStar::getNode(int x, int y)
{
    if (!m_pGrid || !isValid(x, y))
        return nullptr;
    return &m_pGrid[y * m_iColumns + x];
}

void FAStar::addToOpen(FAStarNode* pNode)
{
    pNode->m_eFAStarListType = FASTARLIST_OPEN;

    // Insert into sorted open list (lowest total cost first)
    if (!m_pOpen || pNode->m_iTotalCost <= m_pOpen->m_iTotalCost)
    {
        pNode->m_pNext = m_pOpen;
        pNode->m_pPrev = nullptr;
        if (m_pOpen) m_pOpen->m_pPrev = pNode;
        m_pOpen = pNode;
    }
    else
    {
        FAStarNode* pCurr = m_pOpen;
        while (pCurr->m_pNext && pCurr->m_pNext->m_iTotalCost < pNode->m_iTotalCost)
        {
            pCurr = pCurr->m_pNext;
        }
        pNode->m_pNext = pCurr->m_pNext;
        pNode->m_pPrev = pCurr;
        if (pCurr->m_pNext) pCurr->m_pNext->m_pPrev = pNode;
        pCurr->m_pNext = pNode;
    }

    // Notify callback — BTS convention: (parent, node, data, ...)
    // where 'node' is the tile being added to the list.
    if (m_notifyListFunc)
    {
        m_notifyListFunc(pNode->m_pParent, pNode, ASNL_ADDOPEN, m_pData, this);
    }
}

FAStarNode* FAStar::popBestOpen()
{
    if (!m_pOpen) return nullptr;

    FAStarNode* pBest = m_pOpen;
    m_pOpen = pBest->m_pNext;
    if (m_pOpen) m_pOpen->m_pPrev = nullptr;

    pBest->m_pNext = nullptr;
    pBest->m_pPrev = nullptr;
    pBest->m_eFAStarListType = FASTARLIST_CLOSED;

    // Notify callback that this node is now on the closed list.
    // Critical for joinArea() which assigns area IDs only on ASNL_ADDCLOSED.
    // BTS convention: (parent, node, data, ...) where 'node' is the current tile.
    if (m_notifyListFunc)
    {
        m_notifyListFunc(pBest->m_pParent, pBest, ASNL_ADDCLOSED, m_pData, this);
    }

    return pBest;
}

void FAStar::processNeighbor(FAStarNode* pParent, int iNeighborX, int iNeighborY)
{
    iNeighborX = wrapX(iNeighborX);
    iNeighborY = wrapY(iNeighborY);

    if (!isValid(iNeighborX, iNeighborY))
        return;

    FAStarNode* pNeighbor = getNode(iNeighborX, iNeighborY);
    if (!pNeighbor)
        return;

    // Already on closed list
    if (pNeighbor->m_eFAStarListType == FASTARLIST_CLOSED)
        return;

    // Check validity via callback
    if (m_validFunc)
    {
        if (!m_validFunc(pParent, pNeighbor, ASNC_NEWADD, m_pData, this))
            return;
    }

    // Calculate cost
    int iNewKnownCost = pParent->m_iKnownCost;
    if (m_costFunc)
    {
        iNewKnownCost += m_costFunc(pParent, pNeighbor, 0, m_pData, this);
    }
    else
    {
        iNewKnownCost += 1; // Default unit cost
    }

    if (pNeighbor->m_eFAStarListType == FASTARLIST_OPEN)
    {
        // Already on open list — check if new path is better
        if (iNewKnownCost < pNeighbor->m_iKnownCost)
        {
            // Remove from open list
            if (pNeighbor->m_pPrev) pNeighbor->m_pPrev->m_pNext = pNeighbor->m_pNext;
            else m_pOpen = pNeighbor->m_pNext;
            if (pNeighbor->m_pNext) pNeighbor->m_pNext->m_pPrev = pNeighbor->m_pPrev;

            pNeighbor->m_pParent = pParent;
            pNeighbor->m_iKnownCost = iNewKnownCost;
            pNeighbor->m_iTotalCost = iNewKnownCost + pNeighbor->m_iHeuristicCost;

            addToOpen(pNeighbor);

            // Notify child callback
            if (m_notifyChildFunc)
            {
                m_notifyChildFunc(pParent, pNeighbor, ASNC_OPENADD_UP, m_pData, this);
            }
        }
    }
    else
    {
        // New node — add to open list
        pNeighbor->m_pParent = pParent;
        pNeighbor->m_iKnownCost = iNewKnownCost;

        if (m_heuristicFunc)
        {
            pNeighbor->m_iHeuristicCost = m_heuristicFunc(
                iNeighborX, iNeighborY, m_iXdest, m_iYdest);
        }
        else
        {
            pNeighbor->m_iHeuristicCost = 0;
        }

        pNeighbor->m_iTotalCost = iNewKnownCost + pNeighbor->m_iHeuristicCost;

        // Track parent-child relationship
        if (pParent->m_iNumChildren < 8)
        {
            pParent->m_apChildren[pParent->m_iNumChildren] = pNeighbor;
            pParent->m_iNumChildren++;
        }

        addToOpen(pNeighbor);

        // Notify child callback
        if (m_notifyChildFunc)
        {
            m_notifyChildFunc(pParent, pNeighbor, ASNC_INITIALADD, m_pData, this);
        }
    }
}

bool FAStar::generatePath(int iXstart, int iYstart, int iXdest, int iYdest,
                          bool bCardinalOnly, int iInfo, bool bReuse)
{
    if (!m_pGrid)
        return false;

    m_iInfo = iInfo;
    m_iXdest = iXdest;
    m_iYdest = iYdest;

    if (!bReuse)
    {
        reset();
    }

    // Wrap start coordinates
    iXstart = wrapX(iXstart);
    iYstart = wrapY(iYstart);

    if (!isValid(iXstart, iYstart))
        return false;

    // Validate destination (if callback provided and dest is valid coords)
    if (m_destValidFunc && iXdest >= 0 && iYdest >= 0)
    {
        if (!m_destValidFunc(iXdest, iYdest, m_pData, this))
            return false;
    }

    // Add start node to open list
    FAStarNode* pStart = getNode(iXstart, iYstart);
    if (!pStart)
        return false;

    pStart->m_iKnownCost = 0;
    pStart->m_iHeuristicCost = 0;
    pStart->m_iTotalCost = 0;
    addToOpen(pStart);

    // A* main loop
    while (m_pOpen)
    {
        FAStarNode* pCurrent = popBestOpen();
        if (!pCurrent)
            break;

        // Check if we reached the destination
        // (iXdest < 0 means "no specific destination" — used by area finder)
        if (iXdest >= 0 && iYdest >= 0)
        {
            int iWrappedDestX = wrapX(iXdest);
            int iWrappedDestY = wrapY(iYdest);
            if (pCurrent->m_iX == iWrappedDestX && pCurrent->m_iY == iWrappedDestY)
            {
                m_pBest = pCurrent;
                return true;
            }
        }

        // Expand neighbors
        int iNumDirs = bCardinalOnly ? 4 : 8;
        // Cardinal directions are indices 0, 2, 4, 6 (N, E, S, W)
        for (int i = 0; i < iNumDirs; i++)
        {
            int iDir = bCardinalOnly ? (i * 2) : i;
            int iNX = pCurrent->m_iX + s_iDirX[iDir];
            int iNY = pCurrent->m_iY + s_iDirY[iDir];
            processNeighbor(pCurrent, iNX, iNY);
        }
    }

    // If no specific destination, the search processed all reachable nodes
    // (used by area finder, plot group finder, etc.)
    if (iXdest < 0 || iYdest < 0)
    {
        return true;
    }

    return false; // No path found
}
