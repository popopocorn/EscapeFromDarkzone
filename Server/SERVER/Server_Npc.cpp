#include "Server_Npc.h"

std::array<SERVER_NPC, MAX_NPC> g_npcs;

void init_npcs()
{
    for (short i = 0; i < MAX_NPC; ++i) {
        SERVER_NPC& npc = g_npcs[i];

        npc.id = i;
        npc.kind = 0;
        npc.alive = false;
        npc.state = NPC_STATE_IDLE;

        npc.position = { 0.0f, 0.0f, 0.0f };
        npc.yaw = 0.0f;

        npc.hp = 0;
        npc.max_hp = 0;

        npc.path_update_timer = 0.0f;
        npc.waypoints.clear();
        npc.way_idx = 0;
        npc.die_timer = 0.0f;

        npc.coll_normals.clear();

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