#pragma once
#include <DirectXMath.h>
#include "protocol.h"   // ItemID 등

// 아웃바운드 "의도" 파사드 (인바운드 PacketDispatcher와 대칭).
// 현재는 NetworkManager의 Send*로 전달만 하는 얇은 래퍼.
// collision_server_migration_plan.md 2단계(입력 시퀀스/pending-input)가
// 들어갈 단일 지점 — 그때 Move()에 시퀀스 부여·pending 기록이 붙음.
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
	bool FireHit(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, char weaponId);
	bool FireHitPlayer(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, char weaponId);
	bool Craft(ItemID target);
	bool InventoryClick(char action, short slotidx);
	bool LootPickup(short box_id, short slotidx);
};
