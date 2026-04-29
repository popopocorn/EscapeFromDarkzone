#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>

// 03.27 추가
#include <chrono>
#include <cmath>

#include "protocol.h"

#include "Collision.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

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
	mutex _s_lock;
	S_STATE _state;
	int _id;
	SOCKET _socket;
	//short	x, y;
	float x, y, z;
	char	_name[NAME_SIZE];
	int		_prev_remain;
	int		_last_move_time;

	// 서버 측 deltaTime 계산용 - 마지막 CS_MOVE 수신 시각
	std::chrono::steady_clock::time_point _last_move_recv_time;

public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		//x = y = 0;
		x = z = 0.0f;
		y = 0.1f;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
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
};

array<SESSION, MAX_USER> clients;

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
	do_send(&add_packet);
}

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i]._s_lock };
		if (clients[i]._state == ST_FREE) {
			clients[i]._state = ST_ALLOC;
			return i;
		}
	}
	return -1;
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(clients[c_id]._name, p->name);

		std::cout << "LOGIN " << clients[c_id]._name << ", ID " << clients[c_id]._id << "\n";

		clients[c_id].send_login_info_packet();
		{
			lock_guard<mutex> ll{ clients[c_id]._s_lock };
			clients[c_id]._state = ST_INGAME;
		}

		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl._s_lock);
				if (ST_INGAME != pl._state) continue;
			}
			if (pl._id == c_id) continue;
			pl.send_add_player_packet(c_id);
			printf("ADD_PLAYER: %d, SEND TO %d\n", c_id, pl._id);
			clients[c_id].send_add_player_packet(pl._id);
			printf("ADD_PLAYER: %d, SEND TO %d\n", pl._id, c_id);
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
		float lookX = sinf(fYawRad);
		float lookZ = cosf(fYawRad);
		float rightX = cosf(fYawRad);
		float rightZ = -sinf(fYawRad);

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

		// 이동 속도 (클라이언트와 동일하게 8.0f)
		constexpr float MOVE_SPEED = 8.0f;
		clients[c_id].x += dirX * MOVE_SPEED * fDeltaTime;
		clients[c_id].z += dirZ * MOVE_SPEED * fDeltaTime;

		//// 맵 범위 클램핑
		//constexpr float MAP_MIN = -150.0f;
		//constexpr float MAP_MAX =   50.0f;
		//if (clients[c_id].x < MAP_MIN) clients[c_id].x = MAP_MIN;
		//if (clients[c_id].x > MAP_MAX) clients[c_id].x = MAP_MAX;
		//if (clients[c_id].z < MAP_MIN) clients[c_id].z = MAP_MIN;
		//if (clients[c_id].z > MAP_MAX) clients[c_id].z = MAP_MAX;

		// [DEBUG] 이동 로그 - 테스트 용도, 필요 없을 때 주석 처리
		
		
		

		// 계산된 좌표 전체 중계
		for (auto& cl : clients) {
			if (cl._state != ST_INGAME) continue;
			cl.send_move_packet(c_id);
			printf("[MOVE] id:%d inputs:0x%02X yaw:%.1f dt:%.4f -> (%.2f, %.2f, %.2f) SEND TO %d\n",
				c_id, (unsigned char)p->inputs, p->yaw, fDeltaTime,
				clients[c_id].x, clients[c_id].y, clients[c_id].z, cl._id);
		}

		break;
	}
	}
}

void disconnect(int c_id)
{
	SOCKET old_socket = INVALID_SOCKET;
	{
		lock_guard<mutex> ll(clients[c_id]._s_lock);
		if (clients[c_id]._state == ST_FREE) return; // 이중 호출 방지
		clients[c_id]._state = ST_FREE;
		old_socket = clients[c_id]._socket;           // 핸들 저장
		clients[c_id]._socket = INVALID_SOCKET;       // 슬롯 무효화
	}
	closesocket(old_socket);

	std::cout << "LOGOUT " << clients[c_id]._name << ", ID " << clients[c_id]._id << "\n";

	for (auto& pl : clients) {
		{
			lock_guard<mutex> ll(pl._s_lock);
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
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
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
				clients[client_id]._socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
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

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	closesocket(g_s_socket);
	WSACleanup();
}
