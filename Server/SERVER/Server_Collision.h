#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>

using namespace DirectX;

constexpr XMFLOAT3 PLAYER_OOBB_CENTER = { 0.0f, 0.90f, 0.0f }; // 플레이어 OOBB의 중심 (로컬 좌표계)
constexpr XMFLOAT3 PLAYER_OOBB_EXTENTS = { 0.89f, 0.91f, 0.20f }; // 플레이어 OOBB의 Extents (로컬 좌표계)

struct ColResult {
	bool isCollide;	//충돌 여부
	XMFLOAT3 normal; //충돌 평면 노멀벡터
	XMFLOAT3 mtv; //침범 후 밀어낼 위치
	//태그
};

std::vector<ColResult> CheckCollisionWithMap(const BoundingOrientedBox& playerOOBB, 
	const std::vector<BoundingOrientedBox>& mapOOBBs);

ColResult CalcCollision(
	const BoundingOrientedBox& main, const BoundingOrientedBox& target);

BoundingOrientedBox MakePlayerOOBB(const XMFLOAT3& position, float yawRad);
