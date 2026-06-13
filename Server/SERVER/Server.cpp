#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <fstream>
#include <string>

#include <chrono>
#include <cmath>

#include "protocol.h"
#include "ItemDef.h"

#include "Server_Collision.h"
#include "Server_Npc.h"
#include "Server_AI.h"
#include "Server_Effect.h"
#include "Server_Weapon.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

std::vector<BoundingOrientedBox> g_mapOOBBs;

// 이동 / 거리
constexpr float NPC_MOVE_SPEED				= 5.0f;
constexpr float NPC_DETECTION_RANGE			= 20.0f;
constexpr float NPC_ATTACK_RANGE			= 8.0f;
constexpr float NPC_ATTACK_EXIT_RANGE		= 9.5f;
constexpr float NPC_LEASH_RANGE				= 25.0f;
constexpr float NPC_RETURN_STOP_DIST		= 0.5f;
constexpr float NPC_PREFERRED_COMBAT_RANGE	= 7.0f;
constexpr float NPC_TOO_CLOSE_RANGE			= 4.0f;
constexpr float NPC_COMBAT_MOVE_SPEED_MULT	= 0.45f;

// 사고 주기 / 타이머
constexpr float NPC_THINK_INTERVAL			= 0.2f;
constexpr float NPC_PATH_UPDATE_INTERVAL	= 0.3f;
constexpr float NPC_WAYPOINT_REACH_DIST		= 1.0f;
constexpr float NPC_RETURN_IGNORE_DURATION	= 1.0f;
constexpr float NPC_DIE_DURATION			= 3.4f;

// 사격 / 조준
constexpr float NPC_AIM_DELAY				= 0.35f;
constexpr float NPC_ATTACK_INTERVAL			= 1.0f;
constexpr int   NPC_BURST_SHOT_MIN			= 2;
constexpr int   NPC_BURST_SHOT_MAX			= 4;
constexpr float NPC_BURST_SHOT_INTERVAL		= 0.15f;
constexpr float NPC_BURST_REST_DURATION		= 1.0f;

// 스트레이프 / 재장전
constexpr float NPC_STRAFE_DURATION = 1.2f;
// 탄창 / 재장전시간은 무기 테이블로 

// 시야각
constexpr float NPC_VIEW_ANGLE_DEG			= 120.0f;
constexpr float NPC_LOSE_SIGHT_DURATION		= 1.0f;

// 제곱값 캐시
constexpr float NPC_DETECTION_RANGE_SQ		= NPC_DETECTION_RANGE * NPC_DETECTION_RANGE;
constexpr float NPC_ATTACK_RANGE_SQ			= NPC_ATTACK_RANGE * NPC_ATTACK_RANGE;
constexpr float NPC_ATTACK_EXIT_RANGE_SQ	= NPC_ATTACK_EXIT_RANGE * NPC_ATTACK_EXIT_RANGE;
constexpr float NPC_LEASH_RANGE_SQ			= NPC_LEASH_RANGE * NPC_LEASH_RANGE;
constexpr float NPC_RETURN_STOP_DIST_SQ		= NPC_RETURN_STOP_DIST * NPC_RETURN_STOP_DIST;
constexpr float NPC_TOO_CLOSE_RANGE_SQ		= NPC_TOO_CLOSE_RANGE * NPC_TOO_CLOSE_RANGE;
constexpr float NPC_WAYPOINT_REACH_DIST_SQ	= NPC_WAYPOINT_REACH_DIST * NPC_WAYPOINT_REACH_DIST;

constexpr float NPC_FIRE_SPREAD_RAD = 0.0f;    // 원뿔 탄퍼짐 반각(라디안). 지금 0 = 퍼짐 없음
constexpr float NPC_FIRE_ORIGIN_Y = 0.90f;   // 발사 높이 (고정)

constexpr int PLAYER_SPAWN_COUNT = 8;
struct PlayerSpawnPos { float x, z; };
constexpr PlayerSpawnPos PLAYER_SPAWN_POS[PLAYER_SPAWN_COUNT] = {
	{  42.0f,  45.0f },   // A
	{  46.0f,  14.0f },   // B
	{  44.0f, -34.0f },   // C
	{  44.0f, -90.0f },   // D
	{ -37.0f,  45.0f },   // E
	{  20.0f, -62.0f },   // F
	{  52.0f, -89.0f },   // G
	{ -82.0f, -43.0f },   // H
};

static float DistanceXZ(const XMFLOAT3& a, const XMFLOAT3& b)
{
	float dx = a.x - b.x;
	float dz = a.z - b.z;
	return std::sqrt(dx * dx + dz * dz);
}

static XMFLOAT3 NormalizeXZ(const XMFLOAT3& v)
{
	XMFLOAT3 r = v;
	r.y = 0.0f;

	float len = std::sqrt(r.x * r.x + r.z * r.z);
	if (len < 0.0001f) return XMFLOAT3(0.0f, 0.0f, 0.0f);

	r.x /= len;
	r.z /= len;
	return r;
}

static XMFLOAT3 GetRightFromForwardXZ(const XMFLOAT3& forward)
{
	XMFLOAT3 dir = NormalizeXZ(forward);

	float len_sq = dir.x * dir.x + dir.z * dir.z;
	if (len_sq < 0.0001f * 0.0001f)
		return XMFLOAT3(1.0f, 0.0f, 0.0f);

	return XMFLOAT3(dir.z, 0.0f, -dir.x);
}

static float NormalizeAngleDeg(float angle)
{
	while (angle > 180.0f) angle -= 360.0f;
	while (angle < -180.0f) angle += 360.0f;
	return angle;
}

static float GetDistanceToPlayerXZ(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	return DistanceXZ(npc.position, player_pos);
}

static float GetDistanceFromSpawnXZ(const SERVER_NPC& npc)
{
	return DistanceXZ(npc.position, npc.spawn_position);
}

static XMFLOAT3 GetDirectionToPlayerXZ(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	XMFLOAT3 d;
	d.x = player_pos.x - npc.position.x;
	d.y = 0.0f;
	d.z = player_pos.z - npc.position.z;
	return NormalizeXZ(d);
}

static XMFLOAT3 GetDirectionToSpawnXZ(const SERVER_NPC& npc)
{
	XMFLOAT3 d;
	d.x = npc.spawn_position.x - npc.position.x;
	d.y = 0.0f;
	d.z = npc.spawn_position.z - npc.position.z;
	return NormalizeXZ(d);
}

static bool IsPlayerInDetectRange(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	float dx = player_pos.x - npc.position.x;
	float dz = player_pos.z - npc.position.z;
	return (dx * dx + dz * dz) <= NPC_DETECTION_RANGE_SQ;
}

static bool IsPlayerInAttackRange(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	float dx = player_pos.x - npc.position.x;
	float dz = player_pos.z - npc.position.z;
	return (dx * dx + dz * dz) <= NPC_ATTACK_RANGE_SQ;
}

static bool IsPlayerOutOfAttackRange(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	float dx = player_pos.x - npc.position.x;
	float dz = player_pos.z - npc.position.z;
	return (dx * dx + dz * dz) >= NPC_ATTACK_EXIT_RANGE_SQ;
}

static bool IsOutsideLeashRange(const SERVER_NPC& npc)
{
	float dx = npc.position.x - npc.spawn_position.x;
	float dz = npc.position.z - npc.spawn_position.z;
	return (dx * dx + dz * dz) > NPC_LEASH_RANGE_SQ;
}

static bool IsNearSpawn(const SERVER_NPC& npc)
{
	float dx = npc.position.x - npc.spawn_position.x;
	float dz = npc.position.z - npc.spawn_position.z;
	return (dx * dx + dz * dz) <= NPC_RETURN_STOP_DIST_SQ;
}

static XMFLOAT3 GetForwardXZ(float yaw_rad)
{
	XMFLOAT3 fwd;
	fwd.x = std::sin(yaw_rad);
	fwd.y = 0.0f;
	fwd.z = std::cos(yaw_rad);
	return fwd;  // 단위벡터
}

static bool IsPlayerInViewAngle(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	XMFLOAT3 to_player = GetDirectionToPlayerXZ(npc, player_pos);

	// 너무 가까우면 항상 시야 내로 본다
	float len_sq = to_player.x * to_player.x + to_player.z * to_player.z;
	if (len_sq < 0.0001f * 0.0001f) return true;

	XMFLOAT3 forward = GetForwardXZ(npc.yaw);
	float dot = forward.x * to_player.x + forward.z * to_player.z;

	constexpr float PI = 3.14159265359f;
	float half_angle_rad = (NPC_VIEW_ANGLE_DEG * 0.5f) * (PI / 180.0f);
	float view_cos = std::cos(half_angle_rad);

	return dot >= view_cos;
}

static bool CanDetectPlayer(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	if (!IsPlayerInDetectRange(npc, player_pos)) return false;
	if (!IsPlayerInViewAngle(npc, player_pos))  return false;
	return true;
}

static bool CanShootPlayer(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	if (!IsPlayerInAttackRange(npc, player_pos)) return false;
	if (!IsPlayerInViewAngle(npc, player_pos))   return false;
	return true;
}

static void RefreshLastSeenPlayer(SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	npc.last_seen_player_pos = player_pos;
	npc.lose_sight_timer = 0.0f;
	npc.has_last_seen_player = true;
}

static bool HasRecentLastSeenPlayer(const SERVER_NPC& npc)
{
	return npc.has_last_seen_player
		&& (npc.lose_sight_timer < NPC_LOSE_SIGHT_DURATION);
}

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };
class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_buf, packet, packet[0]);
	}
};

enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class SESSION {
	OVER_EXP _recv_over;

public:
	std::mutex _s_lock;
	S_STATE _state;
	int _id;
	SOCKET _socket;
	float x, y, z;
	float yaw;
	char  player_state;
	char	_name[NAME_SIZE];
	int		_prev_remain;
	int		_last_move_time;

	std::array<ItemSlot, INVENTORY_SIZE> _inventory{};

	short hp = 100;
	short max_hp = 100;

	std::vector<XMFLOAT3> _collNormals; // 서버 측 충돌 계산 결과 저장용

	// 서버 측 deltaTime 계산용 - 마지막 CS_MOVE 수신 시각
	std::chrono::steady_clock::time_point _last_move_recv_time;

public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = z = 0.0f;
		y = 0.1f;
		yaw = 0.0f;
		player_state = PLAYER_STATE_IDLE;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
		hp = 100;
		max_hp = 100;
		_last_move_recv_time = std::chrono::steady_clock::now();
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.id = _id;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.x = x;
		p.y = y;
		p.z = z;
		do_send(&p);
	}
	void send_move_packet(int c_id);
	void send_add_player_packet(int c_id);
	void send_remove_player_packet(int c_id)
	{
		SC_REMOVE_PLAYER_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_PLAYER;
		do_send(&p);
	}
	void send_add_npc_packet(short npc_id, char kind, char outfit, float x, float y, float z, float yaw, short hp)
	{
		SC_ADD_NPC_PACKET p;
		p.size = sizeof(SC_ADD_NPC_PACKET);
		p.type = SC_ADD_NPC;
		p.npc_id = npc_id;
		p.npc_kind = kind;
		p.npc_outfit = outfit;
		p.x = x; p.y = y; p.z = z;
		p.yaw = yaw;
		p.hp = hp;
		do_send(&p);
	}
	void send_move_npc_packet(short npc_id, float x, float y, float z, float yaw)
	{
		SC_MOVE_NPC_PACKET p;
		p.size = sizeof(SC_MOVE_NPC_PACKET);
		p.type = SC_MOVE_NPC;
		p.npc_id = npc_id;
		p.x = x; p.y = y; p.z = z;
		p.yaw = yaw;
		do_send(&p);
	}
	void send_remove_npc_packet(short npc_id)
	{
		SC_REMOVE_NPC_PACKET p;
		p.size = sizeof(SC_REMOVE_NPC_PACKET);
		p.type = SC_REMOVE_NPC;
		p.npc_id = npc_id;
		do_send(&p);
	}
	void send_inventory_update_packet(short slotidx)
	{
		SC_INVENTORY_UPDATE_PACKET p;
		p.size = sizeof(SC_INVENTORY_UPDATE_PACKET);
		p.type = SC_INVENTORY_UPDATE;
		p.slotidx = slotidx;
		p.item_id = _inventory[slotidx].item;
		p.count = _inventory[slotidx].count;
		do_send(&p);
	}
	void send_equipment_update_packet(ItemID equip_id)
	{
		SC_EQUIPMENT_UPDATE_PACKET p;
		p.size = sizeof(SC_EQUIPMENT_UPDATE_PACKET);
		p.type = SC_EQUIPMENT_UPDATE;
		p.equip_id = equip_id;
		do_send(&p);
	}
	void send_player_state_change_packet(int c_id);
};

std::array<SESSION, MAX_USER> clients;

AstarNavigation g_astar;
SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;

void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.x = clients[c_id].x;
	p.y = clients[c_id].y;
	p.z = clients[c_id].z;
	p.yaw = clients[c_id].yaw;
	p.move_time = clients[c_id]._last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(int c_id)
{
	SC_ADD_PLAYER_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, clients[c_id]._name);
	add_packet.size = sizeof(SC_ADD_PLAYER_PACKET);
	add_packet.type = SC_ADD_PLAYER;
	add_packet.x = clients[c_id].x;
	add_packet.y = clients[c_id].y;
	add_packet.z = clients[c_id].z;
	add_packet.yaw = clients[c_id].yaw;
	add_packet.state = clients[c_id].player_state;
	do_send(&add_packet);
}

void SESSION::send_player_state_change_packet(int c_id) {
	SC_PLAYER_STATE_CHANGE_PACKET p;
	p.size = sizeof(SC_PLAYER_STATE_CHANGE_PACKET);
	p.type = SC_PLAYER_STATE_CHANGE;
	p.id = c_id;
	p.state = clients[c_id].player_state;
	do_send(&p);
}

bool load_mapOOBB_from_CSV(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Failed to open CSV file: " << filename << std::endl;
		return false;
	}

	std::string line;
	int loaded = 0;
	int line_no = 0;

	while (std::getline(file, line))
	{
		++line_no;

		if (line.empty()) continue;
		if (line[0] == '#') continue;

		BoundingOrientedBox obb;
		XMFLOAT4 quat;

		int parsed = sscanf_s(line.c_str(),
			"%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
			&obb.Center.x, &obb.Center.y, &obb.Center.z,
			&obb.Extents.x, &obb.Extents.y, &obb.Extents.z,
			&quat.x, &quat.y, &quat.z, &quat.w);

		if (parsed != 10)
		{
			std::cerr << "[LoadMapOOBB] parse error at line " << line_no
				<< ": " << line << std::endl;
			continue;
		}

		// 쿼터니언 정규화
		XMVECTOR q = XMLoadFloat4(&quat);
		q = XMQuaternionNormalize(q);
		XMStoreFloat4(&obb.Orientation, q);

		g_mapOOBBs.push_back(obb);
		++loaded;
	}

	std::cout << "[LoadMapOOBB] loaded " << loaded << " OOBBs from " << filename << std::endl;

	// 검증용: 처음 1개 OOBB 값 출력
	if (loaded > 0)
	{
		const auto& o = g_mapOOBBs[0];
		std::cout << "  First OOBB: center=("
			<< o.Center.x << ", " << o.Center.y << ", " << o.Center.z
			<< ") extents=("
			<< o.Extents.x << ", " << o.Extents.y << ", " << o.Extents.z
			<< ")" << std::endl;
	}

	return loaded > 0;
}

void ResolvePlayerCollision(int c_id, float yawRad)
{
	for (const auto& mapOOBB : g_mapOOBBs)
	{
		// 매 검사마다 보정된 최신 위치로 플레이어 OOBB 재생성
		XMFLOAT3 pos = { clients[c_id].x, clients[c_id].y, clients[c_id].z };
		BoundingOrientedBox playerOOBB = MakePlayerOOBB(pos, yawRad);

		ColResult res = CalcCollision(playerOOBB, mapOOBB);
		if (!res.isCollide) continue;

		// 충돌 노멀 누적
		clients[c_id]._collNormals.push_back(res.normal);

		// mtv로 위치 보정
		// 클라의 HandleCollision: vBackPos -= vNormal * 0.001f
		XMVECTOR vMtv = XMLoadFloat3(&res.mtv);
		XMVECTOR vNormal = XMLoadFloat3(&res.normal);
		vMtv -= vNormal * 0.001f;

		XMFLOAT3 finalBack;
		XMStoreFloat3(&finalBack, vMtv);

		clients[c_id].x += finalBack.x;
		clients[c_id].z += finalBack.z;
	}
}

void ApplySlide(int c_id, float& dirX, float& dirZ)
{
	auto& normals = clients[c_id]._collNormals;

	// 이동 방향이 0이면, 누적 노멀만 클리어하고 종료
	if (dirX == 0.0f && dirZ == 0.0f)
	{
		normals.clear();
		return;
	}

	// XZ 평면 계산
	XMVECTOR currentDirVec = XMVectorSet(dirX, 0.0f, dirZ, 0.0f);

	for (const XMFLOAT3& normal : normals)
	{
		XMVECTOR normalVec = XMLoadFloat3(&normal);

		XMVECTOR dotVec = XMVector3Dot(currentDirVec, normalVec);
		float dot = XMVectorGetX(dotVec);

		if (dot < 0.0f)
		{
			currentDirVec = currentDirVec - (normalVec * dot);
		}
	}

	// 길이가 거의 0이면 제로로 처리
	if (XMVectorGetX(XMVector3LengthSq(currentDirVec)) < 0.0001f)
	{
		currentDirVec = XMVectorZero();
	}

	// 결과 추출 
	XMFLOAT3 result;
	XMStoreFloat3(&result, currentDirVec);
	dirX = result.x;
	dirZ = result.z;

	// 이번 패킷에서 한 번 사용했으니 누적 노멀 클리어 
	normals.clear();
}

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		std::lock_guard<std::mutex> ll{ clients[i]._s_lock };
		if (clients[i]._state == ST_FREE) {
			clients[i]._state = ST_ALLOC;
			return i;
		}
	}
	return -1;
}

struct PlayerSnapshot {
	bool  in_game;
	float x, y, z;
	float yaw;
	short hp;
};

static void SnapshotPlayers(std::array<PlayerSnapshot, MAX_USER>& out)
{
	for (int i = 0; i < MAX_USER; ++i) {
		std::lock_guard<std::mutex> lk(clients[i]._s_lock);
		if (clients[i]._state == ST_INGAME) {
			out[i].in_game = true;
			out[i].x = clients[i].x;
			out[i].y = clients[i].y;
			out[i].z = clients[i].z;
			out[i].yaw = clients[i].yaw;
			out[i].hp = clients[i].hp;
		}
		else {
			out[i].in_game = false;
		}
	}
}

static int FindNearestPlayer(const XMFLOAT3& pos, const std::array<PlayerSnapshot, MAX_USER>& snapshot, float& out_dist_sq)
{
	int   best_id = -1;
	float best_sq = 0.0f;

	for (int i = 0; i < MAX_USER; ++i) {
		if (!snapshot[i].in_game) continue;

		float dx = snapshot[i].x - pos.x;
		float dy = snapshot[i].y - pos.y;
		float dz = snapshot[i].z - pos.z;
		float d_sq = dx * dx + dy * dy + dz * dz;

		if (best_id == -1 || d_sq < best_sq) {
			best_id = i;
			best_sq = d_sq;
		}
	}

	if (best_id >= 0) {
		out_dist_sq = best_sq;
	}
	return best_id;
}

static void StartNpcBurst(SERVER_NPC& npc)
{
	int range = NPC_BURST_SHOT_MAX - NPC_BURST_SHOT_MIN + 1;
	if (range <= 0) {
		npc.burst_shots_left = 3;
	}
	else {
		npc.burst_serial++;
		npc.burst_shots_left = NPC_BURST_SHOT_MIN + (npc.burst_serial % range);
	}
	npc.burst_shot_timer = 0.0f;
}

static WeaponSpec GetNpcWeaponSpec(const SERVER_NPC& npc)
{
	return LookupWeaponSpec(
		static_cast<WeaponType>(npc.weapon_type),
		static_cast<WeaponGrade>(npc.weapon_grade));
}

static void StartNpcReload(SERVER_NPC& npc)
{
	if (npc.reloading) return;
	npc.reloading = true;
	npc.reload_timer = GetNpcWeaponSpec(npc).reloadTime;
	npc.burst_shots_left = 0;
	npc.burst_shot_timer = 0.0f;
	npc.burst_rest_timer = 0.0f;
	std::cout << "[NPC " << npc.id << "] Reload Start\n";
}

static bool UpdateNpcReload(SERVER_NPC& npc, float dt)
{
	if (!npc.reloading) return false;
	npc.reload_timer -= dt;
	if (npc.reload_timer <= 0.0f) {
		npc.reloading = false;
		npc.reload_timer = 0.0f;
		npc.current_ammo = GetNpcWeaponSpec(npc).magazineSize;
		std::cout << "[NPC " << npc.id << "] Reload Finish\n";
		return true;
	}
	return false;
}

static void BroadcastAttachedEffectNoSnapshot(
	EffectID id, EffectEntityKind kind, short entity_id)
{
	SC_PLAY_EFFECT_ATTACHED_PACKET ep;
	ep.size = sizeof(ep);
	ep.type = SC_PLAY_EFFECT_ATTACHED;
	ep.effect_id = static_cast<unsigned char>(id);
	ep.entity_kind = static_cast<unsigned char>(kind);
	ep.entity_id = entity_id;

	for (int i = 0; i < MAX_USER; ++i) {
		{
			std::lock_guard<std::mutex> lk(clients[i]._s_lock);
			if (clients[i]._state != ST_INGAME) continue;
		}
		clients[i].do_send(&ep);
	}
}

static void BroadcastAttachedEffect(
	EffectID id, EffectEntityKind kind, short entity_id,
	const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	SC_PLAY_EFFECT_ATTACHED_PACKET ep;
	ep.size = sizeof(ep);
	ep.type = SC_PLAY_EFFECT_ATTACHED;
	ep.effect_id = static_cast<unsigned char>(id);
	ep.entity_kind = static_cast<unsigned char>(kind);
	ep.entity_id = entity_id;

	for (int i = 0; i < MAX_USER; ++i) {
		if (!player_snapshot[i].in_game) continue;
		clients[i].do_send(&ep);
	}
}

static void BroadcastWorldEffect(
	EffectID id, const XMFLOAT3& pos, const XMFLOAT3& dir,
	const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	SC_PLAY_EFFECT_WORLD_PACKET ep;
	ep.size = sizeof(ep);
	ep.type = SC_PLAY_EFFECT_WORLD;
	ep.effect_id = static_cast<unsigned char>(id);
	ep.x = pos.x; ep.y = pos.y; ep.z = pos.z;
	ep.dx = dir.x; ep.dy = dir.y; ep.dz = dir.z;

	for (int i = 0; i < MAX_USER; ++i) {
		if (!player_snapshot[i].in_game) continue;
		clients[i].do_send(&ep);
	}
}

static void ChangeNpcState(SERVER_NPC&, char, const std::array<PlayerSnapshot, MAX_USER>&);

static void NpcFireAtPlayer(SERVER_NPC& npc, int target_id, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	if (npc.reloading) return;
	if (NPC_STATE_DIE == npc.state) return;
	if (npc.current_ammo <= 0) {
		StartNpcReload(npc);
		ChangeNpcState(npc, NPC_STATE_RELOAD, player_snapshot);
		return;
	}

	npc.current_ammo--;

	// NPC 무기 spec
	WeaponSpec spec = LookupWeaponSpec(
		static_cast<WeaponType>(npc.weapon_type),
		static_cast<WeaponGrade>(npc.weapon_grade));

	XMFLOAT3 forward = GetForwardXZ(npc.yaw);

	if (NPC_FIRE_SPREAD_RAD > 0.0f) {		// 나중에 원뿔 탄퍼짐 적용시킬 때 if문 삭제할 것
		float r = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * NPC_FIRE_SPREAD_RAD;		// rand 말고 다른 함수?
		float c = std::cos(r), s = std::sin(r);
		XMFLOAT3 f = forward;
		forward.x = f.x * c - f.z * s;
		forward.z = f.x * s + f.z * c;
	}

	XMFLOAT3 origin = { npc.position.x, npc.position.y + NPC_FIRE_ORIGIN_Y, npc.position.z };
	XMFLOAT3 dir = { forward.x, 0.0f, forward.z };

	std::cout << "[NPC " << npc.id << "] Fire (ammo=" << npc.current_ammo << ")\n";

	bool hit = false;
	if (target_id >= 0 && player_snapshot[target_id].in_game) {
		XMVECTOR vO = XMVectorSet(origin.x, origin.y, origin.z, 0.0f);
		XMVECTOR vD = XMVector3Normalize(XMVectorSet(dir.x, dir.y, dir.z, 0.0f));

		XMFLOAT3 tpos = { player_snapshot[target_id].x,
						  player_snapshot[target_id].y,
						  player_snapshot[target_id].z };
		BoundingOrientedBox pbox = MakePlayerOOBB(tpos, player_snapshot[target_id].yaw);

		float t{}; // Intersects out 파라미터
		float hit_dist = 0.0f;
		if (pbox.Intersects(vO, vD, t) && t >= 0.0f && t <= spec.range) {
			hit = true;
			hit_dist = t;
		}

		if (hit) {
			short dmg = ComputeDamage(spec, hit_dist);   // 거리 감쇠 포함
			short new_hp = 0;
			{
				std::lock_guard<std::mutex> lk(clients[target_id]._s_lock);
				if (clients[target_id]._state == ST_INGAME) {
					clients[target_id].hp -= dmg;
					if (clients[target_id].hp < 0) clients[target_id].hp = 0;
					new_hp = clients[target_id].hp;
				}
			}

			std::cout << "[NPC " << npc.id << "] HIT player " << target_id
				<< " (hp=" << new_hp << ")\n";
			if (new_hp <= 0) {
				std::cout << "[NPC " << npc.id << "] player " << target_id << " HP 0 (death deferred)\n";
			}

			SC_PLAYER_HP_UPDATE_PACKET hp_pkt;
			hp_pkt.size = sizeof(hp_pkt);
			hp_pkt.type = SC_PLAYER_HP_UPDATE;
			hp_pkt.id = (short)target_id;
			hp_pkt.hp = new_hp;
			clients[target_id].do_send(&hp_pkt);
		}
	}

	// 발사 이펙트: 머즐 플래시
	BroadcastAttachedEffect(EffectID::SPARK, EffectEntityKind::NPC, npc.id, player_snapshot);

	if (npc.current_ammo <= 0) {
		StartNpcReload(npc);
		ChangeNpcState(npc, NPC_STATE_RELOAD, player_snapshot);
	}
}

static XMFLOAT3 ComputeNpcCombatMoveDir(const SERVER_NPC& npc, const XMFLOAT3& player_pos)
{
	XMFLOAT3 to_player = GetDirectionToPlayerXZ(npc, player_pos);
	if (to_player.x == 0.0f && to_player.z == 0.0f)
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	float dx = player_pos.x - npc.position.x;
	float dz = player_pos.z - npc.position.z;
	float dist = std::sqrt(dx * dx + dz * dz);

	XMFLOAT3 move_dir;
	if (dist < NPC_TOO_CLOSE_RANGE) {
		// 너무 가까움 — 플레이어 반대로 후퇴
		move_dir.x = -to_player.x * NPC_COMBAT_MOVE_SPEED_MULT;
		move_dir.y = 0.0f;
		move_dir.z = -to_player.z * NPC_COMBAT_MOVE_SPEED_MULT;
	}
	else {
		// 좌우 스트레이프
		XMFLOAT3 right = GetRightFromForwardXZ(to_player);
		move_dir.x = right.x * npc.strafe_sign * NPC_COMBAT_MOVE_SPEED_MULT;
		move_dir.y = 0.0f;
		move_dir.z = right.z * npc.strafe_sign * NPC_COMBAT_MOVE_SPEED_MULT;
	}
	return move_dir;
}

static void ChangeNpcState(SERVER_NPC& npc, char new_state, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	if (npc.state == new_state) return;

	npc.state = new_state;

	npc.think_timer = 0.0f;

	if (new_state == NPC_STATE_DIE) {
		npc.die_timer = 0.0f;

		// 죽은 NPC 루트박스 활성화
		npc.loot_active = true;
		npc.death_time = std::chrono::steady_clock::now();
	}

	// 디버그용 로그
	std::cout << "[NPC " << npc.id << "] -> state " << (int)new_state << "\n";


	// 모든 ST_INGAME 클라에 즉시 송신
	SC_NPC_STATE_CHANGE_PACKET p;
	p.size = sizeof(SC_NPC_STATE_CHANGE_PACKET);
	p.type = SC_NPC_STATE_CHANGE;
	p.npc_id = npc.id;
	p.state = new_state;

	for (int i = 0; i < MAX_USER; ++i) {
		if (!player_snapshot[i].in_game) continue;
		clients[i].do_send(&p);
	}

	// 박스 정보 송신
	if (new_state == NPC_STATE_DIE) {
		SC_ADD_LOOT_BOX_PACKET box;
		box.size = sizeof(SC_ADD_LOOT_BOX_PACKET);
		box.type = SC_ADD_LOOT_BOX;
		box.npc_id = npc.id;
		box.x = npc.position.x;
		box.y = npc.position.y;
		box.z = npc.position.z;

		std::cout << "BOX CREATED [" << box.x << ", " << box.y << ", " << box.z << "], [" << box.npc_id << "]\n";

		for (int i = 0; i < INVENTORY_SIZE; ++i) {
			box.items[i] = npc._inventory[i].item;
			box.counts[i] = npc._inventory[i].count;
		}
		for (int i = 0; i < MAX_USER; ++i) {
			if (!player_snapshot[i].in_game) continue;
			clients[i].do_send(&box);
		}
	}
}

static void ResolveNpcCollision(SERVER_NPC& npc, float yawRad)
{
	for (const auto& mapOOBB : g_mapOOBBs)
	{
		// 매 검사마다 보정된 최신 위치로 NPC OOBB 재생성
		BoundingOrientedBox npcOOBB = MakeNpcOOBB(npc.position, yawRad);

		ColResult res = CalcCollision(npcOOBB, mapOOBB);
		if (!res.isCollide) continue;

		// 충돌 노멀 누적 (다음 틱 ApplyNpcSlide에서 사용)
		npc.coll_normals.push_back(res.normal);

		// mtv로 위치 보정 — 플레이어 패턴과 동일
		// (vBackPos -= vNormal * 0.001f 로 벽에서 미세하게 더 떨어뜨림)
		XMVECTOR vMtv = XMLoadFloat3(&res.mtv);
		XMVECTOR vNormal = XMLoadFloat3(&res.normal);
		vMtv -= vNormal * 0.001f;

		XMFLOAT3 finalBack;
		XMStoreFloat3(&finalBack, vMtv);

		npc.position.x += finalBack.x;
		npc.position.z += finalBack.z;
		// Y는 건드리지 않음 (XZ 평면 충돌)
	}
}

static void ApplyNpcSlide(SERVER_NPC& npc, XMFLOAT3& move_dir)
{
	auto& normals = npc.coll_normals;

	// 이동 방향이 0이면 누적 노멀만 클리어하고 종료
	if (move_dir.x == 0.0f && move_dir.z == 0.0f)
	{
		normals.clear();
		return;
	}

	// XZ 평면 계산
	XMVECTOR currentDirVec = XMVectorSet(move_dir.x, 0.0f, move_dir.z, 0.0f);

	for (const XMFLOAT3& normal : normals)
	{
		XMVECTOR normalVec = XMLoadFloat3(&normal);

		XMVECTOR dotVec = XMVector3Dot(currentDirVec, normalVec);
		float dot = XMVectorGetX(dotVec);

		// 벽을 향해 들어가는 성분만 제거 (dot < 0이면 침투 방향)
		if (dot < 0.0f)
		{
			currentDirVec = currentDirVec - (normalVec * dot);
		}
	}

	// 길이가 거의 0이면 제로로 처리 (반대 방향 벽 두 개 사이에 끼인 경우)
	if (XMVectorGetX(XMVector3LengthSq(currentDirVec)) < 0.0001f)
	{
		currentDirVec = XMVectorZero();
	}

	// 결과 추출
	XMFLOAT3 result;
	XMStoreFloat3(&result, currentDirVec);
	move_dir.x = result.x;
	move_dir.z = result.z;
	// Y는 건드리지 않음

	// 이번 틱에서 한 번 사용했으니 누적 노멀 클리어
	normals.clear();
}

static void EnterNpcAttack(SERVER_NPC&);

static void ApplyDamage(SERVER_NPC& npc, short damage, int attacker_client_id, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	if (NPC_STATE_DIE == npc.state) return;

	npc.hp -= damage;

	{
		SC_NPC_HP_UPDATE_PACKET p;
		p.size = sizeof(SC_NPC_HP_UPDATE_PACKET);
		p.type = SC_NPC_HP_UPDATE;
		p.npc_id = npc.id;
		p.hp = npc.hp;

		for (int i = 0; i < MAX_USER; ++i) {
			if (!player_snapshot[i].in_game) continue;
			clients[i].do_send(&p);
		}
	}

	if (npc.hp <= 0) {
		ChangeNpcState(npc, NPC_STATE_DIE, player_snapshot);
	}

	if (NPC_STATE_IDLE == npc.state) {
		if (attacker_client_id < 0 || attacker_client_id >= MAX_USER) return;
		if (!player_snapshot[attacker_client_id].in_game) return;

		XMFLOAT3 attacker_pos = {
			player_snapshot[attacker_client_id].x,
			player_snapshot[attacker_client_id].y,
			player_snapshot[attacker_client_id].z
		};

		RefreshLastSeenPlayer(npc, attacker_pos);

		if (CanShootPlayer(npc, attacker_pos)) {
			EnterNpcAttack(npc);
			ChangeNpcState(npc, NPC_STATE_ATTACK, player_snapshot);
		}
		else {
			ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
		}
	}
}

static void HandleNpcEvent(const NpcInputEvent& e, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	switch (e.type) {
	case NpcInputEvent::HIT:
	{
		XMVECTOR origin = XMVectorSet(e.ray_origin.x, e.ray_origin.y, e.ray_origin.z, 0.0f);
		XMVECTOR dir = XMVector3Normalize(XMVectorSet(e.ray_direction.x, e.ray_direction.y, e.ray_direction.z, 0.0f));

		WeaponSpec spec = LookupWeaponSpec((WeaponType)e.weapon_type, (WeaponGrade)e.weapon_grade);

		float max_range = spec.range;
		if (max_range <= 0.0f) return;

		short best_id = -1;
		float best_t = max_range;
		for (auto& npc : g_npcs) {
			if (false == npc.alive || NPC_STATE_DIE == npc.state) continue;
			BoundingOrientedBox oobb = MakeNpcOOBB(npc.position, npc.yaw);
			float t;
			if (oobb.Intersects(origin, dir, t) && t >= 0.0f && t < best_t) {
				best_t = t;
				best_id = npc.id;
			}
		}

		if (best_id >= 0) {
			short dmg = ComputeDamage(spec, best_t);
			if (dmg > 0) {
				ApplyDamage(g_npcs[best_id], dmg, e.attacker_client_id, player_snapshot);
			}
		}
	}

	break;
	case NpcInputEvent::NEW_CLIENT_JOINED:
	{
		std::cout << "[NPC] NEW_CLIENT_JOINED received, client "
			<< e.new_client_id << "\n";

		// 새 클라가 정말 ST_INGAME인지 확인 (미세한 disconnect 타이밍 가드)
		{
			std::lock_guard<std::mutex> lk(clients[e.new_client_id]._s_lock);
			if (clients[e.new_client_id]._state != ST_INGAME) {
				break;
			}
		}

		// 살아있는 모든 NPC를 그 클라에 SC_ADD_NPC 송신
		for (const auto& npc : g_npcs) {
			if (!npc.alive) continue;
			clients[e.new_client_id].send_add_npc_packet(
				npc.id, npc.kind, npc.outfit,
				npc.position.x, npc.position.y, npc.position.z,
				npc.yaw, npc.hp
			);
		}

		break;
	}
	}
}

static void EnterNpcAttack(SERVER_NPC& npc)
{
	npc.aim_timer = 0.0f;
	npc.attack_cooldown = 0.0f;
	npc.burst_shots_left = 0;
	npc.burst_shot_timer = 0.0f;
	npc.burst_rest_timer = 0.0f;
	npc.strafe_timer = NPC_STRAFE_DURATION;
	npc.strafe_sign *= -1.0f;
}

static void UpdateNpcIdle(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	if (npc.return_ignore_timer > 0.0f) {
		npc.return_ignore_timer -= dt;
		return;
	}

	// 사고 주기 — 0.2초마다만 판단
	npc.think_timer += dt;
	if (npc.think_timer < NPC_THINK_INTERVAL) return;
	npc.think_timer = 0.0f;

	float dist_sq;
	int player_id = FindNearestPlayer(npc.position, player_snapshot, dist_sq);
	if (player_id < 0) return;  // 게임 중인 플레이어 없음

	XMFLOAT3 player_pos = {
		player_snapshot[player_id].x,
		player_snapshot[player_id].y,
		player_snapshot[player_id].z
	};

	if (!CanDetectPlayer(npc, player_pos)) return;

	RefreshLastSeenPlayer(npc, player_pos);

	if (CanShootPlayer(npc, player_pos)) {
		EnterNpcAttack(npc);
		ChangeNpcState(npc, NPC_STATE_ATTACK, player_snapshot);
		return;
	}

	ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
}

static void UpdateNpcRun(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	// 1. 가장 가까운 플레이어 검색
	float dist_sq;
	int player_id = FindNearestPlayer(npc.position, player_snapshot, dist_sq);
	if (player_id < 0) {
		// 클라이언트 없으면 Idle 복귀
		ChangeNpcState(npc, NPC_STATE_IDLE, player_snapshot);
		return;
	}

	// 2. 거리 체크 (XZ 평면, Y 무시) — 사거리 밖 또는 공격 거리 안이면 Idle 전환
	XMFLOAT3 player_pos = {
		player_snapshot[player_id].x,
		player_snapshot[player_id].y,
		player_snapshot[player_id].z
	};

	if (IsOutsideLeashRange(npc)) {
		ChangeNpcState(npc, NPC_STATE_RETURN, player_snapshot);
		return;
	}

	bool can_detect = CanDetectPlayer(npc, player_pos);
	if (can_detect) {
		RefreshLastSeenPlayer(npc, player_pos);
	}
	else {
		npc.lose_sight_timer += dt;
	}

	npc.think_timer += dt;
	if (npc.think_timer >= NPC_THINK_INTERVAL) {
		npc.think_timer = 0.0f;

		// (Phase D: CanShootPlayer → ATTACK. 지금은 사격 거리 안이면 IDLE)
		if (CanShootPlayer(npc, player_pos)) {
			EnterNpcAttack(npc);
			ChangeNpcState(npc, NPC_STATE_ATTACK, player_snapshot);
			return;
		}

		if (!can_detect && !HasRecentLastSeenPlayer(npc)) {
			ChangeNpcState(npc, NPC_STATE_RETURN, player_snapshot);
			return;
		}
	}

	XMFLOAT3 target_pos;
	if (can_detect) {
		target_pos = player_pos;
	}
	else if (HasRecentLastSeenPlayer(npc)) {
		target_pos = npc.last_seen_player_pos;
	}
	else {
		// 목표 없음 — 정지 (충돌만 처리)
		XMFLOAT3 zero = { 0.0f, 0.0f, 0.0f };
		ApplyNpcSlide(npc, zero);
		ResolveNpcCollision(npc, npc.yaw);
		return;
	}

	// 3. 1초 주기 A* 재탐색
	npc.path_update_timer += dt;
	if (npc.path_update_timer >= NPC_PATH_UPDATE_INTERVAL) {
		npc.path_update_timer -= NPC_PATH_UPDATE_INTERVAL;
		npc.waypoints = g_astar.FindPath(npc.position, target_pos);
		npc.way_idx = 0;
	}

	// 디버그용 로그
	//std::cout << "[NPC " << npc.id << "] pos=("
	//	<< npc.position.x << "," << npc.position.z
	//	<< ") -> target=(" << player_pos.x << "," << player_pos.z
	//	<< ") path size=" << npc.waypoints.size() << "\n";

	// 4. waypoint 따라가기
	XMFLOAT3 look = { 0.0f, 0.0f, 1.0f };  // 기본 정면
	XMFLOAT3 move_dir = { 0.0f, 0.0f, 0.0f };
	bool is_moving = false;

	while (npc.way_idx < (int)npc.waypoints.size()) {
		const XMFLOAT3& wp = npc.waypoints[npc.way_idx];
		float wdx = wp.x - npc.position.x;
		float wdz = wp.z - npc.position.z;
		float wd_sq = wdx * wdx + wdz * wdz;

		if (wd_sq < NPC_WAYPOINT_REACH_DIST_SQ) {
			// 현재 waypoint 도달 --> 다음
			npc.way_idx++;
		}
		else {
			// 정규화하여 이동 방향 결정
			float wd = std::sqrt(wd_sq);
			look.x = wdx / wd;
			look.y = 0.0f;
			look.z = wdz / wd;
			move_dir = look;
			is_moving = true;
			break;
		}
	}

	// 5. 경로 없거나 모든 waypoint 도달 --> 정지하되 플레이어 방향으로는 향함
	if (!is_moving) {
		// move_dir 유지: (0,0,0) → 위치 적분에서 안 움직임
		float tdx = target_pos.x - npc.position.x;
		float tdz = target_pos.z - npc.position.z;
		float t_sq = tdx * tdx + tdz * tdz;
		if (t_sq > 0.01f) {  // 클라는 0.1f, 거리 비교라 제곱은 0.01f
			float d = std::sqrt(t_sq);
			look.x = tdx / d;
			look.y = 0.0f;
			look.z = tdz / d;
		}
	}

	// 6. yaw 갱신 (라디안 보관)
	//    서버는 yaw 자체만 보관하면 충분. 송신 시 그대로 전송. (추후 yaw 전송 시 변경)
	npc.yaw = std::atan2(look.x, look.z);

	// 7. 직전 틱 누적 노멀로 벽 슬라이드 보정
	ApplyNpcSlide(npc, move_dir);

	// 8. 위치 적분 (XZ 평면만)
	npc.position.x += move_dir.x * NPC_MOVE_SPEED * dt;
	npc.position.z += move_dir.z * NPC_MOVE_SPEED * dt;

	// 9. 충돌 검사 + 보정 + 이번 틱 노멀 누적
	ResolveNpcCollision(npc, npc.yaw);
}

static void UpdateNpcReturn(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	// 복귀 중에도 플레이어가 시야+사거리 안으로 다시 들어오면 즉시 재교전
	npc.think_timer += dt;
	if (npc.think_timer >= NPC_THINK_INTERVAL) {
		npc.think_timer = 0.0f;

		float dist_sq;
		int player_id = FindNearestPlayer(npc.position, player_snapshot, dist_sq);
		if (player_id >= 0) {
			XMFLOAT3 player_pos = {
				player_snapshot[player_id].x,
				player_snapshot[player_id].y,
				player_snapshot[player_id].z
			};
			if (CanDetectPlayer(npc, player_pos)) {
				RefreshLastSeenPlayer(npc, player_pos);
				ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
				return;
			}
		}
	}

	// 스폰 위치 도착 -> IDLE (잠깐 감지 무시 타이머 세팅)
	if (IsNearSpawn(npc)) {
		npc.hp = npc.max_hp;

		npc.has_last_seen_player = false;
		npc.lose_sight_timer = 0.0f;
		npc.return_ignore_timer = NPC_RETURN_IGNORE_DURATION;
		ChangeNpcState(npc, NPC_STATE_IDLE, player_snapshot);
		return;
	}

	// 스폰 위치로 A* 경로 추적 (RUN과 동일 구조, target = spawn_position)
	npc.path_update_timer += dt;
	if (npc.path_update_timer >= NPC_PATH_UPDATE_INTERVAL) {
		npc.path_update_timer -= NPC_PATH_UPDATE_INTERVAL;
		npc.waypoints = g_astar.FindPath(npc.position, npc.spawn_position);
		npc.way_idx = 0;
	}

	XMFLOAT3 look = { 0.0f, 0.0f, 1.0f };
	XMFLOAT3 move_dir = { 0.0f, 0.0f, 0.0f };
	bool is_moving = false;

	while (npc.way_idx < (int)npc.waypoints.size()) {
		const XMFLOAT3& wp = npc.waypoints[npc.way_idx];
		float wdx = wp.x - npc.position.x;
		float wdz = wp.z - npc.position.z;
		float wd_sq = wdx * wdx + wdz * wdz;

		if (wd_sq < NPC_WAYPOINT_REACH_DIST_SQ) {
			npc.way_idx++;
		}
		else {
			float wd = std::sqrt(wd_sq);
			look.x = wdx / wd;
			look.y = 0.0f;
			look.z = wdz / wd;
			move_dir = look;
			is_moving = true;
			break;
		}
	}

	if (!is_moving) {
		// 경로가 비었으면 스폰 방향 직선
		XMFLOAT3 d = GetDirectionToSpawnXZ(npc);
		if (d.x != 0.0f || d.z != 0.0f) {
			look = d;
			move_dir = d;
		}
	}

	npc.yaw = std::atan2(look.x, look.z);
	ApplyNpcSlide(npc, move_dir);
	npc.position.x += move_dir.x * NPC_MOVE_SPEED * dt;
	npc.position.z += move_dir.z * NPC_MOVE_SPEED * dt;
	ResolveNpcCollision(npc, npc.yaw);
}

static void UpdateNpcAttack(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	// 1. Leash 밖이면 Return
	if (IsOutsideLeashRange(npc)) {
		ChangeNpcState(npc, NPC_STATE_RETURN, player_snapshot);
		return;
	}

	float dist_sq;
	int player_id = FindNearestPlayer(npc.position, player_snapshot, dist_sq);
	if (player_id < 0) {
		ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
		return;
	}
	XMFLOAT3 player_pos = {
		player_snapshot[player_id].x,
		player_snapshot[player_id].y,
		player_snapshot[player_id].z
	};

	bool can_detect = CanDetectPlayer(npc, player_pos);
	bool can_shoot = CanShootPlayer(npc, player_pos);

	// 2. 재장전
	if (npc.reloading) {
		bool justFinished = UpdateNpcReload(npc, dt);
		if (can_detect) {
			RefreshLastSeenPlayer(npc, player_pos);
			npc.yaw = std::atan2(player_pos.x - npc.position.x, player_pos.z - npc.position.z);
		}
		if (justFinished) {
			// 재장전 종료 -> ATTACK 복귀 통지
			ChangeNpcState(npc, NPC_STATE_ATTACK, player_snapshot);
		}
		return;  // 이동 없음
	}

	// 3. 시야 판정 + 조준 (yaw)
	if (can_detect) {
		RefreshLastSeenPlayer(npc, player_pos);
		npc.yaw = std::atan2(player_pos.x - npc.position.x, player_pos.z - npc.position.z);
	}
	else {
		npc.lose_sight_timer += dt;
		if (HasRecentLastSeenPlayer(npc)) {
			const XMFLOAT3& ls = npc.last_seen_player_pos;
			npc.yaw = std::atan2(ls.x - npc.position.x, ls.z - npc.position.z);
		}
	}

	// 4. think 주기 - 상태 전환 판단
	npc.think_timer += dt;
	if (npc.think_timer >= NPC_THINK_INTERVAL) {
		npc.think_timer = 0.0f;

		if (IsPlayerOutOfAttackRange(npc, player_pos)) {
			ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
			return;
		}
		if (!can_detect && !HasRecentLastSeenPlayer(npc)) {
			ChangeNpcState(npc, NPC_STATE_RUN, player_snapshot);
			return;
		}
	}

	// 5. 탄약 0이면 재장전
	if (npc.current_ammo <= 0) {
		StartNpcReload(npc);
		ChangeNpcState(npc, NPC_STATE_RELOAD, player_snapshot);   // 클라에 재장전 시작 통지
		return;
	}

	// 6. 조준 딜레이 (0.35s) - 정지 대기
	if (npc.aim_timer < NPC_AIM_DELAY) {
		npc.aim_timer += dt;
		return;
	}

	// 7. 사격 불가 - 스트레이프 이동
	if (!can_shoot) {
		npc.strafe_timer -= dt;
		if (npc.strafe_timer <= 0.0f) {
			npc.strafe_timer = NPC_STRAFE_DURATION;
			npc.strafe_sign *= -1.0f;
		}
		if (can_detect) {
			XMFLOAT3 move_dir = ComputeNpcCombatMoveDir(npc, player_pos);
			ApplyNpcSlide(npc, move_dir);
			npc.position.x += move_dir.x * NPC_MOVE_SPEED * dt;
			npc.position.z += move_dir.z * NPC_MOVE_SPEED * dt;
			ResolveNpcCollision(npc, npc.yaw);
		}
		return;
	}

	// 8. 버스트 휴지 중 - 스트레이프하며 대기
	if (npc.burst_rest_timer > 0.0f) {
		npc.burst_rest_timer -= dt;
		npc.strafe_timer -= dt;
		if (npc.strafe_timer <= 0.0f) {
			npc.strafe_timer = NPC_STRAFE_DURATION;
			npc.strafe_sign *= -1.0f;
		}
		XMFLOAT3 move_dir = ComputeNpcCombatMoveDir(npc, player_pos);
		ApplyNpcSlide(npc, move_dir);
		npc.position.x += move_dir.x * NPC_MOVE_SPEED * dt;
		npc.position.z += move_dir.z * NPC_MOVE_SPEED * dt;
		ResolveNpcCollision(npc, npc.yaw);

		if (npc.burst_rest_timer <= 0.0f) {
			npc.aim_timer = 0.0f;
		}
		return;
	}

	// 9. 버스트 진행 - 정지하고 발사
	if (npc.burst_shots_left <= 0) {
		StartNpcBurst(npc);
	}

	npc.burst_shot_timer -= dt;
	if (npc.burst_shot_timer <= 0.0f) {
		NpcFireAtPlayer(npc, player_id, player_snapshot);   // Phase D: 탄 차감 + 로그
		npc.burst_shots_left--;
		npc.burst_shot_timer = NPC_BURST_SHOT_INTERVAL;

		if (npc.burst_shots_left <= 0) {
			npc.burst_rest_timer = NPC_BURST_REST_DURATION;
			npc.strafe_timer = NPC_STRAFE_DURATION;
			npc.strafe_sign *= -1.0f;
		}
	}
}

static void UpdateNpcDie(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	npc.die_timer += dt;
	if (npc.die_timer < NPC_DIE_DURATION) return;

	// 1.2초 경과 — NPC 제거

	// 1. 모든 ST_INGAME 클라에 SC_REMOVE_NPC 송신
	//    alive=false 처리 전에 보내야 함 (NEW_CLIENT_JOINED와의 race 고려).
	SC_REMOVE_NPC_PACKET p;
	p.size = sizeof(SC_REMOVE_NPC_PACKET);
	p.type = SC_REMOVE_NPC;
	p.npc_id = npc.id;

	for (int i = 0; i < MAX_USER; ++i) {
		if (!player_snapshot[i].in_game) continue;
		clients[i].do_send(&p);
	}

	// 2. NPC 슬롯 해제
	npc.alive = false;

	// 다음 틱부터 UpdateNpc 분기에서 !npc.alive로 걸러져 더 이상 호출되지 않음.
	// npc.state는 그대로 DIE로 유지하지만 의미 없음.
	// 향후 NPC 리스폰이 도입되면 init 함수에서 다시 채워야 함.
}

static void UpdateNpc(SERVER_NPC& npc, float dt, const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	switch (npc.state) {
	case NPC_STATE_IDLE:
		UpdateNpcIdle(npc, dt, player_snapshot);
		break;
	case NPC_STATE_RUN:
		UpdateNpcRun(npc, dt, player_snapshot);
		break;
	case NPC_STATE_RETURN:
		UpdateNpcReturn(npc, dt, player_snapshot);
		break;
	case NPC_STATE_ATTACK:
	case NPC_STATE_RELOAD:	// ATTACK 핸들러가 같이 처리
		UpdateNpcAttack(npc, dt, player_snapshot);
		break;
	case NPC_STATE_DIE:
		UpdateNpcDie(npc, dt, player_snapshot);
		break;
	}
}

static void BroadcastNpcPositions(const std::array<PlayerSnapshot, MAX_USER>& player_snapshot)
{
	for (const auto& npc : g_npcs) {
		if (!npc.alive) continue;

		for (int i = 0; i < MAX_USER; ++i) {
			if (!player_snapshot[i].in_game) continue;

			clients[i].send_move_npc_packet(
				npc.id,
				npc.position.x, npc.position.y, npc.position.z,
				npc.yaw
			);
		}
	}
}

/* 인벤 추가 — 클라 Inventory::AddItem과 동일 로직 + 갱신된 슬롯 인덱스 반환
   반환: 성공 시 갱신된 슬롯 인덱스 (>=0), 실패 시 -1 */
static int AddInventoryItem(std::array<ItemSlot, INVENTORY_SIZE>& inv, ItemID item, int count)
{
	if (item == ItemID::NONE || count <= 0) return -1;

	for (int i = 0; i < INVENTORY_SIZE; ++i) {
		if (inv[i].item != ItemID::NONE && inv[i].item == item) {
			inv[i].count += count;
			return i;
		}
	}
	for (int i = 0; i < INVENTORY_SIZE; ++i) {
		if (inv[i].item == ItemID::NONE) {
			inv[i].item = item;
			inv[i].count = count;
			return i;
		}
	}
	return -1;
}

static void npc_thread()
{
	using clock = std::chrono::steady_clock;
	constexpr auto TICK = std::chrono::milliseconds(33);	// 30Hz
	//constexpr auto TICK = std::chrono::milliseconds(16);	// 60Hz
	constexpr float DT = 1.0f / 30.0f;
	//constexpr float DT = 1.0f / 60.0f;
	constexpr int   BROADCAST_EVERY = 3;

	std::vector<NpcInputEvent>            events;
	events.reserve(32);
	std::array<PlayerSnapshot, MAX_USER>  player_snapshot;

	auto next = clock::now();
	int  tick_count = 0;

	while (true) {
		next += TICK;

		/*
		static int debug_tick = 0;
		if (++debug_tick % 30 == 0) {
			std::cout << "[NPC] tick " << debug_tick << "\n";
		}
		*/

		// 워커가 보낸 입력 처리
		g_npc_input_queue.DrainTo(events);
		for (auto& e : events) {
			HandleNpcEvent(e, player_snapshot);
		}

		// 플레이어 위치 스냅샷
		SnapshotPlayers(player_snapshot);

		// 각 살아있는 NPC 갱신
		for (auto& npc : g_npcs) {
			if (!npc.alive) continue;
			UpdateNpc(npc, DT, player_snapshot);
		}

		const auto now = std::chrono::steady_clock::now();
		for (auto& npc : g_npcs) {
			if (false == npc.loot_active) continue;

			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - npc.death_time).count();
			if (elapsed < 30) continue;

			// 만료 — 비활성화 + 브로드캐스트
			npc.loot_active = false;

			SC_DEACTIVATE_LOOT_BOX_PACKET dp;
			dp.size = sizeof(dp);
			dp.type = SC_DEACTIVATE_LOOT_BOX;
			dp.npc_id = npc.id;

			for (auto& pl : clients) {
				{
					std::lock_guard<std::mutex> ll(pl._s_lock);
					if (ST_INGAME != pl._state) continue;
				}
				pl.do_send(&dp);
			}

			std::cout << "[LOOT_BOX " << npc.id << "] deactivated (lifetime expired)\n";
		}

		// 5Hz 위치 브로드캐스트 (6틱마다) --> (3틱마다로 변경)
		++tick_count;
		if (tick_count >= BROADCAST_EVERY) {
			tick_count = 0;
			BroadcastNpcPositions(player_snapshot);
		}

		std::this_thread::sleep_until(next);
	}
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id]._name, p->name);

		std::cout << "LOGIN " << clients[c_id]._name << ", ID " << clients[c_id]._id << "\n";

		// 접속 id 기반 시작 위치
		{
			const PlayerSpawnPos& sp = PLAYER_SPAWN_POS[c_id % PLAYER_SPAWN_COUNT];
			clients[c_id].x = sp.x;
			clients[c_id].z = sp.z;
			clients[c_id].y = 0.1f;   // 기본 높이
		}

		clients[c_id].send_login_info_packet();
		{
			std::lock_guard<std::mutex> ll{ clients[c_id]._s_lock };
			clients[c_id]._state = ST_INGAME;
		}

		for (auto& pl : clients) {
			{
				std::lock_guard<std::mutex> ll(pl._s_lock);
				if (ST_INGAME != pl._state) continue;
			}
			if (pl._id == c_id) continue;
			pl.send_add_player_packet(c_id);
			printf("ADD_PLAYER: %d, SEND TO %d\n", c_id, pl._id);
			clients[c_id].send_add_player_packet(pl._id);
			printf("ADD_PLAYER: %d, SEND TO %d\n", pl._id, c_id);
		}

		{
			NpcInputEvent ev{};
			ev.type = NpcInputEvent::NEW_CLIENT_JOINED;
			ev.new_client_id = c_id;
			g_npc_input_queue.Push(std::move(ev));
		}

		// 동기화 검증용 테스트 아이템 (05.15)
		{
			std::lock_guard<std::mutex> ll(clients[c_id]._s_lock);

			clients[c_id]._inventory[0] = ItemSlot{ ItemID::MAT_1_FIBER, 10 };
			clients[c_id].send_inventory_update_packet(0);

			clients[c_id]._inventory[1] = ItemSlot{ ItemID::MAT_2_METAL_PLATE, 20 };
			clients[c_id].send_inventory_update_packet(1);

			clients[c_id]._inventory[2] = ItemSlot{ ItemID::MAT_3_BOLT_AND_NUT, 20 };
			clients[c_id].send_inventory_update_packet(2);
		}

		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

		// 패킷 순서 보정: 이미 더 최신 패킷을 처리했으면 무시
		if (p->move_time < (unsigned int)clients[c_id]._last_move_time) break;
		clients[c_id]._last_move_time = p->move_time;

		// 서버 deltaTime 계산
		auto now = std::chrono::steady_clock::now();
		float fDeltaTime = std::chrono::duration<float>(
			now - clients[c_id]._last_move_recv_time).count();
		clients[c_id]._last_move_recv_time = now;

		// deltaTime 상한 (프레임 스파이크 방지 - 최대 100ms)
		if (fDeltaTime > 0.1f) fDeltaTime = 0.1f;

		// yaw(도) → 라디안 변환 후 Look/Right 벡터 계산
		float fYawRad = p->yaw * (3.14159265f / 180.0f);
		clients[c_id].yaw = fYawRad;

		float lookX = sinf(fYawRad);
		float lookZ = cosf(fYawRad);
		float rightX = cosf(fYawRad);
		float rightZ = -sinf(fYawRad);

		char new_state = (p->inputs == 0) ? PLAYER_STATE_IDLE : PLAYER_STATE_RUN;
		if (clients[c_id].player_state != new_state) {
			clients[c_id].player_state = new_state;
			for (auto& cl : clients) {
				if (cl._state != ST_INGAME) continue;
				if (cl._id == c_id) continue;
				cl.send_player_state_change_packet(c_id);
			}
		}

		// inputs 비트 플래그로 이동 방향 계산
		float dirX = 0.0f, dirZ = 0.0f;
		if (p->inputs & MOVE_W) { dirX += lookX;  dirZ += lookZ; }
		if (p->inputs & MOVE_S) { dirX -= lookX;  dirZ -= lookZ; }
		if (p->inputs & MOVE_D) { dirX += rightX; dirZ += rightZ; }
		if (p->inputs & MOVE_A) { dirX -= rightX; dirZ -= rightZ; }

		// 이동이 없으면 중계만 하고 종료
		if (dirX == 0.0f && dirZ == 0.0f) {
			for (auto& cl : clients) {
				if (cl._state != ST_INGAME) continue;
				cl.send_move_packet(c_id);
			}
			break;
		}

		// 방향 정규화
		float len = sqrtf(dirX * dirX + dirZ * dirZ);
		dirX /= len;
		dirZ /= len;

		ApplySlide(c_id, dirX, dirZ);

		// 이동 속도 (클라이언트와 동일하게 8.0f)
		constexpr float MOVE_SPEED = 8.0f;
		clients[c_id].x += dirX * MOVE_SPEED * fDeltaTime;
		clients[c_id].z += dirZ * MOVE_SPEED * fDeltaTime;

		// 충돌 처리: 위치 보정 + 노멀 누적
		ResolvePlayerCollision(c_id, fYawRad);


		//// 맵 범위 클램핑
		//constexpr float MAP_MIN = -150.0f;
		//constexpr float MAP_MAX =   50.0f;
		//if (clients[c_id].x < MAP_MIN) clients[c_id].x = MAP_MIN;
		//if (clients[c_id].x > MAP_MAX) clients[c_id].x = MAP_MAX;
		//if (clients[c_id].z < MAP_MIN) clients[c_id].z = MAP_MIN;
		//if (clients[c_id].z > MAP_MAX) clients[c_id].z = MAP_MAX;


		// 계산된 좌표 전체 중계
		for (auto& cl : clients) {
			if (cl._state != ST_INGAME) continue;
			cl.send_move_packet(c_id);
			/*printf("[MOVE] id:%d inputs:0x%02X yaw:%.1f dt:%.4f -> (%.2f, %.2f, %.2f) SEND TO %d\n",
				c_id, (unsigned char)p->inputs, p->yaw, fDeltaTime,
				clients[c_id].x, clients[c_id].y, clients[c_id].z, cl._id);*/
		}

		break;
	}
	case CS_INVENTORY_CLICK: {
		CS_INVENTORY_CLICK_PACKET* p =
			reinterpret_cast<CS_INVENTORY_CLICK_PACKET*>(packet);

		if (p->slotidx < 0 || p->slotidx >= MAX_SLOTS) {
			std::cout << "[INVENTORY_CLICK] id:" << c_id
				<< " (" << clients[c_id]._name << ")"
				<< " action:" << static_cast<int>(p->action)
				<< " slot:" << p->slotidx << "\n";
			break;
		}
		//std::lock_guard<std::mutex> ll(clients[c_id]._s_lock);
		const ItemSlot& s = clients[c_id]._inventory[p->slotidx];
		std::cout << "[INVENTORY_CLICK] id:" << c_id
			<< " slot:" << p->slotidx
			<< " item:" << static_cast<int>(s.item)
			<< " count:" << s.count << "\n";

		break;
	}
	case CS_CRAFT_REQUEST: {
		CS_CRAFT_REQUEST_PACKET* p =
			reinterpret_cast<CS_CRAFT_REQUEST_PACKET*>(packet);

		const CraftRecipe* recipe = FindCraftRecipe(p->target);
		if (recipe == nullptr) {
			std::cout << "[CRAFT] id:" << c_id
				<< " unknown recipe target:" << static_cast<int>(p->target) << "\n";
			break;
		}

		std::lock_guard<std::mutex> ll(clients[c_id]._s_lock);
		auto& inv = clients[c_id]._inventory;

		// 1) 재료 충분 검사
		bool enough = true;
		for (int k = 0; k < MAX_RECIPE_INGREDIENTS; ++k) {
			const RecipeIngredient& req = recipe->ingredients[k];
			if (req.item == ItemID::NONE) break;
			int have = 0;
			for (int i = 0; i < INVENTORY_SIZE; ++i) {
				if (inv[i].item == req.item) { have = inv[i].count; break; }
			}
			if (have < req.count) { enough = false; break; }
		}
		if (!enough) {
			std::cout << "[CRAFT] id:" << c_id
				<< " target:" << static_cast<int>(p->target)
				<< " FAILED (not enough materials)\n";
			break;
		}

		// 2) 재료 차감 + 변경 슬롯 송신
		for (int k = 0; k < MAX_RECIPE_INGREDIENTS; ++k) {
			const RecipeIngredient& req = recipe->ingredients[k];
			if (req.item == ItemID::NONE) break;
			for (int i = 0; i < INVENTORY_SIZE; ++i) {
				if (inv[i].item == req.item) {
					inv[i].count -= req.count;
					if (inv[i].count <= 0) {
						inv[i].item = ItemID::NONE;
						inv[i].count = 0;
					}
					clients[c_id].send_inventory_update_packet(static_cast<short>(i));
					break;
				}
			}
		}

		// 3) 결과물 장비 슬롯 통보
		clients[c_id].send_equipment_update_packet(recipe->result);

		std::cout << "[CRAFT] id:" << c_id
			<< " target:" << static_cast<int>(p->target)
			<< " SUCCESS -> equip:" << static_cast<int>(recipe->result) << "\n";
		break;
	}
	case CS_HIT_NPC: {
		CS_HIT_NPC_PACKET* p =
			reinterpret_cast<CS_HIT_NPC_PACKET*>(packet);

		NpcInputEvent ev{};
		ev.type = NpcInputEvent::HIT;
		ev.attacker_client_id = c_id;
		ev.ray_origin = { p->ray_ox, p->ray_oy, p->ray_oz };
		ev.ray_direction = { p->ray_dx, p->ray_dy, p->ray_dz };
		ev.weapon_type = p->weapon_type;
		ev.weapon_grade = p->weapon_grade;
		// p->fire_time은 후속 lag compensation 시 사용 — 현재 미사용

		g_npc_input_queue.Push(std::move(ev));

		break;
	}
	case CS_HIT_PLAYER: {
		CS_HIT_PLAYER_PACKET* p =
			reinterpret_cast<CS_HIT_PLAYER_PACKET*>(packet);

		XMVECTOR origin = XMVectorSet(p->ray_ox, p->ray_oy, p->ray_oz, 0.0f);
		XMVECTOR dir = XMVector3Normalize(
			XMVectorSet(p->ray_dx, p->ray_dy, p->ray_dz, 0.0f));

		// 공격자 무기 spec (PvE HIT_NPC와 동일 테이블 공유)
		WeaponSpec spec = LookupWeaponSpec(
			static_cast<WeaponType>(p->weapon_type),
			static_cast<WeaponGrade>(p->weapon_grade));

		/*
		{
			std::cout << "[debug pvp weapon type]: " << static_cast<int>(p->weapon_type)
				<< " grade:" << static_cast<int>(p->weapon_grade)
				<< " range:" << spec.range
				<< " damage:" << spec.damage
				<< "\n";
		}
		*/

		// 락 잡아서 후보만 스냅샷
		short  best_id = -1;
		float  best_t = spec.range;

		for (int i = 0; i < MAX_USER; ++i) {
			if (i == c_id) continue;   // 자기 자신 제외

			XMFLOAT3 tpos;
			float    tyaw;
			{
				std::lock_guard<std::mutex> lk(clients[i]._s_lock);
				if (clients[i]._state != ST_INGAME) continue;
				if (clients[i].hp <= 0) continue;     // 이미 사망
				tpos = { clients[i].x, clients[i].y, clients[i].z };
				tyaw = clients[i].yaw;
			}   // 좌표 복사 끝, 락 해제

			BoundingOrientedBox pbox = MakePlayerOOBB(tpos, tyaw);
			float t;
			if (pbox.Intersects(origin, dir, t) && t >= 0.0f && t < best_t) {
				best_t = t;
				best_id = (short)i;
			}
		}

		// 명중 1번만 짧게 락, hp조정
		if (best_id >= 0) {
			short dmg = ComputeDamage(spec, best_t);   // 거리 감쇠 포함
			short new_hp = 0;
			{
				std::lock_guard<std::mutex> lk(clients[best_id]._s_lock);
				if (clients[best_id]._state == ST_INGAME) {
					clients[best_id].hp -= dmg;
					if (clients[best_id].hp < 0) clients[best_id].hp = 0;
					new_hp = clients[best_id].hp;
				}
			}

			std::cout << "[PvP] client " << c_id << " HIT player "
				<< best_id << " (hp=" << new_hp << ")\n";

			// 피격자 본인에게만 HP 통지
			SC_PLAYER_HP_UPDATE_PACKET hp_pkt;
			hp_pkt.size = sizeof(hp_pkt);
			hp_pkt.type = SC_PLAYER_HP_UPDATE;
			hp_pkt.id = best_id;
			hp_pkt.hp = new_hp;
			clients[best_id].do_send(&hp_pkt);
		}

		// 머즐 플래시 전체 브로드캐스트
		BroadcastAttachedEffectNoSnapshot(
			EffectID::SPARK, EffectEntityKind::PLAYER, (short)c_id);

		break;
	}
	case CS_LOOT_PICKUP: {
		CS_LOOT_PICKUP_PACKET* p = reinterpret_cast<CS_LOOT_PICKUP_PACKET*>(packet);

		// 박스 유효성
		if (p->box_id < 0 || p->box_id >= MAX_NPC) break;
		SERVER_NPC& box = g_npcs[p->box_id];
		if (!box.loot_active) break;

		// 슬롯 범위
		if (p->slotidx < 0 || p->slotidx >= INVENTORY_SIZE) break;

		ItemSlot& boxSlot = box._inventory[p->slotidx];
		if (boxSlot.item == ItemID::NONE || boxSlot.count <= 0) break;

		const ItemID pickItem = boxSlot.item;
		const int    pickCount = boxSlot.count;

		// 플레이어 인벤에 추가 + 갱신 송신 (한 락 안에서)
		int playerSlotIdx = -1;
		{
			std::lock_guard<std::mutex> ll(clients[c_id]._s_lock);
			playerSlotIdx = AddInventoryItem(
				clients[c_id]._inventory, pickItem, pickCount);
			if (playerSlotIdx >= 0) {
				clients[c_id].send_inventory_update_packet(
					static_cast<short>(playerSlotIdx));
			}
		}
		if (playerSlotIdx < 0) break;  // 인벤 가득 — 박스 그대로

		// 박스 슬롯 비우기
		boxSlot.item = ItemID::NONE;
		boxSlot.count = 0;

		// 박스 슬롯 변경 브로드캐스트
		SC_LOOT_BOX_SLOT_UPDATE_PACKET bp;
		bp.size = sizeof(bp);
		bp.type = SC_LOOT_BOX_SLOT_UPDATE;
		bp.box_id = p->box_id;
		bp.slotidx = p->slotidx;
		bp.item_id = ItemID::NONE;
		bp.count = 0;

		for (auto& pl : clients) {
			{
				std::lock_guard<std::mutex> ll(pl._s_lock);
				if (ST_INGAME != pl._state) continue;
			}
			pl.do_send(&bp);
		}

		std::cout << "[LOOT_PICKUP] client:" << c_id
			<< " box:" << p->box_id
			<< " slot:" << p->slotidx
			<< " item:" << static_cast<int>(pickItem)
			<< " count:" << pickCount
			<< " -> playerSlot:" << playerSlotIdx << "\n";

		break;
	}
	}
}

void disconnect(int c_id)
{
	SOCKET old_socket = INVALID_SOCKET;
	{
		std::lock_guard<std::mutex> ll(clients[c_id]._s_lock);
		if (clients[c_id]._state == ST_FREE) return;	// 이중 호출 방지
		clients[c_id]._state = ST_FREE;
		old_socket = clients[c_id]._socket;				// 핸들 저장
		clients[c_id]._socket = INVALID_SOCKET;			// 슬롯 무효화

		clients[c_id]._inventory.fill(ItemSlot{});		// 인벤토리 초기화
	}
	closesocket(old_socket);

	std::cout << "LOGOUT " << clients[c_id]._name << ", ID " << clients[c_id]._id << "\n";

	for (auto& pl : clients) {
		{
			std::lock_guard<std::mutex> ll(pl._s_lock);
			if (ST_INGAME != pl._state) continue;
		}
		if (pl._id == c_id) continue;
		pl.send_remove_player_packet(c_id);
	}

}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) std::cout << "Accept Error";
			else {
				std::cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				//{
				//	lock_guard<mutex> ll(clients[client_id]._s_lock);
				//	clients[client_id]._state = ST_ALLOC;
				//}
				//clients[client_id].x = 0;
				//clients[client_id].y = 0;
				clients[client_id].x = 0.0f;
				clients[client_id].y = 0.1f;
				clients[client_id].z = 0.0f;
				clients[client_id]._id = client_id;
				clients[client_id]._name[0] = 0;
				clients[client_id]._prev_remain = 0;

				clients[client_id]._collNormals.clear();

				clients[client_id]._socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				std::cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + clients[key]._prev_remain;
			char* p = ex_over->_buf;
			while (remain_data > 0) {
				int packet_size = static_cast<unsigned char>(p[0]);
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_buf, p, remain_data);
			}
			clients[key].do_recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		}
	}
}

int main()
{
	if (!load_mapOOBB_from_CSV("map_oobb.csv")) {
		std::cerr << "Failed to load map OOBBs. Exiting.\n";
		return 1;
	}

	// 테스트용 코드 (주석처리됨)
	/*
	{
		// 케이스 1: 맵 안전한 곳 (충돌 없을 거라 예상)
		XMFLOAT3 testPos = { 0.0f, 0.1f, 0.0f };
		auto playerOOBB = MakePlayerOOBB(testPos, 0.0f);
		auto results = CheckCollisionWithMap(playerOOBB, g_mapOOBBs);
		std::cout << "[Test1] pos=(0,0.1,0) yaw=0: collisions=" << results.size() << std::endl;

		// 케이스 2: 맵 청크 위치 근처 (충돌 있을 거라 예상)
		// CSV 두번째 줄 첫 번째 청크 center=(37.82, 7.54, -103.29) extents=(4.08, 8.04, 8.72)
		// 이 청크 중심에 플레이어 박으면 무조건 충돌
		XMFLOAT3 testPos2 = { 37.82f, 0.1f, -103.29f };
		auto playerOOBB2 = MakePlayerOOBB(testPos2, 0.0f);
		auto results2 = CheckCollisionWithMap(playerOOBB2, g_mapOOBBs);
		std::cout << "[Test2] pos=(37.82,0.1,-103.29) yaw=0: collisions=" << results2.size() << std::endl;
		if (!results2.empty()) {
			const auto& r = results2[0];
			std::cout << "  normal=(" << r.normal.x << "," << r.normal.y << "," << r.normal.z
				<< ") mtv=(" << r.mtv.x << "," << r.mtv.y << "," << r.mtv.z << ")" << std::endl;
		}
	}
	*/

	g_astar.LoadNavMeshFromFile("Model/NavMeshData.bin");
	std::cout << "NavMesh loaded from Model / NavMeshData.bin\n";

	init_npcs();

	// NPC 배치: 위치(x,z) + 단계(tier) + 외형(outfit). y는 0.0f 고정.
	// 1·2단계: 10그룹 x 3마리(a,b=1단계, c=2단계). 3단계: A,B,C.
	struct NpcSpawnDef { float x, z; char tier; char outfit; };
	NpcSpawnDef main_npc_def[] = {
		// 그룹01 (outfit a0 b1 c0)
		{   3.0f,  42.0f, 1, 0 }, {   0.0f,  42.0f, 1, 1 }, {   1.0f,  44.0f, 2, 0 },
		// 그룹02 (outfit a1 b2 c1)
		{   5.0f, -14.0f, 1, 1 }, {  -2.0f, -10.0f, 1, 2 }, {   2.0f, -11.0f, 2, 1 },
		// 그룹03 (outfit a0 b2 c2)
		{   2.0f, -59.0f, 1, 0 }, {   3.0f, -63.0f, 1, 2 }, {   0.0f, -62.0f, 2, 2 },
		// 그룹04 (outfit a0 b1 c0)
		{  13.0f, -92.0f, 1, 0 }, {   9.0f, -91.0f, 1, 1 }, {  10.0f, -95.0f, 2, 0 },
		// 그룹05 (outfit a1 b2 c1)
		{ -37.0f,  -5.0f, 1, 1 }, { -40.0f, -11.0f, 1, 2 }, { -42.0f,  -7.0f, 2, 1 },
		// 그룹06 (outfit a0 b2 c2)
		{ -40.0f, -58.0f, 1, 0 }, { -39.0f, -52.0f, 1, 2 }, { -39.0f, -57.0f, 2, 2 },
		// 그룹07 (outfit a0 b1 c0)
		{ -57.0f,  29.0f, 1, 0 }, { -61.0f,  25.0f, 1, 1 }, { -62.0f,  31.0f, 2, 0 },
		// 그룹08 (outfit a1 b2 c1)
		{ -61.0f, -66.0f, 1, 1 }, { -61.0f, -57.0f, 1, 2 }, { -57.0f, -63.0f, 2, 1 },
		// 그룹09 (outfit a0 b2 c2)
		{ -93.0f, -90.0f, 1, 0 }, { -99.0f, -85.0f, 1, 2 }, { -99.0f, -91.0f, 2, 2 },
		// 그룹10 (outfit a0 b1 c0)
		{ -127.0f, -48.0f, 1, 0 }, { -125.0f, -34.0f, 1, 1 }, { -131.0f, -39.0f, 2, 0 },
		// 3단계 A,B,C (outfit 0,1,2)
		{  12.0f, -135.0f, 3, 0 }, { -113.0f, -121.0f, 3, 1 }, { -100.0f,  25.0f, 3, 2 },
	};
	const int npc_count = static_cast<int>(sizeof(main_npc_def) / sizeof(main_npc_def[0]));  // 33

	for (int i = 0; i < npc_count; ++i)
	{
		// NPC 1개 — id 0, (7, 0, -14) 위치
		SERVER_NPC& npc = g_npcs[i];
		npc.alive = true;
		npc.outfit = main_npc_def[i].outfit;									//	임시 적용!!!!!!
		ApplyNpcTier(npc, main_npc_def[i].tier);   // kind/weapon/hp/max_hp 설정		임시 적용!!!!!!
		npc.state = NPC_STATE_IDLE;
		npc.position = { main_npc_def[i].x, 0.0f, main_npc_def[i].z };
		npc.spawn_position = npc.position;
		npc.yaw = 0.0f;
		npc.current_ammo = GetNpcWeaponSpec(npc).magazineSize;   // 스폰 시 탄창 채움
		// path_update_timer, waypoints, way_idx, die_timer는 init_npcs()에서 이미 0/빈 상태

		// NPC 인벤토리 초기화 하드코딩
		npc._inventory[0] = ItemSlot{ ItemID::MAT_3_BOLT_AND_NUT, 2 };
		npc._inventory[1] = ItemSlot{ ItemID::MAT_2_METAL_PLATE, 1 };

		std::cout << "NPC[" << npc.id << "] spawned with inventory: "
			<< "slot0=" << static_cast<int>(npc._inventory[0].item)
			<< "x" << npc._inventory[0].count
			<< ", slot1=" << static_cast<int>(npc._inventory[1].item)
			<< "x" << npc._inventory[1].count << "\n";
	}

	/*
		// 케이스 A: 가까운 두 점 — waypoint 1~2개 기대
		{
			XMFLOAT3 start = { 0.0f, 0.0f, 0.0f };
			XMFLOAT3 end = { 5.0f, 0.0f, -5.0f };
			auto path = g_astar.FindPath(start, end);
			std::cout << "[AI Test A] (0,0,0)->(5,0,-5): " << path.size() << " waypoints\n";
			for (size_t i = 0; i < path.size(); ++i) {
				std::cout << "    [" << i << "] (" << path[i].x << ", "
					<< path[i].y << ", " << path[i].z << ")\n";
			}
		}

		// 케이스 B: 떨어진 두 점 — 여러 waypoint 기대
		{
			XMFLOAT3 start = { 0.0f, 0.0f, 0.0f };
			XMFLOAT3 end = { 7.0f, 0.0f, -14.0f };
			auto path = g_astar.FindPath(start, end);
			std::cout << "[AI Test B] (0,0,0)->(7,0,-14): " << path.size() << " waypoints\n";
			for (size_t i = 0; i < path.size(); ++i) {
				std::cout << "    [" << i << "] (" << path[i].x << ", "
					<< path[i].y << ", " << path[i].z << ")\n";
			}
		}

		// 케이스 C: 의도적 실패 — 맵 밖. "[AI] FindPath failed" 로그가 떠야 정상.
		{
			XMFLOAT3 start = { 0.0f, 0.0f, 0.0f };
			XMFLOAT3 end = { 10000.0f, 0.0f, 10000.0f };
			auto path = g_astar.FindPath(start, end);
			std::cout << "[AI Test C] (0,0,0)->(10000,0,10000): " << path.size()
				<< " waypoints (expected 0 + failure log)\n";
		}
	}
	*/

	HANDLE h_iocp;

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over._buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	std::vector<std::thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency() - 1;
	if (num_threads < 1) num_threads = 1;
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);

	std::thread npc_th(npc_thread);

	for (auto& th : worker_threads)
		th.join();
	npc_th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
