#pragma once
#pragma comment(lib, "winmm.lib")

#define DIR_FORWARD				0x01
#define DIR_BACKWARD			0x02
#define DIR_LEFT				0x04
#define DIR_RIGHT				0x08
#define DIR_UP					0x10
#define DIR_DOWN				0x20

#include "Object.h"
#include "Camera.h"
#include "Network.h"



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
	RUN
};

class PlayerState;
class CPlayerAnimationController;

class CPlayer : public CGameObject
{
protected:
	XMFLOAT3					m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3					m_xmf3Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	float           			m_fPitch = 0.0f;
	float           			m_fYaw = 0.0f;
	float           			m_fRoll = 0.0f;

	XMFLOAT3					m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3     				m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float           			m_fMaxVelocityXZ = 0.0f;
	float           			m_fMaxVelocityY = 0.0f;
	float           			m_fFriction = 0.0f;

	LPVOID						m_pPlayerUpdatedContext = NULL;
	LPVOID						m_pCameraUpdatedContext = NULL;

	CCamera						*m_pCamera = NULL;
	std::unique_ptr<PlayerState> state;
	std::queue<GameEvent>		event_queue;
	
	XMFLOAT3					MoveDir = XMFLOAT3(0, 0, 0);
	float						speed{};


	//충돌 노멀
	std::vector<XMFLOAT3>		CollVector;

	// 네트워크 테스트
	WSADATA WSAData;
	SOCKET c_socket;
	SOCKADDR_IN addr;

	char send_buf[BUF_SIZE];

	CGameObject* m_pWeapon = nullptr;
	XMFLOAT4X4 m_xmf4x4WeaponBaseLocal = Matrix4x4::Identity();
	bool m_bWeaponBaseLocalSaved = false;

	WEAPON_POSE m_eWeaponPose = WEAPON_POSE::IDLE;

	XMFLOAT3 m_xmf3WeaponIdlePos = XMFLOAT3(0.0f, 0.1f, 0.1f);
	XMFLOAT3 m_xmf3WeaponIdleRot = XMFLOAT3(0.0f, -45.0f, 45.0f);

	XMFLOAT3 m_xmf3WeaponRunPos = XMFLOAT3(0.50f, 0.45f, 0.10f);
	XMFLOAT3 m_xmf3WeaponRunRot = XMFLOAT3(-50.0f, 30.0f, 0.0f);

	XMFLOAT3 m_xmf3WeaponScale = XMFLOAT3(1.2f, 1.2f, 1.2f);

	CGameObject* m_pLeftUpperArm = nullptr;
	CGameObject* m_pLeftForeArm = nullptr;
	CGameObject* m_pLeftHand = nullptr;
	CGameObject* m_pLeftHandGrip = nullptr;

	bool  m_bUseLeftHandIK = true;
	float m_fLeftHandIKWeight = 1.0f;

	XMFLOAT3 m_xmf3CachedLeftElbowDir = XMFLOAT3(0.0f, 0.0f, 0.0f);
	bool m_bLeftElbowDirCached = false;

	CGameObject* FindFirstFrameByNames(const char* const* ppNames, int nCount);
	bool InitializeLeftHandIK();
	XMVECTOR GetStableLeftElbowBendDir(FXMVECTOR vShoulder, FXMVECTOR vElbow, FXMVECTOR vTargetDir);
	float GetLeftHandIKWeight() const;

	void RotateBoneTowardTarget(CGameObject* pBone, const XMFLOAT3& xmf3CurrentChildWorldPos, const XMFLOAT3& xmf3TargetChildWorldPos, float fWeight);

	void MatchBoneWorldRotation(CGameObject* pBone, CGameObject* pTarget, float fWeight);

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
	void SetPosition(const XMFLOAT3& xmf3Position) { Move(XMFLOAT3(xmf3Position.x - m_xmf3Position.x, xmf3Position.y - m_xmf3Position.y, xmf3Position.z - m_xmf3Position.z), false); }

	void SetScale(const XMFLOAT3& xmf3Scale) { m_xmf3Scale = xmf3Scale; }

	const XMFLOAT3& GetVelocity() const { return(m_xmf3Velocity); }
	float GetYaw() const { return(m_fYaw); }
	float GetPitch() const { return(m_fPitch); }
	float GetRoll() const { return(m_fRoll); }

	CCamera *GetCamera() { return(m_pCamera); }
	void SetCamera(CCamera *pCamera) { m_pCamera = pCamera; }

	virtual void Move(ULONG nDirection, float fDistance, bool bVelocity = false);
	void Move(const XMFLOAT3& xmf3Shift, bool bVelocity = false);
	void Move(float fxOffset = 0.0f, float fyOffset = 0.0f, float fzOffset = 0.0f);
	void Rotate(float x, float y, float z);

	virtual void Update(float fTimeElapsed);

	virtual void OnPlayerUpdateCallback(float fTimeElapsed) { }
	void SetPlayerUpdatedContext(LPVOID pContext) { m_pPlayerUpdatedContext = pContext; }

	virtual void OnCameraUpdateCallback(float fTimeElapsed) { }
	void SetCameraUpdatedContext(LPVOID pContext) { m_pCameraUpdatedContext = pContext; }

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);

	CCamera *OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode);

	virtual CCamera *ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed) { return(NULL); }
	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nPipelineState, CCamera *pCamera = NULL);

	CAnimationController* GetAnimationController() { return m_pSkinnedAnimationController; }
	void AddEvent(const GameEvent& event) { event_queue.push(event); }
	void ChangeState(std::unique_ptr<PlayerState> new_state);

	void SetMoveDir(XMFLOAT3 dir) { MoveDir = dir; }
	virtual void HandleCollision(XMFLOAT3 normal);
	void UpdateDirection();
	bool IsGrenadeState() const;
	void EquipWeapon(CGameObject* pWeapon, const char* pstrSocketName);
	CGameObject* GetWeapon() { return m_pWeapon; }
	void UpdateWeaponPose();
	void ApplyWeaponPose(WEAPON_POSE ePose);
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

	void SolveLeftHandIK();
	void SetUseLeftHandIK(bool bUse) { m_bUseLeftHandIK = bUse; }
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
	CSoundCallbackHandler() { }
	~CSoundCallbackHandler() { }

public:
	virtual void HandleCallback(void *pCallbackData, float fTrackPosition); 
};


enum PLAYER_ANIM {
	ANIM_IDLE = 0,
	ANIM_RUN_F,
	ANIM_RUN_L,
	ANIM_RUN_R,
	ANIM_RUN_B,
	ANIM_GRENADE
};


class CTerrainPlayer : public CPlayer
{
public:
	CTerrainPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CShader* shader);
	virtual ~CTerrainPlayer();

public:
	virtual CCamera *ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed);

	virtual void OnPlayerUpdateCallback(float fTimeElapsed);
	virtual void OnCameraUpdateCallback(float fTimeElapsed);

	virtual void Move(ULONG nDirection, float fDistance, bool bVelocity = false);

	virtual void Update(float fTimeElapsed);
private:
	int m_nCurTrack = 0; 
	int m_nNextTrack = 1;

	int m_nCurAnimType = -1;

	float m_fBlendTime = 0.0f; 
	float m_fBlendDuration = 0.2f;
	bool  m_bIsBlending = false;
};


class PlayerState {
public:
	virtual ~PlayerState() {};
	virtual bool Enter(CPlayer* Player) { return false; }
	virtual void Update(CPlayer* Player) {}
	virtual void Exit(CPlayer* Player) {}
};


class PlayerIdle : public PlayerState {
	virtual bool Enter(CPlayer* Player);
	virtual void Update(CPlayer* Player);
	virtual void Exit(CPlayer* Player);
};

class PlayerRun : public PlayerState {
	virtual bool Enter(CPlayer* Player);
	virtual void Update(CPlayer* Player);
	virtual void Exit(CPlayer* Player);
};

class PlayerGrenade : public PlayerState {
public:
	virtual bool Enter(CPlayer* Player);
	virtual void Update(CPlayer* Player);
	virtual void Exit(CPlayer* Player);
};

class PlayerDie : public PlayerState {
	virtual bool Enter(CPlayer* Player);
	virtual void Update(CPlayer* Player);
	virtual void Exit(CPlayer* Player);
};
