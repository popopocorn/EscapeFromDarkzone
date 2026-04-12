#pragma once
#include "stdafx.h"
#include "Object.h"
#include"State.h"
#include "Player.h"


class OtherPlayer : public CGameObject
{
private:
	//이곳에 네트워크 통신에 필요한 프라이빗(또는 필요 시 프로텍티드) 인자 선언


public:
	OtherPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual ~OtherPlayer();


	virtual void Animate(float fTimeElapsed) override;
	virtual void Update(float fTimeElapsed);
	CAnimationController* GetAnimationController() { return m_pSkinnedAnimationController; }

	void ChangeState(std::unique_ptr<State<OtherPlayer>> pNewState);
	std::unique_ptr<State<OtherPlayer>> m_pState;

	//네트워크 관련 함수 추가
	//캐릭터 위치변경은 SetPosition으로, 상태 변경은 ChangeState로

	static OtherPlayer* Create(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, 
		float x, float y, float z);

	void UpdatePosition(float x, float y, float z);

	void Kill() { Alive = false; }
};


class OtherPlayerIdle : public State<OtherPlayer> {
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerRun : public State<OtherPlayer> {
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};

class OtherPlayerDie : public State<OtherPlayer> {
	virtual bool Enter(OtherPlayer* Player);
	virtual void Update(OtherPlayer* Player, float fTimeElapsed);
	virtual void Exit(OtherPlayer* Player);
};