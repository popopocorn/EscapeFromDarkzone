#include "stdafx.h"
#include "AI.h"
#include "EnemyObject.h"
#include <fstream>

void EnemyBlackboard::ResetPerception()
{
	bCanSeeTarget = false;
	bCanShootTarget = false;
	bOutsideLeash = false;
	bNeedsReload = false;

	fTargetDistance = FLT_MAX;
}

void EnemyBlackboard::ResetTargetMemory()
{
	pTarget = nullptr;

	bHasLastSeenPosition = false;
	xmf3LastSeenPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bHasMoveTarget = false;
	xmf3MoveTarget = XMFLOAT3(0.0f, 0.0f, 0.0f);

	fLoseSightTimer = 0.0f;
}

void EnemyBlackboard::Reset()
{
	ResetPerception();
	ResetTargetMemory();

	fThinkTimer = 0.0f;
	fReturnIgnoreTimer = 0.0f;

	eCurrentAction = EnemyBehaviorAction::None;

	Hearing.Reset();
	Search = EnemySearchMemory{};
	Damage.Reset();
}

BehaviorNode::BehaviorNode(const char* pName)
{
	if (pName)
		m_strName = pName;
	else
		m_strName = "BehaviorNode";
}

void BehaviorNode::Reset()
{
}

CompositeNode::CompositeNode(const char* pName)
	: BehaviorNode(pName)
{
}

void CompositeNode::AddChild(std::unique_ptr<BehaviorNode> pChild)
{
	if (!pChild)
		return;

	m_Children.push_back(std::move(pChild));
}

void CompositeNode::ResetChildren()
{
	for (auto& child : m_Children)
	{
		if (child)
			child->Reset();
	}
}

void CompositeNode::Reset()
{
	ResetChildren();
}

SequenceNode::SequenceNode(const char* pName)
	: CompositeNode(pName)
{
}

BehaviorStatus SequenceNode::Tick(BehaviorContext& context)
{
	if (m_Children.empty())
		return BehaviorStatus::Success;

	while (m_nRunningChild < m_Children.size())
	{
		BehaviorNode* pChild = m_Children[m_nRunningChild].get();

		if (!pChild)
		{
			m_nRunningChild++;
			continue;
		}

		BehaviorStatus status = pChild->Tick(context);

		if (status == BehaviorStatus::Running)
			return BehaviorStatus::Running;

		if (status == BehaviorStatus::Failure)
		{
			Reset();
			return BehaviorStatus::Failure;
		}

		pChild->Reset();
		m_nRunningChild++;
	}

	Reset();
	return BehaviorStatus::Success;
}

void SequenceNode::Reset()
{
	m_nRunningChild = 0;
	CompositeNode::Reset();
}

ReactiveSequenceNode::ReactiveSequenceNode(const char* pName)
	: CompositeNode(pName)
{
}

BehaviorStatus ReactiveSequenceNode::Tick(BehaviorContext& context)
{
	for (size_t i = 0; i < m_Children.size(); ++i)
	{
		BehaviorNode* pChild = m_Children[i].get();

		if (!pChild)
			continue;

		BehaviorStatus status = pChild->Tick(context);

		if (status == BehaviorStatus::Success)
		{
			pChild->Reset();
			continue;
		}

		if (m_nRunningChild != INVALID_CHILD && m_nRunningChild != i && m_nRunningChild < m_Children.size())
		{
			if (m_Children[m_nRunningChild])
				m_Children[m_nRunningChild]->Reset();
		}

		if (status == BehaviorStatus::Running)
		{
			m_nRunningChild = i;
			return BehaviorStatus::Running;
		}

		m_nRunningChild = INVALID_CHILD;

		for (size_t j = i + 1; j < m_Children.size(); ++j)
		{
			if (m_Children[j])
				m_Children[j]->Reset();
		}

		return BehaviorStatus::Failure;
	}

	m_nRunningChild = INVALID_CHILD;
	return BehaviorStatus::Success;
}

void ReactiveSequenceNode::Reset()
{
	m_nRunningChild = INVALID_CHILD;
	CompositeNode::Reset();
}

SelectorNode::SelectorNode(const char* pName)
	: CompositeNode(pName)
{
}

BehaviorStatus SelectorNode::Tick(BehaviorContext& context)
{
	if (m_Children.empty())
		return BehaviorStatus::Failure;

	while (m_nRunningChild < m_Children.size())
	{
		BehaviorNode* pChild = m_Children[m_nRunningChild].get();

		if (!pChild)
		{
			m_nRunningChild++;
			continue;
		}

		BehaviorStatus status = pChild->Tick(context);

		if (status == BehaviorStatus::Running)
			return BehaviorStatus::Running;

		if (status == BehaviorStatus::Success)
		{
			Reset();
			return BehaviorStatus::Success;
		}

		pChild->Reset();
		m_nRunningChild++;
	}

	Reset();
	return BehaviorStatus::Failure;
}

void SelectorNode::Reset()
{
	m_nRunningChild = 0;
	CompositeNode::Reset();
}

ReactiveSelectorNode::ReactiveSelectorNode(const char* pName)
	: CompositeNode(pName)
{
}

BehaviorStatus ReactiveSelectorNode::Tick(BehaviorContext& context)
{
	size_t selectedChild = INVALID_CHILD;
	BehaviorStatus selectedStatus = BehaviorStatus::Failure;

	for (size_t i = 0; i < m_Children.size(); ++i)
	{
		BehaviorNode* pChild = m_Children[i].get();

		if (!pChild)
			continue;

		BehaviorStatus status = pChild->Tick(context);

		if (status == BehaviorStatus::Failure)
		{
			pChild->Reset();
			continue;
		}

		selectedChild = i;
		selectedStatus = status;
		break;
	}

	if (m_nRunningChild != INVALID_CHILD && m_nRunningChild != selectedChild && m_nRunningChild < m_Children.size())
	{
		if (m_Children[m_nRunningChild])
			m_Children[m_nRunningChild]->Reset();
	}

	if (selectedStatus == BehaviorStatus::Running)
	{
		m_nRunningChild = selectedChild;
	}
	else
	{
		m_nRunningChild = INVALID_CHILD;

		if (selectedChild != INVALID_CHILD && selectedChild < m_Children.size() && m_Children[selectedChild])
			m_Children[selectedChild]->Reset();
	}

	return selectedStatus;
}

void ReactiveSelectorNode::Reset()
{
	m_nRunningChild = INVALID_CHILD;
	CompositeNode::Reset();
}

ConditionNode::ConditionNode(const char* pName, ConditionFunction condition)
	: BehaviorNode(pName), m_Condition(std::move(condition))
{
}

BehaviorStatus ConditionNode::Tick(BehaviorContext& context)
{
	if (!m_Condition)
		return BehaviorStatus::Failure;

	return m_Condition(context) ? BehaviorStatus::Success : BehaviorStatus::Failure;
}

ActionNode::ActionNode(const char* pName, ActionFunction action)
	: BehaviorNode(pName), m_Action(std::move(action))
{
}

BehaviorStatus ActionNode::Tick(BehaviorContext& context)
{
	if (!m_Action)
		return BehaviorStatus::Failure;

	return m_Action(context);
}

void BehaviorTree::SetRoot(std::unique_ptr<BehaviorNode> pRoot)
{
	if (m_pRoot)
		m_pRoot->Reset();

	m_pRoot = std::move(pRoot);
}

BehaviorStatus BehaviorTree::Tick(CEnemyObject* pEnemy, EnemyBlackboard& blackboard, float fTimeElapsed)
{
	if (!m_pRoot || !pEnemy)
		return BehaviorStatus::Failure;

	BehaviorContext context;
	context.pEnemy = pEnemy;
	context.pBlackboard = &blackboard;
	context.fTimeElapsed = fTimeElapsed;

	return m_pRoot->Tick(context);
}

void BehaviorTree::Reset()
{
	if (m_pRoot)
		m_pRoot->Reset();
}

bool IsSamePosition(const XMFLOAT3& p1, const XMFLOAT3& p2)
{
	// float의 오차를 감안하여 1mm (0.001f) 이내면 같은 점으로 취급
	float dx = p1.x - p2.x;
	float dy = p1.y - p2.y;
	float dz = p1.z - p2.z;
	float distSq = (dx * dx) + (dy * dy) + (dz * dz);
	return distSq < 0.000001f;
}

void AstarNavigation::BuildMesh(const tempNavMesh& m)
{
	mesh.resize(m.polyCnt);
	for (int i = 0; i < m.polyCnt; ++i) 
	{
		mesh[i].ID = i;
		mesh[i].vindex =
		{
			m.idx[3 * i],
			m.idx[3 * i + 1],
			m.idx[3 * i + 2]
		};
		mesh[i].positions = 
		{
			m.vertices[m.idx[3 * i]],
			m.vertices[m.idx[3 * i + 1]],
			m.vertices[m.idx[3 * i + 2]],
		};
		for (int j = 0; j < 3; ++j)
		{
			mesh[i].centroid.x += mesh[i].positions[j].x;
			mesh[i].centroid.y += mesh[i].positions[j].y;
			mesh[i].centroid.z += mesh[i].positions[j].z;
		}
		mesh[i].centroid.x /= 3;
		mesh[i].centroid.y /= 3;
		mesh[i].centroid.z /= 3;
	}
}

void AstarNavigation::FindNeighbor()
{
	map<pair<int, int>, vector<int>> edgeMap;

	// 1. 모든 폴리곤을 순회하며 사전을 만듭니다. (1차 순회)
	for (int i = 0; i < mesh.size(); ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			// 삼각형의 선분 3개를 추출 (0-1, 1-2, 2-0 순서)
			int v1 = mesh[i].vindex[j];
			int v2 = mesh[i].vindex[(j + 1) % 3]; // % 3 을 하면 2 다음 다시 0으로 돌아옵니다.

			int minV = min(v1, v2);
			int maxV = max(v1, v2);

			// 사전에 폴리곤 ID 추가
			edgeMap[{minV, maxV}].push_back(i);
		}
	}

	// 2. 완성된 사전을 보면서 이웃을 맺어줍니다. (2차 순회)
	for (const auto& pair : edgeMap)
	{
		const vector<int>& sharedPolys = pair.second;

		// 정상적인 내비메쉬라면, 하나의 선분은 최대 2개의 폴리곤만 공유할 수 있습니다.
		if (sharedPolys.size() == 2)
		{
			int polyA = sharedPolys[0];
			int polyB = sharedPolys[1];

			// PolyA에 PolyB를 이웃으로 등록 (빈칸(-1)을 찾아서 쏙 넣음)
			for (int k = 0; k < 3; ++k) {
				if (mesh[polyA].neighborIDs[k] == -1) {
					mesh[polyA].neighborIDs[k] = polyB;
					break;
				}
			}

			// PolyB에 PolyA를 이웃으로 등록
			for (int k = 0; k < 3; ++k) {
				if (mesh[polyB].neighborIDs[k] == -1) {
					mesh[polyB].neighborIDs[k] = polyA;
					break;
				}
			}
		}
	}
}


void AstarNavigation::LoadNavMeshFromFile(const char* file)
{
	ifstream is(file, ios::binary);

	tempNavMesh temp;

	is.read(reinterpret_cast<char*>(&temp.vertexCnt), sizeof(int));
	is.read(reinterpret_cast<char*>(&temp.polyCnt), sizeof(int));
	temp.vertices.resize(temp.vertexCnt);
	is.read(reinterpret_cast<char*>(temp.vertices.data()), sizeof(XMFLOAT3) * temp.vertexCnt);
	temp.idx.resize(temp.polyCnt * 3);
	is.read(reinterpret_cast<char*>(temp.idx.data()), sizeof(int) * temp.polyCnt * 3);

	is.close();

	vector<XMFLOAT3> uniqueVertices;
	vector<int> indexRemap(temp.vertexCnt);

	for (int i = 0; i < temp.vertexCnt; ++i)
	{
		int foundIndex = -1;

		for (int j = 0; j < uniqueVertices.size(); ++j)
		{
			if (IsSamePosition(temp.vertices[i], uniqueVertices[j]))
			{
				foundIndex = j;
				break;
			}
		}

		if (foundIndex != -1)
		{

			indexRemap[i] = foundIndex;
		}
		else
		{

			indexRemap[i] = uniqueVertices.size();
			uniqueVertices.push_back(temp.vertices[i]);
		}
	}

	
	temp.vertices = uniqueVertices;
	temp.vertexCnt = uniqueVertices.size();

	
	for (int i = 0; i < temp.idx.size(); ++i)
	{
		temp.idx[i] = indexRemap[temp.idx[i]];
	}

	BuildMesh(temp);
	FindNeighbor();

}

float CalculateDistance(const XMFLOAT3& a, const XMFLOAT3& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

#include<queue>
struct Portal
{
	XMFLOAT3 left;
	XMFLOAT3 right;
};

float TriArea2D(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
{
	return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}

vector<XMFLOAT3> AstarNavigation::FindPathPoint(vector<int> polyidx, XMFLOAT3 start, XMFLOAT3 end)
{
	vector<XMFLOAT3> waypoints;
	if (polyidx.empty()) return waypoints;
	if (polyidx.size() == 1) { waypoints.push_back(end); return waypoints; }

	vector<Portal> pts;
	pts.push_back({ start, start });


	for (int i = 0; i < polyidx.size() - 1; ++i)
	{
		int curID = polyidx[i];
		int nextID = polyidx[i + 1];

		vector<XMFLOAT3> shared;
		for (int j = 0; j < 3; ++j) {
			for (int k = 0; k < 3; ++k) {
				if (mesh[curID].vindex[j] == mesh[nextID].vindex[k]) {
					shared.push_back(mesh[curID].positions[j]);
				}
			}
		}

		if (shared.size() == 2)
		{
			XMFLOAT3 p1 = shared[0];
			XMFLOAT3 p2 = shared[1];
			
			// 찌그러진 폴리곤에서도 시선이 벗어나지 않도록 '문지방의 정중앙(mid)'을 바라봅니다.
			XMFLOAT3 mid;
			mid.x = (p1.x + p2.x) * 0.5f;
			mid.y = (p1.y + p2.y) * 0.5f;
			mid.z = (p1.z + p2.z) * 0.5f;

			if (TriArea2D(mesh[curID].centroid, mid, p1) > 0.0f) {
				pts.push_back({ p1, p2 }); // p1이 왼쪽
			} else {
				pts.push_back({ p2, p1 }); // p2가 왼쪽
			}
		}
	}
	pts.push_back({ end, end });

	waypoints.push_back(start);
	if (pts.size() <= 2) { waypoints.push_back(end); return waypoints; }

	XMFLOAT3 Apex = pts[0].left;
	XMFLOAT3 Left = pts[0].left;
	XMFLOAT3 Right = pts[0].right;
	int AIdx = 0, LIdx = 0, RIdx = 0;

	for (int i = 1; i < pts.size(); ++i)
	{
		XMFLOAT3 l = pts[i].left;
		XMFLOAT3 r = pts[i].right;

		if (TriArea2D(Apex, Right, r) >= 0.0f) 
		{
			if (IsSamePosition(Apex, Right) || TriArea2D(Apex, Left, r) < 0.0f) {
				Right = r; RIdx = i;
			} else {
				waypoints.push_back(Left);
				Apex = Left; AIdx = LIdx;
				Left = Apex; Right = Apex;
				i = AIdx; continue;
			}
		}

		if (TriArea2D(Apex, Left, l) <= 0.0f) 
		{
			if (IsSamePosition(Apex, Left) || TriArea2D(Apex, Right, l) > 0.0f) {
				Left = l; LIdx = i;
			} else {
				
				waypoints.push_back(Right);
				Apex = Right; AIdx = RIdx;
				Left = Apex; Right = Apex;
				i = AIdx; continue;
			}
		}
	}

	waypoints.push_back(end);
	return waypoints;
}


vector<XMFLOAT3> AstarNavigation::FindPath(XMFLOAT3 start, XMFLOAT3 end)
{

	vector<int> idx;
	int startID = FindPolyID(start);
	int endID = FindPolyID(end);


	if (startID == -1 || endID == -1)
		return { start, end };

	priority_queue<AStarNode, vector<AStarNode>, greater<AStarNode>> openList;

	unordered_map<int, AStarNode> nodeTracker;
	unordered_map<int, bool> closedList;

	AStarNode startNode =
	{
		startID,
		-1,
		0.0f,
		CalculateDistance(mesh[startID].centroid, mesh[endID].centroid)
	};

	openList.push(startNode);
	nodeTracker[startID] = startNode;

	bool pathFound = false;

	while (not openList.empty())
	{
		AStarNode cur = openList.top();
		openList.pop();
		if (closedList[cur.polyID])continue;
		closedList[cur.polyID] = true;

		if (cur.polyID == endID)
		{
			pathFound = true;
			break;
		}

		for (int i = 0; i < 3; ++i)
		{
			int neighborID = mesh[cur.polyID].neighborIDs[i];

			if (neighborID == -1 || closedList[neighborID])continue;

			float moveCost = CalculateDistance(mesh[cur.polyID].centroid, mesh[neighborID].centroid);
			float newGCost = cur.gCost + moveCost;

			if (nodeTracker.find(neighborID) == nodeTracker.end()
				|| newGCost<nodeTracker[neighborID].gCost)
			{
				AStarNode neighborNode = {
					neighborID,
					cur.polyID,
					newGCost,
					CalculateDistance(mesh[neighborID].centroid, mesh[endID].centroid)
				};
				nodeTracker[neighborID] = neighborNode;
				openList.push(neighborNode);
			}
		}
	}
	if (pathFound)
	{
		int curr = endID;
		while (curr != -1)
		{
			idx.push_back(curr);
			curr = nodeTracker[curr].parentID;
		}
		reverse(idx.begin(), idx.end());
		

	}
	else
	{
		return vector<XMFLOAT3>();
	}


	return FindPathPoint(idx, start, end);
}

bool IsPointInTriangle(const XMFLOAT3& pt, const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
	// (p1 - p3) X (p2 - p3) 의 2D 외적 (Y축 성분)
	auto Sign = [](const XMFLOAT3& p1, const XMFLOAT3& p2, const XMFLOAT3& p3) {
		return (p1.x - p3.x) * (p2.z - p3.z) - (p2.x - p3.x) * (p1.z - p3.z);
		};

	float d1 = Sign(pt, v0, v1);
	float d2 = Sign(pt, v1, v2);
	float d3 = Sign(pt, v2, v0);

	bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	// 음수와 양수가 섞여 있지 않다면 삼각형 내부에 있음
	return !(has_neg && has_pos);
}

int AstarNavigation::FindPolyID(const XMFLOAT3& pos)
{


	for (int i = 0; i < mesh.size(); ++i)
	{

		if (IsPointInTriangle(pos, mesh[i].positions[0], mesh[i].positions[1], mesh[i].positions[2]))
		{
			
			return i;
		}
	}

	return -1;
}

bool AstarNavigation::FindSearchPointAround(const XMFLOAT3& center, float minRadius, float maxRadius, float random01, XMFLOAT3& outPoint) const
{
	vector<int> candidates;
	candidates.reserve(mesh.size());

	float minRadiusSq = minRadius * minRadius;
	float maxRadiusSq = maxRadius * maxRadius;

	for (int i = 0; i < static_cast<int>(mesh.size()); ++i)
	{
		const XMFLOAT3& point = mesh[i].centroid;

		float dx = point.x - center.x;
		float dz = point.z - center.z;

		float distanceSq = dx * dx + dz * dz;

		if (distanceSq < minRadiusSq)
			continue;

		if (distanceSq > maxRadiusSq)
			continue;

		candidates.push_back(i);
	}

	if (candidates.empty())
		return false;

	if (random01 < 0.0f)
		random01 = 0.0f;

	if (random01 >= 1.0f)
		random01 = 0.999999f;

	size_t selectedIndex = static_cast<size_t>(random01 * static_cast<float>(candidates.size()));

	if (selectedIndex >= candidates.size())
		selectedIndex = candidates.size() - 1;

	outPoint = mesh[candidates[selectedIndex]].centroid;

	return true;
}

EnemyBehaviorTree::EnemyBehaviorTree()
{
	BuildTree();
}

void EnemyBehaviorTree::BuildTree()
{
	auto pRoot = std::make_unique<ReactiveSelectorNode>("EnemyRoot");

	// -------------------------------------------------------------------------
	// Die
	// -------------------------------------------------------------------------
	auto pDieSequence = std::make_unique<ReactiveSequenceNode>("DieSequence");

	pDieSequence->AddChild(std::make_unique<ConditionNode>(
		"IsDying",
		[](BehaviorContext& context)
		{
			if (!context.pEnemy)
				return false;

			return context.pEnemy->IsDying();
		}
	));

	pDieSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteDie",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Die);
		}
	));

	pRoot->AddChild(std::move(pDieSequence));


	// -------------------------------------------------------------------------
	// Return
	// -------------------------------------------------------------------------
	auto pReturnSequence = std::make_unique<ReactiveSequenceNode>("ReturnSequence");

	pReturnSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldReturn",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldReturn(context.pEnemy, *context.pBlackboard);
		}
	));

	pReturnSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteReturn",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Return);
		}
	));

	pRoot->AddChild(std::move(pReturnSequence));


	// -------------------------------------------------------------------------
	// Reload
	// -------------------------------------------------------------------------
	auto pReloadSequence = std::make_unique<ReactiveSequenceNode>("ReloadSequence");

	pReloadSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldReload",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldReload(context.pEnemy, *context.pBlackboard);
		}
	));

	pReloadSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteReload",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Reload);
		}
	));

	pRoot->AddChild(std::move(pReloadSequence));


	// -------------------------------------------------------------------------
	// Attack
	// -------------------------------------------------------------------------
	auto pAttackSequence = std::make_unique<ReactiveSequenceNode>("AttackSequence");

	pAttackSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldAttack",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldAttack(context.pEnemy, *context.pBlackboard);
		}
	));

	pAttackSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteAttack",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Attack);
		}
	));

	pRoot->AddChild(std::move(pAttackSequence));


	// -------------------------------------------------------------------------
	// Chase
	// -------------------------------------------------------------------------
	auto pChaseSequence = std::make_unique<ReactiveSequenceNode>("ChaseSequence");

	pChaseSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldChase",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldChase(context.pEnemy, *context.pBlackboard);
		}
	));

	pChaseSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteChase",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Chase);
		}
	));

	pRoot->AddChild(std::move(pChaseSequence));


	// -------------------------------------------------------------------------
	// Investigate
	// -------------------------------------------------------------------------
	auto pInvestigateSequence = std::make_unique<ReactiveSequenceNode>("InvestigateSequence");

	pInvestigateSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldInvestigate",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldInvestigate(context.pEnemy, *context.pBlackboard);
		}
	));

	pInvestigateSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteInvestigate",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Investigate);
		}
	));

	pRoot->AddChild(std::move(pInvestigateSequence));


	// -------------------------------------------------------------------------
	// Search
	// -------------------------------------------------------------------------
	auto pSearchSequence = std::make_unique<ReactiveSequenceNode>("SearchSequence");

	pSearchSequence->AddChild(std::make_unique<ConditionNode>(
		"ShouldSearch",
		[this](BehaviorContext& context)
		{
			if (!context.pEnemy || !context.pBlackboard)
				return false;

			return ShouldSearch(context.pEnemy, *context.pBlackboard);
		}
	));

	pSearchSequence->AddChild(std::make_unique<ActionNode>(
		"ExecuteSearch",
		[this](BehaviorContext& context)
		{
			return ExecuteSearch(context);
		}
	));

	pRoot->AddChild(std::move(pSearchSequence));


	// -------------------------------------------------------------------------
	// Idle
	// -------------------------------------------------------------------------
	pRoot->AddChild(std::make_unique<ActionNode>(
		"ExecuteIdle",
		[this](BehaviorContext& context)
		{
			return SelectAction(context, EnemyBehaviorAction::Idle);
		}
	));

	SetRoot(std::move(pRoot));
}

void EnemyBehaviorTree::UpdateBlackboard(CEnemyObject* pEnemy, EnemyBlackboard& blackboard, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	// 청각 기억 갱신
	if (blackboard.Hearing.bHasSound)
	{
		blackboard.Hearing.fSoundAge += fTimeElapsed;

		// Investigate를 이미 수행 중이라면 목적지까지 기억을 유지한다.
		if (blackboard.eCurrentAction != EnemyBehaviorAction::Investigate &&
			blackboard.Hearing.fSoundAge >= blackboard.Hearing.fSoundMemoryDuration)
		{
			blackboard.Hearing.Reset();
		}
	}

	// 최근 피격 기억 갱신
	// 서버 AI Tick에서도 NPC별 Damage Memory를 동일하게 갱신한다.
	if (blackboard.Damage.bHasDamage)
	{
		blackboard.Damage.fDamageAge += fTimeElapsed;

		if (blackboard.Damage.fDamageAge >= blackboard.Damage.fMemoryDuration)
		{
			blackboard.Damage.Reset();
		}
	}

	blackboard.ResetPerception();

	blackboard.bOutsideLeash = pEnemy->IsOutsideLeashRange();
	blackboard.bNeedsReload = pEnemy->NeedsReload();

	if (blackboard.fReturnIgnoreTimer > 0.0f)
	{
		blackboard.fReturnIgnoreTimer -= fTimeElapsed;

		if (blackboard.fReturnIgnoreTimer < 0.0f)
			blackboard.fReturnIgnoreTimer = 0.0f;
	}

	CGameObject* pPlayer = pEnemy->GetPlayer();

	if (!pPlayer)
	{
		blackboard.fTargetDistance = FLT_MAX;
		return;
	}

	blackboard.fTargetDistance = pEnemy->GetDistanceToPlayerXZ();

	if (blackboard.fReturnIgnoreTimer > 0.0f)
	{
		blackboard.bCanSeeTarget = false;
		blackboard.bCanShootTarget = false;
		return;
	}

	blackboard.bCanSeeTarget = pEnemy->CanDetectPlayer();
	blackboard.bCanShootTarget = blackboard.bCanSeeTarget && pEnemy->CanShootPlayer();

	if (blackboard.bCanSeeTarget)
	{
		pEnemy->RefreshLastSeenPlayer();

		blackboard.Hearing.Reset();
	}
	else if (blackboard.bHasLastSeenPosition)
	{
		blackboard.fLoseSightTimer += fTimeElapsed;
	}
}

BehaviorStatus EnemyBehaviorTree::Tick(CEnemyObject* pEnemy, EnemyBlackboard& blackboard, float fTimeElapsed)
{
	if (!pEnemy)
		return BehaviorStatus::Failure;

	UpdateBlackboard(pEnemy, blackboard, fTimeElapsed);

	return BehaviorTree::Tick(pEnemy, blackboard, fTimeElapsed);
}

bool EnemyBehaviorTree::ShouldReturn(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (blackboard.bOutsideLeash)
		return true;

	if (blackboard.eCurrentAction == EnemyBehaviorAction::Return && !pEnemy->IsNearSpawn())
		return true;

	if (pEnemy->IsReloading())
		return false;

	if (blackboard.bHasLastSeenPosition && !pEnemy->HasRecentLastSeenPlayer())
		return true;

	return false;
}

bool EnemyBehaviorTree::ShouldReload(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (!blackboard.bNeedsReload)
		return false;

	if (pEnemy->IsReloading())
		return true;

	if (blackboard.bCanSeeTarget)
		return true;

	if (pEnemy->HasRecentLastSeenPlayer())
		return true;

	return false;
}

bool EnemyBehaviorTree::ShouldAttack(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (blackboard.bCanShootTarget)
		return true;

	if (blackboard.eCurrentAction == EnemyBehaviorAction::Attack)
	{
		if (!pEnemy->IsPlayerOutOfAttackRange())
		{
			if (blackboard.bCanSeeTarget || pEnemy->HasRecentLastSeenPlayer())
				return true;
		}
	}

	return false;
}

bool EnemyBehaviorTree::ShouldChase(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (blackboard.bCanSeeTarget)
		return true;

	if (pEnemy->HasRecentLastSeenPlayer())
		return true;

	return false;
}

bool EnemyBehaviorTree::ShouldInvestigate(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (pEnemy->IsDying())
		return false;

	if (!blackboard.Hearing.bHasSound)
		return false;

	if (blackboard.bOutsideLeash)
		return false;

	if (blackboard.fReturnIgnoreTimer > 0.0f)
		return false;

	if (blackboard.bCanSeeTarget)
		return false;

	if (pEnemy->HasRecentLastSeenPlayer())
		return false;

	if (blackboard.eCurrentAction == EnemyBehaviorAction::Return && !pEnemy->IsNearSpawn())
		return false;

	return true;
}

float EnemyBehaviorTree::NextSearchRandom01(EnemyBlackboard& blackboard)
{
	EnemySearchMemory& search = blackboard.Search;

	if (search.nRandomSeed == 0)
		search.nRandomSeed = 0xA341316Cu;

	search.nRandomSeed = search.nRandomSeed * 1664525u + 1013904223u;

	return static_cast<float>(search.nRandomSeed & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}
bool EnemyBehaviorTree::ShouldSearch(CEnemyObject* pEnemy, const EnemyBlackboard& blackboard) const
{
	if (!pEnemy)
		return false;

	if (pEnemy->IsDying())
		return false;

	if (!pEnemy->GetNav())
		return false;

	if (blackboard.bOutsideLeash)
		return false;

	if (blackboard.bNeedsReload)
		return false;

	if (blackboard.bCanSeeTarget)
		return false;

	if (pEnemy->HasRecentLastSeenPlayer())
		return false;

	if (blackboard.Hearing.bHasSound)
		return false;

	if (blackboard.fReturnIgnoreTimer > 0.0f)
		return false;

	if (blackboard.eCurrentAction == EnemyBehaviorAction::Return && !pEnemy->IsNearSpawn())
		return false;

	return true;
}

bool EnemyBehaviorTree::BuildSearchTarget(CEnemyObject* pEnemy, EnemyBlackboard& blackboard)
{
	if (!pEnemy)
		return false;

	AstarNavigation* pNav = pEnemy->GetNav();

	if (!pNav)
		return false;

	EnemySearchMemory& search = blackboard.Search;

	if (search.nRandomSeed == 0)
	{
		std::uint64_t address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pEnemy));
		search.nRandomSeed = static_cast<unsigned int>(address ^ (address >> 32));

		if (search.nRandomSeed == 0)
			search.nRandomSeed = 0xA341316Cu;
	}

	XMFLOAT3 spawnPos = pEnemy->GetSpawnPosition();

	float random01 = NextSearchRandom01(blackboard);

	XMFLOAT3 searchTarget;

	if (!pNav->FindSearchPointAround(spawnPos, search.fMinTargetDistance, search.fSearchRadius, random01, searchTarget))
	{
		search.bHasTarget = false;
		search.bReachedTarget = false;
		search.bPathFailed = false;
		search.fWaitTimer = 0.5f;

		blackboard.bHasMoveTarget = false;

		return false;
	}

	search.xmf3Target = searchTarget;
	search.bHasTarget = true;
	search.bReachedTarget = false;
	search.bPathFailed = false;
	search.fWaitTimer = 0.0f;

	blackboard.bHasMoveTarget = true;
	blackboard.xmf3MoveTarget = searchTarget;

	pEnemy->ClearPath();

	return true;
}

//BT는 search목적지만 정하고, 실제 이동은 EnemySearchState에서 처리
BehaviorStatus EnemyBehaviorTree::ExecuteSearch(BehaviorContext& context)
{
	if (!context.pEnemy || !context.pBlackboard)
		return BehaviorStatus::Failure;

	CEnemyObject* pEnemy = context.pEnemy;
	EnemyBlackboard& blackboard = *context.pBlackboard;
	EnemySearchMemory& search = blackboard.Search;

	if (search.bReachedTarget)
	{
		search.bReachedTarget = false;
		search.bHasTarget = false;

		float randomWait = NextSearchRandom01(blackboard);
		search.fWaitTimer = search.fWaitMin + (search.fWaitMax - search.fWaitMin) * randomWait;

		blackboard.bHasMoveTarget = false;
	}

	if (search.bPathFailed)
	{
		search.bPathFailed = false;
		search.bHasTarget = false;
		blackboard.bHasMoveTarget = false;
	}

	if (search.fWaitTimer > 0.0f)
	{
		search.fWaitTimer -= context.fTimeElapsed;

		if (search.fWaitTimer < 0.0f)
			search.fWaitTimer = 0.0f;

		if (blackboard.eCurrentAction != EnemyBehaviorAction::Search)
			SelectAction(context, EnemyBehaviorAction::Search);

		return BehaviorStatus::Running;
	}

	if (!search.bHasTarget)
	{
		if (!BuildSearchTarget(pEnemy, blackboard))
		{
			if (blackboard.eCurrentAction != EnemyBehaviorAction::Search)
				SelectAction(context, EnemyBehaviorAction::Search);

			return BehaviorStatus::Running;
		}
	}

	if (blackboard.eCurrentAction != EnemyBehaviorAction::Search)
		SelectAction(context, EnemyBehaviorAction::Search);

	return BehaviorStatus::Running;
}

BehaviorStatus EnemyBehaviorTree::SelectAction(BehaviorContext& context, EnemyBehaviorAction action)
{
	if (!context.pEnemy || !context.pBlackboard)
		return BehaviorStatus::Failure;

	CEnemyObject* pEnemy = context.pEnemy;
	EnemyBlackboard& blackboard = *context.pBlackboard;

	if (blackboard.eCurrentAction == action)
		return BehaviorStatus::Running;

	blackboard.eCurrentAction = action;

	switch (action)
	{
	case EnemyBehaviorAction::Idle:
		pEnemy->ChangeState(std::make_unique<EnemyIdle>());
		break;

	case EnemyBehaviorAction::Search:
		pEnemy->ChangeState(std::make_unique<EnemySearch>());
		break;

	case EnemyBehaviorAction::Investigate:
		pEnemy->ChangeState(std::make_unique<EnemyInvestigate>());
		break;

	case EnemyBehaviorAction::Chase:
		pEnemy->ChangeState(std::make_unique<EnemyRun>());
		break;

	case EnemyBehaviorAction::Attack:
		pEnemy->ChangeState(std::make_unique<EnemyAttack>());
		break;

	case EnemyBehaviorAction::Reload:
		pEnemy->ChangeState(std::make_unique<EnemyReload>());
		break;

	case EnemyBehaviorAction::Return:
		pEnemy->ChangeState(std::make_unique<EnemyReturn>());
		break;

	case EnemyBehaviorAction::Die:
		pEnemy->ChangeState(std::make_unique<EnemyDie>());
		break;

	case EnemyBehaviorAction::None:
	default:
		return BehaviorStatus::Failure;
	}

	return BehaviorStatus::Running;
}