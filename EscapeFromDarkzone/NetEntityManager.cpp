#include "stdafx.h"
#include "NetEntityManager.h"

#include "Scene.h"            // CScene, MainScene, AddObj, m_ppShaders, SHADERIDX
#include "Player.h"           // CPlayer
#include "OtherPlayer.h"      // OtherPlayer, OtherPlayerIdle/Run
#include "EnemyObject.h"      // CEnemyObject, EnemyIdle/Run/Attack/Die
#include "ShaderManager.h"    // ShaderManager, ShaderType
#include "ResourceManager.h"  // ResourceManager, ModelName, CLoadedModelInfo

void NetEntityManager::Init(ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig,
	ShaderManager* shaderMgr,
	CPlayer* localPlayer)
{
	m_pd3dDevice = device;
	m_pd3dCommandList = cmdList;
	m_pRootSignature = rootSig;
	m_pShaderManager = shaderMgr;
	m_pPlayer = localPlayer;
}

void NetEntityManager::SetActiveScene(CScene* scene)
{
	m_pActiveScene = scene;

	// scene 전환 시 이전 scene의 엔티티들은 소멸
	// 비소유(raw) 레지스트리를 비워 dangling 참조를 끊음 (소유 안 하므로 free 아님)
	// myId / 로컬 플레이어 / 연결은 세션 스코프라 유지
	for (auto& s : m_otherPlayers) { s.id = -1; s.pPlayer = nullptr; }
	for (auto& s : m_npcs) { s.id = -1; s.pNpc = nullptr; }
}

bool NetEntityManager::IsActiveSceneGameplay() const
{
	// 게임플레이 scene(= ENEMY 셰이더/AddObj 보유)에서만 스폰 허용
	// 로비 등에서 스폰 패킷이 와도 안전하게 무시
	return m_pActiveScene && (dynamic_cast<MainScene*>(m_pActiveScene) != nullptr);
}

// ─────────────────────────────────────────────────────────────
// 레지스트리 (GameFramework에서 이주)
// ─────────────────────────────────────────────────────────────
OtherPlayer* NetEntityManager::FindOtherPlayer(short id)
{
	for (auto& s : m_otherPlayers)
		if (s.id == id) return s.pPlayer;
	return nullptr;
}
bool NetEntityManager::AddOtherPlayer(short id, OtherPlayer* p)
{
	for (auto& s : m_otherPlayers)
		if (s.id == -1) { s.id = id; s.pPlayer = p; return true; }
	return false;
}
void NetEntityManager::RemoveOtherPlayer(short id)
{
	for (auto& s : m_otherPlayers)
		if (s.id == id) { s.id = -1; s.pPlayer = nullptr; return; }
}
CEnemyObject* NetEntityManager::FindNpc(short id)
{
	for (auto& s : m_npcs)
		if (s.id == id) return s.pNpc;
	return nullptr;
}
bool NetEntityManager::AddNpc(short id, CEnemyObject* p)
{
	for (auto& s : m_npcs)
		if (s.id == -1) { s.id = id; s.pNpc = p; return true; }
	return false;
}
void NetEntityManager::RemoveNpc(short id)
{
	for (auto& s : m_npcs)
		if (s.id == id) { s.id = -1; s.pNpc = nullptr; return; }
}

// ─────────────────────────────────────────────────────────────
// 수신 핸들러 (dispatcher 본문 이주, gf.X -> 멤버 치환)
// ─────────────────────────────────────────────────────────────
void NetEntityManager::OnLoginInfo(const SC_LOGIN_INFO_PACKET* p)
{
	if (m_pPlayer) {
		m_pPlayer->SetPosition(XMFLOAT3(p->x, p->y, p->z));
		m_pPlayer->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));
	}
	m_myId = p->id;
}

void NetEntityManager::OnAddPlayer(const SC_ADD_PLAYER_PACKET* p)
{
	if (!IsActiveSceneGameplay()) return;   // 스폰 가드

	if (p->id == m_myId) return;
	if (FindOtherPlayer(p->id)) return;

	OtherPlayer* pOther = OtherPlayer::Create(m_pd3dDevice, m_pd3dCommandList, m_pRootSignature, p->x, p->y, p->z);
	pOther->SetServerYaw(p->yaw);

	switch (p->state) {
	case PLAYER_STATE_IDLE:
		pOther->ChangeState(std::make_unique<OtherPlayerIdle>());
		break;
	case PLAYER_STATE_RUN:
		pOther->ChangeState(std::make_unique<OtherPlayerRun>());
		break;
	}

	if (!AddOtherPlayer(p->id, pOther)) {	// 슬롯 부족 - 생성 취소
		OutputDebugString(L"[Network] OtherPlayer slot full.\n");
		pOther->Kill();
		return;
	}
	pOther->SubmitWeaponToShader(m_pShaderManager->GetShader(ShaderType::STANDARD));
	m_pActiveScene->AddObj(pOther);
	m_pActiveScene->m_ppShaders[SHADERIDX::ENEMY]->addObjects(pOther);
}

void NetEntityManager::OnRemovePlayer(const SC_REMOVE_PLAYER_PACKET* p)
{
	if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
		pOther->Kill();
		RemoveOtherPlayer(p->id);
	}
}

void NetEntityManager::OnMovePlayer(const SC_MOVE_PLAYER_PACKET* p)
{
	if (p->id == m_myId) {
		if (m_pPlayer) m_pPlayer->SetServerPosition(XMFLOAT3(p->x, p->y, p->z)); // 보간용 서버 위치
		return;
	}

	if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
		pOther->UpdatePosition(p->x, p->y, p->z);
		pOther->SetServerYaw(p->yaw);
	}
}

void NetEntityManager::OnPlayerStateChange(const SC_PLAYER_STATE_CHANGE_PACKET* p)
{
	if (p->id == m_myId) return;

	if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
		switch (p->state) {
		case PLAYER_STATE_IDLE:
			pOther->ChangeState(std::make_unique<OtherPlayerIdle>());
			break;
		case PLAYER_STATE_RUN:
			pOther->ChangeState(std::make_unique<OtherPlayerRun>());
			break;
		}
	}
}

static ModelName NpcModelFromTierOutfit(int tier, int outfit)
{
	if (tier < 0 || tier > 2) tier = 0;
	if (outfit < 0 || outfit > 2) outfit = 0;
	switch (tier) {
	case 1:
		return static_cast<ModelName>(static_cast<int>(ModelName::ENEMY_02_1) + outfit);
	case 2:
		return static_cast<ModelName>(static_cast<int>(ModelName::ENEMY_03_1) + outfit);
	case 0:
	default:
		//return static_cast<ModelName>(static_cast<int>(ModelName::ENEMY_01_1) + outfit);
		return static_cast<ModelName>(static_cast<int>(ModelName::ENEMY_01_1));
	}
}

void NetEntityManager::OnAddNpc(const SC_ADD_NPC_PACKET* p)
{
	if (!IsActiveSceneGameplay()) return;   // 스폰 가드

	// 디버그 로그
	wchar_t szLog[128];
	swprintf_s(szLog, L"[SC_ADD_NPC] id=%d, pos=(%.1f, %.1f, %.1f)\n",
		p->npc_id, p->x, p->y, p->z);
	OutputDebugStringW(szLog);

	// 중복 방지
	if (FindNpc(p->npc_id)) return;

	// tier(npc_kind) + outfit으로 외형 모델 선택
	ModelName enemyModelName = NpcModelFromTierOutfit(p->npc_kind, p->npc_outfit);

	CLoadedModelInfo* pModel = ResourceManager::Instance().CreateSkinnedModelInstance(enemyModelName);		// 리소스 매니저에서 모델 받아옴
	{
		wchar_t szLog[256];
		swprintf_s(szLog, L"[SC_ADD_NPC] id=%d, tier=%d, outfit=%d, model=%s\n",
			p->npc_id, (int)p->npc_kind, (int)p->npc_outfit,
			(pModel && pModel->m_pModelRootObject) ? L"OK" : L"NULL");
		OutputDebugStringW(szLog);
	}

	CEnemyObject* pNpc = new CEnemyObject(m_pd3dDevice, m_pd3dCommandList, m_pRootSignature, m_pShaderManager->GetShader(ShaderType::SKINNED), pModel);
	{
		wchar_t szLog[256];
		swprintf_s(szLog, L"[SC_ADD_NPC] pNpc->m_pChild=%s, controller=%s\n",
			pNpc->m_pChild ? L"OK" : L"NULL",
			pNpc->m_pSkinnedAnimationController ? L"OK" : L"NULL");
		OutputDebugStringW(szLog);
	}
	pNpc->SetEnemyModelType(static_cast<EnemyModelType>(p->npc_kind));		// 애니메이션 안나오는 원인 (수정완)
	pNpc->ApplyDefaultWeaponByEnemyModelType();

	pNpc->SubmitWeaponToShader(m_pShaderManager->GetShader(ShaderType::STANDARD));

	pNpc->SetPosition(p->x, p->y, p->z);
	pNpc->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));   // lerp 시작점 = 서버 위치
	pNpc->SetServerYaw(p->yaw);
	if (!AddNpc(p->npc_id, pNpc)) {
		OutputDebugString(L"[Network] NPC slot full.\n");
		pNpc->Kill();
		return;
	}

	m_pActiveScene->AddObj(pNpc);
	m_pActiveScene->m_ppShaders[SHADERIDX::ENEMY]->addObjects(pNpc);
}
void NetEntityManager::OnRemoveNpc(const SC_REMOVE_NPC_PACKET* p)
{
	if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
		pNpc->Kill();
		RemoveNpc(p->npc_id);
	}
}

void NetEntityManager::OnMoveNpc(const SC_MOVE_NPC_PACKET* p)
{
	if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
		pNpc->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));
		pNpc->SetServerYaw(p->yaw);
	}
}

void NetEntityManager::OnNpcStateChange(const SC_NPC_STATE_CHANGE_PACKET* p)
{
	if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
		switch (p->state) {
		case NPC_STATE_IDLE:
			pNpc->ChangeState(std::make_unique<EnemyIdle>());
			pNpc->SnapToServerPosition();
			break;
		case NPC_STATE_RUN:
		case NPC_STATE_RETURN:
			pNpc->ChangeState(std::make_unique<EnemyRun>());
			break;
		case NPC_STATE_ATTACK:
			pNpc->ChangeState(std::make_unique<EnemyAttack>());
			break;
		case NPC_STATE_RELOAD:
			pNpc->ChangeState(std::make_unique<EnemyAttack>());   // 이미 ATTACK이면 무시됨
			pNpc->StartReload();
			break;
		case NPC_STATE_DIE:
			pNpc->ChangeState(std::make_unique<EnemyDie>());
			break;
		}
	}
}
