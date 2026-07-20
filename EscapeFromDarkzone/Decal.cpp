#include "stdafx.h"

#include "Object.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "Collision.h"
#include "Decal.h"

static float ClampFloatLocal(float v, float minValue, float maxValue)
{
	if (v < minValue) return minValue;
	if (v > maxValue) return maxValue;
	return v;
}

static XMFLOAT3 NormalizeOrDefault(const XMFLOAT3& v, const XMFLOAT3& fallback)
{
	XMVECTOR vec = XMLoadFloat3(&v);
	float lenSq = XMVectorGetX(XMVector3LengthSq(vec));

	if (lenSq < 0.000001f)
		return fallback;

	XMFLOAT3 out;
	XMStoreFloat3(&out, XMVector3Normalize(vec));
	return out;
}

static float GetHitScoreToOOBB(const BoundingOrientedBox& oobb, const XMFLOAT3& hitPos)
{
	XMVECTOR vHitPos = XMLoadFloat3(&hitPos);
	XMVECTOR vCenter = XMLoadFloat3(&oobb.Center);
	XMVECTOR vOrientation = XMLoadFloat4(&oobb.Orientation);

	XMVECTOR vInverseRot = XMQuaternionInverse(vOrientation);
	XMMATRIX matInverseRot = XMMatrixRotationQuaternion(vInverseRot);

	XMVECTOR vLocalHit = XMVector3TransformCoord(vHitPos - vCenter, matInverseRot);

	XMFLOAT3 localHit;
	XMStoreFloat3(&localHit, vLocalHit);

	float clampedX = ClampFloatLocal(localHit.x, -oobb.Extents.x, oobb.Extents.x);
	float clampedY = ClampFloatLocal(localHit.y, -oobb.Extents.y, oobb.Extents.y);
	float clampedZ = ClampFloatLocal(localHit.z, -oobb.Extents.z, oobb.Extents.z);

	float dx = localHit.x - clampedX;
	float dy = localHit.y - clampedY;
	float dz = localHit.z - clampedZ;

	float outsideDistSq = dx * dx + dy * dy + dz * dz;

	if (outsideDistSq > 0.000001f)
		return outsideDistSq;

	float faceDistX = fabsf(oobb.Extents.x - fabsf(localHit.x));
	float faceDistY = fabsf(oobb.Extents.y - fabsf(localHit.y));
	float faceDistZ = fabsf(oobb.Extents.z - fabsf(localHit.z));

	float insideFaceDist = min(faceDistX, min(faceDistY, faceDistZ));
	return insideFaceDist * 0.0001f;
}

void Decal::BuildTransform()
{
	XMFLOAT3 n = NormalizeOrDefault(normal, XMFLOAT3(0.0f, 1.0f, 0.0f));

	XMFLOAT3 worldUp = XMFLOAT3(0.0f, 1.0f, 0.0f);

	if (fabsf(n.y) > 0.95f)
	{
		worldUp = XMFLOAT3(0.0f, 0.0f, 1.0f);
	}

	XMVECTOR vNormal = XMLoadFloat3(&n);
	XMVECTOR vUpSeed = XMLoadFloat3(&worldUp);

	XMVECTOR vRight = XMVector3Cross(vUpSeed, vNormal);

	if (XMVectorGetX(XMVector3LengthSq(vRight)) < 0.000001f)
	{
		vRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		vRight = XMVector3Normalize(vRight);
	}

	XMVECTOR vUp = XMVector3Normalize(XMVector3Cross(vNormal, vRight));

	XMFLOAT3 right;
	XMFLOAT3 up;

	XMStoreFloat3(&right, vRight);
	XMStoreFloat3(&up, vUp);

	XMFLOAT3 finalPos = position;
	finalPos.x += n.x * 0.015f;
	finalPos.y += n.y * 0.015f;
	finalPos.z += n.z * 0.015f;

	m_xmf4x4ToParent = Matrix4x4::Identity();

	m_xmf4x4ToParent._11 = right.x * size;
	m_xmf4x4ToParent._12 = right.y * size;
	m_xmf4x4ToParent._13 = right.z * size;

	m_xmf4x4ToParent._21 = up.x * size;
	m_xmf4x4ToParent._22 = up.y * size;
	m_xmf4x4ToParent._23 = up.z * size;

	m_xmf4x4ToParent._31 = n.x;
	m_xmf4x4ToParent._32 = n.y;
	m_xmf4x4ToParent._33 = n.z;

	m_xmf4x4ToParent._41 = finalPos.x;
	m_xmf4x4ToParent._42 = finalPos.y;
	m_xmf4x4ToParent._43 = finalPos.z;

	UpdateTransform(NULL);
}

void Decal::Activate(DecalType decalType, XMFLOAT3 pos, XMFLOAT3 n, float decalSize, float decalLifeTime)
{
	type = decalType;

	position = pos;
	normal = NormalizeOrDefault(n, XMFLOAT3(0.0f, 1.0f, 0.0f));
	size = decalSize;
	lifeTime = decalLifeTime;
	elapsed = 0.0f;
	active = true;

	BuildTransform();
}

void Decal::Animate(float fTimeElapsed)
{
	if (!active)
		return;

	elapsed += fTimeElapsed;

	if (elapsed >= lifeTime)
	{
		active = false;
		return;
	}
}

void Decal::Deactivate()
{
	active = false;

	lifeTime = 20.0f;
	elapsed = 0.0f;
	size = 0.28f;

	type = DecalType::BULLET;

	position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

	m_xmf4x4ToParent = Matrix4x4::Identity();
	UpdateTransform(NULL);
}

BoundingOrientedBox* DecalManager::FindBestHitOOBB(CGameObject* pHitObject, const XMFLOAT3& hitPos)
{
	if (!pHitObject)
		return nullptr;

	const auto& oobbs = pHitObject->GetOOBB();

	BoundingOrientedBox* pBestOOBB = nullptr;
	float bestScore = 999999999.0f;

	for (BoundingOrientedBox* pOOBB : oobbs)
	{
		if (!pOOBB)
			continue;

		float score = GetHitScoreToOOBB(*pOOBB, hitPos);

		if (score < bestScore)
		{
			bestScore = score;
			pBestOOBB = pOOBB;
		}
	}

	return pBestOOBB;
}

bool DecalManager::FindFloorBelowHit(const XMFLOAT3& hitPos, const vector<CGameObject*>* mapObjects, XMFLOAT3& outFloorPos, XMFLOAT3& outFloorNormal)
{
	if (!mapObjects)
		return false;

	XMFLOAT3 rayStart = hitPos;
	rayStart.y += 1.0f;

	XMVECTOR rayOrigin = XMLoadFloat3(&rayStart);
	XMVECTOR rayDir = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);

	constexpr float MAX_DOWN_DISTANCE = 8.0f;

	bool bHit = false;
	float bestDist = MAX_DOWN_DISTANCE;
	BoundingOrientedBox* pBestOOBB = nullptr;

	for (CGameObject* pObject : *mapObjects)
	{
		if (!pObject) continue;
		if (!pObject->isColl) continue;

		const auto& oobbs = pObject->GetOOBB();

		for (BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			float fDist = 0.0f;

			if (pOOBB->Intersects(rayOrigin, rayDir, fDist))
			{
				if (fDist >= 0.0f && fDist < bestDist)
				{
					bHit = true;
					bestDist = fDist;
					pBestOOBB = pOOBB;
				}
			}
		}
	}

	if (!bHit || !pBestOOBB)
		return false;

	outFloorPos = rayStart;
	outFloorPos.y -= bestDist;

	CollisionManager collisionManager;
	outFloorNormal = collisionManager.GetOOBBHitNormal(*pBestOOBB, outFloorPos);
	outFloorNormal = NormalizeOrDefault(outFloorNormal, XMFLOAT3(0.0f, 1.0f, 0.0f));

	if (outFloorNormal.y < 0.35f)
	{
		outFloorNormal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	}

	return true;
}

void DecalManager::Init(CShader* s)
{
	bulletDecals.clear();
	bloodDecals.clear();

	bulletDecals.resize(bulletPoolSize);
	bloodDecals.resize(bloodPoolSize);

	lastBulletUse = 0;
	lastBloodUse = 0;

	shader = s;

	CGameObject* pBulletPrototype = ResourceManager::Instance().GetModelPrototype(ModelName::BULLET_DECAL);
	CGameObject* pBloodPrototype = ResourceManager::Instance().GetModelPrototype(ModelName::BLOOD_DECAL);

	for (int i = 0; i < bulletPoolSize; ++i)
	{
		if (pBulletPrototype)
		{
			CGameObject* pDecalInstance = CGameObject::CreateModelInstance(pBulletPrototype);

			if (pDecalInstance)
			{
				bulletDecals[i].SetChild(pDecalInstance);
			}
		}

		bulletDecals[i].Deactivate();
	}

	for (int i = 0; i < bloodPoolSize; ++i)
	{
		if (pBloodPrototype)
		{
			CGameObject* pDecalInstance = CGameObject::CreateModelInstance(pBloodPrototype);

			if (pDecalInstance)
			{
				bloodDecals[i].SetChild(pDecalInstance);
			}
		}

		bloodDecals[i].Deactivate();
	}
}

void DecalManager::ResetForNewRound()
{
	for (auto& decal : bulletDecals)
	{
		decal.Deactivate();
	}

	for (auto& decal : bloodDecals)
	{
		decal.Deactivate();
	}

	lastBulletUse = 0;
	lastBloodUse = 0;

	OutputDebugString(L"[RoundReset] DecalManager 초기화 완료\n");
}

void DecalManager::SpawnBulletDecal(XMFLOAT3 pos, XMFLOAT3 normal)
{
	constexpr float DEFAULT_BULLET_DECAL_SIZE = 0.28f;
	constexpr float DEFAULT_BULLET_DECAL_LIFETIME = 25.0f;

	for (int i = 0; i < bulletDecals.size(); ++i)
	{
		int idx = (lastBulletUse + i) % bulletDecals.size();

		if (!bulletDecals[idx].IsActive())
		{
			bulletDecals[idx].Activate(DecalType::BULLET, pos, normal, DEFAULT_BULLET_DECAL_SIZE, DEFAULT_BULLET_DECAL_LIFETIME);
			lastBulletUse = (idx + 1) % bulletDecals.size();
			return;
		}
	}
}

void DecalManager::SpawnBulletDecal(CGameObject* pHitObject, XMFLOAT3 hitPos)
{
	if (!pHitObject)
		return;

	BoundingOrientedBox* pHitOOBB = FindBestHitOOBB(pHitObject, hitPos);

	if (!pHitOOBB)
		return;

	CollisionManager collisionManager;
	XMFLOAT3 hitNormal = collisionManager.GetOOBBHitNormal(*pHitOOBB, hitPos);

	SpawnBulletDecal(hitPos, hitNormal);
}

void DecalManager::SpawnBloodDecal(XMFLOAT3 pos, XMFLOAT3 normal)
{
	constexpr float DEFAULT_BLOOD_DECAL_SIZE = 1.5f;
	constexpr float DEFAULT_BLOOD_DECAL_LIFETIME = 120.0f;

	for (int i = 0; i < bloodDecals.size(); ++i)
	{
		int idx = (lastBloodUse + i) % bloodDecals.size();

		if (!bloodDecals[idx].IsActive())
		{
			bloodDecals[idx].Activate(DecalType::BLOOD, pos, normal, DEFAULT_BLOOD_DECAL_SIZE, DEFAULT_BLOOD_DECAL_LIFETIME);
			lastBloodUse = (idx + 1) % bloodDecals.size();
			return;
		}
	}
}

void DecalManager::SpawnBloodDecalOnFloor(const XMFLOAT3& hitPos, const vector<CGameObject*>* mapObjects)
{
	XMFLOAT3 floorPos;
	XMFLOAT3 floorNormal;

	if (!FindFloorBelowHit(hitPos, mapObjects, floorPos, floorNormal))
		return;

	SpawnBloodDecal(floorPos, floorNormal);
}

void DecalManager::Update(float fTimeElapsed)
{
	for (auto& decal : bulletDecals)
	{
		if (decal.IsActive())
		{
			decal.Animate(fTimeElapsed);
		}
	}

	for (auto& decal : bloodDecals)
	{
		if (decal.IsActive())
		{
			decal.Animate(fTimeElapsed);
		}
	}
}

void DecalManager::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch)
{
	if (!shader)
		return;

	shader->OnPrepareRender(pd3dCommandList, 0);

	for (auto& decal : bulletDecals)
	{
		if (decal.IsActive())
		{
			decal.Render(pd3dCommandList, batch, 0, pCamera);
		}
	}

	for (auto& decal : bloodDecals)
	{
		if (decal.IsActive())
		{
			decal.Render(pd3dCommandList, batch, 0, pCamera);
		}
	}
}