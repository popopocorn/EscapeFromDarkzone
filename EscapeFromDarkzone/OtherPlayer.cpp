#include "OtherPlayer.h"
#include "ResourceManager.h"
#include"SoundManager.h"

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
static bool DetachChildTemporarily(
	CGameObject* pParent,
	CGameObject* pChild,
	CGameObject*& pOutPrev,
	CGameObject*& pOutSavedSibling)
{
	pOutPrev = nullptr;
	pOutSavedSibling = nullptr;

	if (!pParent || !pChild)
		return false;

	CGameObject* pCur = pParent->m_pChild;

	while (pCur && pCur != pChild)
	{
		pOutPrev = pCur;
		pCur = pCur->m_pSibling;
	}

	if (pCur != pChild)
		return false;

	pOutSavedSibling = pChild->m_pSibling;

	if (pOutPrev)
	{
		pOutPrev->m_pSibling = pOutSavedSibling;
	}
	else
	{
		pParent->m_pChild = pOutSavedSibling;
	}


	pChild->m_pSibling = nullptr;
	return true;
}
static void RestoreDetachedChild(
	CGameObject* pParent,
	CGameObject* pChild,
	CGameObject* pPrev,
	CGameObject* pSavedSibling)
{
	if (!pParent || !pChild)
		return;

	if (pPrev)
	{
		pPrev->m_pSibling = pChild;
	}
	else
	{
		pParent->m_pChild = pChild;
	}

	pChild->m_pSibling = pSavedSibling;
	pChild->m_pParent = pParent;
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

	UpdateTransform(NULL);
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

	if (!m_pWeapon || !m_pWeaponSocket)
	{
		CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);
		return;
	}

	CGameObject* pPrev = nullptr;
	CGameObject* pSavedSibling = nullptr;

	bool bDetached = DetachChildTemporarily(
		m_pWeaponSocket,
		m_pWeapon,
		pPrev,
		pSavedSibling
	);

	if (!bDetached)
	{
		CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);
		return;
	}

	CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);

	m_pWeapon->m_pParent = m_pWeaponSocket;
	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
	m_pWeapon->Render(pd3dCommandList, false, nPipelineState, pCamera);

	RestoreDetachedChild(
		m_pWeaponSocket,
		m_pWeapon,
		pPrev,
		pSavedSibling
	);
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

	m_pWeapon->m_pParent = m_pWeaponSocket;
	m_pWeapon->m_pSibling = m_pWeaponSocket->m_pChild;
	m_pWeaponSocket->m_pChild = m_pWeapon;

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