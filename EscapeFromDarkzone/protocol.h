#pragma once

#include "ItemDef.h"

constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;
constexpr int INVENTORY_SIZE = 10;

constexpr int MAX_USER = 10000;
constexpr int MAX_NPC = 128;

constexpr int W_WIDTH = 400;
constexpr int W_HEIGHT = 400;

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;

constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_ADD_PLAYER = 3;
constexpr char SC_REMOVE_PLAYER = 4;
constexpr char SC_MOVE_PLAYER = 5;

// NPC 패킷 S2C
constexpr char SC_ADD_NPC = 6;
constexpr char SC_REMOVE_NPC = 7;
constexpr char SC_MOVE_NPC = 8;
constexpr char SC_NPC_STATE_CHANGE = 9;
constexpr char SC_NPC_HP_UPDATE = 10;

// NPC 패킷 C2S
constexpr char CS_HIT_NPC = 11;

// 인벤토리 패킷 C2S
constexpr char CS_INVENTORY_CLICK = 12;

// 인벤토리 패킷 S2C
constexpr char SC_INVENTORY_UPDATE = 13;
constexpr char SC_ADD_LOOT_BOX = 14;

// 루트박스 누르기 15, 루트박스 업데이트 16
constexpr char CS_LOOT_PICKUP = 15;
constexpr char SC_LOOT_BOX_SLOT_UPDATE = 16;
constexpr char SC_DEACTIVATE_LOOT_BOX = 17;

constexpr char SC_PLAYER_STATE_CHANGE = 18;

// CS_MOVE_PACKET inputs 비트 플래그
constexpr char MOVE_W = 0x01;
constexpr char MOVE_S = 0x02;
constexpr char MOVE_A = 0x04;
constexpr char MOVE_D = 0x08;

constexpr char INV_ACTION_CLICK = 0;

constexpr char PLAYER_STATE_IDLE = 0;
constexpr char PLAYER_STATE_RUN = 1;

constexpr char NPC_STATE_IDLE = 0;
constexpr char NPC_STATE_RUN = 1;
constexpr char NPC_STATE_DIE = 2;

#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned char size;
	char	type;
	char	name[NAME_SIZE];
};

struct CS_MOVE_PACKET {
	unsigned char size;
	char          type;
	char          inputs;       // WASD 비트 플래그 (MOVE_W, MOVE_S, MOVE_A, MOVE_D)
	float         yaw;          // 카메라 Yaw (도 단위) — 이동 방향 계산용
	unsigned int  move_time;    // 패킷 순서 보정용 
};

struct SC_LOGIN_INFO_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;        // short → float
};

struct SC_ADD_PLAYER_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;
	float         yaw;
	char          state;
	char          name[NAME_SIZE];
};

struct SC_REMOVE_PLAYER_PACKET {
	unsigned char size;
	char	type;
	short	id;
};

struct SC_MOVE_PLAYER_PACKET {
	unsigned char size;
	char          type;
	short         id;
	float         x, y, z;
	float         yaw;
	unsigned int  move_time;
};

struct CS_INVENTORY_CLICK_PACKET {
	unsigned char size;
	char          type;
	char          action;
	short         slotidx;
};

// NPC 패킷 추가

struct SC_ADD_NPC_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
	char          npc_kind;       // 나중에
	float         x, y, z;
	float         yaw;
	short         hp;
};

struct SC_REMOVE_NPC_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
};

struct SC_MOVE_NPC_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
	float         x, y, z;
	float         yaw;
};

struct SC_NPC_STATE_CHANGE_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
	char          state;          // NPC 상태 변환용
};

struct SC_NPC_HP_UPDATE_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
	short         hp;
};

struct CS_HIT_NPC_PACKET {
	unsigned char size;
	char          type;
	float         ray_ox, ray_oy, ray_oz;
	float         ray_dx, ray_dy, ray_dz;
	char          weapon_id;				// 나중에
	unsigned int  fire_time;				// 나중에
};

struct SC_INVENTORY_UPDATE_PACKET {
	unsigned char size;
	char          type;
	short         slotidx;
	ItemID        item_id;   // sizeof = 2 (enum class ItemID : short)
	int           count;
};

struct SC_ADD_LOOT_BOX_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;             // = box_id
	float         x, y, z;            // 사망 위치
	ItemID        items[INVENTORY_SIZE];
	int           counts[INVENTORY_SIZE];
};

struct CS_LOOT_PICKUP_PACKET {
	unsigned char size;
	char          type;
	short         box_id;     // = NPC id
	short         slotidx;
};

struct SC_LOOT_BOX_SLOT_UPDATE_PACKET {
	unsigned char size;
	char          type;
	short         box_id;
	short         slotidx;
	ItemID        item_id;
	int           count;
};

struct SC_DEACTIVATE_LOOT_BOX_PACKET {
	unsigned char size;
	char          type;
	short         npc_id;
};

struct SC_PLAYER_STATE_CHANGE_PACKET {
	unsigned char size;
	char          type;
	short         id;
	char          state;
};

#pragma pack (pop)