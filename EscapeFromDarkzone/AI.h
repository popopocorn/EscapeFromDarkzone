#pragma once

struct tempNav {

};

struct NavigationPoly
{
	int					ID;
	array<XMFLOAT3, 3>	positions;
	array<int, 3>		vindex;
	vector<int>			neighborIDs;
	XMFLOAT3			centroid;

};

class AstarNavigation
{
private:
	vector<NavigationPoly> mesh;

public:
	//밑에는 추출한 메쉬 읽는 함수, 쓰기 위한 함수 등 
	void LoadNavMeshFromFile(const char* file);

};