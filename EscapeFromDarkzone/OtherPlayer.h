#pragma once
#include "stdafx.h"
#include "Object.h"
#include "State.h"
#include "Player.h"
#include "ResourceManager.h"

class OtherPlayer : public CGameObject
{
private:
	XMFLOAT3 m_xmf3ServerPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float    m_fServerYawDeg = 0.0f;
	bool     m_bUseServerLerp = false;
	bool     m_bServerMoving = false;
	bool     m_bDead = false;

private:
	CGameObject* m_pWeapon = nullptr;
	CGameObject* m_pWeaponSocket = nullptr;
	CGameObject* m_pWeaponMuzzleSocket = nullptr;
	unique_ptr<CGameObject> m_pRenderWeapon = nullptr;

	PlayerWeaponType m_eWeaponType = PlayerWeaponType::Pistol;

public:
	OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, short playerID);
	virtual ~OtherPlayer();

	virtual void Animate(float fTimeElapsed) override;
	virtual void Update(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera = NULL) override;

	CAnimationController* GetAnimationController() { return m_pSkinnedAnimationController; }

	StateMachine<OtherPlayer> m_LowerStateMachine;
	StateMachine<OtherPlayer> m_UpperStateMachine;

	void ChangeLowerState(std::unique_ptr<State<OtherPlayer>> pNewState, bool bForce = false);
	void ChangeUpperState(std::unique_ptr<State<OtherPlayer>> pNewState, bool bForce = false);
	void ChangeState(std::unique_ptr<State<OtherPlayer>> pNewState, bool bForce = false); // 기존 네트워크 호출 호환용

	static OtherPlayer* Create(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, float x, float y, float z, short playerID);

	void UpdatePosition(float x, float y, float z);
	void SetServerYaw(float yawRad);
	void Kill() { CGameObject::Kill(); }

	void SetServerMoving(bool bMoving) { if (m_bDead && bMoving) return; m_bServerMoving = bMoving; }
	bool IsServerMoving() const { return m_bServerMoving; }

	bool IsDead() const { return m_bDead; }
	void MarkDeadFromServer();

	PlayerWeaponType GetCurrentPlayerWeaponType() const { return m_eWeaponType; }

	int GetIdleAnimationByWeapon() const;
	int GetForwardRunAnimationByWeapon() const;
	int GetGrenadeAnimationByWeapon() const;
	int GetShootAnimationByWeapon() const;
	int GetReloadAnimationByWeapon() const;
	int GetDieAnimationByWeapon() const;
	int GetLowerAnimationByServerState() const;

	void RefreshBaseAnimationByServerState();

	void TriggerShootAnim();
	void TriggerReloadAnim();
	void TriggerGrenadeAnim();
	void TriggerDieAnim();

	void EquipDefaultPistol();
	void EquipWeaponModel(ModelName modelName);
	void ChangeWeaponFromServer(short weaponType, short weaponGrade);

	CGameObject* GetWeaponMuzzleSocket() const { return m_pWeaponMuzzleSocket; }
	CGameObject* GetRenderWeapon() const { return m_pRenderWeapon.get(); }
	void SubmitWeaponToShader(CShader* shader);
};

class OtherPlayerLowerIdle : public State<OtherPlayer>
{
public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerLowerRun : public State<OtherPlayer>
{
private:
	float m_fFootstepTimer = 0.0f;

public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerUpperIdle : public State<OtherPlayer>
{
public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerUpperGrenade : public State<OtherPlayer>
{
private:
	float m_fElapsed = 0.0f;
	float m_fAnimDuration = 2.80f;

public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerUpperShoot : public State<OtherPlayer>
{
private:
	float m_fElapsed = 0.0f;
	float m_fAnimDuration = 0.35f;

public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerUpperReload : public State<OtherPlayer>
{
private:
	float m_fElapsed = 0.0f;
	float m_fAnimDuration = 1.60f;

public:
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

using OtherPlayerIdle = OtherPlayerLowerIdle;
using OtherPlayerRun = OtherPlayerLowerRun;
using OtherPlayerGrenade = OtherPlayerUpperGrenade;
using OtherPlayerShoot = OtherPlayerUpperShoot;
using OtherPlayerReload = OtherPlayerUpperReload;

class OtherPlayerDie : public State<OtherPlayer>
{
};