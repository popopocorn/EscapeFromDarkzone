//EnemyObject.h
#pragma once
#include "stdafx.h"
#include "Object.h"
#include "State.h"

class CPlayer;

enum ENEMY_ANIM {
	ENEMY_ANIM_IDLE = 0,
	ENEMY_ANIM_RUN = 1
};

class CEnemyObject : public CGameObject
{
public:
	CEnemyObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~CEnemyObject();

	virtual void Animate(float fTimeElapsed) override;
	virtual void Update(float fTimeElapsed);
	virtual void HandleCollision(XMFLOAT3 normal);

	void SetPlayer(CGameObject* pPlayer) { m_pPlayer = pPlayer; }
	

	void ChangeState(std::unique_ptr<State<CEnemyObject>> pNewState);

	void SetMoveDir(const XMFLOAT3& dir) { m_xmf3MoveDir = dir; }
	XMFLOAT3 GetMoveDir() const { return m_xmf3MoveDir; }
	void HandleHP(float value);

	std::unique_ptr<State<CEnemyObject>> m_pState;

	CGameObject*				m_pPlayer = nullptr;
	float						hp = 100;
	

public:
	float						m_fMoveSpeed = 7.0f;
	float						m_fDetectionRange = 10.0f;
	float						m_fAttackRange = 3.0f;

	XMFLOAT3					m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 10.0f);
	XMFLOAT3					m_xmf3MoveDir = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
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



