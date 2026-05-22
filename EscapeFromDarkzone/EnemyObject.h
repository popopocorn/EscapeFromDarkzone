#pragma once
#include "stdafx.h"
#include "Object.h"
#include "State.h"
#include "Item.h"
#include "Shader.h"

class CPlayer;
class Inventory;
class CShader;

enum ENEMY_ANIM {
	ENEMY_ANIM_IDLE = 0,
	ENEMY_ANIM_RUN = 1,
	ENEMY_ANIM_DIE = 2
};

constexpr int MAX_LOOT_SLOTS = 10;

class AstarNavigation;
class CEnemyObject : public CGameObject
{
protected:
	AstarNavigation* AStarNav = NULL;
	
	
	
public:
	CEnemyObject(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pShader = nullptr,
		CLoadedModelInfo* pEnemyModelInstance = nullptr
	);
	virtual ~CEnemyObject();

	virtual void Animate(float fTimeElapsed) override;
	virtual void Update(float fTimeElapsed);
	virtual void HandleCollision(XMFLOAT3 normal);
	virtual void SetPosition(float x, float y, float z);
	void SetPlayer(CGameObject* pPlayer) { m_pPlayer = pPlayer; }

	void ChangeState(std::unique_ptr<State<CEnemyObject>> pNewState);

	void SetMoveDir(const XMFLOAT3& dir) { m_xmf3MoveDir = dir; }
	XMFLOAT3 GetMoveDir() const { return m_xmf3MoveDir; }

	void HandleHP(float value);

	bool ConsumeDeadRemovalRequest();
	bool ConsumeLootSpawnRequest();		// 루팅 오브젝트 생성 요청을 1회 소비			// 서버 권위 구조로 바꾸면서 안씀
	void MarkDeadForRemoval();			// death 연출 종료 후 삭제 대상으로 표시
	void setNav(AstarNavigation* nav) { AStarNav = nav; }
	AstarNavigation* GetNav() { return AStarNav; }
	std::unique_ptr<State<CEnemyObject>> m_pState;

	CGameObject*				m_pPlayer = nullptr;
	float						hp = 100;
	float						updateTimer = 0;
	vector<XMFLOAT3>			ways;
	int							wayIdx = 0;
	float						findTime = 0.5f;

	XMFLOAT3 m_xmf3ServerPosition = { 0.0f, 0.0f, 0.0f };	// 05.10 추가
	bool     m_bUseServerLerp = false;						// 05.10 추가
public:
	float m_fMoveSpeed = 7.0f;
	float m_fDetectionRange = 20.0f;
	float m_fAttackRange = 3.0f;

	XMFLOAT3 m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 10.0f);
	XMFLOAT3 m_xmf3MoveDir = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bool m_bDying = false;
	bool m_bLootSpawnRequested = false;
	float m_fDieElapsed = 0.0f;
	float m_fDieDuration = 3.4f;
	bool m_bDeadRemoveRequested = false;

	void SetServerPosition(const XMFLOAT3& pos);	// 05.10 추가

	void SetServerYaw(float yawRad);				// 05.14 추가: 서버에서 받은 방향으로 회전

	void SnapToServerPosition();					// 05.14 추가: idle 상태 시 즉시 보정
};

class EnemyIdle : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemyRun : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemyDie : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

// 루팅 전용 오브젝트
// 지금은 시각 모델 없이 내부 데이터, 디버그 박스만 사용하고 나중에 적 시체 모델 붙임
class CLootContainerObject : public CGameObject
{
public:
	CLootContainerObject(float fLifeTime = 30.0f);						// 서버 권위 구조로 바꾸면서 안씀 (생성자가 fLifeTime 초기화만 함)
	virtual ~CLootContainerObject() = default;

	void UpdateLifetime(float fTimeElapsed);							// 남은 유지 시간 갱신 후 만료 시 삭제 처리
	//bool AddLoot(unique_ptr<Item> item, int count = 1);					// 루팅 슬롯에 아이템 추가
	void FillInventoryUI(Inventory* pInventory) const;					// 시체 인벤토리 UI에 현재 루팅 아이템 복사

	float GetDistanceSq(const XMFLOAT3& pos);							// 플레이어와의 거리 제곱값 계산

	// 루팅 박스 서버화 
	void SetBoxId(short id) { m_npc_id = id; };
	short GetBoxId() const { return m_npc_id; };
	void SetSlotData(int idx, ItemID item, int count);

private:
	//std::array<unique_ptr<Item>, MAX_LOOT_SLOTS> m_LootItems;
	//std::array<int, MAX_LOOT_SLOTS> m_LootCounts{};
	std::array<ItemSlot, MAX_LOOT_SLOTS> m_slots;
	short m_npc_id = -1;	// 서버에서 루팅 박스 식별용 ID (NPC ID와 동일)
	float m_fRemainTime = 30.0f;		// 서버 권위 구조로 바꾸면서 안씀
};