#include"stdafx.h"
#include "AI.h"
#include<fstream>


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

		// 이미 등록된 고유 정점들 중에 지금 정점과 위치가 같은게 있는지 검사
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
			// 같은 위치의 정점이 이미 존재한다면, 기존 정점 번호로 덮어씌움 (공유)
			indexRemap[i] = foundIndex;
		}
		else
		{
			// 처음 보는 위치의 정점이라면 새 번호를 부여하고 목록에 추가
			indexRemap[i] = uniqueVertices.size();
			uniqueVertices.push_back(temp.vertices[i]);
		}
	}

	// 압축된 고유 정점 배열로 교체
	temp.vertices = uniqueVertices;
	temp.vertexCnt = uniqueVertices.size();

	// 모든 인덱스 버퍼를 새로 매핑된 번호로 업데이트
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
	if (polyidx.empty())return waypoints;
	if (polyidx.size() == 1)
	{
		waypoints.push_back(end);
		return waypoints;
	}

	vector<Portal> pts;

	pts.push_back({ start, start });

	for (int i = 0; i < polyidx.size() - 1; ++i)
	{
		int curID = polyidx[i];
		int nextID = polyidx[i + 1];

		vector<XMFLOAT3>shared;
		for (int j = 0; j < 3; ++j)
		{
			for (int k = 0; k < 3; ++k)
			{
				if (mesh[curID].vindex[j] == mesh[nextID].vindex[k])
				{
					shared.push_back(mesh[curID].positions[j]);
				}
			}
		}

		if (shared.size() == 2)
		{
			XMFLOAT3 p1 = shared[0];
			XMFLOAT3 p2 = shared[1];
			
			if (TriArea2D(mesh[curID].centroid, mesh[nextID].centroid, p1) < 0.0f)
			{
				pts.push_back({ p1,p2 });
			}
			else
			{
				pts.push_back({ p2,p1 });
			}

		}

	}
	pts.push_back({ end, end });
	


	waypoints.push_back(start);
	XMFLOAT3 Apex = pts[0].left;
	XMFLOAT3 Left = pts[0].left;
	XMFLOAT3 Right = pts[0].right;

	int AIdx = 0;
	int LIdx = 0;
	int RIdx = 0;

	for (int i = 1; i < pts.size(); ++i)
	{
		XMFLOAT3 l = pts[i].left;
		XMFLOAT3 r = pts[i].right;


		if (TriArea2D(Apex, Right, r) <= 0.0f)
		{
			if (IsSamePosition(Apex, Right) || TriArea2D(Apex, Left, r) > 0.0f)
			{
				Right = r;
				RIdx = i;
			}
			else
			{
				waypoints.push_back(Left);
				Apex = Left;
				AIdx = LIdx;

				Left = Apex;
				Right = Apex;

				i = AIdx;
				continue;

			}
		}
		if (TriArea2D(Apex, Left, l) <= 0.0f)
		{
			if (IsSamePosition(Apex, Left) || TriArea2D(Apex, Right, l) < 0.0f)
			{
				Left = l;
				LIdx = i;
			}
			else
			{
				waypoints.push_back(Right);
				Apex = Right;
				AIdx = RIdx;

				Left = Apex;
				Right = Apex;

				i = AIdx;
				continue;

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

	wchar_t szDebugMsg[256];


	swprintf_s(szDebugMsg, L"p1:%d p2: %d\n", startID, endID);

	OutputDebugString(szDebugMsg);


	if (startID == -1 || endID == -1)
		return vector<XMFLOAT3>();

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
		wchar_t szDebugMsg[256];

		swprintf_s(szDebugMsg, L"p1:%d p2: %d\n", startID, endID);

		OutputDebugString(szDebugMsg);
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
