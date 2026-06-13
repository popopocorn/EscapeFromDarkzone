#include "Server_Npc.h"

std::array<SERVER_NPC, MAX_NPC> g_npcs;

void init_npcs()
{
    for (short i = 0; i < MAX_NPC; ++i) {
        SERVER_NPC& npc = g_npcs[i];

        npc.id = i;
        npc.kind = NPC_TIER_1;   // 단계(tier) 기본값
        npc.outfit = 0;          // 외형 프리셋 기본값
        npc.alive = false;
        npc.state = NPC_STATE_IDLE;

        npc.position = { 0.0f, 0.0f, 0.0f };
        npc.spawn_position = { 0.0f, 0.0f, 0.0f };
        npc.yaw = 0.0f;

        npc.hp = 0;
        npc.max_hp = 0;

        npc.path_update_timer = 0.0f;
        npc.waypoints.clear();
        npc.way_idx = 0;
        npc.die_timer = 0.0f;

        npc.coll_normals.clear();

        npc.think_timer = 0.0f;             // 사고 주기

        npc.lose_sight_timer = 0.0f;        // 마지막 목격 처리 관련
        npc.has_last_seen_player = false;
        npc.last_seen_player_pos = { 0.0f, 0.0f, 0.0f };

        npc.return_ignore_timer = 0.0f;

        npc.aim_timer = 0.0f;               // 조준 시간, 공격 쿨다운
        npc.attack_cooldown = 0.0f;

        npc.burst_shots_left = 0;           // 버스트 사격
        npc.burst_serial = 0;
        npc.burst_shot_timer = 0.0f;
        npc.burst_rest_timer = 0.0f;

        npc.strafe_timer = 0.0f;            // 스트레이프
        npc.strafe_sign = 1.0f;

        npc.current_ammo = 0;               // 재장전
        npc.reloading = false;
        npc.reload_timer = 0.0f;

        npc.weapon_type = static_cast<short>(WeaponType::PISTOL);   // 기본 무기
        npc.weapon_grade = static_cast<short>(WeaponGrade::BASIC);

        // 인벤토리 관련 초기화
        npc._inventory.fill(ItemSlot{});
        npc.loot_active = false;
        npc.death_time = {};
    }
}

// NpcInputQueue
NpcInputQueue g_npc_input_queue;

void NpcInputQueue::Push(NpcInputEvent e)
{
    std::lock_guard<std::mutex> lk(_mtx);
    _events.push_back(std::move(e));
}

void NpcInputQueue::DrainTo(std::vector<NpcInputEvent>& out)
{
    // 이전 out 비운 뒤 swap 하기
    out.clear();
    std::lock_guard<std::mutex> lk(_mtx);
    std::swap(out, _events);
}