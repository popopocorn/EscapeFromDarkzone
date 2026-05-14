#pragma once
#pragma comment(lib, "winmm.lib")

#define DIR_FORWARD				0x01
#define DIR_BACKWARD			0x02
#define DIR_LEFT				0x04
#define DIR_RIGHT				0x08
#define DIR_UP					0x10
#define DIR_DOWN				0x20

#include "stdafx.h"
#include "Object.h"
#include "Camera.h"
#include "State.h"

enum class EventType {
	Input,
	Timeout,
};

struct InputEvent {
	INPUT_KEY key;
	KEY_STATE state;
};

struct GameEvent {
	EventType type;
	InputEvent keyEvent;
};

enum class WEAPON_POSE
{
	IDLE = 0,
	RUN,
	SHOOT,
	GRENADE
};

class PlayerState;
class CPlayerAnimationController;
class WeaponItem;

class CPlayer : public CGameObject
{
protected:
	XMFLOAT3					m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3					m_xmf3Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	float						m_fPitch = 0.0f;
	float						m_fYaw = 0.0f;
	float						m_fRoll = 0.0f;

	XMFLOAT3					m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float						m_fMaxVelocityXZ = 0.0f;
	float						m_fMaxVelocityY = 0.0f;
	float						m_fFriction = 0.0f;

	LPVOID						m_pPlayerUpdatedContext = NULL;
	LPVOID						m_pCameraUpdatedContext = NULL;

	CCamera* m_pCamera = NULL;
	std::unique_ptr<State<CPlayer>> state;
	std::queue<GameEvent>		event_queue;

	XMFLOAT3					MoveDir = XMFLOAT3(0, 0, 0);
	float						speed{};

	// 충돌 노멀
	std::vector<XMFLOAT3>		CollVector;

	//애니메이션 관련 무기
	CGameObject* m_pWeapon = nullptr;
	CGameObject* m_pWeaponSocket = nullptr;

	XMFLOAT4X4 m_xmf4x4WeaponBaseLocal = Matrix4x4::Identity();
	bool m_bWeaponBaseLocalSaved = false;

	WEAPON_POSE m_eWeaponPose = WEAPON_POSE::IDLE;

	XMFLOAT3 m_xmf3WeaponIdlePos = XMFLOAT3(-0.14f, 0.20f, 0.16f);
	XMFLOAT3 m_xmf3WeaponIdleRot = XMFLOAT3(-90.0f, -90.0f, 28.0f);

	XMFLOAT3 m_xmf3WeaponRunPos = XMFLOAT3(0.18f, 0.1f, -0.08f);
	XMFLOAT3 m_xmf3WeaponRunRot = XMFLOAT3(8.0f, 0.0f, -25.0f);

	XMFLOAT3 m_xmf3WeaponGrenadePos = XMFLOAT3(0.01f, 0.05f, 0.01f);
	XMFLOAT3 m_xmf3WeaponGrenadeRot = XMFLOAT3(-90.0f, 0.0f, 0.0f);
	XMFLOAT4X4 m_xmf4x4WeaponGrenadeStartLocal = Matrix4x4::Identity();	
	bool m_bWeaponGrenadeStartCaptured = false;

	XMFLOAT3 m_xmf3WeaponShootPos = XMFLOAT3(0.02f, 0.00f, 0.00f);
	XMFLOAT3 m_xmf3WeaponShootRot = XMFLOAT3(0.0f, 8.0f, 0.0f);

	XMFLOAT3 m_xmf3WeaponScale = XMFLOAT3(1.2f, 1.2f, 1.2f);

	// 무기 포즈 블렌딩
	bool m_bWeaponBlending = false;
	float m_fWeaponBlendTime = 0.0f;
	float m_fWeaponBlendDuration = 0.25f;
	XMFLOAT4X4 m_xmf4x4WeaponBlendStartWorld = Matrix4x4::Identity();

	// grenade 포즈용
	CGameObject* m_pLeftUpperArm = nullptr;
	CGameObject* m_pLeftForeArm = nullptr;
	CGameObject* m_pLeftHand = nullptr;
	CGameObject* m_pLeftHandGrip = nullptr;

	CGameObject* FindFirstFrameByNames(const char* const* ppNames, int nCount);
	bool InitializeLeftHandIK();

	void ApplyRunWeaponPose();
	void ApplyGrenadeWeaponPose();
	void ApplyShootWeaponPose();

	// 04.10 추가: 서버 위치 보간
	XMFLOAT3 m_xmf3ServerPosition = XMFLOAT3(0, 0, 0);
	
	//현재 장착 중인 무기 데이터
	std::shared_ptr<WeaponItem> m_pEquippedWeaponItem;

	int   m_nCurrentAmmo = 0;
	int   m_nMaxAmmo = 0;

	bool  m_bReloading = false;
	float m_fReloadElapsed = 0.0f;
	float m_fReloadDuration = 0.0f;
	float m_fFireCooldown = 0.0f;
	bool  m_bFireHeld = false;
	bool  m_bShotAnimRequest = false;

public:
	CPlayer();
	virtual ~CPlayer();

	const XMFLOAT3& GetPosition() const { return m_xmf3Position; }
	const XMFLOAT3& GetLookVector() const { return m_xmf3Look; }
	const XMFLOAT3& GetUpVector() const { return m_xmf3Up; }
	const XMFLOAT3& GetRightVector() const { return m_xmf3Right; }

	void SetFriction(float fFriction) { m_fFriction = fFriction; }
	void SetGravity(const XMFLOAT3& xmf3Gravity) { m_xmf3Gravity = xmf3Gravity; }
	void SetMaxVelocityXZ(float fMaxVelocity) { m_fMaxVelocityXZ = fMaxVelocity; }
	void SetMaxVelocityY(float fMaxVelocity) { m_fMaxVelocityY = fMaxVelocity; }
	void SetVelocity(const XMFLOAT3& xmf3Velocity) { m_xmf3Velocity = xmf3Velocity; }
	void SetPosition(const XMFLOAT3& xmf3Position)
	{
		Move(XMFLOAT3(
			xmf3Position.x - m_xmf3Position.x,
			xmf3Position.y - m_xmf3Position.y,
			xmf3Position.z - m_xmf3Position.z
		), false);
	}

	void SetScale(const XMFLOAT3& xmf3Scale) { m_xmf3Scale = xmf3Scale; }

	const XMFLOAT3& GetVelocity() const { return m_xmf3Velocity; }
	float GetYaw() const { return m_fYaw; }
	float GetPitch() const { return m_fPitch; }
	float GetRoll() const { return m_fRoll; }

	CCamera* GetCamera() { return m_pCamera; }
	void SetCamera(CCamera* pCamera) { m_pCamera = pCamera; }

	virtual void Move(ULONG nDirection, float fDistance, bool bVelocity = false);
	void Move(const XMFLOAT3& xmf3Shift, bool bVelocity = false);
	void Move(float fxOffset = 0.0f, float fyOffset = 0.0f, float fzOffset = 0.0f);
	void Rotate(float x, float y, float z);

	virtual void Update(float fTimeElapsed);

	virtual void OnPlayerUpdateCallback(float fTimeElapsed) {}
	void SetPlayerUpdatedContext(LPVOID pContext) { m_pPlayerUpdatedContext = pContext; }

	virtual void OnCameraUpdateCallback(float fTimeElapsed) {}
	void SetCameraUpdatedContext(LPVOID pContext) { m_pCameraUpdatedContext = pContext; }

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	CCamera* OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode);

	virtual CCamera* ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed) { return NULL; }
	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera = NULL);

	CAnimationController* GetAnimationController() { return m_pSkinnedAnimationController; }
	void AddEvent(const GameEvent& event) { event_queue.push(event); }
	void ChangeState(std::unique_ptr<State<CPlayer>> new_state);
	void SetMoveDir(XMFLOAT3 dir) { MoveDir = dir; }

	virtual void HandleCollision(const ColResult& normal);
	void UpdateDirection();
	bool IsGrenadeState() const;

	void EquipWeapon(CGameObject* pWeapon, const char* pstrSocketName);
	CGameObject* GetWeapon() { return m_pWeapon; }
	void UpdateWeaponPose(float fTimeElapsed);
	void ApplyWeaponPose(WEAPON_POSE ePose);

	XMFLOAT2 GetMoveInput2D() const;
	int GetRunAnimationFromInput(const XMFLOAT2& dir) const;
	bool IsMoveInputActive(const XMFLOAT2& dir) const;
	XMFLOAT3 GetMoveDirectionFromInput(const XMFLOAT2& dir) const;

	void SetWeaponIdlePose(const XMFLOAT3& pos, const XMFLOAT3& rot)
	{
		m_xmf3WeaponIdlePos = pos;
		m_xmf3WeaponIdleRot = rot;
	}
	void SetWeaponRunPose(const XMFLOAT3& pos, const XMFLOAT3& rot)
	{
		m_xmf3WeaponRunPos = pos;
		m_xmf3WeaponRunRot = rot;
	}
	void SetWeaponGrenadePose(const XMFLOAT3& pos, const XMFLOAT3& rot)
	{
		m_xmf3WeaponGrenadePos = pos;
		m_xmf3WeaponGrenadeRot = rot;
	}
	void SetWeaponShootPose(const XMFLOAT3& pos, const XMFLOAT3& rot)
	{
		m_xmf3WeaponShootPos = pos;
		m_xmf3WeaponShootRot = rot;
	}

	void SetWeaponBlending(bool bBlending) { m_bWeaponBlending = bBlending; }
	void SetWeaponBlendTime(float fTime) { m_fWeaponBlendTime = fTime; }

	void BeginGrenadeWeaponPose();
	void EndGrenadeWeaponPose();

	//현재 장착 무기 데이터
	bool EquipWeaponItem(const std::shared_ptr<WeaponItem>& pItem, const char* pstrSocketName);

	void InitializeWeaponAmmo();
	void UpdateWeaponCombat(float fTimeElapsed);

	bool TryFireWeapon();
	void StartReload();
	bool CanFireWeapon() const;

	bool IsReloading() const { return m_bReloading; }
	int GetCurrentAmmo() const { return m_nCurrentAmmo; }
	int GetMaxAmmo() const { return m_nMaxAmmo; }

	float GetWeaponShotInterval() const;
	float GetWeaponDamage() const;
	
	void SetFireHeld(bool bHeld) { m_bFireHeld = bHeld; }
	bool IsFireHeld() const { return m_bFireHeld; }

	void NotifyWeaponFired();
	bool ConsumeShotAnimRequest();

	float GetReloadDuration() const { return m_fReloadDuration; }

	bool IsShootState() const;

	// 04.10 추가: 서버 위치 보간
	void SetServerPosition(const XMFLOAT3& pos) { m_xmf3ServerPosition = pos; }
};

class CPlayerAnimationController : public CAnimationController
{
private:
	CPlayer* m_pOwner = nullptr;

public:
	CPlayerAnimationController(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		int nAnimationTracks,
		CLoadedModelInfo* pModel,
		CPlayer* pOwner)
		: CAnimationController(pd3dDevice, pd3dCommandList, nAnimationTracks, pModel)
	{
		m_pOwner = pOwner;
	}

	virtual void OnAnimationIK(CGameObject* pRootGameObject) override;
};

class CSoundCallbackHandler : public CAnimationCallbackHandler
{
public:
	CSoundCallbackHandler() {}
	~CSoundCallbackHandler() {}

public:
	virtual void HandleCallback(void* pCallbackData, float fTrackPosition);
};

enum PLAYER_ANIM {
	ANIM_IDLE = 0,
	ANIM_RUN_F,
	ANIM_RUN_L,
	ANIM_RUN_R,
	ANIM_RUN_B,
	ANIM_GRENADE,
	ANIM_SHOOT,
	ANIM_RELOAD,
	ANIM_DIE
};

class CTerrainPlayer : public CPlayer
{
public:
	CTerrainPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* shader);
	virtual ~CTerrainPlayer();

public:
	virtual CCamera* ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed);

	virtual void OnPlayerUpdateCallback(float fTimeElapsed);
	virtual void OnCameraUpdateCallback(float fTimeElapsed);

	virtual void Move(ULONG nDirection, float fDistance, bool bVelocity = false);

	virtual void Update(float fTimeElapsed);
};

class PlayerIdle : public State<CPlayer> {
	virtual bool Enter(CPlayer* Player) override;
	virtual void Update(CPlayer* Player, float fTimeElapsed) override;
	virtual void Exit(CPlayer* Player) override;
};

class PlayerRun : public State<CPlayer> {
	virtual bool Enter(CPlayer* Player) override;
	virtual void Update(CPlayer* Player, float fTimeElapsed) override;
	virtual void Exit(CPlayer* Player) override;
};

class PlayerGrenade : public State<CPlayer> {
private:
	float m_fElapsed = 0.0f;
	int m_nLastLowerAnim = ANIM_IDLE;
	bool m_bKeepRun = false;

public:
	virtual bool Enter(CPlayer* Player) override;
	virtual void Update(CPlayer* Player, float fTimeElapsed) override;
	virtual void Exit(CPlayer* Player) override;
};

class PlayerShoot : public State<CPlayer> {
private:
	float m_fElapsed = 0.0f;
	float m_fAnimDuration = 0.15f;

public:
	virtual bool Enter(CPlayer* Player) override;
	virtual void Update(CPlayer* Player, float fTimeElapsed) override;
	virtual void Exit(CPlayer* Player) override;
};

class PlayerReload : public State<CPlayer> {
public:
	virtual bool Enter(CPlayer* Player) override;
	virtual void Update(CPlayer* Player, float fTimeElapsed) override;
	virtual void Exit(CPlayer* Player) override;
};

class PlayerDie : public State<CPlayer> {
	virtual bool Enter(CPlayer* Player);
	virtual void Update(CPlayer* Player, float fTimeElapsed);
	virtual void Exit(CPlayer* Player);
};