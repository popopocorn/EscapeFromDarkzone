//EnemyObject.h
#pragma once
#include "Object.h"

class CPlayer;
class CHeightMapTerrain;

class CEnemyObject : public CGameObject
{
public:
	CEnemyObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~CEnemyObject();

	virtual void Animate(float fTimeElapsed) override;

	void SetPlayer(CGameObject* pPlayer) { m_pPlayer = pPlayer; }
	void SetTerrain(CHeightMapTerrain* pTerrain) { m_pTerrain = pTerrain; }

private:
	CGameObject* m_pPlayer = nullptr;
	CHeightMapTerrain* m_pTerrain = nullptr;

	float				m_fMoveSpeed = 5.0f;
	float				m_fDetectionRange = 300.0f;
	float				m_fAttackRange = 1.0f;

	const int ANIM_IDLE = 1;
	const int ANIM_RUN = 2;
};

