#include "OtherPlayer.h"
#include "ResourceManager.h"
#include"SoundManager.h"
#include"Shader.h"

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

OtherPlayer::OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", NULL);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pPlayerModel->m_pModelRootObject, true);

	EquipDefaultPistol();

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel);

	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, PLAYER_RIFLE_SMG_IDLE);
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

	// 서버 위치 보간 (NPC와 동일, ALPHA= 0.08)
	if (m_bUseServerLerp)
	{
		constexpr float ALPHA = 0.08f;
		XMFLOAT3 cur = GetPosition();
		cur.x += (m_xmf3ServerPosition.x - cur.x) * ALPHA;
		cur.y += (m_xmf3ServerPosition.y - cur.y) * ALPHA;
		cur.z += (m_xmf3ServerPosition.z - cur.z) * ALPHA;
		SetPosition(cur);

		m_xmf4x4ToParent = Matrix4x4::Rotate(0.0f, m_fServerYawDeg, 0.0f);
		m_xmf4x4ToParent._41 = cur.x;
		m_xmf4x4ToParent._42 = cur.y;
		m_xmf4x4ToParent._43 = cur.z;
	}

	UpdateTransform(NULL);

	if (m_pRenderWeapon && m_pWeaponSocket)
	{
		m_pRenderWeapon->m_xmf4x4ToParent = m_pWeaponSocket->m_xmf4x4World;
		m_pRenderWeapon->UpdateTransform(NULL);
	}
	else if (m_pWeapon && m_pWeaponSocket)
	{
		m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
	}
}

void OtherPlayer::Render(
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
		pCtrl->SetTrackAnimationSetIfChanged(0, PLAYER_RIFLE_SMG_IDLE);
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
	int nextAnim = PLAYER_RIFLE_SMG_RUN_F;
	static float timeacu = 0;
	timeacu += fTimeElapsed;
	if (timeacu > 0.5)
	{
		SoundManager::Instance().Play(SoundName::ENEMY_FOOSTEP, Player->GetPosition());
		timeacu -= 0.5;
	}

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
		pCtrl->SetTrackAnimationSetIfChanged(0, PLAYER_DIE);
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
	pOther->m_xmf3ServerPosition = XMFLOAT3(x, y, z);   // 보간 목표 초기화(첫 프레임 튐 방지)
	pOther->m_bUseServerLerp = true;
	pOther->SetOOBB(NULL);
	return pOther;
}

void OtherPlayer::UpdatePosition(float x, float y, float z) 
{
	m_xmf3ServerPosition = XMFLOAT3(x, y, z);   // 보간 목표
	m_bUseServerLerp = true;
}

void OtherPlayer::SetServerYaw(float yawRad)
{
	m_fServerYawDeg = XMConvertToDegrees(yawRad);
}

void OtherPlayer::EquipDefaultPistol()
{
	CGameObject* pPistolPrototype =
		ResourceManager::Instance().GetModelPrototype(ModelName::PISTOL);

	if (!pPistolPrototype)
	{
		OutputDebugString(L"[OtherPlayer] PISTOL prototype not found.\n");
		return;
	}

	CGameObject* pPistolInstance =
		CGameObject::CreateModelInstance(pPistolPrototype);

	if (!pPistolInstance)
	{
		OutputDebugString(L"[OtherPlayer] PISTOL instance create failed.\n");
		return;
	}

	static const char* s_ppRightHandNames[] =
	{
		"mixamorig:RightHand",
		"RightHand",
		"Bip001 R Hand",
		"mixamorig:RightHandIndex1"
	};

	CGameObject* pRightHand =
		FindFirstFrameByNames(this, s_ppRightHandNames, _countof(s_ppRightHandNames));

	if (!pRightHand)
	{
		OutputDebugString(L"[OtherPlayer] RightHand frame not found. Pistol not equipped.\n");
		delete pPistolInstance;
		return;
	}

	m_pWeapon = pPistolInstance;
	m_pWeaponSocket = pRightHand;


	m_pWeapon->SetPosition(-0.14f, 0.20f, 0.16f);
	m_pWeapon->SetScale(0.85f, 0.85f, 0.85f);
	m_pWeapon->Rotate(-90.0f, -90.0f, 28.0f);

	m_pWeaponMuzzleSocket = m_pWeapon->FindFrame("Socket_Muzzle");

	if (m_pWeaponMuzzleSocket)
	{
		OutputDebugString(L"[OtherPlayer] Socket_Muzzle found.\n");
	}
	else
	{
		OutputDebugString(L"[OtherPlayer] Socket_Muzzle not found.\n");
	}

	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
}

void OtherPlayer::SubmitWeaponToShader(CShader* shader)
{
	shader->addObjects(m_pRenderWeapon);
}
