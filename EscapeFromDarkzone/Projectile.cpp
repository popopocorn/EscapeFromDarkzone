#include"Object.h"
#include"stdafx.h"
#include"Shader.h"
#include "Projectile.h"
#include"ResourceManager.h"


void Projectile::Activate(CGameObject* bullet, XMFLOAT3 s, XMFLOAT3 e, float sp, float dist)
{
	ReplaceChild(bullet);
	start = s;
	end = e;
	speed = sp;
	XMFLOAT3 dir = Vector3::Subtract(e, s);
	dir = Vector3::Normalize(dir);
	direction = dir;
	totalDistance = dist;
	curDistance = 0.0f;

	active = true;
	SetPosition(s.x, s.y, s.z);
}

void Projectile::Animate(float fTimeElapsed)
{

	float moveDist = speed * fTimeElapsed;
	curDistance += moveDist;
	if (curDistance >= totalDistance)
	{
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
	projectilePool[ProjectileType::RIFLE_BULLET]; // 여기에 리소스 매니저로 불릿 할당;
}

void ProjectileManager::SpawnProjectile(ProjectileType type, XMFLOAT3 s, XMFLOAT3 e)
{
	for (int i = 0; i < bullets.size(); ++i)
	{
		int idx = (lastUse + i) % bullets.size();
		if (not bullets[idx].IsActive())
		{
			float dist = Vector3::Distance(e, s);
			bullets[idx].Activate(projectilePool[type], s, e, 50 , dist);

			lastUse = idx+1;
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
