#include "Server_AI.h"

#include <fstream>
#include <map>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cmath>
#include <iostream>

bool IsSamePosition(const XMFLOAT3& p1, const XMFLOAT3& p2)
{
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
    std::map<std::pair<int, int>, std::vector<int>> edgeMap;

    for (int i = 0; i < mesh.size(); ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            int v1 = mesh[i].vindex[j];
            int v2 = mesh[i].vindex[(j + 1) % 3];

            int minV = std::min(v1, v2);
            int maxV = std::max(v1, v2);

            edgeMap[{minV, maxV}].push_back(i);
        }
    }

    for (const auto& pair : edgeMap)
    {
        const std::vector<int>& sharedPolys = pair.second;

        if (sharedPolys.size() == 2)
        {
            int polyA = sharedPolys[0];
            int polyB = sharedPolys[1];

            for (int k = 0; k < 3; ++k) {
                if (mesh[polyA].neighborIDs[k] == -1) {
                    mesh[polyA].neighborIDs[k] = polyB;
                    break;
                }
            }

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
    std::ifstream is(file, std::ios::binary);

    tempNavMesh temp;

    is.read(reinterpret_cast<char*>(&temp.vertexCnt), sizeof(int));
    is.read(reinterpret_cast<char*>(&temp.polyCnt), sizeof(int));
    temp.vertices.resize(temp.vertexCnt);
    is.read(reinterpret_cast<char*>(temp.vertices.data()), sizeof(XMFLOAT3) * temp.vertexCnt);
    temp.idx.resize(temp.polyCnt * 3);
    is.read(reinterpret_cast<char*>(temp.idx.data()), sizeof(int) * temp.polyCnt * 3);

    is.close();

    std::vector<XMFLOAT3> uniqueVertices;
    std::vector<int> indexRemap(temp.vertexCnt);

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

struct Portal
{
    XMFLOAT3 left;
    XMFLOAT3 right;
};

float TriArea2D(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
{
    return (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
}

std::vector<XMFLOAT3> AstarNavigation::FindPathPoint(std::vector<int> polyidx, XMFLOAT3 start, XMFLOAT3 end)
{
    std::vector<XMFLOAT3> waypoints;
    if (polyidx.empty())return waypoints;
    if (polyidx.size() == 1)
    {
        waypoints.push_back(end);
        return waypoints;
    }

    std::vector<Portal> pts;

    pts.push_back({ start, start });

    for (int i = 0; i < polyidx.size() - 1; ++i)
    {
        int curID = polyidx[i];
        int nextID = polyidx[i + 1];

        std::vector<XMFLOAT3> shared;
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


std::vector<XMFLOAT3> AstarNavigation::FindPath(XMFLOAT3 start, XMFLOAT3 end)
{
    std::vector<int> idx;
    int startID = FindPolyID(start);
    int endID = FindPolyID(end);

    if (startID == -1 || endID == -1)
        return std::vector<XMFLOAT3>();

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openList;

    std::unordered_map<int, AStarNode> nodeTracker;
    std::unordered_map<int, bool> closedList;

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
        std::cout << "[AI] FindPath failed: startID=" << startID
                  << " endID=" << endID << "\n";
        return std::vector<XMFLOAT3>();
    }


    return FindPathPoint(idx, start, end);
}

bool IsPointInTriangle(const XMFLOAT3& pt, const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
    auto Sign = [](const XMFLOAT3& p1, const XMFLOAT3& p2, const XMFLOAT3& p3) {
        return (p1.x - p3.x) * (p2.z - p3.z) - (p2.x - p3.x) * (p1.z - p3.z);
        };

    float d1 = Sign(pt, v0, v1);
    float d2 = Sign(pt, v1, v2);
    float d3 = Sign(pt, v2, v0);

    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

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

bool AstarNavigation::FindSearchPointAround(const XMFLOAT3& center,
    float minRadius, float maxRadius, float random01, XMFLOAT3& outPoint) const
{
    std::vector<int> candidates;
    candidates.reserve(mesh.size());

    const float minRadiusSq = minRadius * minRadius;
    const float maxRadiusSq = maxRadius * maxRadius;

    for (int i = 0; i < static_cast<int>(mesh.size()); ++i)
    {
        const XMFLOAT3& point = mesh[i].centroid;

        const float dx = point.x - center.x;
        const float dz = point.z - center.z;
        const float distanceSq = dx * dx + dz * dz;

        if (distanceSq < minRadiusSq) continue;
        if (distanceSq > maxRadiusSq) continue;

        candidates.push_back(i);
    }

    if (candidates.empty())
        return false;

    if (random01 < 0.0f)   random01 = 0.0f;
    if (random01 >= 1.0f)  random01 = 0.999999f;

    size_t selectedIndex = static_cast<size_t>(random01 * static_cast<float>(candidates.size()));
    if (selectedIndex >= candidates.size())
        selectedIndex = candidates.size() - 1;

    outPoint = mesh[candidates[selectedIndex]].centroid;
    return true;
}

bool AstarNavigation::FindNearestPointOnMesh(const XMFLOAT3& pos, XMFLOAT3& outPoint) const
{
    if (mesh.empty())
        return false;

    int   bestIndex = -1;
    float bestSq    = 0.0f;

    for (int i = 0; i < static_cast<int>(mesh.size()); ++i)
    {
        const XMFLOAT3& c = mesh[i].centroid;

        const float dx = c.x - pos.x;
        const float dz = c.z - pos.z;
        const float dsq = dx * dx + dz * dz;

        if (bestIndex < 0 || dsq < bestSq)
        {
            bestIndex = i;
            bestSq    = dsq;
        }
    }

    if (bestIndex < 0)
        return false;

    outPoint = mesh[bestIndex].centroid;
    return true;
}

void AstarNavigation::DumpNavMeshDiagnostics() const
{
    const int n = static_cast<int>(mesh.size());

    std::cout << "[NAV] polygons = " << n << "\n";
    if (n == 0) return;

    int degCount[4] = { 0, 0, 0, 0 };
    for (const auto& p : mesh)
    {
        int d = 0;
        for (int k = 0; k < 3; ++k)
            if (p.neighborIDs[k] >= 0) ++d;
        ++degCount[d];
    }
    std::cout << "[NAV] 이웃 0개=" << degCount[0]
        << "  1개=" << degCount[1]
        << "  2개=" << degCount[2]
        << "  3개=" << degCount[3] << "\n";

    std::vector<int> comp(n, -1);
    std::vector<int> sizes;
    std::vector<int> stack;

    for (int i = 0; i < n; ++i)
    {
        if (comp[i] != -1) continue;

        const int cid = static_cast<int>(sizes.size());
        int cnt = 0;

        stack.clear();
        stack.push_back(i);
        comp[i] = cid;

        while (!stack.empty())
        {
            const int cur = stack.back();
            stack.pop_back();
            ++cnt;

            for (int k = 0; k < 3; ++k)
            {
                const int nb = mesh[cur].neighborIDs[k];
                if (nb >= 0 && nb < n && comp[nb] == -1)
                {
                    comp[nb] = cid;
                    stack.push_back(nb);
                }
            }
        }
        sizes.push_back(cnt);
    }

    std::vector<int> sorted = sizes;
    std::sort(sorted.rbegin(), sorted.rend());

    std::cout << "[NAV] 연결 성분 = " << sizes.size() << "개,  상위 10: ";
    for (int i = 0; i < static_cast<int>(sorted.size()) && i < 10; ++i)
        std::cout << sorted[i] << " ";
    std::cout << "\n";

    std::map<std::pair<int, int>, int> edgeCount;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const int v1 = mesh[i].vindex[j];
            const int v2 = mesh[i].vindex[(j + 1) % 3];
            ++edgeCount[{ std::min(v1, v2), std::max(v1, v2) }];
        }
    }

    int e1 = 0, e2 = 0, e3 = 0;
    for (const auto& e : edgeCount)
    {
        if (e.second == 1)      ++e1;
        else if (e.second == 2) ++e2;
        else                    ++e3;
    }

    std::cout << "[NAV] 엣지: 1폴리=" << e1 << " (경계)"
        << "  2폴리=" << e2 << " (정상 연결)"
        << "  3폴리이상=" << e3 << " (FindNeighbor가 버림)\n";
}
