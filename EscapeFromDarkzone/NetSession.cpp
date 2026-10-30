#include "stdafx.h"
#include "NetSession.h"
#include "Network.h"

bool NetSession::Move(char inputs, float yaw, unsigned int move_time)
{
	return NetworkManager::Instance().SendMove(inputs, yaw, move_time);
}

bool NetSession::FireHit(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade)
{
	return NetworkManager::Instance().SendHitNpc(rayOrigin, rayDirection, weaponType, weaponGrade);
}

bool NetSession::ChangeWeapon(short weaponType, short weaponGrade)
{
	return NetworkManager::Instance().SendChangeWeapon(weaponType, weaponGrade);
}

bool NetSession::ReloadRequest(short weaponType)
{
	return NetworkManager::Instance().SendReloadRequest(weaponType);
}

bool NetSession::ChangeState(char state)
{
	return NetworkManager::Instance().SendChangeState(state);
}

bool NetSession::GrenadeExplode(const DirectX::XMFLOAT3& pos)
{
	return NetworkManager::Instance().SendGrenadeExplode(pos);
}

bool NetSession::RoundJoin()
{
	return NetworkManager::Instance().SendRoundJoin();
}

bool NetSession::RoundLeave()
{
	return NetworkManager::Instance().SendRoundLeave();
}

bool NetSession::ToggleGodmode()
{
	return NetworkManager::Instance().SendToggleGodmode();
}
