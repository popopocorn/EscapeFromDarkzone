#pragma once

#include <WS2tcpip.h>
#include <queue>
#include <vector>
#include "protocol.h"

#pragma comment(lib, "WS2_32.lib")

constexpr char SERVER_ADDR[] = "127.0.0.1";

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

    NetworkManager(const NetworkManager&)            = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool Init(const char* playerName);
    void Shutdown();
    bool IsConnected();
    void Recv();
    std::vector<char> PopPacket();

    bool SendLogin(const char* name);
    //bool SendMove(float x, float y, float z, unsigned int move_time);
    bool SendMove(char inputs, float yaw, unsigned int move_time);
};
