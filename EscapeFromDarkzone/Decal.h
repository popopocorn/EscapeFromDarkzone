#pragma once

#include "Object.h"
#include "Singletone.h"

enum class DecalType
{
	BULLET,
	BLOOD,
};

class CShader;

class Decal : public CGameObject
{
private:
	bool active = false;
	float lifeTime = 20.0f;
	float elapsed = 0.0f;
	float size = 0.28f;

	DecalType type = DecalType::BULLET;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

private:
	void BuildTransform();

public:
	void Activate(DecalType decalType, XMFLOAT3 pos, XMFLOAT3 normal, float decalSize, float decalLifeTime);
	virtual void Animate(float fTimeElapsed) override;

	bool IsActive() const { return active; }
	void Deactivate();
};

class DecalManager : public Singleton<DecalManager>
{
	friend class Singleton<DecalManager>;

private:
	vector<unique_ptr<Decal>> bulletDecals;
	vector<unique_ptr<Decal>> bloodDecals;

	int lastBulletUse = 0;
	int lastBloodUse = 0;

	const int bulletPoolSize = 200;
	const int bloodPoolSize = 200;

	CShader* shader = nullptr;

private:
	BoundingOrientedBox* FindBestHitOOBB(CGameObject* pHitObject, const XMFLOAT3& hitPos);
	bool FindFloorBelowHit(const XMFLOAT3& hitPos, const vector<CGameObject*>* mapObjects, XMFLOAT3& outFloorPos, XMFLOAT3& outFloorNormal);

public:
	void Init(CShader* s);
	void ResetForNewRound();

	void SpawnBulletDecal(XMFLOAT3 pos, XMFLOAT3 normal);
	void SpawnBulletDecal(CGameObject* pHitObject, XMFLOAT3 hitPos);

	void SpawnBloodDecal(XMFLOAT3 pos, XMFLOAT3 normal);
	void SpawnBloodDecalOnFloor(const XMFLOAT3& hitPos, const vector<CGameObject*>* mapObjects);

	void Update(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch);

private:
	DecalManager() {}
	~DecalManager() {}
};