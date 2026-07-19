#include"stdafx.h"

#include"Object.h"
#include"Shader.h"
#include"ResourceManager.h"
#include "Projectile.h"
#include "Decal.h"

static XMFLOAT3 NormalizeOrDefaultProjectile(XMFLOAT3 v, XMFLOAT3 fallback)
{
	XMVECTOR vec = XMLoadFloat3(&v);
	float lenSq = XMVectorGetX(XMVector3LengthSq(vec));

	if (lenSq < 0.000001f)
		return fallback;

	XMFLOAT3 out;
	XMStoreFloat3(&out, XMVector3Normalize(vec));
	return out;
}

void Projectile::Activate(XMFLOAT3 s, XMFLOAT3 dir, float sp, float dist)
{
	DecalInfo emptyInfo;
	Activate(s, dir, sp, dist, emptyInfo);
}
void Projectile::Activate(XMFLOAT3 s, XMFLOAT3 dir, float sp, float dist, const DecalInfo& info)
{
	start = s;
	speed = sp;
	direction = NormalizeOrDefaultProjectile(dir, XMFLOAT3(0.0f, 0.0f, 1.0f));
	totalDistance = (dist < 0.0f) ? 0.0f : dist;
	curDistance = 0.0f;
	decalInfo = info;
	active = true;

	SetPosition(s.x, s.y, s.z);

	float yaw = atan2f(direction.x, direction.z);
	float yawDegree = XMConvertToDegrees(yaw);
	SetRotate(0.0f, yawDegree, 0.0f);
}

void Projectile::Animate(float fTimeElapsed)
{
	if (!active)
		return;

	float moveDist = speed * fTimeElapsed;
	curDistance += moveDist;

	if (curDistance >= totalDistance)
	{
		XMFLOAT3 hitPos;
		hitPos.x = start.x + direction.x * totalDistance;
		hitPos.y = start.y + direction.y * totalDistance;
		hitPos.z = start.z + direction.z * totalDistance;

		XMVECTOR vNormal = XMLoadFloat3(&decalInfo.normal);
		float normalLenSq = XMVectorGetX(XMVector3LengthSq(vNormal));

		if (decalInfo.enable && decalInfo.decalType == 1 && normalLenSq >= 0.000001f)
		{
			XMFLOAT3 hitNormal;
			XMStoreFloat3(&hitNormal, XMVector3Normalize(vNormal));
			DecalManager::Instance()->SpawnBulletDecal(hitPos, hitNormal);
		}

		active = false;
		return;
	}

	XMFLOAT3 currentPos;
	currentPos.x = start.x + direction.x * curDistance;
	currentPos.y = start.y + direction.y * curDistance;
	currentPos.z = start.z + direction.z * curDistance;

	SetPosition(currentPos.x, currentPos.y, currentPos.z);
}
void ProjectileManager::Init(CShader*s)
{
	bullets.resize(poolSize);
	shader = s;
	for (int i = 0; i < poolSize; ++i)
	{
		CGameObject* pBulletInstance = ResourceManager::Instance().GetModelInstance(ModelName::BULLET);
		bullets[i].SetChild(pBulletInstance);
	}
}

void ProjectileManager::SpawnProjectile(ProjectileType type, XMFLOAT3 s, XMFLOAT3 dir, float dist)
{
	DecalInfo emptyInfo;
	SpawnProjectile(type, s, dir, dist, emptyInfo);
}

void ProjectileManager::SpawnProjectile(ProjectileType type, XMFLOAT3 s, XMFLOAT3 dir, float dist, const DecalInfo& info)
{
	UNREFERENCED_PARAMETER(type);

	for (int i = 0; i < bullets.size(); ++i)
	{
		int idx = (lastUse + i) % bullets.size();

		if (!bullets[idx].IsActive())
		{
			bullets[idx].Activate(s, dir, 100.0f, dist, info);
			lastUse = (idx + 1) % bullets.size();
			break;
		}
	}
}

void ProjectileManager::Update(float fTimeElapsed)
{
	for (auto& b : bullets)
	{
		if (b.IsActive())
		{
			b.Animate(fTimeElapsed);
		}
	}
}

void ProjectileManager::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch)
{
	shader->OnPrepareRender(pd3dCommandList, 0);
	for (auto& b : bullets)
	{
		if (b.IsActive())
		{
			b.Render(pd3dCommandList, batch, 0, pCamera);
		}
	}
}
