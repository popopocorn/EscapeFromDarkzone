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
	outPos.y += outDir.y * 0.05f -0.55;
	outPos.z += outDir.z * 0.05f;
	
	return true;
}

void NetPacketDispatcher::Handle(std::vector<char>& packet)
{
	if (packet.size() < 2)
		return;

	CGameFramework& gf = *m_pFramework;

	char type = packet[1];
	switch (type)
	{
	case SC_LOGIN_INFO:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnLoginInfo(reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet.data()));
		break;
	}
	case SC_ADD_PLAYER:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnAddPlayer(reinterpret_cast<SC_ADD_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_REMOVE_PLAYER:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnRemovePlayer(reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_MOVE_PLAYER:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnMovePlayer(reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(packet.data()));
		break;
	}
	case SC_PLAYER_STATE_CHANGE:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnPlayerStateChange(reinterpret_cast<SC_PLAYER_STATE_CHANGE_PACKET*>(packet.data()));
		break;
	}
	case SC_CHANGE_WEAPON:
	{
		SC_CHANGE_WEAPON_PACKET* p = reinterpret_cast<SC_CHANGE_WEAPON_PACKET*>(packet.data());

		wchar_t buf[256];
		swprintf_s(buf, L"[SC_CHANGE_WEAPON] recv id=%d weapon_type=%d weapon_grade=%d\n", p->id, p->weapon_type, p->weapon_grade);
		OutputDebugStringW(buf);

		if (!gf.m_pNetEntityMgr)
		{
			OutputDebugString(L"[SC_CHANGE_WEAPON] NetEntityManager is null.\n");
			break;
		}

		OtherPlayer* pOther = gf.m_pNetEntityMgr->FindOtherPlayer(p->id);

		if (!pOther)
		{
			OutputDebugString(L"[SC_CHANGE_WEAPON] OtherPlayer not found.\n");
			break;
		}

		pOther->ChangeWeaponFromServer(p->weapon_type, p->weapon_grade);

		OutputDebugString(L"[SC_CHANGE_WEAPON] OtherPlayer weapon changed.\n");

		break;
	}
	case SC_ROUND_START:
	{
		// 라운드 시작 수신. 이 패킷이 온 시점에 서버는 이미 이동 가드를 풀었음.
		// 클라는 이거 받고 나서부터 시작. (약간 수정 필요)
		OutputDebugString(L"[ROUND] SC_ROUND_START received\n");
		// TODO: 대기 화면 해제 / 입력 활성화 등 뭔가 UI를 붙이기

		MainScene* pMainScene = dynamic_cast<MainScene*>(gf.m_pScene.back().get());
		if (not pMainScene)break;
		pMainScene->StartGame();
		break;
	}
	case SC_ESCAPE_SUCCESS:
	{
		SC_ESCAPE_SUCCESS_PACKET* p =
			reinterpret_cast<SC_ESCAPE_SUCCESS_PACKET*>(packet.data());
		wchar_t buf[128];
		swprintf_s(buf, L"[ESCAPE] success! time=%.2fs\n", p->escape_time_sec);
		OutputDebugStringW(buf);
		// TODO: 탈출 성공 UI 표시 (된다면)
		break;
	}
	case SC_GAME_OVER:
	{
		// 라운드 제한 시간 초과 (추후 패킷 확장 필요시 확장해서 사용할 것)
		OutputDebugString(L"[ROUND] SC_GAME_OVER received\n");
		break;
	}
	case SC_ADD_NPC:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnAddNpc(reinterpret_cast<SC_ADD_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_REMOVE_NPC:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnRemoveNpc(reinterpret_cast<SC_REMOVE_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_MOVE_NPC:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnMoveNpc(reinterpret_cast<SC_MOVE_NPC_PACKET*>(packet.data()));
		break;
	}
	case SC_NPC_STATE_CHANGE:
	{
		if (!gf.m_pNetEntityMgr) break;
		gf.m_pNetEntityMgr->OnNpcStateChange(reinterpret_cast<SC_NPC_STATE_CHANGE_PACKET*>(packet.data()));
		break;
	}
	case SC_INVENTORY_UPDATE:
	{
		SC_INVENTORY_UPDATE_PACKET* p = reinterpret_cast<SC_INVENTORY_UPDATE_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;

		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->ApplyPlayerInventorySlotUpdate(p->item_id, p->count, p->slotidx);

		break;
	}
	case SC_EQUIPMENT_UPDATE: {
		SC_EQUIPMENT_UPDATE_PACKET* p =
			reinterpret_cast<SC_EQUIPMENT_UPDATE_PACKET*>(packet.data());
		
		// 장비 시스템 미구현 - 현재는 콘솔 출력만
		//gf.m_pNetEntityMgr->OnApplyEquip(reinterpret_cast<SC_EQUIPMENT_UPDATE_PACKET*>(packet.data()));
		gf.m_pScene.back()->equipUI->EquipItem(p->equip_id);
		wchar_t buf[128];
		swprintf_s(buf, L"[EQUIP_CRAFTED] equip_id:%d\n", static_cast<int>(p->equip_id));
		OutputDebugStringW(buf);


		break;
	}
	case SC_ADD_LOOT_BOX:
	{
		SC_ADD_LOOT_BOX_PACKET* p = reinterpret_cast<SC_ADD_LOOT_BOX_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;

		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->SpawnLootContainer(p->npc_id, XMFLOAT3(p->x, p->y, p->z), p->items, p->counts, INVENTORY_SIZE);

		break;
	}
	case SC_LOOT_BOX_SLOT_UPDATE:
	{
		SC_LOOT_BOX_SLOT_UPDATE_PACKET* p = reinterpret_cast<SC_LOOT_BOX_SLOT_UPDATE_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;

		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->ApplyLootBoxSlotUpdate(p->box_id, p->slotidx, p->item_id, p->count);

		break;
	}
	case SC_DEACTIVATE_LOOT_BOX:
	{
		SC_DEACTIVATE_LOOT_BOX_PACKET* p = reinterpret_cast<SC_DEACTIVATE_LOOT_BOX_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;

		InventoryManager* pInvMgr = gf.m_pScene.back()->GetInventoryManager();
		if (!pInvMgr) break;

		pInvMgr->DeactivateLootBox(p->npc_id);

		break;
	}
	case SC_PLAYER_HP_UPDATE:
	{
		SC_PLAYER_HP_UPDATE_PACKET* p = reinterpret_cast<SC_PLAYER_HP_UPDATE_PACKET*>(packet.data());

		if (gf.m_pPlayer)
		{
			gf.m_pPlayer->SetHP(p->hp);
		}

		break;
	}
	case SC_FIRE_TRACER:
	{
		SC_FIRE_TRACER_PACKET* p = reinterpret_cast<SC_FIRE_TRACER_PACKET*>(packet.data());

		if (gf.m_pScene.empty()) break;

		MainScene* pMainScene = dynamic_cast<MainScene*>(gf.m_pScene.back().get());
		if (!pMainScene) break;

		XMFLOAT3 origin = XMFLOAT3(p->ox, p->oy, p->oz);
		XMFLOAT3 dir = XMFLOAT3(p->dx, p->dy, p->dz);

		if (Vector3::Length(dir) < 0.0001f)
			dir = XMFLOAT3(0.0f, 0.0f, 1.0f);

		dir = Vector3::Normalize(dir);

		XMFLOAT3 normal = XMFLOAT3(p->nx, p->ny, p->nz);
		ItemType type = static_cast<ItemType>(p->weapon_type);

		pMainScene->ProcessFireRequest(type, origin, dir, p->distance, p->hit_kind, normal);

		break;
	}
	case SC_PLAY_EFFECT_ATTACHED:
	{
		SC_PLAY_EFFECT_ATTACHED_PACKET* p = reinterpret_cast<SC_PLAY_EFFECT_ATTACHED_PACKET*>(packet.data());

		if (!gf.m_pNetEntityMgr)
		{
			break;
		}

		EffectID effectId = static_cast<EffectID>(p->effect_id);
		const unsigned char entityKind = p->entity_kind;
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

		if (effectId == EffectID::SPARK)
		{
			if (entityKind == 0)
			{
				if (CEnemyObject* pNpc = dynamic_cast<CEnemyObject*>(pTarget))
				{
					pNpc->TriggerShootAnim();
				}
			}
			else if (entityKind == 1)
			{
				if (OtherPlayer* pOther = dynamic_cast<OtherPlayer*>(pTarget))
				{
					pOther->TriggerShootAnim();
				}
			}
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

		int ownerId = (static_cast<int>(entityKind) << 16) | static_cast<unsigned short>(entityId);

		pMainScene->PlayEffectFromServerLikeRequest(effectId, effectPos, effectDir, ownerId, 0.0f);

		break;
	}
	case SC_PLAY_EFFECT_WORLD:
	{
		SC_PLAY_EFFECT_WORLD_PACKET* p = reinterpret_cast<SC_PLAY_EFFECT_WORLD_PACKET*>(packet.data());

		EffectID effectId = static_cast<EffectID>(p->effect_id);
		XMFLOAT3 pos = { p->x, p->y, p->z };
		XMFLOAT3 dir = { p->dx, p->dy, p->dz };

		if (Vector3::Length(dir) < 0.0001f)
		{
			dir = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		if (effectId == EffectID::GRENADE_EXPLOSION)
		{
			dir = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		if (gf.m_pScene.empty())
		{
			break;
		}

		MainScene* pMainScene = dynamic_cast<MainScene*>(gf.m_pScene.back().get());

		if (!pMainScene)
		{
			break;
		}

		static int s_nWorldEffectOwnerId = 0x40000000;
		int ownerId = s_nWorldEffectOwnerId++;

		if (s_nWorldEffectOwnerId < 0)
		{
			s_nWorldEffectOwnerId = 0x40000000;
		}

		pMainScene->PlayEffectFromServerLikeRequest(effectId, pos, dir, ownerId, 0.0f);

		break;
	}
	default:
		break;
	}
}