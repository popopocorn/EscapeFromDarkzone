#pragma once
#include"stdafx.h"

class CGameObject;

struct ColResult {
	bool isCollide;
	XMFLOAT3 normal;
	XMFLOAT3 mtv; //pushvector
};

class CollisionManager{
public:
	void DoCollision(
		CGameObject* main, 
		std::vector<std::unique_ptr<CGameObject>>* target
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

