#include "stdafx.h"
#include "EnemyObject.h"
#include "Player.h"

CEnemyObject::CEnemyObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	CLoadedModelInfo* pEnemyModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", NULL);

	if (!pEnemyModel->m_pAnimationSets) pEnemyModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pEnemyModel->m_pModelRootObject, true);

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pEnemyModel);

	m_pSkinnedAnimationController->SetTrackAnimationSet(0, ENEMY_ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	m_pSkinnedAnimationController->SetTrackAnimationSet(1, ENEMY_ANIM_RUN);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (pEnemyModel) delete pEnemyModel;

	//SetOOBB(NULL);

	ChangeState(std::make_unique<EnemyIdle>());
}

CEnemyObject::~CEnemyObject()
{
}

void CEnemyObject::ChangeState(std::unique_ptr<EnemyState> pNewState)
{
	if (!pNewState) return;

	if (m_pState && typeid(*m_pState) == typeid(*pNewState)) return;

	if (m_pState) m_pState->Exit(this);

	m_pState = std::move(pNewState);
	m_pState->Enter(this);
}

void CEnemyObject::HandleHP(float value)
{
	if(hp>0) hp -= value;
	else {
		Alive = false;
		ChangeState(std::make_unique<EnemyDie>());
		OutputDebugString(L"Dead\n");
	}
	
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	CGameObject::Animate(fTimeElapsed);

	Update(fTimeElapsed);
}

void CEnemyObject::Update(float fTimeElapsed)
{
	m_xmf3PrevPos = m_xmf3Position;

	if ( m_pState)
	{
		m_pState->Update(this, fTimeElapsed);
	}
	if (not Alive)return;
	XMFLOAT3 direction = m_xmf3MoveDir;
	m_xmf3Velocity = Vector3::ScalarProduct(direction, m_fMoveSpeed, false);

	XMFLOAT3 shift = Vector3::ScalarProduct(m_xmf3Velocity, fTimeElapsed, false);
	m_xmf3Position = Vector3::Add(m_xmf3Position, shift);

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

bool EnemyIdle::Enter(CEnemyObject* pEnemy)
{
	pEnemy->SetMoveDir(XMFLOAT3(0, 0, 0));

	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackEnable(0, true);
	pController->SetTrackPosition(0, 0.0f);
	pController->SetTrackEnable(1, false);
	return true;
}

void EnemyIdle::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy->m_pPlayer) return;

	XMFLOAT3 xmf3MyPos = pEnemy->GetPosition();
	XMFLOAT3 xmf3PlayerPos = pEnemy->m_pPlayer->GetPosition();

	XMFLOAT3 xmf3Dist = Vector3::Subtract(xmf3PlayerPos, xmf3MyPos);
	float fDistance = Vector3::Length(xmf3Dist);

	if (fDistance <= pEnemy->m_fDetectionRange && fDistance > pEnemy->m_fAttackRange)
	{
		pEnemy->ChangeState(std::make_unique<EnemyRun>());
	}
}

void EnemyIdle::Exit(CEnemyObject* pEnemy)
{
}

bool EnemyRun::Enter(CEnemyObject* pEnemy)
{
	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackEnable(0, false);
	pController->SetTrackEnable(1, true);
	pController->SetTrackPosition(1, 0.0f);
	return true;
}

void EnemyRun::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
	if (!pEnemy->m_pPlayer) return;

	XMFLOAT3 xmf3MyPos = pEnemy->GetPosition();
	XMFLOAT3 xmf3PlayerPos = pEnemy->m_pPlayer->GetPosition();

	XMFLOAT3 xmf3Dir = Vector3::Subtract(xmf3PlayerPos, xmf3MyPos);
	xmf3Dir.y = 0.0f;
	float fDistance = Vector3::Length(xmf3Dir);

	if (fDistance > pEnemy->m_fDetectionRange || fDistance <= pEnemy->m_fAttackRange)
	{
		pEnemy->ChangeState(std::make_unique<EnemyIdle>());
		return;
	}

	XMFLOAT3 xmf3Look = Vector3::Normalize(xmf3Dir);

	float fAngleRad = atan2(xmf3Look.x, xmf3Look.z);
	float fAngleDeg = XMConvertToDegrees(fAngleRad);
	XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);

	pEnemy->m_xmf4x4ToParent = xmf4x4Rotate;
	pEnemy->m_xmf4x4ToParent._41 = xmf3MyPos.x;
	pEnemy->m_xmf4x4ToParent._42 = xmf3MyPos.y;
	pEnemy->m_xmf4x4ToParent._43 = xmf3MyPos.z;

	pEnemy->SetMoveDir(xmf3Look);
}

void EnemyRun::Exit(CEnemyObject* pEnemy)
{
}

bool EnemyDie::Enter(CEnemyObject* pEnemy)
{
	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	pController->SetTrackEnable(0, false);
	pController->SetTrackEnable(1, true);
	pController->SetTrackAnimationSet(1, 0);
	pController->SetTrackPosition(1, 0.0f);
	return true;
}

void EnemyDie::Update(CEnemyObject* pEnemy, float fTimeElapsed)
{
}

void EnemyDie::Exit(CEnemyObject* pEnemy)
{
}
