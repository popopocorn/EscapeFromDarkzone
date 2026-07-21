#pragma once

#include <DirectXMath.h>

#include <WS2tcpip.h>
#include <queue>
#include <vector>
#include "protocol.h"


#pragma comment(lib, "WS2_32.lib")

constexpr char SERVER_ADDR[] = "127.0.0.1";
//constexpr char SERVER_ADDR[] = "119.195.220.93";


// 패킷 큐 최대 크기
constexpr int MAX_PACKET_QUEUE_SIZE = 100;

class NetworkManager {
private:
    NetworkManager() = default;
    ~NetworkManager() = default;

    bool SendRaw(const char* data, int size);

    SOCKET      m_socket = INVALID_SOCKET;
    WSADATA     m_wsaData = {};
    SOCKADDR_IN m_serverAddr = {};

    char        m_recvBuf[BUF_SIZE] = {};
    int         m_prevRemain = 0;

    std::queue<std::vector<char>> m_packetQueue;
    bool        m_bConnected = false;

public:
    static NetworkManager& Instance()
    {
        static NetworkManager inst;
        return inst;
    }

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool Init(const char* playerName);
    void Shutdown();
    bool IsConnected();
    void Recv();
    std::vector<char> PopPacket();

    bool SendLogin(const char* name);
    bool SendMove(char inputs, float yaw, unsigned int move_time);
    bool SendInventoryClick(char action, short slotidx);
    bool SendHitNpc(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade);
    bool SendHitPlayer(const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, short weaponType, short weaponGrade);
    bool SendChangeWeapon(short weaponType, short weaponGrade);
    bool SendReloadRequest(short weaponType);
    bool SendChangeState(char state);
    bool SendGrenadeExplode(const DirectX::XMFLOAT3& pos);

    bool SendLootPickup(short box_id, short slotidx);
    bool SendCraftRequest(ItemID target);

    bool SendRoundJoin();
    bool SendRoundLeave();

    bool SendToggleGodmode();       // 디버그용 갓모드
};