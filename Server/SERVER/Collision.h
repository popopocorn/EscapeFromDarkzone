#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

struct ColResult {
	bool isCollide;	//충돌 여부
	XMFLOAT3 normal; //충돌 평면 노멀벡터
	XMFLOAT3 mtv; //침범 후 밀어낼 위치
	//태그
};

ColResult CalcCollision(
	const BoundingOrientedBox& main, const BoundingOrientedBox& target);
