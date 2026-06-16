#pragma once
#include"Singletone.h"
enum class ProjectileType {
	RIFLE_BULLET,
};

class Projectile :public CGameObject{
private:
	XMFLOAT3 start;
	XMFLOAT3 direction;
	bool active = false;
	float speed = 100.0f;
	float totalDistance = 0.0f;
	float curDistance = 0.0f;
public:
	void Activate(XMFLOAT3 s, XMFLOAT3 dir, float speed, float dist);
	virtual void Animate(float fTimeElapsed);
	bool IsActive() { return active; }
};

class CShader;

class ProjectileManager : public Singleton<ProjectileManager>{
	friend class Singleton<ProjectileManager>;
private:
	vector<Projectile> bullets;
	//unordered_map<ProjectileType, CGameObject*>projectilePool;
	int lastUse = 0;
	const int poolSize = 200;
	CShader* shader=NULL;
public:
	void Init(CShader* s);
	void SpawnProjectile(ProjectileType type, XMFLOAT3 s, XMFLOAT3 dir, float dist);
	void Update(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch);

private:
	ProjectileManager() {}
	~ProjectileManager() {}
};