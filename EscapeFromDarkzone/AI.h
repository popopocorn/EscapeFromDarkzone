#pragma once
#include<functional>
#include "stdafx.h"

class CEnemyObject;
class CGameObject;

struct tempNavMesh
{
	int vertexCnt{};
	int polyCnt{};
	vector<XMFLOAT3> vertices;
	vector<int> idx;
};

struct NavigationPoly
{
	int ID;
	array<XMFLOAT3, 3> positions;
	array<int, 3> vindex;
	array<int, 3> neighborIDs = { -1, -1, -1 };
	XMFLOAT3 centroid = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

class AstarNavigation
{
private:
	vector<NavigationPoly> mesh;

private:
	void BuildMesh(const tempNavMesh& m);
	void FindNeighbor();
	vector<XMFLOAT3> FindPathPoint(vector<int> polyidx, XMFLOAT3 start, XMFLOAT3 end);

public:
	//밑에는 추출한 메쉬 읽는 함수, 쓰기 위한 함수 등 
	void LoadNavMeshFromFile(const char* file);
	vector<XMFLOAT3> FindPath(XMFLOAT3 start, XMFLOAT3 end);

	int FindPolyID(const XMFLOAT3& pos);
};

struct AStarNode
{
	int polyID;
	int parentID;
	float gCost;
	float hCost;

	float GetCost() const { return gCost + hCost; }

	bool operator>(const AStarNode& other) const
	{
		return GetCost() > other.GetCost();
	}
};


// ============================================================================
// Behavior Tree
// ============================================================================

enum class BehaviorStatus
{
	Success,
	Failure,
	Running
};

enum class EnemyBehaviorAction
{
	None,
	Idle,
	Chase,
	Attack,
	Reload,
	Return,
	Die
};


// NPC마다 하나씩 소유하는 행동트리 공유 데이터
// 행동 판단에 필요한 값만 저장
struct EnemyBlackboard
{
	// 현재 타겟
	CGameObject* pTarget = nullptr;		// 비소유 포인터

	// 현재 프레임에서 갱신되는 감지 정보
	bool bCanSeeTarget = false;
	bool bCanShootTarget = false;
	bool bOutsideLeash = false;
	bool bNeedsReload = false;

	float fTargetDistance = FLT_MAX;

	// 타겟 기억
	bool bHasLastSeenPosition = false;
	XMFLOAT3 xmf3LastSeenPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	// 현재 이동 목적지
	bool bHasMoveTarget = false;
	XMFLOAT3 xmf3MoveTarget = XMFLOAT3(0.0f, 0.0f, 0.0f);

	// 판단용 시간
	float fThinkTimer = 0.0f;
	float fLoseSightTimer = 0.0f;
	float fReturnIgnoreTimer = 0.0f;

	// 현재 행동(디버그 및 동일 행동 재진입 방지)
	EnemyBehaviorAction eCurrentAction = EnemyBehaviorAction::None;

	void ResetPerception();
	void ResetTargetMemory();
	void Reset();
};


struct BehaviorContext
{
	CEnemyObject* pEnemy = nullptr;
	EnemyBlackboard* pBlackboard = nullptr;
	float fTimeElapsed = 0.0f;
};


class BehaviorNode
{
public:
	explicit BehaviorNode(const char* pName = "BehaviorNode");
	virtual ~BehaviorNode() = default;

	virtual BehaviorStatus Tick(BehaviorContext& context) = 0;
	virtual void Reset();

	const std::string& GetName() const { return m_strName; }

protected:
	std::string m_strName;
};


class CompositeNode : public BehaviorNode
{
public:
	explicit CompositeNode(const char* pName);
	virtual ~CompositeNode() = default;

	void AddChild(std::unique_ptr<BehaviorNode> pChild);
	void Reset() override;

protected:
	void ResetChildren();

protected:
	std::vector<std::unique_ptr<BehaviorNode>> m_Children;
};


// 실행 중인 자식 위치를 기억하는 Sequence
class SequenceNode : public CompositeNode
{
public:
	explicit SequenceNode(const char* pName = "Sequence");

	BehaviorStatus Tick(BehaviorContext& context) override;
	void Reset() override;

private:
	size_t m_nRunningChild = 0;
};


// 실행 중인 자식 위치를 기억하는 Selector
class SelectorNode : public CompositeNode
{
public:
	explicit SelectorNode(const char* pName = "Selector");

	BehaviorStatus Tick(BehaviorContext& context) override;
	void Reset() override;

private:
	size_t m_nRunningChild = 0;
};


// 매 Tick마다 가장 높은 우선순위부터 다시 검사하는 Selector.
// Die, Return 등 높은 우선순위 행동이 현재 행동을 즉시 끊을 수 있도록 사용한다.
class ReactiveSelectorNode : public CompositeNode
{
public:
	explicit ReactiveSelectorNode(const char* pName = "ReactiveSelector");

	BehaviorStatus Tick(BehaviorContext& context) override;
	void Reset() override;

private:
	static constexpr size_t INVALID_CHILD = static_cast<size_t>(-1);
	size_t m_nRunningChild = INVALID_CHILD;
};


class ConditionNode : public BehaviorNode
{
public:
	using ConditionFunction = std::function<bool(BehaviorContext&)>;

	ConditionNode(const char* pName, ConditionFunction condition);

	BehaviorStatus Tick(BehaviorContext& context) override;

private:
	ConditionFunction m_Condition;
};


class ActionNode : public BehaviorNode
{
public:
	using ActionFunction = std::function<BehaviorStatus(BehaviorContext&)>;

	ActionNode(const char* pName, ActionFunction action);

	BehaviorStatus Tick(BehaviorContext& context) override;

private:
	ActionFunction m_Action;
};


class BehaviorTree
{
public:
	BehaviorTree() = default;
	virtual ~BehaviorTree() = default;

	void SetRoot(std::unique_ptr<BehaviorNode> pRoot);

	BehaviorStatus Tick(CEnemyObject* pEnemy, EnemyBlackboard& blackboard, float fTimeElapsed);
	void Reset();

	BehaviorNode* GetRoot() const { return m_pRoot.get(); }

protected:
	std::unique_ptr<BehaviorNode> m_pRoot;
};