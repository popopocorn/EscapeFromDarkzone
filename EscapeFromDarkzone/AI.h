#pragma once
#include<functional>

struct tempNavMesh {
	int vertexCnt{};
	int polyCnt{};
	vector<XMFLOAT3>vertices;
	vector<int>idx;

};

struct NavigationPoly
{
	int					ID;
	array<XMFLOAT3, 3>	positions;
	array<int, 3>		vindex;
	array<int, 3>		neighborIDs = {-1, -1, -1};
	XMFLOAT3			centroid = XMFLOAT3(0.0, 0.0, 0.0);

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
	//밑에는 추출한 메쉬 읽는 함수, 사용하기 위한 함수 등 
	void LoadNavMeshFromFile(const char* file);
	vector<XMFLOAT3> FindPath(XMFLOAT3 start, XMFLOAT3 end);

	int FindPolyID(const XMFLOAT3& pos);

};

struct AStarNode {
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

enum class NodeStat {
	Success,
	Fail,
	Runnig,
};

class BehaviorNode {
public:
	virtual ~BehaviorNode() {}
	virtual NodeStat Status() {};
};

class Action : public BehaviorNode {
public:
	Action(function<NodeStat()> act) { action = act; }

	NodeStat Status() {
		return action();
	}

private:
	function<NodeStat()> action;
};

class Selector : public BehaviorNode {
public:
	void addChild(BehaviorNode* child) {
		children.push_back(child);
	}
	NodeStat Status() {
		for (auto& c : children)
		{
			NodeStat stat = c->Status();
			if (stat != NodeStat::Fail)
			{
				return stat;
			}
		}
		return NodeStat::Fail;
	}


private:
	vector<BehaviorNode*>children;
};

class Sequence : public BehaviorNode {
public:
	void addChild(BehaviorNode* child) {
		children.push_back(child);
	}
	NodeStat Status() {
		for (auto& c : children)
		{
			NodeStat stat = c->Status();
			if (stat != NodeStat::Success)
			{
				return stat;
			}
		}
		return NodeStat::Success;
	}

private:
	vector<BehaviorNode*>children;
};
