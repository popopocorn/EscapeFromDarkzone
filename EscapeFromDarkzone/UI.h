#pragma once
#include "Object.h"


struct CheckBox {
	float minX;
	float minY;
	float maxX;
	float maxY;
	bool Intersects(POINT p)
	{
		float ndcX = (2.0f * static_cast<float>(p.x)) / FRAME_BUFFER_WIDTH - 1.0f;
		float ndcY = 1.0f - (2.0f * static_cast<float>(p.y)) / FRAME_BUFFER_HEIGHT; // DirectX 기준 Y축 상하 반전
		if (ndcX < minX) return false;
		if (ndcX > maxX) return false;
		if (ndcY < minY) return false;
		if (ndcY > maxY) return false;
		return true;

	}
};


class UIObject : public CGameObject {
protected:
	std::function<void()>	Task;
	CheckBox				CollisionBox;
public:
	void HandleClick() { if (Task) Task(); }
	void SetFunc(std::function<void()> func) { Task = func; }
	void setAABB();
	CheckBox GetBox() { return CollisionBox; }
};
