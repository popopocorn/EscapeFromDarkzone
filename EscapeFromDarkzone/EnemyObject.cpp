#include "stdafx.h"
#include "EnemyObject.h"
#include "Player.h"

CEnemyObject::CEnemyObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	CLoadedModelInfo* pEnemyModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/X_Bot.bin", NULL);

	if (!pEnemyModel->m_pAnimationSets) pEnemyModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pEnemyModel->m_pModelRootObject, true);

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pEnemyModel);

	m_pSkinnedAnimationController->SetTrackAnimationSet(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	m_pSkinnedAnimationController->SetTrackAnimationSet(1, ANIM_RUN);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (pEnemyModel) delete pEnemyModel;
}

CEnemyObject::~CEnemyObject()
{
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	CGameObject::Animate(fTimeElapsed);

	if (!m_pPlayer) return;

	XMFLOAT3 xmf3MyPos = GetPosition();
	XMFLOAT3 xmf3PlayerPos = m_pPlayer->GetPosition();

	XMFLOAT3 xmf3Dir = Vector3::Subtract(xmf3PlayerPos, xmf3MyPos);
	xmf3Dir.y = 0.0f;
	float fDistance = Vector3::Length(xmf3Dir);

	CAnimationController* pController = m_pSkinnedAnimationController;

	if (fDistance <= m_fDetectionRange && fDistance > m_fAttackRange)
	{
		XMFLOAT3 xmf3Look = Vector3::Normalize(xmf3Dir);

		float fAngleRad = atan2(xmf3Look.x, xmf3Look.z);
		float fAngleDeg = XMConvertToDegrees(fAngleRad);
		XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);

		m_xmf4x4ToParent = xmf4x4Rotate;
		m_xmf4x4ToParent._41 = xmf3MyPos.x;
		m_xmf4x4ToParent._42 = xmf3MyPos.y;
		m_xmf4x4ToParent._43 = xmf3MyPos.z;

		XMFLOAT3 xmf3Shift = Vector3::ScalarProduct(xmf3Look, m_fMoveSpeed * fTimeElapsed, false);
		SetPosition(Vector3::Add(xmf3MyPos, xmf3Shift));

		if (pController && !pController->m_pAnimationTracks[1].m_bEnable)
		{
			pController->SetTrackEnable(0, false);	// Idle 끄기
			pController->SetTrackEnable(1, true);  // Run 켜기
			pController->SetTrackPosition(1, 0.0f); // Run 처음부터 재생
		}
	}
	else
	{
		if (pController && !pController->m_pAnimationTracks[0].m_bEnable)
		{
			pController->SetTrackEnable(1, false); // Run 끄기
			pController->SetTrackEnable(0, true);  // Idle 켜기
			pController->SetTrackPosition(0, 0.0f);
		}
	}
}