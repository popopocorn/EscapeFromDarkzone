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
static void DeleteOtherPlayerObjectTree(CGameObject* pObject)
{
	if (!pObject)
		return;

	CGameObject* pChild = pObject->m_pChild;

	while (pChild)
	{
		CGameObject* pNext = pChild->m_pSibling;

		pChild->m_pSibling = nullptr;
		pChild->m_pParent = nullptr;

		DeleteOtherPlayerObjectTree(pChild);

		pChild = pNext;
	}

	pObject->m_pChild = nullptr;
	pObject->m_pSibling = nullptr;
	pObject->m_pParent = nullptr;

	delete pObject;
}
static ModelName GetOtherPlayerWeaponModelNameFromPacket(short weaponType)
{
	ItemType itemType = static_cast<ItemType>(weaponType);

	switch (itemType)
	{
	case ItemType::PISTOL:
		return ModelName::PISTOL;

	case ItemType::SMG:
		return ModelName::SMG;

	case ItemType::SHOTGUN:
		return ModelName::SHOTGUN;

	case ItemType::RIFLE:
	default:
		return ModelName::RIFLE;
	}
}
static void GetOtherPlayerWeaponVisualConfig(ModelName modelName, XMFLOAT3& outPos, XMFLOAT3& outRot, XMFLOAT3& outScale)
{
	switch (modelName)
	{
	//권총은 애니메이션 추가된 후 추가 수정
	case ModelName::PISTOL:
		outPos = XMFLOAT3(-0.0f, 0.0f, 0.05f);
		outRot = XMFLOAT3(180.0f, 180.0f, 0.0f);
		outScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		break;

	case ModelName::SMG:
		outPos = XMFLOAT3(0.1f, 0.10f, 0.16f);
		outRot = XMFLOAT3(105.0f, 0.0f, 0.0f);
		outScale = XMFLOAT3(1.1f * 0.90f, 1.1f * 0.90f, 1.1f * 0.90f);
		break;

	case ModelName::SHOTGUN:
		outPos = XMFLOAT3(-0.00f, 0.15f, 0.15f);
		outRot = XMFLOAT3(180.0f, 0.0f, 105.0f);
		outScale = XMFLOAT3(1.1f, 1.1f, 1.1f);
		break;

	case ModelName::RIFLE:
	default:
		outPos = XMFLOAT3(-0.05f, 0.30f, 0.1f);
		outRot = XMFLOAT3(105.0f, 0.0f, 0.0f);
		outScale = XMFLOAT3(1.1f, 1.1f, 1.1f);
		break;
	}
}

OtherPlayer::OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	UNREFERENCED_PARAMETER(pd3dGraphicsRootSignature);

	CLoadedModelInfo* pPlayerModel = ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_02);

	if (!pPlayerModel)
	{
		return;
	}

	if (!pPlayerModel->m_pModelRootObject)
	{
		delete pPlayerModel;
		return;
	}

	SetChild(pPlayerModel->m_pModelRootObject, true);

	m_pRenderWeapon = new CGameObject();
	m_pRenderWeapon->SetOOBB(NULL);
	m_pRenderWeapon->isColl = false;

	EquipDefaultPistol();

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel);

	if (m_pSkinnedAnimationController)
	{
		m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
		m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, PLAYER_RIFLE_SMG_IDLE);
		m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
		m_pSkinnedAnimationController->SetTrackEnable(0, true);

		m_pSkinnedAnimationController->SetTrackType(1, ANIMATION_TYPE_LOOP);
		m_pSkinnedAnimationController->SetTrackEnable(1, false);
	}

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	delete pPlayerModel;

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
{
	
}

void OtherPlayerIdle::Exit(OtherPlayer* Player)
{
	
}

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
		SoundManager::Instance()->Play(SoundName::ENEMY_FOOSTEP, Player->GetPosition());
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
	EquipWeaponModel(ModelName::PISTOL);
}

void OtherPlayer::EquipWeaponModel(ModelName modelName)
{
	CGameObject* pWeaponPrototype = ResourceManager::Instance().GetModelPrototype(modelName);

	if (!pWeaponPrototype)
	{
		return;
	}

	CGameObject* pWeaponInstance = CGameObject::CreateModelInstance(pWeaponPrototype);

	if (!pWeaponInstance)
	{
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
		DeleteOtherPlayerObjectTree(pWeaponInstance);
		return;
	}

	if (!m_pRenderWeapon)
	{
		m_pRenderWeapon = new CGameObject();
		m_pRenderWeapon->isColl = false;
	}

	if (m_pWeapon)
	{
		if (m_pRenderWeapon->m_pChild == m_pWeapon)
		{
			m_pRenderWeapon->m_pChild = nullptr;
		}

		DeleteOtherPlayerObjectTree(m_pWeapon);
	}

	m_pWeapon = pWeaponInstance;
	m_pWeaponSocket = pRightHand;

	m_pWeapon->m_pParent = m_pRenderWeapon;
	m_pWeapon->m_pSibling = nullptr;
	m_pRenderWeapon->m_pChild = m_pWeapon;

	XMFLOAT3 weaponPos;
	XMFLOAT3 weaponRot;
	XMFLOAT3 weaponScale;

	GetOtherPlayerWeaponVisualConfig(modelName, weaponPos, weaponRot, weaponScale);

	m_pWeapon->SetPosition(weaponPos.x, weaponPos.y, weaponPos.z);
	m_pWeapon->SetScale(weaponScale.x, weaponScale.y, weaponScale.z);
	m_pWeapon->Rotate(weaponRot.x, weaponRot.y, weaponRot.z);

	m_pWeaponMuzzleSocket = m_pWeapon->FindFrame("Socket_Muzzle");

	m_pRenderWeapon->m_xmf4x4ToParent = m_pWeaponSocket->m_xmf4x4World;
	m_pRenderWeapon->ClearOOBB(true);
	m_pRenderWeapon->SetOOBB(NULL);
	m_pRenderWeapon->isColl = false;
	m_pRenderWeapon->UpdateTransform(NULL);
}

void OtherPlayer::ChangeWeaponFromServer(short weaponType, short weaponGrade)
{
	UNREFERENCED_PARAMETER(weaponGrade);

	ModelName modelName = GetOtherPlayerWeaponModelNameFromPacket(weaponType);
	EquipWeaponModel(modelName);
}

void OtherPlayer::SubmitWeaponToShader(CShader* shader)
{
	if (!shader || !m_pRenderWeapon)
		return;

	shader->addObjects(m_pRenderWeapon);
}
