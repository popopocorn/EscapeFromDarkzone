#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

#include "protocol.h"
#include "Server_Npc.h"

// ===== 룸 상수 =====
constexpr int MAX_ROOMS = 100;      // 배열 용량. 
constexpr int ROOM_CAPACITY = 8;    // 룸 정원

enum RoomState : int {
    ROOM_WAITING = 0,
    ROOM_IN_PROGRESS = 1,
    ROOM_FINISHED = 2,
};

// 플레이어 위치 스냅샷.
struct PlayerSnapshot {
    bool  in_game;
    int   client_id = -1;           // 전역 clients[] 인덱스. -1 = 없음
    float x, y, z;
    float yaw;
    short hp;

    bool  targetable;
};

struct Room {
    int      id = -1;               // 배열 인덱스. 기동 시 1회 설정
    bool     alive = false;
    uint32_t generation = 0;        // 슬롯 재사용마다 +1 (R4에서 stale 이벤트 가드에 사용)

    std::atomic<int> state{ ROOM_WAITING };

    std::chrono::steady_clock::time_point start_time;
    bool     game_over_sent = false;

    std::array<int, ROOM_CAPACITY> participants;

    std::array<SERVER_NPC, MAX_NPC_PER_ROOM>  npcs;

    std::array<PlayerSnapshot, ROOM_CAPACITY> player_snapshot;

    Room()
    {
        participants.fill(-1);
    }
};

extern std::array<Room, MAX_ROOMS> g_rooms;