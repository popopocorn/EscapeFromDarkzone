#pragma once

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
public:
	//밑에는 추출한 메쉬 읽는 함수, 쓰기 위한 함수 등 
	void LoadNavMeshFromFile(const char* file);
	

};