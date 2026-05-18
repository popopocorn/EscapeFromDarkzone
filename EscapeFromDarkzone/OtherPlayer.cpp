#include "OtherPlayer.h"

OtherPlayer::OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", NULL);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pPlayerModel->m_pModelRootObject, true);

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel);

	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);


	if (pPlayerModel) delete pPlayerModel;

	ChangeState(std::make_unique<OtherPlayerIdle>());
}

OtherPlayer::~OtherPlayer()
{

}
void OtherPlayer::Animate(float fTimeElapsed)
{
	CGameObject::Animate(fTimeElapsed);
	Update(fTimeElapsed);

}

void OtherPlayer::Update(float fTimeElapsed)
{
	if (m_pState)
		m_pState->Update(this, fTimeElapsed);

	UpdateTransform(NULL);
}

void OtherPlayer::ChangeState(std::unique_ptr<State<OtherPlayer>> pNewState)
{
	if (!pNewState)
		return;

	if (m_pState && typeid(*m_pState) == typeid(*pNewState))
		return;

	if (m_pState)
		m_pState->Exit(this);

	m_pState = std::move(pNewState);

	m_pState->Enter(this);
}

bool OtherPlayerIdle::Enter(OtherPlayer* Player)
{
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackAnimationSetIfChanged(0, ANIM_IDLE);
	}
	return true;
}

void OtherPlayerIdle::Update(OtherPlayer* Player, float fTimeElapsed)
{}

void OtherPlayerIdle::Exit(OtherPlayer* Player)
{}

bool OtherPlayerRun::Enter(OtherPlayer* Player)
{
	return true;
}

void OtherPlayerRun::Update(OtherPlayer* Player, float fTimeElapsed)
{
	//int nextAnim = 0;
	int nextAnim = ANIM_RUN_F;
	
	//네트워크 전송시 플레이어의 방향(월드좌표가 아닌 화면기준 이동 방향)을 여기의 angle로 사용

	/*if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4)
		nextAnim = ANIM_RUN_F;
	else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4)
		nextAnim = ANIM_RUN_R;
	else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4)
		nextAnim = ANIM_RUN_L;
	else
		nextAnim = ANIM_RUN_B;*/

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackAnimationSetIfChanged(0, nextAnim);
	}
}

void OtherPlayerRun::Exit(OtherPlayer * Player)
{}

bool OtherPlayerDie::Enter(OtherPlayer* Player)
{
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackAnimationSetIfChanged(0, ANIM_DIE);
	}
	return true;
}

void OtherPlayerDie::Update(OtherPlayer* Player, float fTimeElapsed)
{}

void OtherPlayerDie::Exit(OtherPlayer * Player)
{}

OtherPlayer* OtherPlayer::Create(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature,
	float x, float y, float z)
{
	OtherPlayer* pOther = new OtherPlayer(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature); 
	pOther->SetPosition(XMFLOAT3(x, y, z));
	pOther->SetOOBB(NULL);
	return pOther;
}

void OtherPlayer::UpdatePosition(float x, float y, float z) 
{
	SetPosition(XMFLOAT3(x, y, z));
}

void OtherPlayer::SetServerYaw(float yawRad)
{
	float yawDeg = XMConvertToDegrees(yawRad);

	XMFLOAT3 pos = GetPosition();
	m_xmf4x4ToParent = Matrix4x4::Rotate(0.0f, yawDeg, 0.0f);
	m_xmf4x4ToParent._41 = pos.x;
	m_xmf4x4ToParent._42 = pos.y;
	m_xmf4x4ToParent._43 = pos.z;
}
