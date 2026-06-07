#pragma once
#include"stdafx.h"

class CGameObject;

struct ColResult {
	bool isCollide;	//충돌 여부
	XMFLOAT3 normal; //충돌 평면 노멀벡터
	XMFLOAT3 mtv; //침범 후 밀어낼 위치
	//태그
};

class CollisionManager{
public:
	void DoCollision(
		CGameObject* main, 
		std::vector<CGameObject*>* target
	);
	void CheckCollision(
		CGameObject* main, 
		CGameObject* target);
	//void ResolveCollision(CGameObject* object);
	ColResult CalcCollision(
		const BoundingOrientedBox& main, 
		const BoundingOrientedBox& target
	);
};

