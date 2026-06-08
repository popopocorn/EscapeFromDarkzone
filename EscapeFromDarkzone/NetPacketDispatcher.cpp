#include "stdafx.h"
#include "NetPacketDispatcher.h"
#include "GameFramework.h"
#include "NetEntityManager.h"

#include "InputManager.h"
#include "EffectManager.h"
#include "InventoryManager.h"
#include "ResourceManager.h"
#include "ShaderManager.h"
#include "SoundManager.h"

static XMFLOAT3 SafeNormalizeOrDefault(XMFLOAT3 v, XMFLOAT3 fallback)
{
	if (Vector3::Length(v) < 0.0001f)
		return fallback;

	return Vector3::Normalize(v);
}

static bool GetAttachedEffectMuzzleInfo(
	CGameObject* pTarget,
	XMFLOAT3& outPos,
	XMFLOAT3& outDir)
{
	if (!pTarget)
		return false;

	pTarget->UpdateTransform(NULL);

	CGameObject* pMuzzle = nullptr;

	if (CEnemyObject* pNpc = dynamic_cast<CEnemyObject*>(pTarget))
	{
		pMuzzle = pNpc->GetWeaponMuzzleSocket();
	}
	else if (OtherPlayer* pOther = dynamic_cast<OtherPlayer*>(pTarget))
	{
		pMuzzle = pOther->GetWeaponMuzzleSocket();
	}

	if (!pMuzzle)
	{
		pMuzzle = pTarget->FindFrame("Socket_Muzzle");
	}

	if (!pMuzzle)
	{
		//OutputDebugString(L"[Effect] Socket_Muzzle not found. attached effect skipped.\n");
		return false;
	}

	outPos = pMuzzle->GetPosition();

	outDir = pTarget->GetLook();
	outDir = SafeNormalizeOrDefault(outDir, XMFLOAT3(0.0f, 0.0f, 1.0f));

	outPos.x += outDir.x * 0.05f;
	outPos.y += outDir.y * 0.05f;
	outPos.z += outDir.z * 0.05f;

	return true;
}

void NetPacketDispatcher::Handle(std::vector<char>& packet)
{
    CGameFramework& gf = *m_pFramework;

    char type = packet[1];
	switch (type)
	{
	case SC_LOGIN_INFO:
	{
		gf.m_pNetEntityMgr->OnLoginInfo(reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet.data()));
		break;
	}
	case SC_ADD_PLAYER:
	{
		gf.m_pNetEntityMgr->OnAddPlayer(reinterpret_cast<SC_ADD_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_REMOVE_PLAYER:
	{
		gf.m_pNetEntityMgr->OnRemovePlayer(reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_MOVE_PLAYER:
	{
		gf.m_pNetEntityMgr->OnMovePlayer(reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_PLAYER_STATE_CHANGE:
	{
		gf.m_pNetEntityMgr->OnPlayerStateChange(reinterpret_cast<SC_PLAYER_STATE_CHANGE_PACKET*>(packet.data()));
		break;
	}
	case SC_ADD_NPC:
	{
		gf.m_pNetEntityMgr->OnAddNpc(reinterpret_cast<SC_ADD_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_REMOVE_NPC:
	{
		gf.m_pNetEntityMgr->OnRemoveNpc(reinterpret_cast<SC_REMOVE_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_MOVE_NPC:
	{
		gf.m_pNetEntityMgr->OnMoveNpc(reinterpret_cast<SC_MOVE_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_NPC_STATE_CHANGE: {
		gf.m_pNetEntityMgr->OnNpcStateChange(reinterpret_cast<SC_NPC_STATE_CHANGE_PACKET*>(packet.data()));
		break;
	}
	case SC_INVENTORY_UPDATE: {
		SC_INVENTORY_UPDATE_PACKET* p =
			reinterpret_cast<SC_INVENTORY_UPDATE_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;
		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		bool ok = pInvMgr->ApplyPlayerInventorySlotUpdate(
			p->item_id, p->count, p->slotidx);

		// 서버로부터 받은 패킷을 디버그콘솔에 출력
		//wchar_t buf[256];
		//swprintf_s(buf, L"[INV_APPLY] slot:%d item:%d count:%d %s\n",
		//	p->slotidx, static_cast<int>(p->item_id), p->count);
		//OutputDebugStringW(buf);

		break;
	}
	case SC_EQUIPMENT_UPDATE: {
		SC_EQUIPMENT_UPDATE_PACKET* p =
			reinterpret_cast<SC_EQUIPMENT_UPDATE_PACKET*>(packet.data());

		// 장비 시스템 미구현 - 현재는 콘솔 출력만
		wchar_t buf[128];
		swprintf_s(buf, L"[EQUIP_CRAFTED] equip_id:%d\n",
			static_cast<int>(p->equip_id));
		OutputDebugStringW(buf);
		break;
	}
	case SC_ADD_LOOT_BOX: {
		SC_ADD_LOOT_BOX_PACKET* p =
			reinterpret_cast<SC_ADD_LOOT_BOX_PACKET*>(packet.data());

		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();

		pInvMgr->SpawnLootContainer(p->npc_id,
			XMFLOAT3(p->x, p->y, p->z),
			p->items, p->counts, INVENTORY_SIZE);

		//wchar_t buf[256];
		//swprintf_s(buf, L"[LOOT_BOX_ADD] id:%d pos:(%.2f,%.2f,%.2f)\n",
		//	p->npc_id, p->x, p->y, p->z);
		//OutputDebugStringW(buf);

		break;
	}
	case SC_LOOT_BOX_SLOT_UPDATE: {
		SC_LOOT_BOX_SLOT_UPDATE_PACKET* p =
			reinterpret_cast<SC_LOOT_BOX_SLOT_UPDATE_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;
		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->ApplyLootBoxSlotUpdate(p->box_id, p->slotidx, p->item_id, p->count);

		//wchar_t buf[256];
		//swprintf_s(buf, L"[BOX_APPLY] box_id: %d slot:%d item:%d count:%d %s\n",
		//	p->box_id, p->slotidx, static_cast<int>(p->item_id), p->count);
		//OutputDebugStringW(buf);

		break;
	}
	case SC_DEACTIVATE_LOOT_BOX: {
		SC_DEACTIVATE_LOOT_BOX_PACKET* p =
			reinterpret_cast<SC_DEACTIVATE_LOOT_BOX_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;
		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->DeactivateLootBox(p->npc_id);
		break;
	}
	case SC_PLAYER_HP_UPDATE: {
		SC_PLAYER_HP_UPDATE_PACKET* p =
			reinterpret_cast<SC_PLAYER_HP_UPDATE_PACKET*>(packet.data());
		// 본인 플레이어 HP 갱신 (UI 체력바 등 연결은 후속)
		if (gf.m_pPlayer) {
			gf.m_pPlayer->SetHP(p->hp);   // 임시 체력 추가
		}
		break;
	}
	case SC_PLAY_EFFECT_ATTACHED:
	{
		SC_PLAY_EFFECT_ATTACHED_PACKET* p =
			reinterpret_cast<SC_PLAY_EFFECT_ATTACHED_PACKET*>(packet.data());

		EffectID effectId = static_cast<EffectID>(p->effect_id);
		const unsigned char entityKind = p->entity_kind;   // 0 = NPC, 1 = OtherPlayer
		const short entityId = p->entity_id;

		CGameObject* pTarget = nullptr;

		if (entityKind == 0)
		{
			pTarget = gf.m_pNetEntityMgr->FindNpc(entityId);
		}
		else if (entityKind == 1)
		{
			pTarget = gf.m_pNetEntityMgr->FindOtherPlayer(entityId);
		}

		if (!pTarget)
		{
			OutputDebugString(L"[Effect] attached target not found.\n");
			break;
		}

		if (gf.m_pScene.empty())
		{
			break;
		}

		if (entityKind == 0 && effectId == EffectID::SPARK)
		{
			if (CEnemyObject* pNpc = static_cast<CEnemyObject*>(pTarget))
				pNpc->TriggerShootAnim();
		}

		MainScene* pMainScene = dynamic_cast<MainScene*>(gf.m_pScene.back().get());
		if (!pMainScene)
		{
			break;
		}

		XMFLOAT3 effectPos;
		XMFLOAT3 effectDir;

		if (!GetAttachedEffectMuzzleInfo(pTarget, effectPos, effectDir))
		{
			break;
		}

		int ownerId =
			(static_cast<int>(entityKind) << 16) |
			static_cast<unsigned short>(entityId);

		pMainScene->PlayEffectFromServerLikeRequest(
			effectId,
			effectPos,
			effectDir,
			ownerId,
			0.0f
		);

		break;
	}
	case SC_PLAY_EFFECT_WORLD: {
		// 수류탄 등등?
		SC_PLAY_EFFECT_WORLD_PACKET* p =
			reinterpret_cast<SC_PLAY_EFFECT_WORLD_PACKET*>(packet.data());

		EffectID effectId = static_cast<EffectID>(p->effect_id);
		XMFLOAT3 pos = { p->x, p->y, p->z };
		XMFLOAT3 dir = { p->dx, p->dy, p->dz };


		break;
	}
	default:
		break;
	}
}