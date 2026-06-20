#include "stdafx.h"
#include "NetSession.h"
#include "Network.h"

// 현재는 순수 전달. Stage 2에서 Move()에 시퀀스 번호 부여 +
// pending-input 버퍼 기록이 여기 추가될 예정(호출처는 그대로).

bool NetSession::Move(char inputs, float yaw, unsigned int move_time)
{
	return NetworkManager::Instance().SendMove(inputs, yaw, move_time);
}

bool NetSession::FireHit(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade)
{
	return NetworkManager::Instance().SendHitNpc(rayOrigin, rayDirection, weaponType, weaponGrade);
}

bool NetSession::FireHitPlayer(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade)
{
	return NetworkManager::Instance().SendHitPlayer(rayOrigin, rayDirection, weaponType, weaponGrade);
}

bool NetSession::ChangeWeapon(short weaponType, short weaponGrade)
{
	return NetworkManager::Instance().SendChangeWeapon(weaponType, weaponGrade);
}

bool ReloadRequest(short weaponType)
{
	return NetworkManager::Instance().SendReloadRequest(weaponType);
}

bool NetSession::ChangeState(char state)
{
	return NetworkManager::Instance().SendChangeState(state);
}

bool NetSession::Craft(ItemID target)
{
	return NetworkManager::Instance().SendCraftRequest(target);
}

bool NetSession::GrenadeExplode(const DirectX::XMFLOAT3& pos)
{
	return NetworkManager::Instance().SendGrenadeExplode(pos);
}

bool NetSession::InventoryClick(char action, short slotidx)
{
	return NetworkManager::Instance().SendInventoryClick(action, slotidx);
}

bool NetSession::LootPickup(short box_id, short slotidx)
{
	return NetworkManager::Instance().SendLootPickup(box_id, slotidx);
}