//EnemyObject.h
#pragma once
#include "stdafx.h"
#include "Object.h"

class CPlayer;
class CHeightMapTerrain;
class EnemyState;

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

	void SetPlayer(CGameObject* pPlayer) { m_pPlayer = pPlayer; }
	void SetTerrain(CHeightMapTerrain* pTerrain) { m_pTerrain = pTerrain; }

	void ChangeState(std::unique_ptr<EnemyState> pNewState);

	std::unique_ptr<EnemyState> m_pState;

	CGameObject* m_pPlayer = nullptr;
	CHeightMapTerrain* m_pTerrain = nullptr;

public:
	float				m_fMoveSpeed = 2.0f;
	float				m_fDetectionRange = 10.0f;
	float				m_fAttackRange = 3.0f;

};

class EnemyState
{
public:
	virtual ~EnemyState() {}
	virtual bool Enter(CEnemyObject* pEnemy) = 0;
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed) = 0;
	virtual void Exit(CEnemyObject* pEnemy) = 0;
};

class EnemyIdle : public EnemyState
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};

class EnemyRun : public EnemyState
{
public:
	virtual bool Enter(CEnemyObject* pEnemy);
	virtual void Update(CEnemyObject* pEnemy, float fTimeElapsed);
	virtual void Exit(CEnemyObject* pEnemy);
};