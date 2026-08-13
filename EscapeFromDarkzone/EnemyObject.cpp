#include "stdafx.h"
#include "EnemyObject.h"
#include "Player.h"
#include "OtherPlayer.h"
#include "AI.h"
#include "Shader.h"
#include"ShaderManager.h"
#include "ResourceManager.h"
#include"SoundManager.h"
#include "Network.h"

static float DistanceXZ(const XMFLOAT3& a, const XMFLOAT3& b)
{
	float dx = a.x - b.x;
	float dz = a.z - b.z;
	return sqrtf(dx * dx + dz * dz);
}
static XMFLOAT3 NormalizeXZ(const XMFLOAT3& v)
{
	XMFLOAT3 result = v;
	result.y = 0.0f;

	if (Vector3::Length(result) < 0.0001f)
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	return Vector3::Normalize(result);
}
static XMFLOAT3 GetRightFromForwardXZ(const XMFLOAT3& forward)
{
	XMFLOAT3 dir = NormalizeXZ(forward);

	if (Vector3::Length(dir) < 0.0001f)
		return XMFLOAT3(1.0f, 0.0f, 0.0f);

	return XMFLOAT3(dir.z, 0.0f, -dir.x);
}
static float NormalizeAngleDeg(float angle)
{
	while (angle > 180.0f) angle -= 360.0f;
	while (angle < -180.0f) angle += 360.0f;
	return angle;
}
static CGameObject* FindFirstFrameByNames(CGameObject* pRoot, const char* const* ppNames, int nCount)
{
	if (!pRoot) return nullptr;

	for (int i = 0; i < nCount; ++i)
	{
		CGameObject* pFrame = pRoot->FindFrame(ppNames[i]);
		if (pFrame) return pFrame;
	}

	return nullptr;
}
struct EnemyWeaponVisualConfig
{
	XMFLOAT3 position;
	XMFLOAT3 rotation;
	XMFLOAT3 scale;
};
static EnemyWeaponVisualConfig GetEnemyWeaponVisualConfig(EnemyModelType enemyModelType, EnemyWeaponType weaponType)
{
	EnemyWeaponVisualConfig config{};
	config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
	config.rotation = XMFLOAT3(-90.0f, -90.0f, 28.0f);
	config.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	switch (enemyModelType)
	{
	case EnemyModelType::Enemy01:
		switch (weaponType)
		{
		case EnemyWeaponType::Pistol:
			config.position = XMFLOAT3(0.05f, 0.10f, 0.0f);
			config.rotation = XMFLOAT3(180.0f, 180.0f, 0.0f);
			config.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
			break;

		case EnemyWeaponType::SMG:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(180.0f, 90.0f, 0.0f);
			config.scale = XMFLOAT3(10.9f, 10.9f, 10.9f);
			break;

		case EnemyWeaponType::Rifle:
		default:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(-90.0f, -90.0f, 28.0f);
			config.scale = XMFLOAT3(10.0f, 10.0f, 10.0f);
			break;
		}
		break;

	case EnemyModelType::Enemy02:
		switch (weaponType)
		{
		case EnemyWeaponType::Pistol:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(90.0f, -90.0f, 14.0f);
			config.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
			break;

		case EnemyWeaponType::SMG:
			config.position = XMFLOAT3(0.05f, 0.25f, 0.05f);
			config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
			config.scale = XMFLOAT3(0.9f, 0.9f, 0.9f);
			break;

		case EnemyWeaponType::Rifle:
		default:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(-90.0f, -90.0f, 28.0f);
			config.scale = XMFLOAT3(1.1f, 1.1f, 1.1f);
			break;
		}
		break;

	case EnemyModelType::Enemy03:
	default:
		switch (weaponType)
		{
		case EnemyWeaponType::Pistol:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(90.0f, -90.0f, 14.0f);
			config.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
			break;

		case EnemyWeaponType::SMG:
			config.position = XMFLOAT3(-0.14f, 0.20f, 0.16f);
			config.rotation = XMFLOAT3(-90.0f, -90.0f, 28.0f);
			config.scale = XMFLOAT3(0.9f, 0.9f, 0.9f);
			break;

		case EnemyWeaponType::Rifle:
		default:
			config.position = XMFLOAT3(0.0f, 0.40f, 0.05f);
			config.rotation = XMFLOAT3(90.0f, 0.0f, 0.0f);
			config.scale = XMFLOAT3(1.1f, 1.1f, 1.1f);
			break;
		}
		break;
	}

	return config;
}
static EnemyWeaponType GetDefaultWeaponTypeByEnemyModelType(EnemyModelType enemyModelType)
{
	switch (enemyModelType)
	{
	case EnemyModelType::Enemy01:
		return EnemyWeaponType::Pistol;

	case EnemyModelType::Enemy02:
		return EnemyWeaponType::SMG;

	case EnemyModelType::Enemy03:
	default:
		return EnemyWeaponType::Rifle;
	}
}

CEnemyObject::CEnemyObject(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pShader,
	ModelInstance* pEnemyModel)
{
	UNREFERENCED_PARAMETER(pd3dGraphicsRootSignature);
	UNREFERENCED_PARAMETER(pShader);

	if (!pEnemyModel)
	{
		OutputDebugString(L"Error: Enemy model instance is null.\n");
		return;
	}

	if (!pEnemyModel->m_pAnimationSets)
	{
		pEnemyModel->m_pAnimationSets = new CAnimationSets(0);
	}

	m_pRenderWeapon = make_unique<CGameObject>();
	m_pWeapon = nullptr;
	m_pWeaponSocket = nullptr;
	m_pWeaponMuzzleSocket = nullptr;

	ClearOOBB(false);

	if (pEnemyModel->m_pRootObject)
	{
		pEnemyModel->m_pRootObject->ClearOOBB(true);
	}

	BoundingOrientedBox enemyBox;
	enemyBox.Center = XMFLOAT3(0.0f, 1.0f, 0.0f);
	enemyBox.Extents = XMFLOAT3(0.35f, 1.0f, 0.35f);
	enemyBox.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	SetOOBB(enemyBox);

	m_pSkinnedAnimationController = new CAnimationController(
		pd3dDevice,
		pd3dCommandList,
		1,
		pEnemyModel
	);

	SetChild(pEnemyModel->m_pRootObject.release(), true);

	m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, ENEMY_RIFLE_SMG_IDLE);
	m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	ConfigureWeaponStats();

	m_Blackboard.Reset();

	m_pBehaviorTree = std::make_unique<EnemyBehaviorTree>();

	if (pEnemyModel)
	{
		delete pEnemyModel;
	}

	ChangeState(std::make_unique<EnemyIdle>());
}

CEnemyObject::~CEnemyObject()
{

}

void CEnemyObject::SubmitWeaponToShader(CShader* shader)
{
	if (!shader || !m_pRenderWeapon)
		return;

	shader->addObjects(m_pRenderWeapon.get());
}

void CEnemyObject::ChangeState(std::unique_ptr<State<CEnemyObject>> pNewState)
{
	m_StateMachine.ChangeState(this, std::move(pNewState));
}

void CEnemyObject::HandleHP(float value)
{
	if (m_bDying) return;
	if (value <= 0.0f) return;

	hp -= value;

	if (hp <= 0.0f)
	{
		hp = 0.0f;
		m_bDying = true;
		m_bLootSpawnRequested = true;				// 서버 권위 구조로 바꾸면서 안씀
		m_bDeadRemoveRequested = false;
		m_fDieElapsed = 0.0f;

		ChangeState(std::make_unique<EnemyDie>());
		OutputDebugString(L"Enemy Die Start\n");
	}
}

bool CEnemyObject::ConsumeDeadRemovalRequest()
{
	if (!m_bDeadRemoveRequested) return false;
	m_bDeadRemoveRequested = false;
	return true;
}

bool CEnemyObject::ConsumeLootSpawnRequest()		// 서버 권위 구조로 바꾸면서 안씀
{
	if (!m_bLootSpawnRequested) return false;
	m_bLootSpawnRequested = false;
	return true;
}

void CEnemyObject::MarkDeadForRemoval()
{
	Kill();
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	CGameObject::Animate(fTimeElapsed);
	Update(fTimeElapsed);
}

void CEnemyObject::Update(float fTimeElapsed)
{
	m_xmf3PrevPos = m_xmf3Position;

	if (m_fShootAnimTimer > 0.0f)
	{
		m_fShootAnimTimer -= fTimeElapsed;
	}

	m_StateMachine.Update(this, fTimeElapsed);

	if (m_bDying)
	{
		m_xmf4x4ToParent._41 = m_xmf3Position.x;
		m_xmf4x4ToParent._42 = m_xmf3Position.y;
		m_xmf4x4ToParent._43 = m_xmf3Position.z;
		UpdateTransform(NULL);

		if (m_pRenderWeapon && m_pWeaponSocket)
		{
			m_pRenderWeapon->m_xmf4x4ToParent = m_pWeaponSocket->m_xmf4x4World;
			m_pRenderWeapon->UpdateTransform(NULL);
		}

		return;
	}

	if (!IsAlive())
		return;

	XMFLOAT3 direction = m_xmf3MoveDir;
	m_xmf3Velocity = Vector3::ScalarProduct(direction, m_fMoveSpeed, false);

	XMFLOAT3 shift = Vector3::ScalarProduct(m_xmf3Velocity, fTimeElapsed, false);
	m_xmf3Position = Vector3::Add(m_xmf3Position, shift);

	if (m_bUseServerLerp)
	{
		constexpr float ALPHA = 0.08f;

		m_xmf3Position.x += (m_xmf3ServerPosition.x - m_xmf3Position.x) * ALPHA;
		m_xmf3Position.y += (m_xmf3ServerPosition.y - m_xmf3Position.y) * ALPHA;
		m_xmf3Position.z += (m_xmf3ServerPosition.z - m_xmf3Position.z) * ALPHA;
	}

	m_xmf4x4ToParent._41 = m_xmf3Position.x;
	m_xmf4x4ToParent._42 = m_xmf3Position.y;
	m_xmf4x4ToParent._43 = m_xmf3Position.z;

	UpdateTransform(NULL);

	if (m_pRenderWeapon && m_pWeaponSocket)
	{
		m_pRenderWeapon->m_xmf4x4ToParent = m_pWeaponSocket->m_xmf4x4World;
		m_pRenderWeapon->UpdateTransform(NULL);
	}
}

void CEnemyObject::SetPosition(float x, float y, float z)
{
	CGameObject::SetPosition(x, y, z);
	m_xmf3Position.x = x;
	m_xmf3Position.y = y;
	m_xmf3Position.z = z;

}

void CEnemyObject::Render(
	ID3D12GraphicsCommandList* pd3dCommandList,
	bool batch,
	int nPipelineState,
	CCamera* pCamera)
{
	if (m_pSkinnedAnimationController)
	{
		m_pSkinnedAnimationController->UpdateShaderVariables(pd3dCommandList);
	}

	CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);

}

float CEnemyObject::GetDistanceToPlayerXZ() const
{
	if (!m_pPlayer) return FLT_MAX;

	XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	return DistanceXZ(m_xmf3Position, playerPos);
}
float CEnemyObject::GetDistanceFromSpawnXZ() const
{
	return DistanceXZ(m_xmf3Position, m_xmf3SpawnPosition);
}
float CEnemyObject::GetDistanceToSpawnXZ() const
{
	return DistanceXZ(m_xmf3Position, m_xmf3SpawnPosition);
}

bool CEnemyObject::IsPlayerInDetectRange() const
{
	return GetDistanceToPlayerXZ() <= m_fDetectionRange;
}
bool CEnemyObject::IsPlayerInAttackRange() const
{
	return GetDistanceToPlayerXZ() <= m_fAttackRange;
}
bool CEnemyObject::IsPlayerOutOfAttackRange() const
{
	return GetDistanceToPlayerXZ() >= m_fAttackExitRange;
}
bool CEnemyObject::IsOutsideLeashRange() const
{
	return GetDistanceFromSpawnXZ() > m_fLeashRange;
}
bool CEnemyObject::IsNearSpawn() const
{
	return GetDistanceToSpawnXZ() <= m_fReturnStopDistance;
}

void CEnemyObject::FaceToPosition(const XMFLOAT3& targetPos)
{
	XMFLOAT3 dir = Vector3::Subtract(targetPos, m_xmf3Position);
	dir.y = 0.0f;

	if (Vector3::Length(dir) < 0.0001f)
		return;

	dir = Vector3::Normalize(dir);

	float fAngleRad = atan2f(dir.x, dir.z);
	float fAngleDeg = XMConvertToDegrees(fAngleRad);

	SetYawDeg(fAngleDeg);
}

bool CEnemyObject::UpdatePathToPosition(const XMFLOAT3& targetPos, float fTimeElapsed)
{
	updateTimer += fTimeElapsed;

	if (updateTimer < findTime)
	{
		return !ways.empty() && wayIdx < static_cast<int>(ways.size());
	}

	updateTimer = 0.0f;

	if (!AStarNav)
	{
		ways.clear();
		wayIdx = 0;
		return false;
	}

	std::vector<XMFLOAT3> newWays = AStarNav->FindPath(m_xmf3Position, targetPos);

	if (newWays.empty())
	{
		return !ways.empty() && wayIdx < static_cast<int>(ways.size());
	}

	ways = std::move(newWays);
	wayIdx = 0;

	return true;
}

bool CEnemyObject::FollowCurrentPath()
{
	if (ways.empty())
	{
		SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return false;
	}

	while (wayIdx < static_cast<int>(ways.size()))
	{
		XMFLOAT3 nextWaypoint = ways[wayIdx];
		XMFLOAT3 dirToWaypoint = Vector3::Subtract(nextWaypoint, m_xmf3Position);
		dirToWaypoint.y = 0.0f;

		float fDistance = Vector3::Length(dirToWaypoint);

		if (fDistance < 0.8f)
		{
			wayIdx++;
			continue;
		}

		XMFLOAT3 moveDir = Vector3::Normalize(dirToWaypoint);
		SetMoveDir(moveDir);
		FaceToPosition(nextWaypoint);
		return true;
	}

	SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	return false;
}

void CEnemyObject::ClearPath()
{
	ways.clear();
	wayIdx = 0;
	updateTimer = findTime;
}

XMFLOAT3 CEnemyObject::GetDirectionToPlayerXZ() const
{
	if (!m_pPlayer)
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	XMFLOAT3 dir = Vector3::Subtract(playerPos, m_xmf3Position);

	return NormalizeXZ(dir);
}

XMFLOAT3 CEnemyObject::GetDirectionToSpawnXZ() const
{
	XMFLOAT3 dir = Vector3::Subtract(m_xmf3SpawnPosition, m_xmf3Position);
	return NormalizeXZ(dir);
}

void CEnemyObject::StartNewBurst()
{
	int range = m_nBurstShotMax - m_nBurstShotMin + 1;

	if (range <= 0)
	{
		m_nBurstShotsLeft = 3;
	}
	else
	{
		m_nBurstSerial++;
		m_nBurstShotsLeft = m_nBurstShotMin + (m_nBurstSerial % range);
	}

	m_fBurstShotTimer = 0.0f;
}

void CEnemyObject::SetEnemyAnimation(int nAnim, bool bLoop, bool bRestart)
{
	auto* pController = m_pSkinnedAnimationController;

	if (!pController)
		return;

	if (bLoop)
		pController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	else
		pController->SetTrackType(0, ANIMATION_TYPE_ONCE);

	pController->SetTrackAnimationSetIfChanged(0, nAnim);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);

	if (bRestart)
	{
		pController->SetTrackPosition(0, 0.0f);
	}
}

void CEnemyObject::EquipWeaponModel(ModelName modelName)
{
	CGameObject* pWeaponInstance = ResourceManager::Instance().GetModelInstance(modelName);

	if (!pWeaponInstance)
	{
		wchar_t debugText[128];
		swprintf_s(debugText, L"[Enemy] weapon prototype not found. ModelName = %d\n", static_cast<int>(modelName));
		OutputDebugString(debugText);
		return;
	}


	if (!pWeaponInstance)
	{
		OutputDebugString(L"[Enemy] weapon instance create failed.\n");
		return;
	}

	static const char* s_ppRightHandNames[] =
	{
		"mixamorig:RightHand",
		"RightHand",
		"Bip001 R Hand",
		"mixamorig:RightHandIndex1"
	};

	CGameObject* pRightHand = FindFirstFrameByNames(this, s_ppRightHandNames, _countof(s_ppRightHandNames));

	if (!pRightHand)
	{
		OutputDebugString(L"[Enemy] RightHand frame not found. weapon not equipped.\n");
		delete pWeaponInstance;
		return;
	}

	if (!m_pRenderWeapon)
	{
		m_pRenderWeapon = make_unique<CGameObject>();
	}
	m_pWeaponSocket = pRightHand;

	pWeaponInstance->m_pParent = m_pRenderWeapon.get();   
	pWeaponInstance->m_pSibling.reset();                  

	m_pRenderWeapon->m_pChild = unique_ptr<CGameObject>(pWeaponInstance);
	m_pWeapon = m_pRenderWeapon->m_pChild.get();


	m_pRenderWeapon->SetOOBB(NULL);
	m_pRenderWeapon->isColl = false;

	EnemyWeaponVisualConfig visualConfig = GetEnemyWeaponVisualConfig(m_eEnemyModelType, m_eWeaponType);

	m_pWeapon->SetPosition(visualConfig.position);
	m_pWeapon->SetScale(visualConfig.scale.x, visualConfig.scale.y, visualConfig.scale.z);
	m_pWeapon->Rotate(visualConfig.rotation.x, visualConfig.rotation.y, visualConfig.rotation.z);

	m_pWeaponMuzzleSocket = m_pWeapon->FindFrame("Socket_Muzzle");

	if (m_pWeaponMuzzleSocket)
	{
		OutputDebugString(L"[Enemy] Socket_Muzzle found.\n");
	}
	else
	{
		OutputDebugString(L"[Enemy] Socket_Muzzle not found.\n");
	}

	m_pRenderWeapon->m_xmf4x4ToParent = m_pWeaponSocket->m_xmf4x4World;
	m_pRenderWeapon->UpdateTransform(NULL);
}

void CEnemyObject::EquipDefaultPistol()
{
	SetEnemyWeaponType(EnemyWeaponType::Pistol);
}
void CEnemyObject::SetEnemyModelType(EnemyModelType eModelType)
{
	m_eEnemyModelType = eModelType;
}
void CEnemyObject::ApplyDefaultWeaponByEnemyModelType()
{
	EnemyWeaponType defaultWeaponType = GetDefaultWeaponTypeByEnemyModelType(m_eEnemyModelType);
	SetEnemyWeaponType(defaultWeaponType);
}
ModelName CEnemyObject::GetWeaponModelNameByType(EnemyWeaponType eWeaponType) const
{
	switch (eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ModelName::PISTOL;

	case EnemyWeaponType::SMG:
		return ModelName::SMG;

	case EnemyWeaponType::Rifle:
	default:
		return ModelName::RIFLE;
	}
}
void CEnemyObject::EquipWeaponByType(EnemyWeaponType eWeaponType)
{
	EquipWeaponModel(GetWeaponModelNameByType(eWeaponType));
}

void CEnemyObject::SetEnemyWeaponType(EnemyWeaponType eWeaponType)
{
	m_eWeaponType = eWeaponType;
	ConfigureWeaponStats();
	EquipWeaponByType(eWeaponType);
}

int CEnemyObject::GetIdleAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_IDLE;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_IDLE;
	}
}

int CEnemyObject::GetForwardRunAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_RUN_F;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_RUN_F;
	}
}

int CEnemyObject::GetLeftRunAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_RUN_L;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_RUN_L;
	}
}

int CEnemyObject::GetRightRunAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_RUN_R;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_RUN_R;
	}
}

// 현재 총기 타입에 맞는 공격 애니메이션 번호를 반환한다.
int CEnemyObject::GetAttackAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_SHOOT;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_SHOOT;
	}
}

int CEnemyObject::GetReloadAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		return ENEMY_PISTOL_RELOAD;

	case EnemyWeaponType::SMG:
	case EnemyWeaponType::Rifle:
	default:
		return ENEMY_RIFLE_SMG_RELOAD;
	}
}

int CEnemyObject::GetDieAnimationByWeapon() const
{
	return ENEMY_DIE;
}

// 현재 총기 타입에 맞게 탄창, 재장전, 버스트, 사거리 값을 설정한다.
void CEnemyObject::ConfigureWeaponStats()
{
	switch (m_eWeaponType)
	{
	case EnemyWeaponType::Pistol:
		m_nMagazineAmmo = 8;
		m_nCurrentAmmo = m_nMagazineAmmo;
		m_fReloadDuration = 1.8f;
		m_nBurstShotMin = 1;
		m_nBurstShotMax = 2;
		m_fBurstShotInterval = 0.35f;
		m_fBurstRestDuration = 0.8f;
		m_fAimDelay = 0.25f;
		m_fAttackRange = 9.0f;
		m_fAttackExitRange = 10.5f;
		m_fPreferredCombatRange = 6.0f;
		break;

	case EnemyWeaponType::SMG:
		m_nMagazineAmmo = 30;
		m_nCurrentAmmo = m_nMagazineAmmo;
		m_fReloadDuration = 2.2f;
		m_nBurstShotMin = 4;
		m_nBurstShotMax = 7;
		m_fBurstShotInterval = 0.08f;
		m_fBurstRestDuration = 0.7f;
		m_fAimDelay = 0.25f;
		m_fAttackRange = 8.0f;
		m_fAttackExitRange = 9.5f;
		m_fPreferredCombatRange = 7.0f;
		break;

	case EnemyWeaponType::Rifle:
	default:
		m_nMagazineAmmo = 20;
		m_nCurrentAmmo = m_nMagazineAmmo;
		m_fReloadDuration = 2.4f;
		m_nBurstShotMin = 2;
		m_nBurstShotMax = 4;
		m_fBurstShotInterval = 0.15f;
		m_fBurstRestDuration = 1.0f;
		m_fAimDelay = 0.35f;
		m_fAttackRange = 12.0f;
		m_fAttackExitRange = 13.5f;
		m_fPreferredCombatRange = 9.0f;
		break;
	}

	m_nBurstShotsLeft = 0;
	m_fBurstShotTimer = 0.0f;
	m_fBurstRestTimer = 0.0f;
	m_bReloading = false;
	m_fReloadTimer = 0.0f;
}

void CEnemyObject::UpdateCombatMove(float fTimeElapsed)
{
	UNREFERENCED_PARAMETER(fTimeElapsed);

	if (!m_pPlayer)
	{
		SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetEnemyAnimation(GetAttackAnimationByWeapon(), true, false);
		return;
	}

	XMFLOAT3 toPlayer = GetDirectionToPlayerXZ();
	float distToPlayer = GetDistanceToPlayerXZ();

	if (Vector3::Length(toPlayer) < 0.0001f)
	{
		SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetEnemyAnimation(GetAttackAnimationByWeapon(), true, false);
		return;
	}

	XMFLOAT3 moveDir = XMFLOAT3(0.0f, 0.0f, 0.0f);

	if (distToPlayer < m_fTooCloseRange)
	{
		moveDir = Vector3::ScalarProduct(toPlayer, -m_fCombatMoveSpeedMultiplier, false);

		if (m_fStrafeSign >= 0.0f)
			SetEnemyAnimation(GetRightRunAnimationByWeapon(), true, false);
		else
			SetEnemyAnimation(GetLeftRunAnimationByWeapon(), true, false);
	}
	else
	{
		XMFLOAT3 right = GetRightFromForwardXZ(toPlayer);

		moveDir = Vector3::ScalarProduct(
			right,
			m_fStrafeSign * m_fCombatMoveSpeedMultiplier,
			false
		);

		if (m_fStrafeSign >= 0.0f)
			SetEnemyAnimation(GetRightRunAnimationByWeapon(), true, false);
		else
			SetEnemyAnimation(GetLeftRunAnimationByWeapon(), true, false);
	}

	SetMoveDir(moveDir);
}

void CEnemyObject::StartReload()
{
	if (m_bReloading)
		return;

	m_bReloading = true;
	m_fReloadTimer = m_fReloadDuration;

	m_nBurstShotsLeft = 0;
	m_fBurstShotTimer = 0.0f;
	m_fBurstRestTimer = 0.0f;

	m_Blackboard.bNeedsReload = true;
	m_Blackboard.eCurrentAction = EnemyBehaviorAction::Reload;
	m_Blackboard.bHasMoveTarget = false;

	SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	SetEnemyAnimation(GetReloadAnimationByWeapon(), false, true);

	OutputDebugString(L"[Enemy AI] Reload Start\n");
}

void CEnemyObject::UpdateReload(float fTimeElapsed)
{
	if (!m_bReloading)
		return;

	SetEnemyAnimation(GetReloadAnimationByWeapon(), false, false);

	m_fReloadTimer -= fTimeElapsed;

	if (m_fReloadTimer <= 0.0f)
	{
		m_bReloading = false;
		m_fReloadTimer = 0.0f;

		m_nCurrentAmmo = m_nMagazineAmmo;
		m_nBurstShotsLeft = 0;
		m_fBurstShotTimer = 0.0f;
		m_fBurstRestTimer = 0.0f;
		m_fAimTimer = 0.0f;

		m_Blackboard.bNeedsReload = false;

		OutputDebugString(L"[Enemy AI] Reload Finish\n");
	}
}

void CEnemyObject::SetYawDeg(float yawDeg)
{
	m_fCurrentYawDeg = NormalizeAngleDeg(yawDeg);

	XMFLOAT4X4 rotate = Matrix4x4::Rotate(0.0f, m_fCurrentYawDeg, 0.0f);

	rotate._41 = m_xmf3Position.x;
	rotate._42 = m_xmf3Position.y;
	rotate._43 = m_xmf3Position.z;

	m_xmf4x4ToParent = rotate;
}

XMFLOAT3 CEnemyObject::GetForwardXZ() const
{
	float yawRad = XMConvertToRadians(m_fCurrentYawDeg);

	XMFLOAT3 forward;
	forward.x = sinf(yawRad);
	forward.y = 0.0f;
	forward.z = cosf(yawRad);

	return NormalizeXZ(forward);
}

bool CEnemyObject::IsPlayerInViewAngle() const
{
	if (!m_pPlayer)
		return false;

	XMFLOAT3 toPlayer = GetDirectionToPlayerXZ();

	if (Vector3::Length(toPlayer) < 0.0001f)
		return true;

	XMFLOAT3 forward = GetForwardXZ();

	if (Vector3::Length(forward) < 0.0001f)
		return false;

	float dot = Vector3::DotProduct(forward, toPlayer);

	float halfAngleRad = XMConvertToRadians(m_fViewAngle * 0.5f);
	float viewCos = cosf(halfAngleRad);

	return dot >= viewCos;
}

bool CEnemyObject::CanDetectPlayer() const
{
	if (!m_pPlayer)
		return false;

	if (!IsPlayerInDetectRange())
		return false;

	if (!IsPlayerInViewAngle())
		return false;

	return true;
}

bool CEnemyObject::CanShootPlayer() const
{
	if (!m_pPlayer)
		return false;

	if (!IsPlayerInAttackRange())
		return false;

	if (!IsPlayerInViewAngle())
		return false;

	return true;
}

void CEnemyObject::RefreshLastSeenPlayer()
{
	if (!m_pPlayer)
		return;

	m_Blackboard.pTarget = m_pPlayer;
	m_Blackboard.xmf3LastSeenPosition = m_pPlayer->GetPosition();
	m_Blackboard.fLoseSightTimer = 0.0f;
	m_Blackboard.bHasLastSeenPosition = true;

	m_Blackboard.bCanSeeTarget = true;
	m_Blackboard.fTargetDistance = GetDistanceToPlayerXZ();
}

bool CEnemyObject::HasRecentLastSeenPlayer() const
{
	return m_Blackboard.bHasLastSeenPosition && (m_Blackboard.fLoseSightTimer < m_fLoseSightDuration);
}

void CEnemyObject::UpdateIdleLook(float fTimeElapsed)
{
	m_fIdleLookTimer -= fTimeElapsed;

	if (m_fIdleLookTimer <= 0.0f)
	{
		m_fIdleLookTimer = m_fIdleLookInterval;

		m_nIdleLookDir *= -1;
		m_fIdleYawTarget = m_fIdleBaseYawDeg + (m_fIdleYawRange * static_cast<float>(m_nIdleLookDir));
		m_fIdleYawTarget = NormalizeAngleDeg(m_fIdleYawTarget);
	}

	float yawDiff = NormalizeAngleDeg(m_fIdleYawTarget - m_fCurrentYawDeg);
	float maxStep = m_fIdleTurnSpeed * fTimeElapsed;

	if (fabsf(yawDiff) <= maxStep)
	{
		SetYawDeg(m_fIdleYawTarget);
	}
	else
	{
		if (yawDiff > 0.0f)
			SetYawDeg(m_fCurrentYawDeg + maxStep);
		else
			SetYawDeg(m_fCurrentYawDeg - maxStep);
	}
}

void CEnemyObject::FireAtPlayer()
{
	if (!m_pPlayer) return;
	if (m_bDying) return;
	if (m_bReloading) return;

	if (m_nCurrentAmmo <= 0)
	{
		StartReload();
		return;
	}

	m_nCurrentAmmo--;

	m_fShootAnimTimer = m_fShootAnimHold;
	m_bShootEffectRequested = true;

	OutputDebugString(L"[Enemy AI] Enemy Burst Fire\n");

	if (m_nCurrentAmmo <= 0)
	{
		StartReload();
	}
}

bool CEnemyObject::ConsumeShootEffectRequest()
{
	if (!m_bShootEffectRequested) return false;

	m_bShootEffectRequested = false;
	return true;
}

bool EnemyIdle::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Idle;
	blackboard.bHasMoveTarget = false;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	pEnemy->m_fIdleBaseYawDeg = pEnemy->m_fCurrentYawDeg;
	pEnemy->m_fIdleYawTarget = pEnemy->m_fCurrentYawDeg;
	pEnemy->m_fIdleLookTimer = pEnemy->m_fIdleLookInterval;

	pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, true);

	return true;
}

void EnemyIdle::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->m_bDying)
		return;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->UpdateIdleLook(fTimeElapsed);
}

void EnemyIdle::Exit(CEnemyObject* pEnemy)
{
}

bool EnemySearch::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Search;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	if (blackboard.Search.bHasTarget)
		pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, true);
	else
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, true);

	return true;
}

void EnemySearch::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->IsDying())
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();
	EnemySearchMemory& search = blackboard.Search;

	// 수색 지점 도착 후 잠깐 멈춰서 주변을 살핀다.
	if (search.fWaitTimer > 0.0f)
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, false);
		pEnemy->UpdateIdleLook(fTimeElapsed);
		return;
	}

	// 아직 행동트리에서 수색 지점을 생성하지 못한 프레임
	if (!search.bHasTarget)
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return;
	}

	XMFLOAT3 currentPos = pEnemy->GetPosition();
	XMFLOAT3 toTarget = Vector3::Subtract(search.xmf3Target, currentPos);
	toTarget.y = 0.0f;

	float distance = Vector3::Length(toTarget);

	if (distance < 0.9f)
	{
		search.bHasTarget = false;
		search.bReachedTarget = true;

		blackboard.bHasMoveTarget = false;

		pEnemy->ClearPath();
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, true);

		return;
	}

	pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, false);

	bool hasPath = pEnemy->UpdatePathToPosition(search.xmf3Target, fTimeElapsed);

	if (!hasPath)
	{
		search.bHasTarget = false;
		search.bPathFailed = true;

		blackboard.bHasMoveTarget = false;

		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ClearPath();

		return;
	}

	if (!pEnemy->FollowCurrentPath())
	{
		search.bHasTarget = false;
		search.bPathFailed = true;

		blackboard.bHasMoveTarget = false;

		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ClearPath();
	}
}

void EnemySearch::Exit(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.Search.ResetRuntime();
	blackboard.bHasMoveTarget = false;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyInvestigate::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Investigate;

	// Search 중 선택했던 목적지는 폐기한다.
	blackboard.Search.ResetRuntime();

	blackboard.bHasMoveTarget = false;

	m_bReachedTarget = false;
	m_fWaitTimer = 0.0f;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

	if (blackboard.Hearing.bHasSound)
	{
		blackboard.bHasMoveTarget = true;
		blackboard.xmf3MoveTarget = blackboard.Hearing.xmf3SoundPosition;

		pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, true);
	}
	else
	{
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, true);
	}

	return true;
}

void EnemyInvestigate::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->IsDying())
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();
	EnemyHearingMemory& hearing = blackboard.Hearing;

	if (!hearing.bHasSound)
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return;
	}

	// 소리가 난 위치까지 도착했으면 잠시 주변을 확인한다.
	if (m_bReachedTarget)
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, false);
		pEnemy->UpdateIdleLook(fTimeElapsed);

		m_fWaitTimer += fTimeElapsed;

		if (m_fWaitTimer >= m_fWaitDuration)
		{
			hearing.Reset();

			blackboard.bHasMoveTarget = false;

			pEnemy->ClearPath();
		}

		return;
	}

	XMFLOAT3 currentPos = pEnemy->GetPosition();
	XMFLOAT3 soundPos = hearing.xmf3SoundPosition;

	XMFLOAT3 toSound = Vector3::Subtract(soundPos, currentPos);
	toSound.y = 0.0f;

	float distance = Vector3::Length(toSound);

	if (distance < 0.9f)
	{
		m_bReachedTarget = true;
		m_fWaitTimer = 0.0f;

		blackboard.bHasMoveTarget = false;

		pEnemy->ClearPath();
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->SetEnemyAnimation(pEnemy->GetIdleAnimationByWeapon(), true, true);

		return;
	}

	blackboard.bHasMoveTarget = true;
	blackboard.xmf3MoveTarget = soundPos;

	pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, false);

	bool hasPath = pEnemy->UpdatePathToPosition(soundPos, fTimeElapsed);

	if (!hasPath)
	{
		hearing.Reset();

		blackboard.bHasMoveTarget = false;

		pEnemy->ClearPath();
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

		return;
	}

	if (!pEnemy->FollowCurrentPath())
	{
		hearing.Reset();

		blackboard.bHasMoveTarget = false;

		pEnemy->ClearPath();
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	}
}

void EnemyInvestigate::Exit(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.Hearing.Reset();
	blackboard.bHasMoveTarget = false;

	m_bReachedTarget = false;
	m_fWaitTimer = 0.0f;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyRun::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Chase;
	blackboard.bHasMoveTarget = false;

	m_fFootstepTimer = 0.0f;

	pEnemy->ClearPath();
	pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, true);

	return true;
}

void EnemyRun::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->m_bDying)
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	m_fFootstepTimer += fTimeElapsed;

	if (m_fFootstepTimer > 0.5f)
	{
		SoundManager::Instance()->Play(SoundName::ENEMY_FOOSTEP, pEnemy->GetPosition());
		m_fFootstepTimer -= 0.5f;
	}

	XMFLOAT3 targetPos;

	if (blackboard.bCanSeeTarget && blackboard.pTarget)
	{
		targetPos = blackboard.pTarget->GetPosition();
	}
	else if (blackboard.bHasLastSeenPosition)
	{
		targetPos = blackboard.xmf3LastSeenPosition;
	}
	else
	{
		blackboard.bHasMoveTarget = false;
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return;
	}

	blackboard.bHasMoveTarget = true;
	blackboard.xmf3MoveTarget = targetPos;

	bool hasPath = pEnemy->UpdatePathToPosition(blackboard.xmf3MoveTarget, fTimeElapsed);

	if (hasPath && pEnemy->FollowCurrentPath())
	{
		return;
	}

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

void EnemyRun::Exit(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return;

	pEnemy->GetBlackboard().bHasMoveTarget = false;

	m_fFootstepTimer = 0.0f;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyAttack::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Attack;
	blackboard.bHasMoveTarget = false;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	pEnemy->m_fAimTimer = 0.0f;
	pEnemy->m_fAttackCooldown = 0.0f;

	pEnemy->m_nBurstShotsLeft = 0;
	pEnemy->m_fBurstShotTimer = 0.0f;
	pEnemy->m_fBurstRestTimer = 0.0f;

	pEnemy->m_fStrafeTimer = pEnemy->m_fStrafeDuration;
	pEnemy->m_fStrafeSign *= -1.0f;

	pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, true);

	return true;
}

void EnemyAttack::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->m_bDying)
		return;

	if (NetworkManager::Instance().IsConnected())
	{
		if (pEnemy->IsReloading())
		{
			pEnemy->UpdateReload(fTimeElapsed);
			pEnemy->SetEnemyAnimation(pEnemy->GetReloadAnimationByWeapon(), false, false);
			return;
		}

		if (pEnemy->m_fShootAnimTimer > 0.0f)
		{
			pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);
			return;
		}

		XMFLOAT3 toServer;
		toServer.x = pEnemy->m_xmf3ServerPosition.x - pEnemy->m_xmf3Position.x;
		toServer.y = 0.0f;
		toServer.z = pEnemy->m_xmf3ServerPosition.z - pEnemy->m_xmf3Position.z;

		float moveLen = Vector3::Length(toServer);

		constexpr float MOVE_ANIM_THRESHOLD = 0.05f;		// 임시

		if (moveLen < MOVE_ANIM_THRESHOLD)
		{
			pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);
		}
		else
		{
			XMFLOAT3 moveDir = NormalizeXZ(toServer);
			XMFLOAT3 forward = pEnemy->GetForwardXZ();
			XMFLOAT3 right = GetRightFromForwardXZ(forward);

			float fwdDot = moveDir.x * forward.x + moveDir.z * forward.z;
			float rightDot = moveDir.x * right.x + moveDir.z * right.z;

			if (fabsf(rightDot) > fabsf(fwdDot))
			{
				if (rightDot >= 0.0f)
					pEnemy->SetEnemyAnimation(pEnemy->GetRightRunAnimationByWeapon(), true, false);
				else
					pEnemy->SetEnemyAnimation(pEnemy->GetLeftRunAnimationByWeapon(), true, false);
			}
			else
			{
				pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, false);
			}
		}

		return;
	}

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	if (blackboard.bCanSeeTarget && blackboard.pTarget)
	{
		pEnemy->FaceToPosition(blackboard.pTarget->GetPosition());
	}
	else if (blackboard.bHasLastSeenPosition)
	{
		pEnemy->FaceToPosition(blackboard.xmf3LastSeenPosition);
	}

	if (pEnemy->NeedsReload())
	{
		pEnemy->StartReload();
		return;
	}

	// 사격 전 조준 딜레이
	if (pEnemy->m_fAimTimer < pEnemy->m_fAimDelay)
	{
		pEnemy->m_fAimTimer += fTimeElapsed;
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);
		return;
	}

	if (!blackboard.bCanShootTarget)
	{
		pEnemy->m_fStrafeTimer -= fTimeElapsed;

		if (pEnemy->m_fStrafeTimer <= 0.0f)
		{
			pEnemy->m_fStrafeTimer = pEnemy->m_fStrafeDuration;
			pEnemy->m_fStrafeSign *= -1.0f;
		}

		if (blackboard.bCanSeeTarget)
		{
			pEnemy->UpdateCombatMove(fTimeElapsed);
		}
		else
		{
			pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
			pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);
		}

		return;
	}

	if (pEnemy->m_fBurstRestTimer > 0.0f)
	{
		pEnemy->m_fBurstRestTimer -= fTimeElapsed;
		pEnemy->m_fStrafeTimer -= fTimeElapsed;

		if (pEnemy->m_fStrafeTimer <= 0.0f)
		{
			pEnemy->m_fStrafeTimer = pEnemy->m_fStrafeDuration;
			pEnemy->m_fStrafeSign *= -1.0f;
		}

		pEnemy->UpdateCombatMove(fTimeElapsed);

		if (pEnemy->m_fBurstRestTimer <= 0.0f)
		{
			pEnemy->m_fAimTimer = 0.0f;
			pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
			pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);
		}

		return;
	}

	if (pEnemy->m_nBurstShotsLeft <= 0)
	{
		pEnemy->StartNewBurst();
	}

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->SetEnemyAnimation(pEnemy->GetAttackAnimationByWeapon(), true, false);

	pEnemy->m_fBurstShotTimer -= fTimeElapsed;

	if (pEnemy->m_fBurstShotTimer <= 0.0f)
	{
		pEnemy->FireAtPlayer();
		pEnemy->m_nBurstShotsLeft--;

		pEnemy->m_fBurstShotTimer = pEnemy->m_fBurstShotInterval;

		if (pEnemy->m_nBurstShotsLeft <= 0)
		{
			pEnemy->m_fBurstRestTimer = pEnemy->m_fBurstRestDuration;
			pEnemy->m_fStrafeTimer = pEnemy->m_fStrafeDuration;
			pEnemy->m_fStrafeSign *= -1.0f;
		}
	}
}

void EnemyAttack::Exit(CEnemyObject* pEnemy)
{
	pEnemy->m_fAimTimer = 0.0f;
	pEnemy->m_fAttackCooldown = 0.0f;

	pEnemy->m_nBurstShotsLeft = 0;
	pEnemy->m_fBurstShotTimer = 0.0f;
	pEnemy->m_fBurstRestTimer = 0.0f;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyReload::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Reload;
	blackboard.bHasMoveTarget = false;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	if (!pEnemy->IsReloading())
	{
		pEnemy->StartReload();
	}
	else
	{
		pEnemy->SetEnemyAnimation(pEnemy->GetReloadAnimationByWeapon(), false, false);
	}

	return true;
}

void EnemyReload::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->m_bDying)
		return;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

	if (blackboard.bCanSeeTarget && blackboard.pTarget)
	{
		pEnemy->FaceToPosition(blackboard.pTarget->GetPosition());
	}

	pEnemy->UpdateReload(fTimeElapsed);
}

void EnemyReload::Exit(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyReturn::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Return;
	blackboard.bHasMoveTarget = true;
	blackboard.xmf3MoveTarget = pEnemy->GetSpawnPosition();

	pEnemy->updateTimer = pEnemy->findTime;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

	pEnemy->SetEnemyAnimation(pEnemy->GetForwardRunAnimationByWeapon(), true, true);

	return true;
}

void EnemyReturn::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy)
		return;

	if (pEnemy->m_bDying)
		return;

	if (NetworkManager::Instance().IsConnected())
	{
		return;
	}

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	XMFLOAT3 spawnPos = pEnemy->GetSpawnPosition();

	blackboard.bHasMoveTarget = true;
	blackboard.xmf3MoveTarget = spawnPos;

	if (pEnemy->IsNearSpawn())
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ClearPath();

		pEnemy->SetPosition(spawnPos.x, spawnPos.y, spawnPos.z);

		blackboard.ResetTargetMemory();
		blackboard.fReturnIgnoreTimer = pEnemy->m_fReturnIgnoreDuration;
		blackboard.bHasMoveTarget = false;

		return;
	}

	bool hasPath = pEnemy->UpdatePathToPosition(blackboard.xmf3MoveTarget, fTimeElapsed);

	if (hasPath && pEnemy->FollowCurrentPath())
	{
		return;
	}

	XMFLOAT3 dirToSpawn = pEnemy->GetDirectionToSpawnXZ();

	if (Vector3::Length(dirToSpawn) > 0.0001f)
	{
		pEnemy->SetMoveDir(dirToSpawn);
		pEnemy->FaceToPosition(spawnPos);
	}
	else
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	}
}

void EnemyReturn::Exit(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return;

	pEnemy->GetBlackboard().bHasMoveTarget = false;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyDie::Enter(CEnemyObject* pEnemy)
{
	if (!pEnemy)
		return false;

	EnemyBlackboard& blackboard = pEnemy->GetBlackboard();

	blackboard.eCurrentAction = EnemyBehaviorAction::Die;
	blackboard.bHasMoveTarget = false;
	blackboard.bCanSeeTarget = false;
	blackboard.bCanShootTarget = false;

	pEnemy->isColl = false;
	pEnemy->m_bDying = true;
	pEnemy->m_fDieElapsed = 0.0f;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	pEnemy->SetEnemyAnimation(pEnemy->GetDieAnimationByWeapon(), false, true);

	return true;
}

void EnemyDie::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	pEnemy->m_fDieElapsed += fTimeElapsed;

	if (!pEnemy->m_bDeadRemoveRequested && pEnemy->m_fDieElapsed >= pEnemy->m_fDieDuration)
	{
		pEnemy->m_bDeadRemoveRequested = true;
	}
}

void EnemyDie::Exit(CEnemyObject* pEnemy)
{
}

//루팅 오브젝트
CLootContainerObject::CLootContainerObject(float fLifeTime)
{
	m_fRemainTime = fLifeTime;
}

void CLootContainerObject::UpdateLifetime(float fTimeElapsed)		// 서버 권위 구조로 바꾸면서 안씀
{
	if (!IsAlive()) return;

	m_fRemainTime -= fTimeElapsed;
	if (m_fRemainTime <= 0.0f)
	{
		Kill();
	}
}

/*bool CLootContainerObject::AddLoot(unique_ptr<Item> item, int count)
{
	if (!item || count <= 0) return false;

	for (int i = 0; i < MAX_LOOT_SLOTS; ++i)
	{
		if (!m_LootItems[i])
		{
			m_LootItems[i] = move(item);
			m_LootCounts[i] = count;
			return true;
		}
	}

	return false;
}*/

void CLootContainerObject::SetSlotData(int idx, ItemID item, int count)
{
	if (idx < 0 || idx >= MAX_LOOT_SLOTS) return;
	m_slots[idx].item = item;
	m_slots[idx].count = count;
}

void CLootContainerObject::FillInventoryUI(Inventory* pInventory) const
{
	if (!pInventory) return;
	pInventory->ClearItems();

	for (int i = 0; i < MAX_LOOT_SLOTS; ++i) {
		if (m_slots[i].item != ItemID::NONE && m_slots[i].count > 0) {
			pInventory->ApplyServerSlotUpdate(i, m_slots[i].item, m_slots[i].count);
		}
	}
}

/*void CLootContainerObject::FillInventoryUI(Inventory* pInventory) const
{
	if (!pInventory) return;

	pInventory->ClearItems();

	for (int i = 0; i < MAX_LOOT_SLOTS; ++i)
	{
		if (m_LootItems[i] && m_LootCounts[i] > 0)
		{
			//pInventory->AddItem(std::move(m_LootItems[i]), m_LootCounts[i]);
		}
	}
}*/

float CLootContainerObject::GetDistanceSq(const XMFLOAT3& pos)
{
	XMFLOAT3 myPos = GetPosition();
	float dx = myPos.x - pos.x;
	float dy = myPos.y - pos.y;
	float dz = myPos.z - pos.z;
	return dx * dx + dy * dy + dz * dz;
}

bool CLootContainerObject::SetVisualModel(CGameObject* pModelInstance)
{
	if (!pModelInstance)
		return false;

	SetChild(pModelInstance, true);

	SetOOBB(NULL);

	return true;
}

void CEnemyObject::SetServerPosition(const XMFLOAT3& pos)
{
	m_xmf3ServerPosition = pos;
	m_bUseServerLerp = true;
}

void CEnemyObject::SetServerYaw(float yawRad)
{
	float yawDeg = XMConvertToDegrees(yawRad);

	SetYawDeg(yawDeg);
}

void CEnemyObject::SnapToServerPosition()
{
	m_xmf3Position = m_xmf3ServerPosition;

	m_xmf4x4ToParent._41 = m_xmf3Position.x;
	m_xmf4x4ToParent._42 = m_xmf3Position.y;
	m_xmf4x4ToParent._43 = m_xmf3Position.z;
}

