#pragma once
#include "stdafx.h"
#include "Object.h"
#include "State.h"
#include "Item.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "AI.h"

class CPlayer;
class Inventory;
class CShader;

enum ENEMY_ANIM
{
	ENEMY_RIFLE_SMG_IDLE = 0,
	ENEMY_RIFLE_SMG_RUN_F = 1,
	ENEMY_RIFLE_SMG_RUN_L = 2,
	ENEMY_RIFLE_SMG_RUN_R = 3,
	ENEMY_RIFLE_SMG_SHOOT = 4,
	ENEMY_RIFLE_SMG_RELOAD = 5,

	ENEMY_DIE = 6,

	ENEMY_PISTOL_IDLE = 7,
	ENEMY_PISTOL_RUN_F = 8,
	ENEMY_PISTOL_RUN_L = 9,
	ENEMY_PISTOL_RUN_R = 10,
	ENEMY_PISTOL_RELOAD = 12,
	ENEMY_PISTOL_SHOOT = 11
};

enum class EnemyWeaponType
{
	Pistol,
	SMG,
	Rifle
};

enum class EnemyModelType
{
	Enemy01,
	Enemy02,
	Enemy03
};

constexpr int MAX_LOOT_SLOTS = 10;

class AstarNavigation;
class CEnemyObject : public CGameObject
{
protected:
	AstarNavigation* AStarNav = NULL;

	CGameObject* m_pWeapon = nullptr;
	CGameObject* m_pWeaponSocket = nullptr;
	CGameObject* m_pWeaponMuzzleSocket = nullptr;
	unique_ptr<CGameObject> m_pRenderWeapon = nullptr;

public:
	CEnemyObject(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pShader = nullptr,
		ModelInstance* pEnemyModelInstance = nullptr
	);
	virtual ~CEnemyObject();
	void SubmitWeaponToShader(CShader* shader);
	virtual void Animate(float fTimeElapsed) override;
	virtual void Update(float fTimeElapsed);
	virtual void HandleCollision(XMFLOAT3 normal);
	virtual void SetPosition(float x, float y, float z);
	void SetPlayer(CGameObject* pPlayer)
	{
		m_pPlayer = pPlayer;
		m_Blackboard.pTarget = nullptr;
	}
	virtual void Render(
		ID3D12GraphicsCommandList* pd3dCommandList,
		bool batch,
		int nPipelineState,
		CCamera* pCamera = NULL
	) override;

	void ChangeState(std::unique_ptr<State<CEnemyObject>> pNewState);

	void SetMoveDir(const XMFLOAT3& dir) { m_xmf3MoveDir = dir; }
	XMFLOAT3 GetMoveDir() const { return m_xmf3MoveDir; }

	void HandleHP(float value);

	bool ConsumeDeadRemovalRequest();
	bool ConsumeLootSpawnRequest();		// 루팅 오브젝트 생성 요청을 1회 소비			// 서버 권위 구조로 바꾸면서 안씀
	void MarkDeadForRemoval();			// death 연출 종료 후 삭제 대상으로 표시

	void setNav(AstarNavigation* nav) { AStarNav = nav; }
	AstarNavigation* GetNav() { return AStarNav; }

	StateMachine<CEnemyObject> m_StateMachine;

	EnemyBlackboard m_Blackboard;
	std::unique_ptr<BehaviorTree> m_pBehaviorTree;

	EnemyBlackboard& GetBlackboard() { return m_Blackboard; }
	const EnemyBlackboard& GetBlackboard() const { return m_Blackboard; }

	BehaviorTree* GetBehaviorTree() { return m_pBehaviorTree.get(); }
	const BehaviorTree* GetBehaviorTree() const { return m_pBehaviorTree.get(); }

	CGameObject* m_pPlayer = nullptr;
	float						hp = 100;
	float						updateTimer = 0;
	vector<XMFLOAT3>			ways;
	int							wayIdx = 0;
	float						findTime = 0.3f;

	XMFLOAT3 m_xmf3ServerPosition = { 0.0f, 0.0f, 0.0f };	// 05.10 추가
	bool     m_bUseServerLerp = false;						// 05.10 추가
public:
	void SetEnemyModelType(EnemyModelType eModelType);
	void ApplyDefaultWeaponByEnemyModelType();
	void EquipWeaponByType(EnemyWeaponType eWeaponType);
	void EquipWeaponModel(ModelName modelName);
	void EquipDefaultPistol();
	ModelName GetWeaponModelNameByType(EnemyWeaponType eWeaponType) const;
	CGameObject* GetWeaponMuzzleSocket() const { return m_pWeaponMuzzleSocket; }
	CGameObject* GetRenderWeapon() const { return m_pRenderWeapon.get(); }

	EnemyModelType m_eEnemyModelType = EnemyModelType::Enemy01;
	EnemyWeaponType m_eWeaponType = EnemyWeaponType::Pistol;

	float m_fMoveSpeed = 5.0f;
	float m_fDetectionRange = 20.0f;
	float m_fAttackRange = 8.0f;
	float m_fAttackExitRange = 9.5f;
	float m_fLeashRange = 25.0f;
	float m_fReturnStopDistance = 0.5f;

	float m_fThinkInterval = 0.2f;

	float m_fAimTimer = 0.0f;
	float m_fAimDelay = 0.35f;
	float m_fAttackInterval = 1.0f;

	float m_fAttackCooldown = 0.0f;

	float m_fReturnIgnoreDuration = 1.0f;

	//거리 유지
	float m_fPreferredCombatRange = 7.0f;
	float m_fTooCloseRange = 4.0f;

	//버스트 사격
	int m_nBurstShotsLeft = 0;
	int m_nBurstShotMin = 2;
	int m_nBurstShotMax = 4;
	int m_nBurstSerial = 0;

	float m_fBurstShotTimer = 0.0f;
	float m_fBurstShotInterval = 0.15f;

	float m_fBurstRestTimer = 0.0f;
	float m_fBurstRestDuration = 1.0f;

	//좌우 이동
	float m_fStrafeTimer = 0.0f;
	float m_fStrafeDuration = 1.2f;
	float m_fStrafeSign = 1.0f;
	float m_fCombatMoveSpeedMultiplier = 0.45f;

	//재장전
	int m_nMagazineAmmo = 12;			//이건 나중에 총마다 다르게 설정할 수 있도록 바꿀 예정
	int m_nCurrentAmmo = 12;
	bool m_bReloading = false;
	float m_fReloadTimer = 0.0f;
	float m_fReloadDuration = 2.0f;

	// 시야각 관련 값 - 삭제된 데이터는 BlackBoard로 이동
	float m_fViewAngle = 120.0f;
	float m_fLoseSightDuration = 1.0f;

	//Idle 상태에서 시야 회전 관련 값
	float m_fCurrentYawDeg = 0.0f;
	float m_fIdleBaseYawDeg = 0.0f;
	float m_fIdleLookTimer = 0.0f;
	float m_fIdleLookInterval = 2.0f;
	float m_fIdleYawTarget = 0.0f;
	float m_fIdleYawRange = 45.0f;
	float m_fIdleTurnSpeed = 90.0f;
	int m_nIdleLookDir = 1;

	XMFLOAT3 m_xmf3SpawnPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 10.0f);
	XMFLOAT3 m_xmf3MoveDir = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bool m_bDying = false;
	bool m_bLootSpawnRequested = false;
	float m_fDieElapsed = 0.0f;
	float m_fDieDuration = 3.4f;
	bool m_bDeadRemoveRequested = false;

	float m_fShootAnimTimer = 0.0f;				// 서버 패킷으로부터 사격 애니메이션을 구분하기 위해 추가
	float m_fShootAnimHold = 0.18f;				// 버스트 간격(0.15) 보다 조금 더 길게 해서 안 끊기게 유지
	bool m_bShootEffectRequested = false;

public:
	void SetSpawnPosition(const XMFLOAT3& pos) { m_xmf3SpawnPosition = pos; }
	XMFLOAT3 GetSpawnPosition() const { return m_xmf3SpawnPosition; }

	float GetDistanceToPlayerXZ() const;
	float GetDistanceFromSpawnXZ() const;
	float GetDistanceToSpawnXZ() const;

	bool IsPlayerInDetectRange() const;
	bool IsPlayerInAttackRange() const;
	bool IsPlayerOutOfAttackRange() const;
	bool IsOutsideLeashRange() const;
	bool IsNearSpawn() const;

	void FaceToPosition(const XMFLOAT3& targetPos);
	bool UpdatePathToPosition(const XMFLOAT3& targetPos, float fTimeElapsed);
	bool FollowCurrentPath();

	void ClearPath();
	XMFLOAT3 GetDirectionToPlayerXZ() const;
	XMFLOAT3 GetDirectionToSpawnXZ() const;
	void StartNewBurst();
	void SetEnemyAnimation(int nAnim, bool bLoop = true, bool bRestart = false);
	void UpdateCombatMove(float fTimeElapsed);

	// 총기 설정	관련 함수
	void SetEnemyWeaponType(EnemyWeaponType eWeaponType);

	EnemyWeaponType GetEnemyWeaponType() const { return m_eWeaponType; }

	int GetIdleAnimationByWeapon() const;
	int GetForwardRunAnimationByWeapon() const;
	int GetLeftRunAnimationByWeapon() const;
	int GetRightRunAnimationByWeapon() const;
	int GetAttackAnimationByWeapon() const;
	int GetReloadAnimationByWeapon() const;
	int GetDieAnimationByWeapon() const;

	void ConfigureWeaponStats();

	//재장전 함수
	void StartReload();
	void UpdateReload(float fTimeElapsed);
	bool IsReloading() const { return m_bReloading; }

	//시야각/감지 함수
	void SetYawDeg(float yawDeg);
	XMFLOAT3 GetForwardXZ() const;
	bool IsPlayerInViewAngle() const;
	bool CanDetectPlayer() const;
	bool CanShootPlayer() const;
	void RefreshLastSeenPlayer();
	bool HasRecentLastSeenPlayer() const;

	void UpdateIdleLook(float fTimeElapsed);

	void FireAtPlayer();
	bool ConsumeShootEffectRequest();

	void SetServerPosition(const XMFLOAT3& pos);	// 05.10 추가

	void SetServerYaw(float yawRad);				// 05.14 추가: 서버에서 받은 방향으로 회전

	void SnapToServerPosition();					// 05.14 추가: idle 상태 시 즉시 보정

	void  TriggerShootAnim() { m_fShootAnimTimer = m_fShootAnimHold; }
public:
	CGameObject* GetPlayer() const { return m_pPlayer; }

	bool IsDying() const { return m_bDying; }

	bool NeedsReload() const
	{
		return m_bReloading || (m_nCurrentAmmo <= 0);
	}

	int GetCurrentAmmo() const { return m_nCurrentAmmo; }
};

class EnemyIdle : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemySearch : public State<CEnemyObject>
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

private:
	float m_fFootstepTimer = 0.0f;
};

class EnemyAttack : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemyReload : public State<CEnemyObject>
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemyReturn : public State<CEnemyObject>
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

	bool SetVisualModel(CGameObject* pModelInstance);
private:
	//std::array<unique_ptr<Item>, MAX_LOOT_SLOTS> m_LootItems;
	//std::array<int, MAX_LOOT_SLOTS> m_LootCounts{};
	std::array<ItemSlot, MAX_LOOT_SLOTS> m_slots;
	short m_npc_id = -1;	// 서버에서 루팅 박스 식별용 ID (NPC ID와 동일)
	float m_fRemainTime = 30.0f;		// 서버 권위 구조로 바꾸면서 안씀
};