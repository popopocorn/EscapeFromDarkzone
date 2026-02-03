#pragma once
#include"stdafx.h"

class CGameObject;

struct Collision_Info {
	bool isCollide = false;
	CGameObject* other = nullptr;
	XMFLOAT3 normal = { 0, 0, 0 };
	float depth = 0.0f;
};

class Collision_Manager{

};

