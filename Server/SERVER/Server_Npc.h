#pragma once

#include <array>
#include <vector>
#include <mutex>
#include <DirectXMath.h>
#include "protocol.h"

using namespace DirectX;

struct SERVER_NPC {
    short    id;                 // NPC ID
    char     kind;               // 나중에 NPC 종류가 늘어나면 쓸 것
    bool     alive;              // 살아있는지 (죽으면 slot 안 쓰게?)
    char     state;              // NPC상태  NPC_STATE_IDLE / NPC_STATE_RUN / NPC_STATE_DIE

    XMFLOAT3 position;
    float    yaw;

    short    hp;
    short    max_hp;

    float                  path_update_timer;  // A* 주기 (1초마다)
    std::vector<XMFLOAT3>  waypoints;          // 현재 경로
    int                    way_idx;            // 다음 목표 waypoint 인덱스
    
    float                  die_timer;          // Die 상태 진입 후 경과 시간

    std::vector<XMFLOAT3>  coll_normals;       // 이번 틱 누적 충돌 노멀 (아직안씀?)
};

extern std::array<SERVER_NPC, MAX_NPC> g_npcs;

void init_npcs();

// NpcInputEvent
struct NpcInputEvent {
    enum Type { HIT, NEW_CLIENT_JOINED };
    Type type;

    int      attacker_client_id;
    XMFLOAT3 ray_origin;
    XMFLOAT3 ray_direction;
    char     weapon_id;

    int      new_client_id;
};

// NpcInputQueue
class NpcInputQueue {
    std::mutex                  _mtx;
    std::vector<NpcInputEvent>  _events;
public:
    void Push(NpcInputEvent e);
    void DrainTo(std::vector<NpcInputEvent>& out);
};

extern NpcInputQueue g_npc_input_queue;
