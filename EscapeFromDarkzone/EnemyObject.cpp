#include "stdafx.h"
#include "EnemyObject.h"
#include "Player.h"

CEnemyObject::CEnemyObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel)
{
	// 1. 모델 설정 (Child로 부착)
	// 로드된 모델의 Root를 이 객체의 자식으로 설정하여 렌더링되게 함
	if (pModel) SetChild(pModel->m_pModelRootObject, true);

	// 2. 애니메이션 컨트롤러 생성 (트랙 2개 할당: 0번은 기본, 1번은 블렌딩용 등 여유 있게)
	// X_Bot 모델의 애니메이션을 제어할 컨트롤러 생성
	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pModel);

	// 3. 초기 애니메이션 설정 (Idle)
	m_pSkinnedAnimationController->SetTrackAnimationSet(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	// 쉐이더 변수 생성
	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

CEnemyObject::~CEnemyObject()
{
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	// 부모 클래스 업데이트 (애니메이션 시간 진행 등 필수)
	CGameObject::Animate(fTimeElapsed);

	if (!m_pPlayer) return;

	// 1. 거리 및 방향 계산
	XMFLOAT3 xmf3MyPos = GetPosition();
	XMFLOAT3 xmf3PlayerPos = m_pPlayer->GetPosition();

	// 방향 벡터 (Target - My)
	XMFLOAT3 xmf3Dir = Vector3::Subtract(xmf3PlayerPos, xmf3MyPos);

	xmf3Dir.y = 0.0f;

	float fDistance = Vector3::Length(xmf3Dir);

	// 2. AI 로직: 추적
	CAnimationController* pController = m_pSkinnedAnimationController;

	if (fDistance <= m_fDetectionRange && fDistance > m_fAttackRange)
	{
		// (1) 방향 정규화
		XMFLOAT3 xmf3Look = Vector3::Normalize(xmf3Dir);

		// (2) 회전: 플레이어를 바라보게 설정
		float fAngleRad = atan2(xmf3Look.x, xmf3Look.z);
		float fAngleDeg = XMConvertToDegrees(fAngleRad);

		// 회전 매트릭스 생성 (Y축 회전)
		XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Rotate(0.0f, fAngleDeg, 0.0f);

		// 현재 위치 유지하면서 회전 적용
		// m_xmf4x4ToParent는 로컬 변환 행렬입니다. (회전 + 위치)
		m_xmf4x4ToParent = xmf4x4Rotate;
		m_xmf4x4ToParent._41 = xmf3MyPos.x;
		m_xmf4x4ToParent._42 = xmf3MyPos.y;
		m_xmf4x4ToParent._43 = xmf3MyPos.z;

		// (3) 이동
		XMFLOAT3 xmf3Shift = Vector3::ScalarProduct(xmf3Look, m_fMoveSpeed * fTimeElapsed, false);
		SetPosition(Vector3::Add(xmf3MyPos, xmf3Shift));

		// (4) 애니메이션 변경: Run
		if (pController && pController->m_pAnimationTracks[0].m_nAnimationSet != ANIM_RUN)
		{
			pController->SetTrackAnimationSet(0, ANIM_RUN);
		}
	}
	else
	{
		if (pController && pController->m_pAnimationTracks[0].m_nAnimationSet != ANIM_IDLE)
		{
			pController->SetTrackAnimationSet(0, ANIM_IDLE);
		}
	}

	// 3. 지형 높이 보정 (나중에 삭제 예정)
	if (m_pTerrain)
	{
		XMFLOAT3 pos = GetPosition();
		float fHeight = m_pTerrain->GetHeight(pos.x, pos.z);

		if (pos.y != fHeight)
		{
			pos.y = fHeight;
			SetPosition(pos);
		}
	}
}