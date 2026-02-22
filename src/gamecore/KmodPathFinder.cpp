// KmodPathFinder - Optimized pathfinding for Civ4
// Ported from K-Mod for performance improvements
// Modified for BTS SDK compatibility

#include "CvGameCoreDLL.h"
#include "KmodPathFinder.h"
#include "CvGameCoreUtils.h"
#include "CvSelectionGroup.h"
#include "CvUnit.h"
#include "CvPlot.h"
#include "CvMap.h"
#include "CvTeamAI.h"
#include "CvPlayerAI.h"
#include "CvSelectionGroupAI.h"

int KmodPathFinder::admissible_scaled_weight = 1;
int KmodPathFinder::admissible_base_weight = 1;

CvPathSettings::CvPathSettings(const CvSelectionGroup* pGroup, int iFlags, int iMaxPath, int iHW)
	: pGroup(const_cast<CvSelectionGroup*>(pGroup)), iFlags(iFlags), iMaxPath(iMaxPath), iHeuristicWeight(iHW)
{
}


// KmodPathFinder-specific callbacks (replace BTS pathDestValid/pathCost/pathAdd)

bool KmodPathFinder::KmodPathDestValid(int iToX, int iToY) const
{
	CvPlot* pToPlot = GC.getMapINLINE().plotSorenINLINE(iToX, iToY);
	if (pToPlot == NULL)
		return false;

	CvSelectionGroup* pGroup = settings.pGroup;
	if (pGroup == NULL)
		return false;

	if (pGroup->getNumUnits() <= 0)
		return false;

	if (pGroup->atPlot(pToPlot))
		return true;

	if (pGroup->getDomainType() == DOMAIN_IMMOBILE)
		return false;

	bool bAIControl = pGroup->AI_isControlled();

	if (bAIControl)
	{
		if (!(settings.iFlags & MOVE_IGNORE_DANGER))
		{
			if (!(pGroup->canFight()) && !(pGroup->alwaysInvisible()))
			{
				if (GET_PLAYER(pGroup->getHeadOwner()).AI_getPlotDanger(pToPlot) > 0)
				{
					return false;
				}
			}
		}
	}

	if (bAIControl || pToPlot->isRevealed(pGroup->getHeadTeam(), false))
	{
		if (pGroup->canMoveOrAttackInto(pToPlot))
		{
			return true;
		}
	}

	return false;
}


int KmodPathFinder::KmodPathCost(FAStarNode* parent, FAStarNode* node) const
{
	CvPlot* pFromPlot = GC.getMapINLINE().plotSorenINLINE(parent->m_iX, parent->m_iY);
	CvPlot* pToPlot = GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);

	// Safety checks for null plots
	if (pFromPlot == NULL || pToPlot == NULL)
		return MAX_INT;

	CvSelectionGroup* pGroup = settings.pGroup;
	if (pGroup == NULL || pGroup->getNumUnits() <= 0)
		return MAX_INT;

	int iWorstCost = 0;
	int iWorstMovesLeft = MAX_INT;

	int iFlags = settings.iFlags;
	bool bAIControl = pGroup->AI_isControlled();

	CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
	while (pUnitNode != NULL)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pGroup->nextUnitNode(pUnitNode);
		if (pLoopUnit == NULL)
			continue;

		int iMaxMoves = pLoopUnit->baseMoves() * GC.getMOVE_DENOMINATOR();
		int iMovesLeft = parent->m_iData1 > 0 ? parent->m_iData1 : iMaxMoves;
		
		int iCost = pToPlot->movementCost(pLoopUnit, pFromPlot);

		// Extra penalty for danger
		if (bAIControl && !(iFlags & MOVE_IGNORE_DANGER))
		{
			if (GET_PLAYER(pLoopUnit->getOwnerINLINE()).AI_getPlotDanger(pToPlot) > 0)
			{
				iCost += GC.getMOVE_DENOMINATOR();
			}
		}

		// Remaining moves after this step
		int iNewMovesLeft;
		if (iCost >= iMovesLeft)
		{
			// Use all movement this turn, start fresh next turn
			iCost = iMovesLeft;
			iNewMovesLeft = iMaxMoves;
		}
		else
		{
			iNewMovesLeft = iMovesLeft - iCost;
		}

		if (iCost > iWorstCost)
		{
			iWorstCost = iCost;
			iWorstMovesLeft = iNewMovesLeft;
		}
	}

	// Store remaining moves in the node's data1 field
	node->m_iData1 = iWorstMovesLeft;

	// Ensure we never return 0 (would cause issues)
	if (iWorstCost <= 0)
		iWorstCost = 1;

	return iWorstCost;
}

void KmodPathFinder::KmodPathAdd(FAStarNode* parent, FAStarNode* node, int data)
{
	// This function handles parent-child relationships for A* nodes
	// data parameter indicates the type of add operation (ASNC_INITIALADD, ASNC_NEWADD, etc.)

	CvSelectionGroup* pGroup = settings.pGroup;

	int iTurns = 1;
	int iMoves = MAX_INT;

	if (data == ASNC_INITIALADD)
	{
		// Initial node - calculate starting moves from units in group
		node->m_pParent = NULL;
		node->m_iKnownCost = 0;

		if (pGroup != NULL)
		{
			bool bMaxMoves = (settings.iFlags & MOVE_MAX_MOVES);
			if (bMaxMoves)
			{
				iMoves = 0;
			}

			CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
			while (pUnitNode != NULL)
			{
				CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = pGroup->nextUnitNode(pUnitNode);
				if (pLoopUnit == NULL)
					continue;

				int iUnitMoves = pLoopUnit->movesLeft();
				if (bMaxMoves)
				{
					iMoves = std::max(iMoves, pLoopUnit->baseMoves() * GC.getMOVE_DENOMINATOR());
				}
				else
				{
					iMoves = std::min(iMoves, iUnitMoves);
				}
			}
		}
		else
		{
			iMoves = 0;
		}
	}
	else
	{
		// Non-initial node - set parent relationship and calculate remaining moves
		node->m_pParent = parent;

		if (parent != NULL && pGroup != NULL)
		{
			CvPlot* pFromPlot = GC.getMapINLINE().plotSorenINLINE(parent->m_iX, parent->m_iY);
			CvPlot* pToPlot = GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);

			int iStartMoves = parent->m_iData1;
			iTurns = parent->m_iData2;
			if (iStartMoves == 0)
			{
				iTurns++;
			}

			if (pFromPlot != NULL && pToPlot != NULL)
			{
				CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
				while (pUnitNode != NULL)
				{
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pGroup->nextUnitNode(pUnitNode);
					if (pLoopUnit == NULL)
						continue;

					int iUnitMoves = (iStartMoves == 0) ? (pLoopUnit->baseMoves() * GC.getMOVE_DENOMINATOR()) : iStartMoves;
					iUnitMoves -= pToPlot->movementCost(pLoopUnit, pFromPlot);
					iUnitMoves = std::max(iUnitMoves, 0);

					iMoves = std::min(iMoves, iUnitMoves);
				}
			}
			else
			{
				iMoves = 0;
			}
		}
		else
		{
			iMoves = 0;
		}
	}

	node->m_iData1 = iMoves;
	node->m_iData2 = iTurns;
}


void KmodPathFinder::InitHeuristicWeights()
{
	admissible_base_weight = GC.getMOVE_DENOMINATOR()/2;
	admissible_scaled_weight = GC.getMOVE_DENOMINATOR()/2;
	for (int r = 0; r < GC.getNumRouteInfos(); r++)
	{
		const CvRouteInfo& kInfo = GC.getRouteInfo((RouteTypes)r);
		int iCost = kInfo.getMovementCost();
		for (int t = 0; t < GC.getNumTechInfos(); t++)
		{
			if (kInfo.getTechMovementChange(t) < 0)
				iCost += kInfo.getTechMovementChange(t);
		}
		admissible_base_weight = std::min(admissible_base_weight, iCost);
		admissible_scaled_weight = std::min(admissible_scaled_weight, kInfo.getFlatMovementCost());
	}
}


int KmodPathFinder::MinimumStepCost(int BaseMoves)
{
	return std::max(1, std::min(admissible_base_weight, BaseMoves * admissible_scaled_weight));
}


bool KmodPathFinder::OpenList_sortPred::operator()(const FAStarNode* &left, const FAStarNode* &right)
{
	return left->m_iTotalCost < right->m_iTotalCost;
}


KmodPathFinder::KmodPathFinder() :
	end_node(0),
	map_width(0),
	map_height(0),
	node_data(0),
	dest_x(0),
	dest_y(0),
	start_x(0),
	start_y(0)
{
}


KmodPathFinder::~KmodPathFinder()
{
	free(node_data);
}


bool KmodPathFinder::ValidateNodeMap()
{
	PROFILE_FUNC();
	if (!GC.getGameINLINE().isFinalInitialized())
		return false;

	if (map_width != GC.getMapINLINE().getGridWidthINLINE() || map_height != GC.getMapINLINE().getGridHeightINLINE())
	{
		map_width = GC.getMapINLINE().getGridWidthINLINE();
		map_height = GC.getMapINLINE().getGridHeightINLINE();
		node_data = (FAStarNode*)realloc(node_data, sizeof(*node_data)*map_width*map_height);
		end_node = NULL;
	}
	return true;
}


bool KmodPathFinder::GeneratePath(int x1, int y1, int x2, int y2)
{
	PROFILE_FUNC();

	// Bounds checking
	FAssert(x1 >= 0 && x1 < map_width);
	FAssert(y1 >= 0 && y1 < map_height);
	FAssert(x2 >= 0 && x2 < map_width);
	FAssert(y2 >= 0 && y2 < map_height);

	end_node = NULL;

	if (!settings.pGroup)
		return false;

	if (!KmodPathDestValid(x2, y2))
		return false;

	if (x1 != start_x || y1 != start_y)
	{
		Reset();
	}
	bool bRecalcHeuristics = false;

	if (dest_x != x2 || dest_y != y2)
		bRecalcHeuristics = true;

	start_x = x1;
	start_y = y1;
	dest_x = x2;
	dest_y = y2;

	if (GetNode(x1, y1).m_bOnStack)
	{
		// Use baseMoves * MOVE_DENOMINATOR as proxy for maxMoves
		int iMoves = settings.pGroup->baseMoves() * GC.getMOVE_DENOMINATOR();
		if (iMoves != GetNode(x1, y1).m_iData1)
		{
			Reset();
			FAssert(!GetNode(x1, y1).m_bOnStack);
		}
	}

	if (!GetNode(x1, y1).m_bOnStack)
	{
		AddStartNode();
		bRecalcHeuristics = true;
	}
	{
		if (GetNode(x2, y2).m_bOnStack)
			end_node = &GetNode(x2, y2);
	}

	if (bRecalcHeuristics)
		RecalculateHeuristics();

	while (ProcessNode())
	{
		// nothing
	}

	if (end_node && (settings.iMaxPath < 0 || end_node->m_iData2 <= settings.iMaxPath))
		return true;

	return false;
}


bool KmodPathFinder::GeneratePath(const CvPlot* pToPlot)
{
	if (!settings.pGroup || !pToPlot)
		return false;
	return GeneratePath(settings.pGroup->plot()->getX_INLINE(), settings.pGroup->plot()->getY_INLINE(),
		pToPlot->getX_INLINE(), pToPlot->getY_INLINE());
}


int KmodPathFinder::GetPathTurns() const
{
	FAssert(end_node);
	return end_node ? end_node->m_iData2 : 0;
}


int KmodPathFinder::GetFinalMoves() const
{
	FAssert(end_node);
	return end_node ? end_node->m_iData1 : 0;
}


CvPlot* KmodPathFinder::GetPathFirstPlot() const
{
	FAssert(end_node);
	if (!end_node)
		return NULL;

	FAStarNode* node = end_node;

	if (!node->m_pParent)
		return GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);

	while (node->m_pParent->m_pParent)
	{
		node = node->m_pParent;
	}

	return GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);
}


CvPlot* KmodPathFinder::GetPathEndTurnPlot() const
{
	FAssert(end_node);

	FAStarNode* node = end_node;

	FAssert(!node || node->m_iData2 == 1 || node->m_pParent);

	while (node && node->m_iData2 > 1)
	{
		node = node->m_pParent;
	}
	FAssert(node);
	return node ? GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY) : NULL;
}


void KmodPathFinder::SetSettings(const CvPathSettings& new_settings)
{
	if (!ValidateNodeMap())
	{
		FAssertMsg(false, "Failed to validate node map for pathfinder.");
		settings.pGroup = NULL;
		return;
	}

	// Flags that affect pathfinding
	int relevant_flags = ~(MOVE_DIRECT_ATTACK);

	if (settings.pGroup != new_settings.pGroup)
	{
		Reset();
	}
	else if ((settings.iFlags & relevant_flags) != (new_settings.iFlags & relevant_flags))
	{
		relevant_flags &= ~MOVE_DECLARE_WAR;
		if ((settings.iFlags & relevant_flags) != (new_settings.iFlags & relevant_flags))
		{
			Reset();
		}
	}
	settings = new_settings;

	if (settings.iHeuristicWeight < 0)
	{
		if (!settings.pGroup)
		{
			settings.iHeuristicWeight = 1;
		}
		else
		{
			if (settings.pGroup->getDomainType() == DOMAIN_SEA)
			{
				settings.iHeuristicWeight = GC.getMOVE_DENOMINATOR();
			}
			else
			{
				settings.iHeuristicWeight = MinimumStepCost(settings.pGroup->baseMoves());
			}
		}
	}
}


void KmodPathFinder::Reset()
{
	if (node_data && map_width > 0 && map_height > 0)
	{
		memset(&node_data[0], 0, sizeof(*node_data)*map_width*map_height);
	}
	open_list.clear();
	end_node = NULL;
}


void KmodPathFinder::AddStartNode()
{
	FAssert(start_x >= 0 && start_x < map_width);
	FAssert(start_y >= 0 && start_y < map_height);

	FAStarNode* start_node = &GetNode(start_x, start_y);
	start_node->m_iX = start_x;
	start_node->m_iY = start_y;
	pathAdd(0, start_node, ASNC_INITIALADD, &settings, 0);
	start_node->m_iKnownCost = 0;

	open_list.push_back(start_node);

	start_node->m_eFAStarListType = FASTARLIST_OPEN;
	start_node->m_bOnStack = true;
}


void KmodPathFinder::RecalculateHeuristics()
{
	for (OpenList_t::iterator i = open_list.begin(); i != open_list.end(); ++i)
	{
		int h = settings.iHeuristicWeight * pathHeuristic((*i)->m_iX, (*i)->m_iY, dest_x, dest_y);
		(*i)->m_iHeuristicCost = h;
		(*i)->m_iTotalCost = h + (*i)->m_iKnownCost;
	}
}


// Helper function to check if path between nodes is valid (replaces K-Mod's pathValid_join)
bool KmodPathFinder::IsValidStep(FAStarNode* parent, FAStarNode* child) const
{
	if (!settings.pGroup)
		return false;

	CvPlot* pFromPlot = GC.getMapINLINE().plotSorenINLINE(parent->m_iX, parent->m_iY);
	CvPlot* pToPlot = GC.getMapINLINE().plotSorenINLINE(child->m_iX, child->m_iY);

	if (!pFromPlot || !pToPlot)
		return false;

	// For sea units, check diagonal movement through land
	if (settings.pGroup->getDomainType() == DOMAIN_SEA)
	{
		if (pFromPlot->isWater() && pToPlot->isWater())
		{
			CvPlot* pCorner1 = GC.getMapINLINE().plotINLINE(pFromPlot->getX_INLINE(), pToPlot->getY_INLINE());
			CvPlot* pCorner2 = GC.getMapINLINE().plotINLINE(pToPlot->getX_INLINE(), pFromPlot->getY_INLINE());
			if (pCorner1 && pCorner2 && !pCorner1->isWater() && !pCorner2->isWater())
			{
				return false;
			}
		}
	}
	return true;
}


// Helper function to check if we can move from a node (replaces K-Mod's pathValid_source)
bool KmodPathFinder::CanMoveFrom(FAStarNode* node) const
{
	if (!settings.pGroup)
		return false;

	CvPlot* pPlot = GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);
	if (!pPlot)
		return false;

	// If we're at our starting position, we can always move
	if (settings.pGroup->atPlot(pPlot))
		return true;

	// Check safe territory flag
	if (settings.iFlags & MOVE_SAFE_TERRITORY)
	{
		if (pPlot->isOwned() && pPlot->getTeam() != settings.pGroup->getHeadTeam())
			return false;
		if (!pPlot->isRevealed(settings.pGroup->getHeadTeam(), false))
			return false;
	}

	return true;
}


bool KmodPathFinder::ProcessNode()
{
	OpenList_t::iterator best_it = open_list.end();
	{
		int iLowestCost = end_node ? end_node->m_iKnownCost : MAX_INT;
		for (OpenList_t::iterator it = open_list.begin(); it != open_list.end(); ++it)
		{
			if ((*it)->m_iTotalCost < iLowestCost &&
				(settings.iMaxPath < 0 || (*it)->m_iData2 <= settings.iMaxPath))
			{
				best_it = it;
				iLowestCost = (*it)->m_iTotalCost;
			}
		}
	}

	if (best_it == open_list.end())
		return false;

	FAStarNode* parent_node = (*best_it);

	open_list.erase(best_it);
	parent_node->m_eFAStarListType = FASTARLIST_CLOSED;

	FAssert(&GetNode(parent_node->m_iX, parent_node->m_iY) == parent_node);

	for (int i = 0; i < NUM_DIRECTION_TYPES; i++)
	{
		CvPlot* pAdjacentPlot = plotDirection(parent_node->m_iX, parent_node->m_iY, (DirectionTypes)i);
		if (!pAdjacentPlot)
			continue;

		const int& x = pAdjacentPlot->getX_INLINE();
		const int& y = pAdjacentPlot->getY_INLINE();

		if (parent_node->m_pParent && parent_node->m_pParent->m_iX == x && parent_node->m_pParent->m_iY == y)
			continue;

		FAStarNode* child_node = &GetNode(x, y);
		bool bNewNode = !child_node->m_bOnStack;

		if (bNewNode)
		{
			child_node->m_iX = x;
			child_node->m_iY = y;
			pathAdd(parent_node, child_node, ASNC_NEWADD, &settings, 0);

			if (pathValid_join(parent_node, child_node, settings.pGroup, settings.iFlags))
			{
				child_node->m_iKnownCost = MAX_INT;
				child_node->m_iHeuristicCost = settings.iHeuristicWeight * pathHeuristic(x, y, dest_x, dest_y);

				child_node->m_bOnStack = true;

				if (pathValid_source(child_node, settings.pGroup, settings.iFlags))
				{
					open_list.push_back(child_node);
					child_node->m_eFAStarListType = FASTARLIST_OPEN;
				}
				else
				{
					child_node->m_eFAStarListType = FASTARLIST_CLOSED;
				}
			}
			else
			{
				child_node = NULL;
			}
		}
		else
		{
			if (!pathValid_join(parent_node, child_node, settings.pGroup, settings.iFlags))
				child_node = NULL;
		}

		if (child_node == NULL)
			continue;

		FAssert(child_node->m_iX == x && child_node->m_iY == y);

		if (x == dest_x && y == dest_y)
			end_node = child_node;

		if (parent_node->m_iKnownCost < child_node->m_iKnownCost)
		{
			int iNewCost = parent_node->m_iKnownCost + pathCost(parent_node, child_node, 666, &settings, 0);
			FAssert(iNewCost > 0);

			if (iNewCost < child_node->m_iKnownCost)
			{
				int cost_delta = iNewCost - child_node->m_iKnownCost;

				child_node->m_iKnownCost = iNewCost;
				child_node->m_iTotalCost = child_node->m_iKnownCost + child_node->m_iHeuristicCost;

				if (child_node->m_pParent)
				{
					FAssert(!bNewNode);
					FAStarNode* x_parent = child_node->m_pParent;
					int temp = x_parent->m_iNumChildren;
					for (int j = 0; j < x_parent->m_iNumChildren; j++)
					{
						if (x_parent->m_apChildren[j] == child_node)
						{
							for (j++; j < x_parent->m_iNumChildren; j++)
								x_parent->m_apChildren[j-1] = x_parent->m_apChildren[j];
							x_parent->m_apChildren[j-1] = 0;

							x_parent->m_iNumChildren--;
						}
					}
					FAssert(x_parent->m_iNumChildren == temp - 1);
					pathAdd(parent_node, child_node, ASNC_PARENTADD_UP, &settings, 0);
				}

				FAssert(parent_node->m_iNumChildren < NUM_DIRECTION_TYPES);
				parent_node->m_apChildren[parent_node->m_iNumChildren] = child_node;
				parent_node->m_iNumChildren++;
				child_node->m_pParent = parent_node;

				FAssert(child_node->m_iNumChildren == 0 || !bNewNode);
				ForwardPropagate(child_node, cost_delta);

				FAssert(child_node->m_iKnownCost > parent_node->m_iKnownCost);
			}
		}
	}
	return true;
}


void KmodPathFinder::ForwardPropagate(FAStarNode* head, int cost_delta)
{
	for (int i = 0; i < head->m_iNumChildren; i++)
	{
		FAssert(head->m_apChildren[i]->m_pParent == head);

		int iOldMoves = head->m_apChildren[i]->m_iData1;
		int iOldTurns = head->m_apChildren[i]->m_iData2;
		int iNewDelta = cost_delta;
		pathAdd(head, head->m_apChildren[i], ASNC_PARENTADD_UP, &settings, 0);

		{
			int iPathCost = pathCost(head, head->m_apChildren[i], 666, &settings, 0);
			iNewDelta = head->m_iKnownCost + iPathCost - head->m_apChildren[i]->m_iKnownCost;
		}

		head->m_apChildren[i]->m_iKnownCost += iNewDelta;
		head->m_apChildren[i]->m_iTotalCost += iNewDelta;

		FAssert(head->m_apChildren[i]->m_iKnownCost > head->m_iKnownCost);

		if (iNewDelta != 0 || iOldMoves != head->m_apChildren[i]->m_iData1 || iOldTurns != head->m_apChildren[i]->m_iData2)
			ForwardPropagate(head->m_apChildren[i], iNewDelta);
	}
}

