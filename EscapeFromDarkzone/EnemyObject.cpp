#include "stdafx.h"
#include "EnemyObject.h"
#include "Player.h"
#include "OtherPlayer.h"
#include "AI.h"
#include "Shader.h"

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

CEnemyObject::CEnemyObject(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pShader,
	CLoadedModelInfo* pEnemyModel)
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

	SetChild(pEnemyModel->m_pModelRootObject, true);

	m_pSkinnedAnimationController = new CAnimationController(
		pd3dDevice,
		pd3dCommandList,
		1,
		pEnemyModel
	);

	m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (pEnemyModel)
	{
		delete pEnemyModel;
	}

	ChangeState(std::make_unique<EnemyIdle>());
}

CEnemyObject::~CEnemyObject()
{

}

void CEnemyObject::ChangeState(std::unique_ptr<State<CEnemyObject>> pNewState)
{
	if (!pNewState) return;

	if (m_pState && typeid(*m_pState) == typeid(*pNewState)) return;

	if (m_pState) m_pState->Exit(this);

	m_pState = std::move(pNewState);
	m_pState->Enter(this);
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

	if (m_pState)
	{
		m_pState->Update(this, fTimeElapsed);
	}

	//if (not Alive)return;

	if (m_bDying)
	{
		m_xmf4x4ToParent._41 = m_xmf3Position.x;
		m_xmf4x4ToParent._42 = m_xmf3Position.y;
		m_xmf4x4ToParent._43 = m_xmf3Position.z;
		UpdateTransform(NULL);
		return;
	}

	if (!IsAlive()) return;

	XMFLOAT3 direction = m_xmf3MoveDir;
	m_xmf3Velocity = Vector3::ScalarProduct(direction, m_fMoveSpeed, false);

	XMFLOAT3 shift = Vector3::ScalarProduct(m_xmf3Velocity, fTimeElapsed, false);
	m_xmf3Position = Vector3::Add(m_xmf3Position, shift);

	// 05.10 추가 (보간)
	if (m_bUseServerLerp) {
		//constexpr float ALPHA = 0.15f;  // 60FPS 기준 매 프레임 15% 보정
		constexpr float ALPHA = 0.08f;
		m_xmf3Position.x += (m_xmf3ServerPosition.x - m_xmf3Position.x) * ALPHA;
		m_xmf3Position.y += (m_xmf3ServerPosition.y - m_xmf3Position.y) * ALPHA;
		m_xmf3Position.z += (m_xmf3ServerPosition.z - m_xmf3Position.z) * ALPHA;
	}

	m_xmf4x4ToParent._41 = m_xmf3Position.x;
	m_xmf4x4ToParent._42 = m_xmf3Position.y;
	m_xmf4x4ToParent._43 = m_xmf3Position.z;

	UpdateTransform(NULL);
}

void CEnemyObject::HandleCollision(XMFLOAT3 normal)
{
	if (normal.y > 0.5f)
	{
		if (m_xmf3Velocity.y < 0.0f) m_xmf3Velocity.y = 0.0f;
		return;
	}

	XMVECTOR vNormal = XMLoadFloat3(&normal);
	XMVECTOR vVelocity = XMLoadFloat3(&m_xmf3Velocity);
	XMVECTOR vCurrPos = XMLoadFloat3(&m_xmf3Position);
	XMVECTOR vPrevPos = XMLoadFloat3(&m_xmf3PrevPos);

	XMVECTOR vMoveDelta = vCurrPos - vPrevPos;
	XMVECTOR vDot = XMVector3Dot(vMoveDelta, vNormal);
	float fPenetrationDepth = XMVectorGetX(vDot);

	if (fPenetrationDepth < 0.0f)
	{
		XMVECTOR vCorrection = vNormal * (fabs(fPenetrationDepth) + 0.0001f);

		vCurrPos += vCorrection;
		XMStoreFloat3(&m_xmf3Position, vCurrPos);

		CGameObject::SetPosition(m_xmf3Position);
		UpdateTransform(NULL);
	}

	XMVECTOR vVelDot = XMVector3Dot(vVelocity, vNormal);
	float fVelDot = XMVectorGetX(vVelDot);

	if (fVelDot < 0.0f)
	{
		XMVECTOR vSlideVel = vVelocity - (vNormal * fVelDot);

		if (XMVectorGetX(XMVector3Length(vSlideVel)) < 0.01f)
		{
			vSlideVel = XMVectorZero();
		}
		XMStoreFloat3(&m_xmf3Velocity, vSlideVel);
	}
}

void CEnemyObject::SetPosition(float x, float y, float z)
{
	CGameObject::SetPosition(x, y, z);
	m_xmf3Position.x = x;
	m_xmf3Position.y = y;
	m_xmf3Position.z = z;

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

	XMFLOAT4X4 rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);

	rotate._41 = m_xmf3Position.x;
	rotate._42 = m_xmf3Position.y;
	rotate._43 = m_xmf3Position.z;

	m_xmf4x4ToParent = rotate;
}

bool CEnemyObject::UpdatePathToPosition(const XMFLOAT3& targetPos, float fTimeElapsed)
{
	updateTimer += fTimeElapsed;

	if (updateTimer < findTime)
	{
		// 수정: 아직 재탐색 시간이 아니면 기존 경로가 남아있는지 반환
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

void CEnemyObject::UpdateCombatMove(float fTimeElapsed)
{
	UNREFERENCED_PARAMETER(fTimeElapsed);

	if (!m_pPlayer)
	{
		SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return;
	}

	XMFLOAT3 toPlayer = GetDirectionToPlayerXZ();
	float distToPlayer = GetDistanceToPlayerXZ();

	if (Vector3::Length(toPlayer) < 0.0001f)
	{
		SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		return;
	}

	XMFLOAT3 moveDir = XMFLOAT3(0.0f, 0.0f, 0.0f);

	if (distToPlayer < m_fTooCloseRange)
	{
		moveDir = Vector3::ScalarProduct(toPlayer, -m_fCombatMoveSpeedMultiplier, false);
	}
	else
	{
		XMFLOAT3 right = GetRightFromForwardXZ(toPlayer);

		moveDir = Vector3::ScalarProduct(
			right,
			m_fStrafeSign * m_fCombatMoveSpeedMultiplier,
			false
		);
	}

	SetMoveDir(moveDir);
}

void CEnemyObject::FireAtPlayer()
{
	if (!m_pPlayer) return;
	if (m_bDying) return;

	// 나중에 여기에서 총알, 레이캐스트, 서버 패킷, 이펙트 등을 연결하면 됨.
	OutputDebugString(L"[Enemy AI] Enemy Burst Fire\n");	// 수정: 버스트 사격 구조로 바뀐 것을 확인하기 위한 출력
}

bool EnemyIdle::Enter(CEnemyObject* pEnemy)
{
	pEnemy->SetMoveDir(XMFLOAT3(0, 0, 0));
	pEnemy->ClearPath();

	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_IDLE);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);

	return true;
}

void EnemyIdle::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy->m_pPlayer) return;
	if (pEnemy->m_bDying) return;

	if (pEnemy->m_fReturnIgnoreTimer > 0.0f)
	{
		pEnemy->m_fReturnIgnoreTimer -= fTimeElapsed;
		return;
	}

	pEnemy->m_fThinkTimer += fTimeElapsed;

	if (pEnemy->m_fThinkTimer < pEnemy->m_fThinkInterval)
		return;

	pEnemy->m_fThinkTimer = 0.0f;

	if (!pEnemy->IsPlayerInDetectRange())
		return;

	if (pEnemy->IsPlayerInAttackRange())
	{
		pEnemy->ChangeState(std::make_unique<EnemyAttack>());
	}
	else
	{
		pEnemy->ChangeState(std::make_unique<EnemyRun>());
	}
}

void EnemyIdle::Exit(CEnemyObject* pEnemy)
{
}

bool EnemyRun::Enter(CEnemyObject* pEnemy)
{
	pEnemy->m_fThinkTimer = 0.0f;
	pEnemy->ClearPath();

	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_RUN);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);

	return true;
}

void EnemyRun::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy->m_pPlayer) return;
	if (pEnemy->m_bDying) return;

	if (pEnemy->IsOutsideLeashRange())
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ClearPath();
		pEnemy->ChangeState(std::make_unique<EnemyReturn>());
		return;
	}

	pEnemy->m_fThinkTimer += fTimeElapsed;

	if (pEnemy->m_fThinkTimer >= pEnemy->m_fThinkInterval)
	{
		pEnemy->m_fThinkTimer = 0.0f;

		if (pEnemy->IsPlayerInAttackRange())
		{
			pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
			pEnemy->ClearPath();
			pEnemy->ChangeState(std::make_unique<EnemyAttack>());
			return;
		}

		if (!pEnemy->IsPlayerInDetectRange())
		{
			pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
			pEnemy->ClearPath();
			pEnemy->ChangeState(std::make_unique<EnemyReturn>());
			return;
		}
	}

	XMFLOAT3 playerPos = pEnemy->m_pPlayer->GetPosition();

	bool hasPath = pEnemy->UpdatePathToPosition(playerPos, fTimeElapsed);

	if (hasPath && pEnemy->FollowCurrentPath())
	{
		return;
	}

	XMFLOAT3 dirToPlayer = pEnemy->GetDirectionToPlayerXZ();

	if (Vector3::Length(dirToPlayer) > 0.0001f)
	{
		pEnemy->SetMoveDir(dirToPlayer);
		pEnemy->FaceToPosition(playerPos);
	}
	else
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	}
}

void EnemyRun::Exit(CEnemyObject* pEnemy)
{
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyAttack::Enter(CEnemyObject* pEnemy)
{
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	pEnemy->m_fAimTimer = 0.0f;
	pEnemy->m_fAttackCooldown = 0.0f;
	pEnemy->m_fThinkTimer = 0.0f;

	pEnemy->m_nBurstShotsLeft = 0;
	pEnemy->m_fBurstShotTimer = 0.0f;
	pEnemy->m_fBurstRestTimer = 0.0f;

	pEnemy->m_fStrafeTimer = pEnemy->m_fStrafeDuration;
	pEnemy->m_fStrafeSign *= -1.0f;

	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	// 현재 적에게 별도 사격 애니메이션이 없으므로 일단 Idle로 조준 자세 대체
	pController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_IDLE);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);

	return true;
}

void EnemyAttack::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy->m_pPlayer) return;
	if (pEnemy->m_bDying) return;

	if (pEnemy->IsOutsideLeashRange())
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ChangeState(std::make_unique<EnemyReturn>());
		return;
	}

	XMFLOAT3 playerPos = pEnemy->m_pPlayer->GetPosition();
	pEnemy->FaceToPosition(playerPos);

	pEnemy->m_fThinkTimer += fTimeElapsed;

	if (pEnemy->m_fThinkTimer >= pEnemy->m_fThinkInterval)
	{
		pEnemy->m_fThinkTimer = 0.0f;

		if (pEnemy->IsPlayerOutOfAttackRange())
		{
			pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
			pEnemy->ChangeState(std::make_unique<EnemyRun>());
			return;
		}
	}

	// 사격 전 조준 딜레이
	if (pEnemy->m_fAimTimer < pEnemy->m_fAimDelay)
	{
		pEnemy->m_fAimTimer += fTimeElapsed;
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
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
		}

		return;
	}

	if (pEnemy->m_nBurstShotsLeft <= 0)
	{
		pEnemy->StartNewBurst();
	}

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

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

bool EnemyReturn::Enter(CEnemyObject* pEnemy)
{
	pEnemy->m_fThinkTimer = 0.0f;
	pEnemy->updateTimer = pEnemy->findTime;

	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));

	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_RUN);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);

	return true;
}

void EnemyReturn::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (pEnemy->m_bDying) return;

	XMFLOAT3 spawnPos = pEnemy->GetSpawnPosition();

	if (pEnemy->IsNearSpawn())
	{
		pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
		pEnemy->ClearPath();

		pEnemy->SetPosition(spawnPos.x, spawnPos.y, spawnPos.z);

		pEnemy->m_fReturnIgnoreTimer = pEnemy->m_fReturnIgnoreDuration;

		pEnemy->ChangeState(std::make_unique<EnemyIdle>());
		return;
	}

	bool hasPath = pEnemy->UpdatePathToPosition(spawnPos, fTimeElapsed);

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
	pEnemy->ClearPath();
	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

bool EnemyDie::Enter(CEnemyObject* pEnemy)
{
	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pEnemy->m_bDying = true;
	pEnemy->m_fDieElapsed = 0.0f;

	pEnemy->SetMoveDir(XMFLOAT3(0.0f, 0.0f, 0.0f));
	pEnemy->ClearPath();

	pController->SetTrackType(0, ANIMATION_TYPE_ONCE);
	pController->SetTrackAnimationSetIfChanged(0, ENEMY_ANIM_DIE);
	pController->SetTrackWeight(0, 1.0f);
	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);

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

	XMFLOAT4X4 rotate = Matrix4x4::Rotate(0.0f, yawDeg, 0.0f);

	rotate._41 = m_xmf3Position.x;
	rotate._42 = m_xmf3Position.y;
	rotate._43 = m_xmf3Position.z;

	m_xmf4x4ToParent = rotate;
}

void CEnemyObject::SnapToServerPosition()
{
	m_xmf3Position = m_xmf3ServerPosition;

	m_xmf4x4ToParent._41 = m_xmf3Position.x;
	m_xmf4x4ToParent._42 = m_xmf3Position.y;
	m_xmf4x4ToParent._43 = m_xmf3Position.z;
}