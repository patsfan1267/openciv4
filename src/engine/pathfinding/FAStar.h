// OpenCiv4 — FAStar A* pathfinding implementation
// This replaces the engine-proprietary FAStar class that the original
// Civ4 exe provided. The SDK only has FAStarNode; FAStar was opaque.

#pragma once

#include <cstddef>      // NULL, nullptr, size_t
#include "FAStarNode.h"
#include "CvDLLFAStarIFaceBase.h"  // For callback typedefs

class FAStar
{
public:
    FAStar();
    ~FAStar();

    void initialize(int iColumns, int iRows, bool bWrapX, bool bWrapY,
                    FAPointFunc destValidFunc, FAHeuristic heuristicFunc,
                    FAStarFunc costFunc, FAStarFunc validFunc,
                    FAStarFunc notifyChildFunc, FAStarFunc notifyListFunc,
                    void* pData);

    bool generatePath(int iXstart, int iYstart, int iXdest, int iYdest,
                      bool bCardinalOnly, int iInfo, bool bReuse);

    void setData(const void* pData) { m_pData = pData; }
    FAStarNode* getLastNode() { return m_pBest; }

    // Accessors used by callback functions
    int getColumns() const { return m_iColumns; }
    int getRows() const { return m_iRows; }
    bool isWrapX() const { return m_bWrapX; }
    bool isWrapY() const { return m_bWrapY; }
    int getInfo() const { return m_iInfo; }
    const void* getData() const { return m_pData; }
    int getXdest() const { return m_iXdest; }
    int getYdest() const { return m_iYdest; }
    FAStarNode* getNode(int x, int y);

private:
    void reset();
    void addToOpen(FAStarNode* pNode);
    FAStarNode* popBestOpen();
    void processNeighbor(FAStarNode* pParent, int iNeighborX, int iNeighborY);
    int wrapX(int iX) const;
    int wrapY(int iY) const;
    bool isValid(int iX, int iY) const;

    FAStarNode* m_pGrid;    // 2D node array [rows * columns]
    int m_iColumns;
    int m_iRows;
    bool m_bWrapX;
    bool m_bWrapY;

    FAPointFunc m_destValidFunc;
    FAHeuristic m_heuristicFunc;
    FAStarFunc m_costFunc;
    FAStarFunc m_validFunc;
    FAStarFunc m_notifyChildFunc;
    FAStarFunc m_notifyListFunc;
    const void* m_pData;

    int m_iInfo;
    int m_iXdest;
    int m_iYdest;

    FAStarNode* m_pOpen;    // Open list head
    FAStarNode* m_pBest;    // Last path end node
};
