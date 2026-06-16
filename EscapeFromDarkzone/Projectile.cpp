#include"stdafx.h"

#include"Object.h"
#include"Shader.h"
#include"ResourceManager.h"
#include "Projectile.h"

void Projectile::Activate(XMFLOAT3 s, XMFLOAT3 dir, float sp, float dist)
{
	start = s;
	speed = sp;
	direction = dir;
	totalDistance = dist;
	curDistance = 0.0f;
	active = true;
	SetPosition(s.x, s.y, s.z);

	float yaw = atan2f(dir.x, dir.z);
	float yawDegree = XMConvertToDegrees(yaw);
	SetRotate(0.0f, yawDegree, 0.0f);
}

void Projectile::Animate(float fTimeElapsed)
{

	float moveDist = speed * fTimeElapsed;
	curDistance += moveDist;
	if (curDistance >= totalDistance)
	{
		//데칼 활성화
		active = false;
		return;
	}
	XMFLOAT3 currentPos = GetPosition();
	currentPos.x += direction.x * moveDist;
	currentPos.y += direction.y * moveDist;
	currentPos.z += direction.z * moveDist;

	SetPosition(currentPos.x, currentPos.y, currentPos.z);
}

void ProjectileManager::Init(CShader*s)
{
	bullets.resize(poolSize);
	shader = s;
	for (int i = 0; i < poolSize; ++i)
	{
		CGameObject* pBulletInstance = ResourceManager::Instance().GetModelPrototype(ModelName::BULLET);
		bullets[i].SetChild(pBulletInstance);
	}
}

void ProjectileManager::SpawnProjectile(ProjectileType type, XMFLOAT3 s, XMFLOAT3 dir, float  dist)
{
	for (int i = 0; i < bullets.size(); ++i)
	{
		int idx = (lastUse + i) % bullets.size();
		if (not bullets[idx].IsActive())
		{
			bullets[idx].Activate(s, dir ,100 , dist);

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
