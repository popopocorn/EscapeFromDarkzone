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

//void CEnemyObject::Animate(float fTimeElapsed)
//{
//	CGameObject::Animate(fTimeElapsed);
//
//	if (!m_pPlayer) return;
//
//	XMFLOAT3 xmf3MyPos = GetPosition();
//	XMFLOAT3 xmf3PlayerPos = m_pPlayer->GetPosition();
//
//	XMFLOAT3 xmf3Dir = Vector3::Subtract(xmf3PlayerPos, xmf3MyPos);
//	xmf3Dir.y = 0.0f;
//	float fDistance = Vector3::Length(xmf3Dir);
//
//	CAnimationController* pController = m_pSkinnedAnimationController;
//
//	if (fDistance <= m_fDetectionRange && fDistance > m_fAttackRange)
//	{
//		XMFLOAT3 xmf3Look = Vector3::Normalize(xmf3Dir);
//
//		float fAngleRad = atan2(xmf3Look.x, xmf3Look.z);
//		float fAngleDeg = XMConvertToDegrees(fAngleRad);
//		XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);
//
//		m_xmf4x4ToParent = xmf4x4Rotate;
//		m_xmf4x4ToParent._41 = xmf3MyPos.x;
//		m_xmf4x4ToParent._42 = xmf3MyPos.y;
//		m_xmf4x4ToParent._43 = xmf3MyPos.z;
//
//		XMFLOAT3 xmf3Shift = Vector3::ScalarProduct(xmf3Look, m_fMoveSpeed * fTimeElapsed, false);
//		SetPosition(Vector3::Add(xmf3MyPos, xmf3Shift));
//
//		if (pController && !pController->m_pAnimationTracks[1].m_bEnable)
//		{
//			pController->SetTrackEnable(0, false);	// Idle 끄기
//			pController->SetTrackEnable(1, true);  // Run 켜기
//			pController->SetTrackPosition(1, 0.0f); // Run 처음부터 재생
//		}
//	}
//	else
//	{
//		if (pController && !pController->m_pAnimationTracks[0].m_bEnable)
//		{
//			pController->SetTrackEnable(1, false); // Run 끄기
//			pController->SetTrackEnable(0, true);  // Idle 켜기
//			pController->SetTrackPosition(0, 0.0f);
//		}
//	}
//}

void CEnemyObject::Animate(float fTimeElapsed)
{
	CGameObject::Animate(fTimeElapsed);

	if (m_pState)
	{
		m_pState->Update(this, fTimeElapsed);
	}
}

bool EnemyIdle::Enter(CEnemyObject* pEnemy)
{
	auto* pController = pEnemy->m_pSkinnedAnimationController;
	if (!pController) return false;

	// IDLE 켜기, RUN 끄기
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

	// RUN 켜기, IDLE 끄기
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

	if (fDistance > pEnemy->m_fDetectionRange)
	{
		pEnemy->ChangeState(std::make_unique<EnemyIdle>());
		return;
	}

	if (fDistance > pEnemy->m_fAttackRange)
	{
		XMFLOAT3 xmf3Look = Vector3::Normalize(xmf3Dir);

		float fAngleRad = atan2(xmf3Look.x, xmf3Look.z);
		float fAngleDeg = XMConvertToDegrees(fAngleRad);

		XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);
		pEnemy->m_xmf4x4ToParent = xmf4x4Rotate;

		pEnemy->m_xmf4x4ToParent._41 = xmf3MyPos.x;
		pEnemy->m_xmf4x4ToParent._42 = xmf3MyPos.y;
		pEnemy->m_xmf4x4ToParent._43 = xmf3MyPos.z;

		XMFLOAT3 xmf3Shift = Vector3::ScalarProduct(xmf3Look, pEnemy->m_fMoveSpeed * fTimeElapsed, false);
		pEnemy->SetPosition(Vector3::Add(xmf3MyPos, xmf3Shift));
	}
	else
	{
		pEnemy->ChangeState(std::make_unique<EnemyIdle>());
	}
}

void EnemyRun::Exit(CEnemyObject* pEnemy)
{
}