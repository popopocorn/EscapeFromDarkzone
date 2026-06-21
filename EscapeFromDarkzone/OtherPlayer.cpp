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
		outPos = XMFLOAT3(-0.20f, 0.15f, 0.f);
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
static PlayerWeaponType GetOtherPlayerWeaponTypeFromPacket(short weaponType)
{
	ItemType itemType = static_cast<ItemType>(weaponType);

	switch (itemType)
	{
	case ItemType::PISTOL:
		return PlayerWeaponType::Pistol;

	case ItemType::SMG:
		return PlayerWeaponType::SMG;

	case ItemType::SHOTGUN:
		return PlayerWeaponType::Shotgun;

	case ItemType::RIFLE:
	default:
		return PlayerWeaponType::Rifle;
	}
}
static constexpr float OTHER_PLAYER_RUN_ANIM_SPEED = 1.2f;
static constexpr float OTHER_PLAYER_NORMAL_ANIM_SPEED = 1.0f;


OtherPlayer::OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, short playerID)
{
	UNREFERENCED_PARAMETER(pd3dGraphicsRootSignature);
	short a = (playerID % 2) + 1;
	CLoadedModelInfo* pPlayerModel;
	switch (a)
	{
	case 2:
		pPlayerModel = ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_02);
		break;
	case 3:
		pPlayerModel = ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_03);
		break;
	default:
		pPlayerModel = ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_03);
		break;
	}
	

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
		m_pSkinnedAnimationController->BuildUpperBodyMask(this, "mixamorig:Spine");
		m_pSkinnedAnimationController->SetSplitBodyTrackIndices(0, 1);

		m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
		m_pSkinnedAnimationController->SetTrackType(1, ANIMATION_TYPE_LOOP);

		m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, PLAYER_PISTOL_IDLE);
		m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(1, PLAYER_PISTOL_IDLE);

		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);

		m_pSkinnedAnimationController->SetTrackEnable(0, true);
		m_pSkinnedAnimationController->SetTrackEnable(1, true);

		m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
		m_pSkinnedAnimationController->SetTrackWeight(1, 1.0f);
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

void OtherPlayer::ChangeState(std::unique_ptr<State<OtherPlayer>> pNewState, bool bForce)
{
	if (!pNewState)
		return;

	if (!bForce && m_pState && typeid(*m_pState) == typeid(*pNewState))
		return;

	if (m_pState)
		m_pState->Exit(this);

	m_pState = std::move(pNewState);

	m_pState->Enter(this);
}
int OtherPlayer::GetIdleAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_IDLE;

	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_IDLE;
	}
}
int OtherPlayer::GetForwardRunAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_RUN_F;

	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_RUN_F;
	}
}
int OtherPlayer::GetGrenadeAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_GRENADE;

	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_GRENADE;
	}
}
int OtherPlayer::GetShootAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_SHOOT;

	case PlayerWeaponType::Shotgun:
		return PLAYER_SHOTGUN_SHOOT;

	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_SHOOT;
	}
}
int OtherPlayer::GetReloadAnimationByWeapon() const
{
	switch (m_eWeaponType)
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_RELOAD;

	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_RELOAD;
	}
}
int OtherPlayer::GetDieAnimationByWeapon() const
{
	return PLAYER_DIE;
}
int OtherPlayer::GetLowerAnimationByServerState() const
{
	if (m_bServerMoving)
		return GetForwardRunAnimationByWeapon();

	return GetIdleAnimationByWeapon();
}
void OtherPlayer::RefreshBaseAnimationByServerState()
{
	auto* pCtrl = GetAnimationController();
	if (!pCtrl)
		return;

	int lowerAnim = GetLowerAnimationByServerState();

	pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
	pCtrl->SetTrackEnable(0, true);
	pCtrl->SetTrackWeight(0, 1.0f);

	pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);
	pCtrl->SetTrackAnimationSetIfChanged(1, lowerAnim);
	pCtrl->SetTrackEnable(1, true);
	pCtrl->SetTrackWeight(1, 1.0f);
}
void OtherPlayer::TriggerShootAnim()
{
	ChangeState(std::make_unique<OtherPlayerShoot>(), true);
}
void OtherPlayer::TriggerReloadAnim()
{
	ChangeState(std::make_unique<OtherPlayerReload>(), true);
}
void OtherPlayer::TriggerGrenadeAnim()
{
	ChangeState(std::make_unique<OtherPlayerGrenade>(), true);
}
void OtherPlayer::TriggerDieAnim()
{
	ChangeState(std::make_unique<OtherPlayerDie>(), true);
}

bool OtherPlayerIdle::Enter(OtherPlayer* Player)
{
	Player->SetServerMoving(false);

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		int idleAnim = Player->GetIdleAnimationByWeapon();

		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		pCtrl->SetTrackAnimationSetIfChanged(0, idleAnim);
		pCtrl->SetTrackAnimationSetIfChanged(1, idleAnim);

		pCtrl->SetTrackSpeed(0, OTHER_PLAYER_NORMAL_ANIM_SPEED);
		pCtrl->SetTrackSpeed(1, OTHER_PLAYER_NORMAL_ANIM_SPEED);

		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackEnable(1, true);

		pCtrl->SetTrackWeight(0, 1.0f);
		pCtrl->SetTrackWeight(1, 1.0f);
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
	Player->SetServerMoving(true);

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		int runAnim = Player->GetForwardRunAnimationByWeapon();
		int idleAnim = Player->GetIdleAnimationByWeapon();

		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		pCtrl->SetTrackAnimationSetIfChanged(0, runAnim);
		pCtrl->SetTrackAnimationSetIfChanged(1, idleAnim);

		pCtrl->SetTrackSpeed(0, OTHER_PLAYER_RUN_ANIM_SPEED);
		pCtrl->SetTrackSpeed(1, OTHER_PLAYER_NORMAL_ANIM_SPEED);

		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackEnable(1, true);

		pCtrl->SetTrackWeight(0, 1.0f);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	return true;
}

void OtherPlayerRun::Update(OtherPlayer* Player, float fTimeElapsed)
{
	m_fFootstepTimer += fTimeElapsed;

	if (m_fFootstepTimer > 0.5f / OTHER_PLAYER_RUN_ANIM_SPEED)
	{
		SoundManager::Instance()->Play(SoundName::FOOSTEP, Player->GetPosition());
		m_fFootstepTimer -= 0.5f / OTHER_PLAYER_RUN_ANIM_SPEED;
	}

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		int runAnim = Player->GetForwardRunAnimationByWeapon();
		int idleAnim = Player->GetIdleAnimationByWeapon();

		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		pCtrl->SetTrackAnimationSetIfChanged(0, runAnim);
		pCtrl->SetTrackAnimationSetIfChanged(1, idleAnim);

		pCtrl->SetTrackSpeed(0, OTHER_PLAYER_RUN_ANIM_SPEED);
		pCtrl->SetTrackSpeed(1, OTHER_PLAYER_NORMAL_ANIM_SPEED);

		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackEnable(1, true);

		pCtrl->SetTrackWeight(0, 1.0f);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
}

void OtherPlayerRun::Exit(OtherPlayer* Player)
{
}

bool OtherPlayerGrenade::Enter(OtherPlayer* Player)
{
	m_fElapsed = 0.0f;
	m_nLastLowerAnim = Player->GetLowerAnimationByServerState();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, m_nLastLowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetGrenadeAnimationByWeapon());
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	return true;
}

void OtherPlayerGrenade::Update(OtherPlayer* Player, float fTimeElapsed)
{
	m_fElapsed += fTimeElapsed;

	m_nLastLowerAnim = Player->GetLowerAnimationByServerState();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, m_nLastLowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	if (m_fElapsed >= m_fAnimDuration)
	{
		if (Player->IsServerMoving())
			Player->ChangeState(std::make_unique<OtherPlayerRun>());
		else
			Player->ChangeState(std::make_unique<OtherPlayerIdle>());
	}
}

void OtherPlayerGrenade::Exit(OtherPlayer* Player)
{
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(1, m_nLastLowerAnim);

		float fLowerPosition = pCtrl->GetTrackPosition(0);
		pCtrl->SetTrackPosition(1, fLowerPosition);
	}
}

bool OtherPlayerShoot::Enter(OtherPlayer* Player)
{
	m_fElapsed = 0.0f;

	bool bPistolIdleShoot = (!Player->IsServerMoving() && Player->GetCurrentPlayerWeaponType() == PlayerWeaponType::Pistol);

	int lowerAnim = bPistolIdleShoot ? Player->GetShootAnimationByWeapon() : Player->GetLowerAnimationByServerState();
	int upperAnim = Player->GetShootAnimationByWeapon();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		if (bPistolIdleShoot)
		{
			pCtrl->SetTrackType(0, ANIMATION_TYPE_ONCE);
			pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
			pCtrl->SetTrackPosition(0, 0.0f);
			pCtrl->SetTrackEnable(0, true);
			pCtrl->SetTrackWeight(0, 1.0f);
		}
		else
		{
			pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
			pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
			pCtrl->SetTrackEnable(0, true);
			pCtrl->SetTrackWeight(0, 1.0f);
		}

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, upperAnim);
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	return true;
}

void OtherPlayerShoot::Update(OtherPlayer* Player, float fTimeElapsed)
{
	m_fElapsed += fTimeElapsed;

	bool bPistolIdleShoot = (!Player->IsServerMoving() && Player->GetCurrentPlayerWeaponType() == PlayerWeaponType::Pistol);

	int lowerAnim = bPistolIdleShoot ? Player->GetShootAnimationByWeapon() : Player->GetLowerAnimationByServerState();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		if (bPistolIdleShoot)
		{
			pCtrl->SetTrackType(0, ANIMATION_TYPE_ONCE);
			pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
			pCtrl->SetTrackEnable(0, true);
			pCtrl->SetTrackWeight(0, 1.0f);
		}
		else
		{
			pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
			pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
			pCtrl->SetTrackEnable(0, true);
			pCtrl->SetTrackWeight(0, 1.0f);
		}

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	if (m_fElapsed >= m_fAnimDuration)
	{
		if (Player->IsServerMoving())
			Player->ChangeState(std::make_unique<OtherPlayerRun>());
		else
			Player->ChangeState(std::make_unique<OtherPlayerIdle>());
	}
}

void OtherPlayerShoot::Exit(OtherPlayer* Player)
{
	auto* pCtrl = Player->GetAnimationController();
	if (!pCtrl)
		return;

	int lowerAnim = Player->GetLowerAnimationByServerState();

	pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
	pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
	pCtrl->SetTrackEnable(0, true);
	pCtrl->SetTrackWeight(0, 1.0f);

	pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);
	pCtrl->SetTrackAnimationSetIfChanged(1, lowerAnim);
	pCtrl->SetTrackEnable(1, true);
	pCtrl->SetTrackWeight(1, 1.0f);
}

bool OtherPlayerReload::Enter(OtherPlayer* Player)
{
	m_fElapsed = 0.0f;

	int lowerAnim = Player->GetLowerAnimationByServerState();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetReloadAnimationByWeapon());
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	return true;
}

void OtherPlayerReload::Update(OtherPlayer* Player, float fTimeElapsed)
{
	m_fElapsed += fTimeElapsed;

	int lowerAnim = Player->GetLowerAnimationByServerState();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);
	}

	if (m_fElapsed >= m_fAnimDuration)
	{
		if (Player->IsServerMoving())
			Player->ChangeState(std::make_unique<OtherPlayerRun>());
		else
			Player->ChangeState(std::make_unique<OtherPlayerIdle>());
	}
}

void OtherPlayerReload::Exit(OtherPlayer* Player)
{
}

bool OtherPlayerDie::Enter(OtherPlayer* Player)
{
	Player->SetServerMoving(false);

	auto* pCtrl = Player->GetAnimationController();
	if (!pCtrl) return false;

	pCtrl->SetTrackType(0, ANIMATION_TYPE_ONCE);
	pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);

	pCtrl->SetTrackAnimationSetIfChanged(0, Player->GetDieAnimationByWeapon());
	pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetDieAnimationByWeapon());

	pCtrl->SetTrackPosition(0, 0.0f);
	pCtrl->SetTrackPosition(1, 0.0f);

	pCtrl->SetTrackEnable(0, true);
	pCtrl->SetTrackEnable(1, true);

	pCtrl->SetTrackWeight(0, 1.0f);
	pCtrl->SetTrackWeight(1, 1.0f);

	return true;
}

void OtherPlayerDie::Update(OtherPlayer* Player, float fTimeElapsed)
{
}

void OtherPlayerDie::Exit(OtherPlayer* Player)
{
}

OtherPlayer* OtherPlayer::Create(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature,
	float x, float y, float z, short playerID)
{
	OtherPlayer* pOther = new OtherPlayer(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, playerID);
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
	CGameObject* pWeaponInstance = ResourceManager::Instance().GetModelPrototype(modelName);

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

	m_eWeaponType = GetOtherPlayerWeaponTypeFromPacket(weaponType);

	ModelName modelName = GetOtherPlayerWeaponModelNameFromPacket(weaponType);
	EquipWeaponModel(modelName);

	RefreshBaseAnimationByServerState();
}

void OtherPlayer::SubmitWeaponToShader(CShader* shader)
{
	if (!shader || !m_pRenderWeapon)
		return;

	shader->addObjects(m_pRenderWeapon);
}
