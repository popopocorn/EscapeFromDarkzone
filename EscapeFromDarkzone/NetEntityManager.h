#pragma once
#include <array>
#include "protocol.h"

// 전방 선언 
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12RootSignature;
class CScene;
class CPlayer;
class OtherPlayer;
class CEnemyObject;
class ShaderManager;

struct OtherPlayerSlot {
	short id = -1;
	OtherPlayer* pPlayer = nullptr;
};

struct NpcSlot {
	short id = -1;
	CEnemyObject* pNpc = nullptr;
};

// 엔티티 복제 전용 (player / npc add / remove / move / state)
// effect / player hp / inventory / loot 은 포함하지 않음 (dispatcher가 기존 소유자로 라우팅)
class NetEntityManager
{
public:
	NetEntityManager() = default;

	// 안정 컨텍스트 - 1회만 설정 (프레임워크 레벨, scene 전환에도 불변)
	void Init(ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ShaderManager* shaderMgr,
		CPlayer* localPlayer);

	// 휘발 컨텍스트 - scene 전환마다 호출 (엔티티 레지스트리 clear 포함)
	void SetActiveScene(CScene* scene);

	short GetMyId() const { return m_myId; }

	// 수신 핸들러 (엔티티 복제)
	void OnLoginInfo(const SC_LOGIN_INFO_PACKET* p);
	void OnAddPlayer(const SC_ADD_PLAYER_PACKET* p);
	void OnRemovePlayer(const SC_REMOVE_PLAYER_PACKET* p);
	void OnMovePlayer(const SC_MOVE_PLAYER_PACKET* p);
	void OnPlayerStateChange(const SC_PLAYER_STATE_CHANGE_PACKET* p);
	void OnAddNpc(const SC_ADD_NPC_PACKET* p);
	void OnRemoveNpc(const SC_REMOVE_NPC_PACKET* p);
	void OnMoveNpc(const SC_MOVE_NPC_PACKET* p);
	void OnNpcStateChange(const SC_NPC_STATE_CHANGE_PACKET* p);
	void OnApplyEquip(const SC_EQUIPMENT_UPDATE_PACKET* p);

	// 조회 (dispatcher의 cross-cutting 핸들러용: SC_PLAY_EFFECT_ATTACHED)
	OtherPlayer* FindOtherPlayer(short id);
	CEnemyObject* FindNpc(short id);

private:
	bool AddOtherPlayer(short id, OtherPlayer* p);
	void RemoveOtherPlayer(short id);
	bool AddNpc(short id, CEnemyObject* p);
	void RemoveNpc(short id);

	bool IsActiveSceneGameplay() const;   // 스폰 가드

	// 안정 컨텍스트
	ID3D12Device*              m_pd3dDevice = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCommandList = nullptr;
	ID3D12RootSignature*       m_pRootSignature = nullptr;
	ShaderManager*             m_pShaderManager = nullptr;
	CPlayer*                   m_pPlayer = nullptr;   // non-owning (GameFramework 소유)

	// 휘발 컨텍스트
	CScene*                    m_pActiveScene = nullptr;

	// 세션 스코프 상태
	short                      m_myId = -1;

	// Scene 스코프 레지스트리 (SetActiveScene에서 clear - 이전 scene 객체는 소멸하므로)
	std::array<OtherPlayerSlot, 16> m_otherPlayers;
	std::array<NpcSlot, MAX_NPC>    m_npcs;
};
