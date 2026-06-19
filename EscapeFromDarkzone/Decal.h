#pragma once

#include "Object.h"
#include "Singletone.h"

enum class DecalType
{
	BULLET,
};

class CShader;

class Decal : public CGameObject
{
private:
	bool active = false;
	float lifeTime = 20.0f;
	float elapsed = 0.0f;
	float size = 0.28f;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

private:
	void BuildTransform();

public:
	void Activate(DecalType type, XMFLOAT3 pos, XMFLOAT3 normal, float decalSize, float decalLifeTime);
	virtual void Animate(float fTimeElapsed) override;

	bool IsActive() const { return active; }
	void Deactivate() { active = false; }
};

class DecalManager : public Singleton<DecalManager>
{
	friend class Singleton<DecalManager>;

private:
	vector<Decal> decals;
	int lastUse = 0;
	const int poolSize = 200;
	CShader* shader = nullptr;

private:
	BoundingOrientedBox* FindBestHitOOBB(CGameObject* pHitObject, const XMFLOAT3& hitPos);

public:
	void Init(CShader* s);

	void SpawnBulletDecal(XMFLOAT3 pos, XMFLOAT3 normal);
	void SpawnBulletDecal(CGameObject* pHitObject, XMFLOAT3 hitPos);

	void Update(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch);

private:
	DecalManager() {}
	~DecalManager() {}
};