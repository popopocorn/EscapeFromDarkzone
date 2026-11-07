#pragma once
#include <DirectXMath.h>
#include "protocol.h"   // ItemID 등

// 서버로 보내는 게임 동작 요청을 NetworkManager에 전달한다.
class NetSession
{
private:
	NetSession() = default;
	~NetSession() = default;

public:
	static NetSession& Instance()
	{
		static NetSession inst;
		return inst;
	}

	NetSession(const NetSession&) = delete;
	NetSession& operator=(const NetSession&) = delete;

	// NetworkManager의 Send*와 인자 1:1 동일
	bool Move(char inputs, float yaw, unsigned int move_time);
	bool FireHit(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade);
	bool ChangeWeapon(short weaponType, short weaponGrade);
	bool ReloadRequest(short weaponType);
	bool ChangeState(char state);
	bool GrenadeExplode(const DirectX::XMFLOAT3& pos);
	bool RoundJoin();
	bool RoundLeave();

	bool ToggleGodmode();				// 디버그용 갓모드
};
