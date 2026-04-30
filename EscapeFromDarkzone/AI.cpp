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

}
