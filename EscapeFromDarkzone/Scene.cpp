//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "Player.h"
#include "EnemyObject.h"
#include "Shader.h"
#include"ShaderManager.h"
#include "InputManager.h"
#include "ShadowMap.h"
#include "EffectShader.h"
#include "Collision.h"

#include "Item.h"
#include "AI.h"
#include "EffectManager.h"
#include "InventoryManager.h"
#include "ResourceManager.h"
#include "GameFramework.h"

#include "Network.h"
#include "NetSession.h"

static void GatherVisionBlockersFromShader(CShader* pShader, std::vector<CGameObject*>& outBlockers)
{
	if (!pShader) return;

	auto* objs = pShader->GetObj();
	if (!objs) return;

	for (auto& obj : *objs)
	{
		if (!obj) continue;
		outBlockers.push_back(obj);
	}
}
static void GatherVisionBlockersNearPlayer(CShader* pShader, const XMFLOAT3& playerPos, float maxDistance, std::vector<CGameObject*>& outBlockers)
{
	if (!pShader) return;

	auto* objs = pShader->GetObj();
	if (!objs) return;

	const float maxDistSq = maxDistance * maxDistance;

	for (auto& obj : *objs)
	{
		if (!obj) continue;

		XMFLOAT3 objPos = obj->GetPosition();
		float dx = objPos.x - playerPos.x;
		float dz = objPos.z - playerPos.z;
		float distSq = dx * dx + dz * dz;

		if (distSq <= maxDistSq)
		{
			outBlockers.push_back(obj);
		}
	}
}
static bool IntersectsVisionQueryRect2D(BoundingOrientedBox* pOOBB, const XMFLOAT3& playerPos, float halfExtent)
{
	if (!pOOBB) return false;

	const XMFLOAT3& c = pOOBB->Center;

	float radiusXZ = sqrtf(
		pOOBB->Extents.x * pOOBB->Extents.x +
		pOOBB->Extents.z * pOOBB->Extents.z
	);

	if (fabsf(c.x - playerPos.x) > (halfExtent + radiusXZ)) return false;
	if (fabsf(c.z - playerPos.z) > (halfExtent + radiusXZ)) return false;

	return true;
}
static void GatherVisionMapBlockersInRectFromList(const std::vector<CGameObject*>& mapChunks, const XMFLOAT3& playerPos, float halfExtent, std::vector<CGameObject*>& outBlockers)
{
	for (CGameObject* pObj : mapChunks)
	{
		if (!pObj) continue;

		bool intersects = false;

		const auto& oobbs = pObj->GetOOBB();
		for (BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			if (IntersectsVisionQueryRect2D(pOOBB, playerPos, halfExtent))
			{
				intersects = true;
				break;
			}
		}

		if (intersects)
		{
			outBlockers.push_back(pObj);
		}
	}
}
static bool GetCurrentPlayerMuzzleInfo(CPlayer* pPlayer,XMFLOAT3& outPos,XMFLOAT3& outLook,XMFLOAT3& outRight,XMFLOAT3& outUp)
{
	if (!pPlayer)
		return false;

	outLook = pPlayer->GetLookVector();
	outRight = pPlayer->GetRightVector();
	outUp = pPlayer->GetUpVector();

	if (Vector3::Length(outLook) < 0.0001f)
		outLook = XMFLOAT3(0.0f, 0.0f, 1.0f);

	if (Vector3::Length(outRight) < 0.0001f)
		outRight = XMFLOAT3(1.0f, 0.0f, 0.0f);

	if (Vector3::Length(outUp) < 0.0001f)
		outUp = XMFLOAT3(0.0f, 1.0f, 0.0f);

	outLook = Vector3::Normalize(outLook);
	outRight = Vector3::Normalize(outRight);
	outUp = Vector3::Normalize(outUp);

	CGameObject* pMuzzle = pPlayer->GetWeaponMuzzleSocket();

	if (pMuzzle)
	{
		pPlayer->UpdateTransform(NULL);

		outPos = pMuzzle->GetPosition();

		outPos.x += outLook.x * 0.05f;
		outPos.y += outLook.y * 0.05f;
		outPos.z += outLook.z * 0.05f;

		return true;
	}

	// Socket_Muzzle이 없을 때 fallback
	outPos = pPlayer->GetPosition();

	outPos.x += outLook.x * 0.8f + outRight.x * 0.25f + outUp.x * 1.2f;
	outPos.y += outLook.y * 0.8f + outRight.y * 0.25f + outUp.y * 1.2f;
	outPos.z += outLook.z * 0.8f + outRight.z * 0.25f + outUp.z * 1.2f;

	return false;
}
static EFFECT_TYPE GetSparkEffectTypeByWeapon(PlayerWeaponType weaponType)
{
	switch (weaponType)
	{
	case PlayerWeaponType::Shotgun:
		return EFFECT_SPARK_SHOTGUN;

	case PlayerWeaponType::Pistol:
		return EFFECT_SPARK_PISTOL;

	case PlayerWeaponType::Rifle:
	case PlayerWeaponType::SMG:
	default:
		return EFFECT_SPARK_RIFLE_SMG;
	}
}
static EFFECT_TYPE GetSparkEffectTypeByEnemyWeapon(EnemyWeaponType weaponType)
{
	switch (weaponType)
	{
	case EnemyWeaponType::Pistol:
		return EFFECT_SPARK_PISTOL;

	case EnemyWeaponType::SMG:
		return EFFECT_SPARK_RIFLE_SMG;

	case EnemyWeaponType::Rifle:
	default:
		return EFFECT_SPARK_RIFLE_SMG;
	}
}
static XMFLOAT3 GetSparkPositionByWeapon(PlayerWeaponType weaponType, const XMFLOAT3& muzzlePos, const XMFLOAT3& muzzleLook, const XMFLOAT3& muzzleUp)
{
	XMFLOAT3 sparkPos = muzzlePos;

	float forwardOffset = 0.0f;
	float upOffset = 0.0f;

	switch (weaponType)
	{
	case PlayerWeaponType::Rifle:
		forwardOffset = 0.05f;
		upOffset = -0.50f;
		break;

	case PlayerWeaponType::SMG:
		forwardOffset = -0.1f;
		upOffset = -0.50f;
		break;

	case PlayerWeaponType::Shotgun:
		forwardOffset = 0.1f;
		upOffset = 0.50f;
		break;

	case PlayerWeaponType::Pistol:
		forwardOffset = 0.18f;
		upOffset = -0.30f;
		break;

	default:
		forwardOffset = 0.0f;
		upOffset = 0.0f;
		break;
	}

	sparkPos.x += muzzleLook.x * forwardOffset;
	sparkPos.y += muzzleLook.y * forwardOffset;
	sparkPos.z += muzzleLook.z * forwardOffset;

	sparkPos.x += muzzleUp.x * upOffset;
	sparkPos.y += muzzleUp.y * upOffset;
	sparkPos.z += muzzleUp.z * upOffset;

	return sparkPos;
}
static bool GetCurrentEnemyMuzzleInfo(CEnemyObject* pEnemy, XMFLOAT3& outPos, XMFLOAT3& outLook, XMFLOAT3& outRight, XMFLOAT3& outUp)
{
	if (!pEnemy)
		return false;

	pEnemy->UpdateTransform(NULL);

	outLook = pEnemy->GetForwardXZ();
	outRight = pEnemy->GetRight();
	outUp = pEnemy->GetUp();

	if (Vector3::Length(outLook) < 0.0001f)
	{
		outLook = pEnemy->GetLook();
		outLook.y = 0.0f;
	}

	if (Vector3::Length(outLook) < 0.0001f)
		outLook = XMFLOAT3(0.0f, 0.0f, 1.0f);

	if (Vector3::Length(outRight) < 0.0001f)
		outRight = XMFLOAT3(1.0f, 0.0f, 0.0f);

	if (Vector3::Length(outUp) < 0.0001f)
		outUp = XMFLOAT3(0.0f, 1.0f, 0.0f);

	outLook = Vector3::Normalize(outLook);
	outRight = Vector3::Normalize(outRight);
	outUp = Vector3::Normalize(outUp);

	CGameObject* pMuzzle = pEnemy->GetWeaponMuzzleSocket();

	if (pMuzzle)
	{
		outPos = pMuzzle->GetPosition();

		outPos.x += outLook.x * 0.05f;
		outPos.y += outLook.y * 0.05f;
		outPos.z += outLook.z * 0.05f;

		return true;
	}

	outPos = pEnemy->GetPosition();

	outPos.x += outLook.x * 0.8f + outRight.x * 0.25f + outUp.x * 1.2f;
	outPos.y += outLook.y * 0.8f + outRight.y * 0.25f + outUp.y * 1.2f;
	outPos.z += outLook.z * 0.8f + outRight.z * 0.25f + outUp.z * 1.2f;

	return false;
}
static XMFLOAT3 GetSparkPositionByEnemyWeapon(EnemyWeaponType weaponType, const XMFLOAT3& muzzlePos, const XMFLOAT3& muzzleLook, const XMFLOAT3& muzzleUp)
{
	XMFLOAT3 sparkPos = muzzlePos;

	float forwardOffset = 0.0f;
	float upOffset = 0.0f;

	switch (weaponType)
	{
	case EnemyWeaponType::Pistol:
		forwardOffset = 0.1f;
		upOffset = -0.30f;
		break;

	case EnemyWeaponType::SMG:
		forwardOffset = -0.08f;
		upOffset = -0.50f;
		break;

	case EnemyWeaponType::Rifle:
	default:
		forwardOffset = -0.1f;
		upOffset = -0.50f;
		break;
	}

	sparkPos.x += muzzleLook.x * forwardOffset;
	sparkPos.y += muzzleLook.y * forwardOffset;
	sparkPos.z += muzzleLook.z * forwardOffset;

	sparkPos.x += muzzleUp.x * upOffset;
	sparkPos.y += muzzleUp.y * upOffset;
	sparkPos.z += muzzleUp.z * upOffset;

	return sparkPos;
}

CScene::CScene(CGameFramework* game)
{
	

	m_pPlayer = nullptr;

	m_pd3dGraphicsRootSignature = nullptr;
	m_pd3dcbLights = nullptr;
	m_pcbMappedLights = nullptr;

	m_pSkyBox = nullptr;
	
	UIShader = make_unique<UIObjectShader>();

	m_nLights = 0;
	m_fElapsedTime = 0.0f;
	frame = game;
	ShadowCameraManager = new LightCameraManager();
	uiManager = make_unique<HUDManager>();
	shadermanager = game->GetShaderManager();
}

CScene::~CScene()
{
}


void CScene::BuildDefaultLightsAndMaterials()
{
	m_xmf4GlobalAmbient = XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);

	LIGHT directional;
	directional.m_bEnable = true;
	directional.m_nType = DIRECTIONAL_LIGHT;
	directional.m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	directional.m_xmf4Diffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	directional.m_xmf4Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 0.0f);
	directional.m_xmf3Direction = XMFLOAT3(1.0f, -1.0f, 0.0f);
	m_pLights.push_back(directional);

	m_nLights = m_pLights.size();
}

void CScene::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); //256의 배수
	m_pd3dcbLights = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);

	m_pd3dcbLights->Map(0, NULL, (void **)&m_pcbMappedLights);
}
void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	::memcpy(m_pcbMappedLights->m_pLights, m_pLights.data(), sizeof(LIGHT) * m_nLights);
	::memcpy(&m_pcbMappedLights->m_xmf4GlobalAmbient, &m_xmf4GlobalAmbient, sizeof(XMFLOAT4));
	::memcpy(&m_pcbMappedLights->m_nLights, &m_nLights, sizeof(int));
}
void CScene::ReleaseShaderVariables()
{
	if (m_pd3dcbLights)
	{
		m_pd3dcbLights->Unmap(0, NULL);
		m_pd3dcbLights->Release();
	}
		}

CCamera* CScene::GetLightCamera(int idx)
{
	return &ShadowCameraManager->GetCameras()[idx];
}

void CScene::DeleteDeadObject(UINT64 Fence)
{
	for (auto& shader : m_ppShaders) {
		shader->DeleteObject(Fence);
	}
}

void CScene::DeleteTrash(UINT64 Fence)
{
	for (auto& shader : m_ppShaders) {
		shader->ProcessingGarbageQueue(Fence);
	}
}


void CScene::DumpMapOOBBToCSV(const char* filename)
{
	if (m_ppShaders.size() <= SHADERIDX::MAP) return;
	if (!m_ppShaders[SHADERIDX::MAP]) return;

	auto* objs = m_ppShaders[SHADERIDX::MAP]->GetObj();
	if (!objs) return;

	FILE* fp = nullptr;
	fopen_s(&fp, filename, "w");
	if (!fp)
	{
		OutputDebugStringA("DumpMapOOBBToCSV: failed to open file\n");
		return;
	}

	// 헤더 주석
	fprintf(fp, "# center_x,center_y,center_z,ext_x,ext_y,ext_z,quat_x,quat_y,quat_z,quat_w\n");

	int count = 0;
	for (const auto& obj : *objs)
	{
		if (!obj) continue;

		const auto& oobbs = obj->GetOOBB();
		for (const BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			// 쿼터니언 정규화 (단위 쿼터니언 보장)
			XMVECTOR q = XMLoadFloat4(&pOOBB->Orientation);
			q = XMQuaternionNormalize(q);
			XMFLOAT4 quat;
			XMStoreFloat4(&quat, q);

			fprintf(fp, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
				pOOBB->Center.x, pOOBB->Center.y, pOOBB->Center.z,
				pOOBB->Extents.x, pOOBB->Extents.y, pOOBB->Extents.z,
				quat.x, quat.y, quat.z, quat.w);
			++count;
		}
	}

	fclose(fp);

	char msg[128];
	sprintf_s(msg, "DumpMapOOBBToCSV: dumped %d OOBBs to %s\n", count, filename);
	OutputDebugStringA(msg);
}

bool CScene::ProcessInput(UCHAR *pKeysBuffer)
{
	return(false);
}

MainScene::MainScene(CGameFramework* game) : CScene(game)
{
	colManager = std::make_unique<CollisionManager>();
	m_pFogOverlayShader = nullptr;
	m_pDebugShader = nullptr;

	m_pEffectManager = nullptr;
	m_pInventoryManager = nullptr;

	m_bLaserActive = false;
	m_bSparkFireActive = false;
	m_fSparkSpawnTimer = 0.0f;
	m_fSparkSpawnInterval = 0.03f;
	m_fLaserLength = 15.0f;

	m_bGrenadeAimMode = false;
	m_fGrenadeAimDistance = 6.0f;
	m_fGrenadeAimMinDistance = 2.0f;
	m_fGrenadeAimMaxDistance = 18.0f;

	m_bGrenadeFlying = false;
	m_xmf3GrenadePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3GrenadeVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fGrenadeLifeTimer = 0.0f;
	m_fGrenadeExplodeDelay = 2.5f;
	m_fGrenadeGravity = -9.8f;
	m_fGrenadeBounceDamping = 0.45f;
	m_fGrenadeGroundY = 0.2f;
}

MainScene::~MainScene()
{

}

void MainScene::SetGrenadeAimMode(HWND hWnd, bool bEnable)
{
	if (m_bGrenadeAimMode == bEnable) return;

	if (bEnable)
	{
		if (!m_pPlayer) return;
		if (IsAnyInventoryOpen()) return;
		if (m_bGrenadeFlying) return;

		m_bGrenadeAimMode = true;
		m_bSparkFireActive = false;
		m_bLaserActive = false;
		m_fSparkSpawnTimer = 0.0f;

		if (m_pEffectManager)
		{
			m_pEffectManager->HideLaser(0);
		}

		m_fGrenadeAimMaxDistance = CalculateGrenadeMaxAimDistance();
		if (m_fGrenadeAimMaxDistance < m_fGrenadeAimMinDistance)
			m_fGrenadeAimMaxDistance = m_fGrenadeAimMinDistance;

		m_fGrenadeAimDistance = m_fGrenadeAimMinDistance;

		m_pPlayer->ApplyWeaponPose(WEAPON_POSE::GRENADE);

		ClampGameplayCursorToAimLine(hWnd);

		OutputDebugString(L"[Grenade] Aim Mode ON\n");
	}
	else
	{
		m_bGrenadeAimMode = false;

		if (m_pPlayer)
		{
			m_pPlayer->ApplyWeaponPose(WEAPON_POSE::IDLE);
		}

		ClampGameplayCursorToAimLine(hWnd);

		OutputDebugString(L"[Grenade] Aim Mode OFF\n");
	}
}

void MainScene::ReleaseGameplayCursor()
{
	::ClipCursor(NULL);
	::ShowCursor(TRUE);
}

void MainScene::ClampGameplayCursorToAimLine(HWND hWnd)
{
	if (!hWnd) return;

	if (IsAnyInventoryOpen())
	{
		ReleaseGameplayCursor();
		return;
	}

	if (!m_bGrenadeAimMode) return;

	RECT rc;
	::GetClientRect(hWnd, &rc);

	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;

	if (width <= 0 || height <= 0) return;

	int centerY = height / 2;

	POINT mouse;
	::GetCursorPos(&mouse);
	::ScreenToClient(hWnd, &mouse);

	if (mouse.y < 0) mouse.y = 0;
	if (mouse.y > centerY) mouse.y = centerY;

	m_fGrenadeAimMaxDistance = CalculateGrenadeMaxAimDistance();
	if (m_fGrenadeAimMaxDistance < m_fGrenadeAimMinDistance)
		m_fGrenadeAimMaxDistance = m_fGrenadeAimMinDistance;

	float t = 1.0f - (float(mouse.y) / float(centerY));
	m_fGrenadeAimDistance = m_fGrenadeAimMinDistance + (m_fGrenadeAimMaxDistance - m_fGrenadeAimMinDistance) * t;
}

float MainScene::CalculateGrenadeMaxAimDistance()
{
	if (!m_pPlayer) return 18.0f;

	XMFLOAT3 origin = m_pPlayer->GetPosition();
	origin.y += 1.2f;

	XMFLOAT3 dir = m_pPlayer->GetLookVector();
	dir.y = 0.0f;

	if (Vector3::Length(dir) < 0.0001f)
		return 18.0f;

	dir = Vector3::Normalize(dir);

	float maxDistance = 18.0f;
	float nearest = maxDistance;

	std::vector<CGameObject*> blockers;
	blockers.reserve(64);

	XMFLOAT3 playerPos = m_pPlayer->GetPosition();

	if (!m_vVisionMapChunks.empty())
	{
		GatherVisionMapBlockersInRectFromList(m_vVisionMapChunks, playerPos, maxDistance, blockers);
	}
	else if (m_ppShaders.size() > SHADERIDX::MAP && m_ppShaders[SHADERIDX::MAP])
	{
		GatherVisionBlockersFromShader(m_ppShaders[SHADERIDX::MAP], blockers);
	}

	XMVECTOR rayOrigin = XMLoadFloat3(&origin);
	XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&dir));

	for (CGameObject* pObj : blockers)
	{
		if (!pObj) continue;

		const auto& oobbs = pObj->GetOOBB();

		for (BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;
			if (pOOBB->Extents.y < 0.3f) continue;

			float hitDist = 0.0f;
			if (pOOBB->Intersects(rayOrigin, rayDir, hitDist))
			{
				if (hitDist > 0.2f && hitDist < nearest)
				{
					nearest = hitDist - 0.3f;
				}
			}
		}
	}

	if (nearest < m_fGrenadeAimMinDistance)
		nearest = m_fGrenadeAimMinDistance;

	if (nearest > maxDistance)
		nearest = maxDistance;

	return nearest;
}

void MainScene::ThrowGrenade()
{
	if (!m_pPlayer) return;
	if (!m_bGrenadeAimMode) return;

	XMFLOAT3 look = m_pPlayer->GetLookVector();
	look.y = 0.0f;

	if (Vector3::Length(look) < 0.0001f)
		look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	look = Vector3::Normalize(look);

	m_fGrenadeAimMaxDistance = CalculateGrenadeMaxAimDistance();
	if (m_fGrenadeAimDistance > m_fGrenadeAimMaxDistance)
		m_fGrenadeAimDistance = m_fGrenadeAimMaxDistance;

	if (m_fGrenadeAimDistance < m_fGrenadeAimMinDistance)
		m_fGrenadeAimDistance = m_fGrenadeAimMinDistance;

	XMFLOAT3 startPos = m_pPlayer->GetPosition();
	startPos.x += look.x * 0.8f;
	startPos.y += 1.2f;
	startPos.z += look.z * 0.8f;

	float flightTime = 0.75f;
	float targetDistance = m_fGrenadeAimDistance;

	XMFLOAT3 targetPos = startPos;
	targetPos.x += look.x * targetDistance;
	targetPos.z += look.z * targetDistance;
	targetPos.y = m_fGrenadeGroundY;

	m_xmf3GrenadePosition = startPos;

	m_xmf3GrenadeVelocity.x = (targetPos.x - startPos.x) / flightTime;
	m_xmf3GrenadeVelocity.z = (targetPos.z - startPos.z) / flightTime;
	m_xmf3GrenadeVelocity.y = ((targetPos.y - startPos.y) - (0.5f * m_fGrenadeGravity * flightTime * flightTime)) / flightTime;

	m_fGrenadeLifeTimer = 0.0f;
	m_bGrenadeFlying = true;
	m_bGrenadeAimMode = false;

	if (!m_pGrenadeDebugObject)
	{
		CGameObject* pGrenadePrototype = ResourceManager::Instance().GetModelPrototype(ModelName::GRENADE);

		if (pGrenadePrototype)
		{
			m_pGrenadeDebugObject = CGameObject::CreateModelInstance(pGrenadePrototype);
			m_pGrenadeDebugObject->SetOOBB(NULL);
			m_pGrenadeDebugObject->isColl = false;
			m_pGrenadeDebugObject->SetPosition(m_xmf3GrenadePosition);
			m_pGrenadeDebugObject->SetScale(1.0f, 1.0f, 1.0f);

			GameObjects.push_back(unique_ptr<CGameObject>(m_pGrenadeDebugObject));

			CShader* pStandardShader = shadermanager->GetShader(ShaderType::STANDARD);
			if (pStandardShader)
			{
				pStandardShader->addObjects(m_pGrenadeDebugObject);
			}
		}
		else
		{
			OutputDebugString(L"[Grenade] ModelName::GRENADE prototype not found.\n");
		}
	}
	else
	{
		m_pGrenadeDebugObject->SetPosition(m_xmf3GrenadePosition);
		m_pGrenadeDebugObject->UpdateTransform(NULL);
	}

	OutputDebugString(L"[Grenade] Real Throw\n");
}

void MainScene::UpdateGrenade(float fTimeElapsed)
{
	if (m_bGrenadeThrowPending)
	{
		m_fGrenadeThrowTimer += fTimeElapsed;

		if (m_fGrenadeThrowTimer < m_fGrenadeReleaseTime)
		{
			return;
		}

		m_bGrenadeThrowPending = false;
		m_fGrenadeThrowTimer = 0.0f;

		ThrowGrenade();
	}

	if (!m_bGrenadeFlying) return;

	m_fGrenadeLifeTimer += fTimeElapsed;

	m_xmf3GrenadeVelocity.y += m_fGrenadeGravity * fTimeElapsed;

	m_xmf3GrenadePosition.x += m_xmf3GrenadeVelocity.x * fTimeElapsed;
	m_xmf3GrenadePosition.y += m_xmf3GrenadeVelocity.y * fTimeElapsed;
	m_xmf3GrenadePosition.z += m_xmf3GrenadeVelocity.z * fTimeElapsed;

	if (m_xmf3GrenadePosition.y <= m_fGrenadeGroundY)
	{
		m_xmf3GrenadePosition.y = m_fGrenadeGroundY;

		if (m_xmf3GrenadeVelocity.y < 0.0f)
		{
			m_xmf3GrenadeVelocity.y = -m_xmf3GrenadeVelocity.y * m_fGrenadeBounceDamping;
			m_xmf3GrenadeVelocity.x *= 0.70f;
			m_xmf3GrenadeVelocity.z *= 0.70f;

			if (fabsf(m_xmf3GrenadeVelocity.y) < 0.6f)
				m_xmf3GrenadeVelocity.y = 0.0f;
		}
	}

	if (m_pGrenadeDebugObject)
	{
		m_pGrenadeDebugObject->SetPosition(m_xmf3GrenadePosition);
		m_pGrenadeDebugObject->UpdateTransform(NULL);
	}

	if (m_fGrenadeLifeTimer >= m_fGrenadeExplodeDelay)
	{
		ExplodeGrenade();
	}
}

void MainScene::ExplodeGrenade()
{
	if (!m_bGrenadeFlying) return;

	XMFLOAT3 explosionPos = m_xmf3GrenadePosition;

	if (m_pGrenadeDebugObject)
	{
		explosionPos = m_pGrenadeDebugObject->GetPosition();
	}

	explosionPos.y = m_fGrenadeGroundY;

	XMFLOAT3 effectDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
	if (m_pPlayer)
	{
		effectDir = m_pPlayer->GetLookVector();
		effectDir.y = 0.0f;

		if (Vector3::Length(effectDir) < 0.0001f)
		{
			effectDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		}

		effectDir = Vector3::Normalize(effectDir);
	}

	float effectVisualOffset = 1.5f;
	explosionPos.x -= effectDir.x * effectVisualOffset;
	explosionPos.z -= effectDir.z * effectVisualOffset;

	m_xmf3GrenadePosition = explosionPos;

	m_bGrenadeFlying = false;
	m_fGrenadeLifeTimer = 0.0f;

	if (m_pEffectManager)
	{
		EffectSpawnDesc desc;
		desc.id = EffectID::GRENADE_EXPLOSION;
		desc.position = explosionPos;
		desc.direction = effectDir;
		desc.ownerId = 0;
		desc.value = 0.0f;

		m_pEffectManager->PlayEffectByID(desc);
	}

	if (m_pGrenadeDebugObject)
	{
		m_pGrenadeDebugObject->SetPosition(0.0f, -10000.0f, 0.0f);
		m_pGrenadeDebugObject->UpdateTransform(NULL);
	}

	OutputDebugString(L"[Grenade] Explosion\n");
}

void MainScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	ClampGameplayCursorToAimLine(hWnd);

	switch (nMessageID)
	{
	case WM_MOUSEMOVE:
	{
		ClampGameplayCursorToAimLine(hWnd);
		break;
	}

	case WM_LBUTTONDOWN:
	{
		if (IsAnyInventoryOpen())
		{
			ReleaseGameplayCursor();

			if (m_pInventoryManager)
			{
				m_pInventoryManager->ProcessClick(InputManager::Instance().GetMousePos());
			}

			m_bSparkFireActive = false;
			m_bLaserActive = false;
			m_fSparkSpawnTimer = 0.0f;

			if (m_pEffectManager)
			{
				m_pEffectManager->HideLaser(0);
			}

			return;
		}

		if (m_bGrenadeAimMode)
		{
			ClampGameplayCursorToAimLine(hWnd);
			return;
		}

		if (!m_pPlayer)
			return;

		if (!m_pPlayer->TryFireWeapon())
		{
			m_bSparkFireActive = false;
			m_bLaserActive = false;
			m_fSparkSpawnTimer = 0.0f;

			if (m_pEffectManager)
			{
				m_pEffectManager->HideLaser(0);
			}

			return;
		}

		m_bSparkFireActive = m_pPlayer->IsCurrentWeaponAutomatic();
		m_bLaserActive = true;
		m_fSparkSpawnTimer = 0.0f;
		m_fSparkSpawnInterval = m_pPlayer->GetWeaponShotInterval();

		m_pPlayer->NotifyWeaponFired();

		XMFLOAT3 muzzlePos;
		XMFLOAT3 muzzleLook;
		XMFLOAT3 muzzleRight;
		XMFLOAT3 muzzleUp;

		GetCurrentPlayerMuzzleInfo(m_pPlayer, muzzlePos, muzzleLook, muzzleRight, muzzleUp);

		PlayerWeaponType weaponType = m_pPlayer->GetCurrentPlayerWeaponType();
		EFFECT_TYPE sparkType = GetSparkEffectTypeByWeapon(weaponType);
		XMFLOAT3 sparkPos = GetSparkPositionByWeapon(weaponType, muzzlePos, muzzleLook, muzzleUp);

		if (m_pEffectManager)
		{
			m_pEffectManager->RequestPlayEffect(sparkType, sparkPos, muzzleRight, muzzleUp);
			m_pEffectManager->UpdateLaser(0, muzzlePos, muzzleRight, muzzleUp, muzzleLook, m_fLaserLength);
		}

		if (m_ppShaders[SHADERIDX::ENEMY] && !m_ppShaders[SHADERIDX::ENEMY]->GetObj()->empty())
		{
			if (NetworkManager::Instance().IsConnected())
			{
				NetSession::Instance().FireHit(muzzlePos, muzzleLook, 0);
			}
			else
			{
				XMVECTOR rayOrigin = XMLoadFloat3(&muzzlePos);
				XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&muzzleLook));

				auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();
				for (auto& obj : *objs)
				{
					if (!obj) continue;

					const auto& oobbs = obj->GetOOBB();

					for (BoundingOrientedBox* pOOBB : oobbs)
					{
						if (!pOOBB) continue;

						float fDist = 0.0f;

						if (pOOBB->Intersects(rayOrigin, rayDir, fDist))
						{
							CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);
							if (pEnemy)
							{
								pEnemy->HandleHP(m_pPlayer->GetWeaponDamage());
							}
							break;
						}
					}
				}
			}
		}

		break;
	}

	case WM_LBUTTONUP:
	{
		if (m_bGrenadeAimMode)
		{
			if (!m_bGrenadeThrowPending && !m_bGrenadeFlying)
			{
				m_bGrenadeThrowPending = true;
				m_fGrenadeThrowTimer = 0.0f;

				if (m_pPlayer && !m_pPlayer->IsGrenadeState())
				{
					m_pPlayer->ChangeState(std::make_unique<PlayerGrenade>());
				}

				OutputDebugString(L"[Grenade] Throw Anim Start. Real grenade delayed.\n");
			}

			ClampGameplayCursorToAimLine(hWnd);
			return;
		}

		m_bSparkFireActive = false;
		m_bLaserActive = false;
		m_fSparkSpawnTimer = 0.0f;

		if (m_pEffectManager)
		{
			m_pEffectManager->HideLaser(0);
		}

		if (IsAnyInventoryOpen())
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->ProcessClick(InputManager::Instance().GetMousePos());
			}
		}
		else
		{
			if (uiManager)
			{
				uiManager->ProcessClick(InputManager::Instance().GetMousePos());
			}
		}

		break;
	}

	case WM_MOUSEWHEEL:
	{
		ClampGameplayCursorToAimLine(hWnd);

		if (m_bGrenadeAimMode)
		{
			return;
		}

		if (!m_pPlayer) return;
		if (IsAnyInventoryOpen()) return;

		short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

		float currentDistX10 = round(m_pPlayer->cameraDistance * 10.0f);

		if (zDelta > 0)
			currentDistX10 -= 10.0f;
		else
			currentDistX10 += 10.0f;

		m_pPlayer->cameraDistance = currentDistX10 / 10.0f;
		m_pPlayer->cameraDistance = clamp(m_pPlayer->cameraDistance, 3.0f, 15.0f);

		if (m_pPlayer->GetCamera())
		{
			m_pPlayer->GetCamera()->SetOffset(XMFLOAT3(0.0f, m_pPlayer->cameraDistance, -5.0f));
		}

		break;
	}

	default:
		break;
	}
}

bool MainScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (!m_pPlayer) return false;

	auto SendPlayerKeyEvent = [this](WPARAM keyCode, KEY_STATE state) -> bool
		{
			INPUT_KEY key;
			bool validKey = true;

			switch (keyCode)
			{
			case VK_UP:     key = INPUT_KEY::UP; break;
			case VK_DOWN:   key = INPUT_KEY::DOWN; break;
			case VK_LEFT:   key = INPUT_KEY::LEFT; break;
			case VK_RIGHT:  key = INPUT_KEY::RIGHT; break;

			case 'W': key = INPUT_KEY::W; break;
			case 'A': key = INPUT_KEY::A; break;
			case 'S': key = INPUT_KEY::S; break;
			case 'D': key = INPUT_KEY::D; break;

			case 'E': key = INPUT_KEY::E; break;
			case 'G': key = INPUT_KEY::G; break;
			case 'I': key = INPUT_KEY::I; break;
			case 'R': key = INPUT_KEY::R; break;

			case '1': key = INPUT_KEY::KEY_1; break;
			case '2': key = INPUT_KEY::KEY_2; break;
			case '3': key = INPUT_KEY::KEY_3; break;
			case '4': key = INPUT_KEY::KEY_4; break;

			case VK_SHIFT:   key = INPUT_KEY::SHIFT; break;
			case VK_SPACE:   key = INPUT_KEY::SPACE; break;

			case VK_LBUTTON: key = INPUT_KEY::LBUTTON; break;
			case VK_RBUTTON: key = INPUT_KEY::RBUTTON; break;

			default:
				validKey = false;
				break;
			}

			if (!validKey) return false;

			GameEvent e;
			e.type = EventType::Input;
			e.keyEvent = { key, state };

			m_pPlayer->AddEvent(e);
			return true;
		};

	switch (nMessageID)
	{
	case WM_KEYDOWN:
	{
		bool wasDownBefore = (lParam & (1 << 30)) != 0;

		switch (wParam)
		{
		case VK_F6:
		case VK_F7:
		{
			if (wasDownBefore) return true;

			PlayTestEffectByKey(wParam);
			return true;
		}

		case 'E':
		{
			if (wasDownBefore) return true;

			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleIKeyToggle(m_fLootInteractDistance);
			}

			if (uiManager)
			{
				for (auto& o : *uiManager->GetPannels())
				{
					o->ToggleOpen();
				}
			}

			if (IsAnyInventoryOpen())
			{
				m_bGrenadeAimMode = false;

				if (m_pPlayer)
				{
					m_pPlayer->ApplyWeaponPose(WEAPON_POSE::IDLE);
				}

				ReleaseGameplayCursor();
			}
			else
			{
				ClampGameplayCursorToAimLine(hWnd);
			}

			return true;
		}

		case 'G':
		{
			if (wasDownBefore) return true;

			if (IsAnyInventoryOpen())
			{
				return true;
			}

			if (m_bGrenadeFlying)
			{
				return true;
			}

			if (m_pPlayer && m_pPlayer->IsGrenadeState())
			{
				return true;
			}

			SetGrenadeAimMode(hWnd, true);
			ClampGameplayCursorToAimLine(hWnd);

			OutputDebugString(L"[Grenade] Aim Mode Enter By G\n");
			return true;
		}

		case VK_TAB:
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleTabPressed(m_fLootInteractDistance);
			}

			if (IsAnyInventoryOpen())
			{
				m_bGrenadeAimMode = false;

				if (m_pPlayer)
				{
					m_pPlayer->ApplyWeaponPose(WEAPON_POSE::IDLE);
				}

				ReleaseGameplayCursor();
			}
			else
			{
				ClampGameplayCursorToAimLine(hWnd);
			}

			return true;
		}

		case VK_F11:
			DumpMapOOBBToCSV("map_oobb.csv");
			break;

		case '9':
		{
			if (wasDownBefore) return true;
			if (NetworkManager::Instance().IsConnected()) {
				NetSession::Instance().Craft(ItemID::WEAPON_RIFLE);
			}
			return true;
		}

		case '0':
		{
			if (wasDownBefore) return true;
			if (NetworkManager::Instance().IsConnected()) {
				NetSession::Instance().Craft(ItemID::ARMOR_BODY_01);
			}
			return true;
		}

		default:
			break;
		}

		if (wasDownBefore)
			return true;

		if (IsAnyInventoryOpen())
			return true;

		ClampGameplayCursorToAimLine(hWnd);
		return SendPlayerKeyEvent(wParam, KEY_STATE::DOWN);
	}

	case WM_KEYUP:
	{
		if (wParam == 'G')
		{
			return true;
		}

		if (wParam == VK_TAB)
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleTabReleased();
			}

			if (IsAnyInventoryOpen())
			{
				ReleaseGameplayCursor();
			}
			else
			{
				ClampGameplayCursorToAimLine(hWnd);
			}

			return true;
		}

		if (IsAnyInventoryOpen())
			return true;

		ClampGameplayCursorToAimLine(hWnd);
		return SendPlayerKeyEvent(wParam, KEY_STATE::UP);
	}
	}

	return false;
}

void MainScene::PlayEffectFromServerLikeRequest(
	EffectID effectId,
	const XMFLOAT3& position,
	const XMFLOAT3& direction,
	int ownerId,
	float value)
{
	if (!m_pEffectManager)
		return;

	EffectSpawnDesc desc;
	desc.id = effectId;
	desc.position = position;
	desc.direction = direction;
	desc.ownerId = ownerId;
	desc.value = value;

	m_pEffectManager->PlayEffectByID(desc);
}

void MainScene::PlayTestEffectByKey(WPARAM keyCode)
{
	if (!m_pPlayer)
		return;

	XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	XMFLOAT3 look = m_pPlayer->GetLookVector();
	XMFLOAT3 up = m_pPlayer->GetUpVector();

	if (Vector3::Length(look) < 0.0001f)
	{
		look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	}

	look = Vector3::Normalize(look);

	XMFLOAT3 effectPos;
	effectPos.x = playerPos.x + look.x * 3.0f;
	effectPos.y = playerPos.y + 1.0f + look.y * 3.0f;
	effectPos.z = playerPos.z + look.z * 3.0f;

	XMFLOAT3 effectDir = look;

	switch (keyCode)
	{
	case VK_F6:
	{
		PlayEffectFromServerLikeRequest(
			EffectID::SPARK,
			effectPos,
			effectDir,
			1,
			0.0f
		);

		OutputDebugString(L"[Effect Test] SPARK\n");
		break;
	}

	case VK_F7:
	{
		XMFLOAT3 explosionPos = effectPos;
		explosionPos.y = playerPos.y + 0.3f;

		PlayEffectFromServerLikeRequest(
			EffectID::GRENADE_EXPLOSION,
			explosionPos,
			XMFLOAT3(0.0f, 1.0f, 0.0f),
			1,
			0.0f
		);

		OutputDebugString(L"[Effect Test] GRENADE_EXPLOSION\n");
		break;
	}

	default:
		break;
	}
}

void MainScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	BuildDefaultLightsAndMaterials();
	frame->mouseMove = false;
	m_pSkyBox = std::make_unique<CSkyBox>(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	
	UIShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_pFogOverlayShader = std::make_unique<CFogOverlayShader>();
	m_pFogOverlayShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_pInventoryManager = new InventoryManager();
	m_pInventoryManager->Initialize(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		UIShader.get()
	);

	// 맵 쉐이더
	CShader* stdshader = shadermanager->GetShader(ShaderType::STANDARD);
	m_vVisionMapChunks.clear();
	m_vVisionMapChunks.reserve(70);
	ModelName name = ModelName::MAP_FLOOR;
	for (int i = 0; i < s_mapFiles.size(); ++i)
	{
		
		CGameObject* map = new CGameObject();
		map->SetChild(ResourceManager::Instance().GetModelPrototype(name));
		map->SetPosition(-150, 0.0f, -150);
		map->SetOOBB(NULL);
		m_vVisionMapChunks.push_back(map);
		GameObjects.push_back(unique_ptr<CGameObject>(map));
		stdshader->addObjects(map);
		++name;
	}



	m_ppShaders.push_back(stdshader);

	// 시야 객체 생성
	CShader* view = shadermanager->GetShader(ShaderType::VIEW);
	{
		ViewObject* viewobj = new ViewObject();
		viewobj->SetPosition(0.0f, 0.03f, 0.0f);

		auto pCircleObj = std::make_unique<CGameObject>();
		strcpy_s(pCircleObj->m_pstrFrameName, 64, "ViewCircle");
		pCircleObj->SetMesh(new CViewCircleMesh(pd3dDevice, pd3dCommandList, 3.0f, 72));
		pCircleObj->SetShader(view);
		pCircleObj->SetPosition(0.0f, 0.7f, 0.0f);

		auto pConeObj = std::make_unique<CGameObject>();
		strcpy_s(pConeObj->m_pstrFrameName, 64, "ViewCone");
		pConeObj->SetMesh(new CViewConeMesh(pd3dDevice, pd3dCommandList, 20.0f, 75.0f, 72));
		pConeObj->SetShader(view);
		pConeObj->SetPosition(0.0f, 0.7f, 0.0f);

		viewobj->SetCircleObject(pCircleObj.get());
		viewobj->SetConeObject(pConeObj.get());

		viewobj->SetChild(pCircleObj.get());
		viewobj->SetChild(pConeObj.get());

		pCircleObj.release();
		pConeObj.release();
		GameObjects.push_back(unique_ptr<CGameObject>(viewobj));
		view->addObjects(viewobj);
	}
	m_ppShaders.push_back(view);

	// 디버그 쉐이더
	m_pDebugShader = std::make_unique<CBoundingBoxShader>(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		false
	);

	// 시체박스 전용 솔리드 큐브
	m_pLootBoxShader = std::make_unique<CBoundingBoxShader>(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		true
	);

	// 적 쉐이더
	auto pSkinnedShader = shadermanager->GetShader(ShaderType::SKINNED);

	AStarNav = make_unique<AstarNavigation>();
	AStarNav->LoadNavMeshFromFile("Model/NavMeshData.bin");

	//적 오브젝트 - 네트워크가 안 될 때에만
	CEnemyObject* pEnemy = new CEnemyObject(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, pSkinnedShader, ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::ENEMY_01_1));
	pEnemy->SetEnemyModelType(EnemyModelType::Enemy01);
	pEnemy->ApplyDefaultWeaponByEnemyModelType();
	pEnemy->SetPosition(0.0f, 0.0f, 0.0f);
	pEnemy->SetSpawnPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->SetScale(1.0f, 1.0f, 1.0f);
	pEnemy->setNav(AStarNav.get());
	pEnemy->SubmitWeaponToShader(stdshader);
	GameObjects.push_back(unique_ptr<CGameObject>(pEnemy));
	pSkinnedShader->addObjects(pEnemy);

	m_ppShaders.push_back(pSkinnedShader);

	if (m_pInventoryManager)
	{
		m_pInventoryManager->BindLootWorld(
			nullptr,
			(CStandardObjectsShader*)stdshader,
			m_pDebugShader.get(),
			m_pLootBoxShader.get()
		);
	}

	// EffectManager 생성
	m_pEffectManager = new EffectManager();
	m_pEffectManager->Initialize(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	for (const auto& shader : m_ppShaders)
	{
		auto* objs = shader->GetObj();
		if (objs != nullptr && !objs->empty())
		{
			for (auto& obj : *objs)
			{
				m_pDebugShader->AddObject(obj);
			}
		}
	}
	LinkToPlayer();
	EquipUI* e = new EquipUI(m_pPlayer);
	e->Init(pd3dDevice, pd3dCommandList);
	uiManager->AddToManager(e);

	PlayerStatus* s = new PlayerStatus(m_pPlayer);
	s->Init(pd3dDevice, pd3dCommandList);
	uiManager->AddToManager(s);

	ShadowCameraManager->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (!NetworkManager::Instance().Init("Player"))
	{
		OutputDebugString(L"DEBUG: Server Connect Fail.\n");
	}
}

void MainScene::ReleaseObjects()
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->Release();
		delete m_pInventoryManager;
		m_pInventoryManager = nullptr;
	}

	if (m_pEffectManager)
	{
		m_pEffectManager->Release();
		delete m_pEffectManager;
		m_pEffectManager = nullptr;
	}
	for (auto& s : m_ppShaders)
	{
		s->ReleaseObjects();
	}
	m_ppShaders.clear();

	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature = nullptr;

	//ResourceManager::Instance().ReleaseModelPrototypes();
	ReleaseShaderVariables();
	ShadowCameraManager->ReleaseShaderVariables();
	m_pLights.clear();
}

bool MainScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return false;
}

void MainScene::AnimateObjects(float fTimeElapsed)
{
	m_fElapsedTime = fTimeElapsed;

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i]) m_ppShaders[i]->AnimateObjects(fTimeElapsed);
	}

	if (m_pInventoryManager)
	{
		// 서버 없이 혼자 테스트할 때만 로컬 루팅박스 생성
		if (!NetworkManager::Instance().IsConnected())
		{
			if (m_ppShaders.size() > SHADERIDX::ENEMY && m_ppShaders[SHADERIDX::ENEMY])
			{
				auto* enemyObjs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();

				if (enemyObjs)
				{
					for (auto& obj : *enemyObjs)
					{
						if (!obj) continue;

						CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);
						if (!pEnemy) continue;

						if (pEnemy->ConsumeLootSpawnRequest())
						{
							m_pInventoryManager->SpawnLootContainerFromEnemy(pEnemy);
						}
					}
				}
			}
		}

		m_pInventoryManager->Update(fTimeElapsed);
	}

	if (m_pEffectManager && m_ppShaders.size() > SHADERIDX::ENEMY && m_ppShaders[SHADERIDX::ENEMY])
	{
		auto* enemyObjs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();

		if (enemyObjs)
		{
			for (auto& obj : *enemyObjs)
			{
				if (!obj) continue;

				CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);
				if (!pEnemy) continue;

				if (!pEnemy->ConsumeShootEffectRequest())
					continue;

				XMFLOAT3 muzzlePos;
				XMFLOAT3 muzzleLook;
				XMFLOAT3 muzzleRight;
				XMFLOAT3 muzzleUp;

				GetCurrentEnemyMuzzleInfo(
					pEnemy,
					muzzlePos,
					muzzleLook,
					muzzleRight,
					muzzleUp
				);

				EnemyWeaponType weaponType = pEnemy->GetEnemyWeaponType();
				EFFECT_TYPE sparkType = GetSparkEffectTypeByEnemyWeapon(weaponType);
				XMFLOAT3 sparkPos = GetSparkPositionByEnemyWeapon(weaponType, muzzlePos, muzzleLook, muzzleUp);

				m_pEffectManager->RequestPlayEffect(sparkType, sparkPos, muzzleRight, muzzleUp);
			}
		}
	}

	if (m_pPlayer && m_pLights.size() > 1)
	{
		m_pLights[1].m_xmf3Position = m_pPlayer->GetPosition();
		m_pLights[1].m_xmf3Direction = m_pPlayer->GetLookVector();
	}

	// 시야 객체 blocker 갱신
	{
		if (m_pPlayer && m_ppShaders.size() > SHADERIDX::VIEW && m_ppShaders[SHADERIDX::VIEW])
		{
			std::vector<CGameObject*> visionBlockers;
			visionBlockers.reserve(32);

			XMFLOAT3 playerPos = m_pPlayer->GetPosition();

			if (!m_vVisionMapChunks.empty())
			{
				GatherVisionMapBlockersInRectFromList(m_vVisionMapChunks, playerPos, 18.0f, visionBlockers);
			}
			else if (m_ppShaders.size() > SHADERIDX::MAP && m_ppShaders[SHADERIDX::MAP])
			{
				GatherVisionBlockersFromShader(m_ppShaders[SHADERIDX::MAP], visionBlockers);
			}

			auto* viewObjs = m_ppShaders[SHADERIDX::VIEW]->GetObj();
			if (viewObjs && !viewObjs->empty())
			{
				ViewObject* pViewObj = dynamic_cast<ViewObject*>(viewObjs->at(0));
				if (pViewObj)
				{
					pViewObj->UpdateClippedMeshes(visionBlockers);
				}
			}
		}
	}

	// 연사 처리
	if (m_bSparkFireActive && m_pPlayer && m_pPlayer->IsCurrentWeaponAutomatic() && !IsAnyInventoryOpen())
	{
		m_fSparkSpawnInterval = m_pPlayer->GetWeaponShotInterval();
		m_fSparkSpawnTimer += fTimeElapsed;

		while (m_fSparkSpawnTimer >= m_fSparkSpawnInterval)
		{
			if (!m_pPlayer->CanFireWeapon())
			{
				m_fSparkSpawnTimer = 0.0f;
				m_bSparkFireActive = false;
				m_bLaserActive = false;

				if (m_pEffectManager)
				{
					m_pEffectManager->HideLaser(0);
				}

				break;
			}

			if (!m_pPlayer->TryFireWeapon())
			{
				break;
			}

			m_fSparkSpawnTimer -= m_fSparkSpawnInterval;

			m_pPlayer->NotifyWeaponFired();

			XMFLOAT3 muzzlePos;
			XMFLOAT3 muzzleLook;
			XMFLOAT3 muzzleRight;
			XMFLOAT3 muzzleUp;

			GetCurrentPlayerMuzzleInfo(
				m_pPlayer,
				muzzlePos,
				muzzleLook,
				muzzleRight,
				muzzleUp
			);

			PlayerWeaponType weaponType = m_pPlayer->GetCurrentPlayerWeaponType();
			EFFECT_TYPE sparkType = GetSparkEffectTypeByWeapon(weaponType);
			XMFLOAT3 sparkPos = GetSparkPositionByWeapon(weaponType, muzzlePos, muzzleLook, muzzleUp);

			if (m_pEffectManager)
			{
				m_pEffectManager->RequestPlayEffect(sparkType, sparkPos, muzzleRight, muzzleUp);
				m_pEffectManager->UpdateLaser(0, muzzlePos, muzzleRight, muzzleUp, muzzleLook, m_fLaserLength);
			}

			if (NetworkManager::Instance().IsConnected())
			{
				NetSession::Instance().FireHitPlayer(muzzlePos, muzzleLook, 0);
			}

			if (m_ppShaders[SHADERIDX::ENEMY] && !m_ppShaders[SHADERIDX::ENEMY]->GetObj()->empty())
			{
				if (NetworkManager::Instance().IsConnected())
				{
					//NetworkManager::Instance().SendHitNpc(muzzlePos, muzzleLook, 0);
					NetSession::Instance().FireHit(muzzlePos, muzzleLook, 0);
				}
				else
				{
					XMVECTOR rayOrigin = XMLoadFloat3(&muzzlePos);
					XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&muzzleLook));

					auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();

					for (auto& obj : *objs)
					{
						if (!obj) continue;

						const auto& oobbs = obj->GetOOBB();

						for (BoundingOrientedBox* pOOBB : oobbs)
						{
							if (!pOOBB) continue;

							float fDist = 0.0f;

							if (pOOBB->Intersects(rayOrigin, rayDir, fDist))
							{
								CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);
								if (pEnemy)
								{
									pEnemy->HandleHP(m_pPlayer->GetWeaponDamage());
								}
								break;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		m_fSparkSpawnTimer = 0.0f;

		if (m_pEffectManager)
		{
			m_pEffectManager->HideLaser(0);
		}
	}

	UpdateGrenade(fTimeElapsed);

	if (m_pEffectManager)
	{
		m_pEffectManager->Update(fTimeElapsed);
	}

	// 레이저 transform 갱신
	if (m_bLaserActive && m_pPlayer && !IsAnyInventoryOpen() && m_pEffectManager)
	{
		XMFLOAT3 muzzlePos;
		XMFLOAT3 muzzleLook;
		XMFLOAT3 muzzleRight;
		XMFLOAT3 muzzleUp;

		GetCurrentPlayerMuzzleInfo(
			m_pPlayer,
			muzzlePos,
			muzzleLook,
			muzzleRight,
			muzzleUp
		);

		m_pEffectManager->UpdateLaser(
			0,
			muzzlePos,
			muzzleRight,
			muzzleUp,
			muzzleLook,
			m_fLaserLength
		);
	}
	else if (m_pEffectManager)
	{
		m_pEffectManager->HideLaser(0);
	}

	ShadowCameraManager->Update();

	if (m_pInventoryManager)
	{
		m_pInventoryManager->SubmitToShader(UIShader.get());
	}

	uiManager->Update(fTimeElapsed);
	colManager->DoCollision(m_pPlayer, m_ppShaders[SHADERIDX::MAP]->GetObj());	// 서버 충돌처리 확인을 위한 주석처리
}

void MainScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera)
{
	if (m_pd3dGraphicsRootSignature) pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
	ID3D12DescriptorHeap* heap = ResourceManager::Instance().GetDescriptorHeap();
	pd3dCommandList->SetDescriptorHeaps(1, &heap);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dcbLightsGpuVirtualAddress);

	pd3dCommandList->OMSetStencilRef(0xff);

	if (nPipelineState == SHADOW)
	{
		for (int i = 0; i < m_ppShaders.size(); i++)
		{
			if (m_ppShaders[i] && m_ppShaders[i]->DoShadow())
				m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
			if (m_pPlayer)
			{
				pd3dCommandList->OMSetStencilRef(0x04);
				m_pPlayer->Render(pd3dCommandList, nPipelineState, pCamera);
			}
		}
		return;
	}

	if (m_pSkyBox) m_pSkyBox->Render(pd3dCommandList, false, nPipelineState, pCamera);

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i])
			m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}

	//if (m_pInventoryManager)
	//{
	//	m_pInventoryManager->RenderLootWorld(pd3dCommandList, pCamera, nPipelineState);
	//}

	if (m_pEffectManager)
	{
		m_pEffectManager->Render(pd3dCommandList, pCamera, nPipelineState);
	}

	if (m_pFogOverlayShader)
	{
		pd3dCommandList->OMSetStencilRef(0x00);
		m_pFogOverlayShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
		pd3dCommandList->OMSetStencilRef(0xff);
	}
	uiManager->SubmitToShader(UIShader.get());
	if (UIShader)
	{
		UIShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}

#ifdef _DEBUG
	if (m_pDebugShader)
	{
		m_pDebugShader->Render(pd3dCommandList, pCamera, nPipelineState);
	}
#endif
	if (m_pInventoryManager)
	{
		m_pInventoryManager->RenderLootWorld(pd3dCommandList, pCamera, MAIN);
	}

	if (m_pPlayer)
	{
		pd3dCommandList->OMSetStencilRef(0x04);
		m_pPlayer->Render(pd3dCommandList, nPipelineState, pCamera);
	}
}

void MainScene::ThroughRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	if (!pCamera) return;

	if (m_pPlayer)
	{
		m_pPlayer->Render(pd3dCommandList, THROUGH, pCamera);
	}

}

void MainScene::ReleaseUploadBuffers()
{
	if (m_pSkyBox) m_pSkyBox->ReleaseUploadBuffers();

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i])
			m_ppShaders[i]->ReleaseUploadBuffers();
	}

	if (m_pEffectManager)
	{
		m_pEffectManager->ReleaseUploadBuffers();
	}
}

void MainScene::LinkToPlayer()
{
	std::vector<CGameObject*>* pVector = m_ppShaders[SHADERIDX::VIEW]->GetObj();

	for (auto& obj : *pVector)
	{
		ViewObject* pViewObj = static_cast<ViewObject*>(obj);
		pViewObj->setPlayer(m_pPlayer);
	}

	if (m_pPlayer)
	{
		m_pDebugShader->AddObject(m_pPlayer);
	}

	ShadowCameraManager->SetPlayer(m_pPlayer->GetCamera());
	ShadowCameraManager->SetDir(m_pLights[0].m_xmf3Direction);

	if (m_ppShaders[SHADERIDX::ENEMY])
	{
		auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();

		for (auto& obj : *objs)
		{
			CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);

			if (pEnemy)
			{
				pEnemy->SetPlayer(m_pPlayer);
			}
		}
	}

	if (m_pInventoryManager)
	{
		m_pInventoryManager->SetPlayer(m_pPlayer);
	}
}
bool MainScene::IsAnyInventoryOpen() const
{
	return (m_pInventoryManager) ? m_pInventoryManager->IsAnyInventoryOpen() : false;
}

void MainScene::CloseCorpseInventory()
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->CloseLootInventory();
	}
}

void MainScene::OpenLootContainer(CLootContainerObject* pLoot)
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->OpenLootContainer(pLoot);
	}
}

LobbyScene::LobbyScene(CGameFramework* game) : CScene(game)
{
	
}

void LobbyScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	{
		uiManager->ProcessClick(InputManager::Instance().GetMousePos());
		break;
	}
	default:
		break;
	}
}

bool LobbyScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return false;
}

void LobbyScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	frame->mouseMove = true;
	BuildDefaultLightsAndMaterials();
	UIShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	UIObject* lobbyBg = new UIObject();
	lobbyBg->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::LOBBY_BACKGROUND));
	lobbyBg->SetScale(2.0, 2.0, 1.0);
	lobbyBg->SetLocate(0.0, 0.0, 0.5);
	uiManager->AddToManager(lobbyBg);

	UIObject* lobbybutton = new UIObject();
	lobbybutton->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::LOBBY_START_BUTTON));
	lobbybutton->SetScale(0.5, 0.5, 1.0);
	lobbybutton->SetLocate(0.0, -0.5, 0.5);

	lobbybutton->setAABB();
	lobbybutton->SetFunc([this]() {
		this->frame->nextScene = new MainScene(frame);
		});
	
	uiManager->AddToManager(lobbybutton);


	ShadowCameraManager->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	CScene::CreateShaderVariables(pd3dDevice, pd3dCommandList);


}

void LobbyScene::ReleaseObjects()
{
	ShadowCameraManager->ReleaseShaderVariables();
	ReleaseShaderVariables();
}

void LobbyScene::Render(ID3D12GraphicsCommandList * pd3dCommandList, int nPipelineState, CCamera * pCamera)
{
	uiManager->SubmitToShader(UIShader.get());
	if (m_pd3dGraphicsRootSignature) pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
	ID3D12DescriptorHeap* heap = ResourceManager::Instance().GetDescriptorHeap();
	pd3dCommandList->SetDescriptorHeaps(1, &heap);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dcbLightsGpuVirtualAddress);
	UIShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
}
