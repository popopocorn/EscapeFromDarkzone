#pragma once

#include <vector>
#include <array>
#include <functional>
#include <DirectXMath.h>

using namespace DirectX;

struct tempNavMesh {
    int vertexCnt{};
    int polyCnt{};
    std::vector<XMFLOAT3> vertices;
    std::vector<int>      idx;
};

struct NavigationPoly
{
    int                 ID;
    std::array<XMFLOAT3, 3>  positions;
    std::array<int, 3>       vindex;
    std::array<int, 3>       neighborIDs = { -1, -1, -1 };
    XMFLOAT3            centroid    = XMFLOAT3(0.0, 0.0, 0.0);
};

class AstarNavigation
{
private:
    std::vector<NavigationPoly> mesh;

private:
    void              BuildMesh(const tempNavMesh& m);
    void              FindNeighbor();
    std::vector<XMFLOAT3>  FindPathPoint(std::vector<int> polyidx, XMFLOAT3 start, XMFLOAT3 end);

public:
    void              LoadNavMeshFromFile(const char* file);
    std::vector<XMFLOAT3>  FindPath(XMFLOAT3 start, XMFLOAT3 end);
    int               FindPolyID(const XMFLOAT3& pos);

    // 수색용: center에서 XZ 거리 [minRadius, maxRadius] 안에 있는 폴리곤 중심점 하나를
    // random01(0~1)로 골라 outPoint에 담는다. 후보가 없으면 false.
    bool              FindSearchPointAround(const XMFLOAT3& center,
                                            float minRadius, float maxRadius,
                                            float random01, XMFLOAT3& outPoint) const;
};

struct AStarNode {
    int   polyID;
    int   parentID;
    float gCost;
    float hCost;

    float GetCost() const { return gCost + hCost; }

    bool operator>(const AStarNode& other) const
    {
        return GetCost() > other.GetCost();
    }
};
