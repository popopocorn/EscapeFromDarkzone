//#include "stdafx.h"
#include "Network.h"

bool NetworkManager::Init(const char* playerName) 
{
	// WSAStartup
	if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0) {
		OutputDebugString(L"[Network] WSAStartup() Fail.\n");
		return false;
	}

	// 소켓 생성
	m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	if (m_socket == INVALID_SOCKET) {
		WSACleanup();
		OutputDebugString(L"[Network] WSASocket() Fail.\n");
		return false;
	}

	// TCP_NODELAY (Nagle 비활성화)
	int flag = 1;
	setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(flag));

	// non-blocking
	u_long mode = 1;
	ioctlsocket(m_socket, FIONBIO, &mode);

	// 서버 주소 설정
	ZeroMemory(&m_serverAddr, sizeof(m_serverAddr));
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_port   = htons(PORT_NUM);
	inet_pton(AF_INET, SERVER_ADDR, &m_serverAddr.sin_addr);

	// WSAConnect
	int ret = WSAConnect(m_socket, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr), nullptr, nullptr, nullptr, nullptr);

	if (ret == SOCKET_ERROR) {
		int err = WSAGetLastError();
		wchar_t szLog[128];
		if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != 0) {
			swprintf_s(szLog, L"[Network] WSAConnect() result: %d, WSAError: %d\n", ret, err);
			OutputDebugStringW(szLog);

			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			WSACleanup();
			return false;
		}

		// connect 완료 대기 (최대 5초)
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(m_socket, &writeSet);

		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		int result = select(0, nullptr, &writeSet, nullptr, &timeout);

		if (result <= 0) {
			wchar_t szLog2[128];
			swprintf_s(szLog2, L"[Network] select() result: %d, WSAError: %d\n", result, WSAGetLastError());
			OutputDebugStringW(szLog2);

			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			WSACleanup();
			return false;
		}
	}
	else
	{
		//OutputDebugString(L"[Network] WSAConnect() 연결 성공\n");
	}

	// 수신 버퍼 및 prev_remain 초기화
	ZeroMemory(m_recvBuf, sizeof(m_recvBuf));
	m_prevRemain = 0;

	m_bConnected = true;

	// 로그인 패킷 전송
	if (!SendLogin(playerName)) {
		OutputDebugString(L"[Network] SendLogin() Fail.\n");
		Shutdown();
		return false;
	}

	return true;
}

void NetworkManager::Shutdown()
{
	m_bConnected = false;

	if (m_socket != INVALID_SOCKET) {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	WSACleanup();

	// 수신 버퍼 및 큐 초기화
	ZeroMemory(m_recvBuf, sizeof(m_recvBuf));
	m_prevRemain = 0;

	while (!m_packetQueue.empty()) {
		m_packetQueue.pop();
	}
}

bool NetworkManager::IsConnected()
{
	if (!m_bConnected) {
		return false;
	}

	int err    = 0;
	int errLen = sizeof(err);
	if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &errLen) == SOCKET_ERROR || err != 0) {
		OutputDebugString(L"[Network] IsConnected() False.\n");
		m_bConnected = false;
		return false;
	}

	return true;
}

void NetworkManager::Recv()
{
	WSABUF wsaBuf;
	wsaBuf.buf = m_recvBuf + m_prevRemain;
	wsaBuf.len = BUF_SIZE - m_prevRemain;

	DWORD recvLen = 0;
	DWORD flags = 0;

	int ret = WSARecv(m_socket, &wsaBuf, 1, &recvLen, &flags, nullptr, nullptr);

	if (ret == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) {
			return; // 수신할 데이터 없음 - 정상, 다음 프레임에 재시도
		}
		// 그 외 에러는 연결 끊김으로 처리
		OutputDebugString(L"[Network] Recv(): SOCKET_ERROR return.\n");
		m_bConnected = false;
		return;
	}

	if (recvLen == 0) {
		// 서버가 연결을 정상 종료
		OutputDebugString(L"[Network] Recv(): Disconnect from server.\n");
		m_bConnected = false;
		return;
	}

	int remain = static_cast<int>(recvLen) + m_prevRemain;
	char* p = m_recvBuf;

	while (remain > 0) {
		if (remain < 1) {
			break;
		}

		int packetSize = static_cast<unsigned char>(p[0]);

		// 패킷 예외처리
		if (packetSize == 0 || packetSize > BUF_SIZE) {
		    OutputDebugString(L"[Network] Incorrect packet recieved.\n");
		    m_bConnected = false;
		    return;
		}

		// 패킷이 전부 오지 않았으면 탈출
		if (remain < packetSize) {
			break;
		}

		// 완성된 패킷을 큐에 싣기
		if (static_cast<int>(m_packetQueue.size()) < MAX_PACKET_QUEUE_SIZE) {
			std::vector<char> packet(p, p + packetSize);
			m_packetQueue.push(std::move(packet));
		}

		p += packetSize;
		remain -= packetSize;
	}

	// 남은 데이터를 앞으로 이동
	if (remain > 0 && p != m_recvBuf) {
		memmove(m_recvBuf, p, remain);
	}

	m_prevRemain = remain;
}

std::vector<char> NetworkManager::PopPacket()
{
	if (m_packetQueue.empty()) {
		return {};
	}

	std::vector<char> packet = std::move(m_packetQueue.front());
	m_packetQueue.pop();
	return packet;
}

bool NetworkManager::SendRaw(const char* data, int size)
{
	if (!m_bConnected) {
		return false;
	}

	int sent = 0;
	while (sent < size) {
		WSABUF wsaBuf;
		wsaBuf.buf = const_cast<char*>(data + sent);
		wsaBuf.len = size - sent;

		DWORD bytesSent = 0;

		int ret = WSASend(m_socket, &wsaBuf, 1, &bytesSent, 0, nullptr, nullptr);

		if (ret == SOCKET_ERROR || bytesSent == 0) {
			OutputDebugString(L"[Network] SendRaw(): SOCKET_ERROR return.\n");
			m_bConnected = false;
			return false;
		}

		sent += static_cast<int>(bytesSent);
	}
	return true;
}

bool NetworkManager::SendLogin(const char* name)
{
	CS_LOGIN_PACKET pkt;
	ZeroMemory(&pkt, sizeof(pkt));
	pkt.size = sizeof(CS_LOGIN_PACKET);
	pkt.type = CS_LOGIN;
	strncpy_s(pkt.name, NAME_SIZE, name, _TRUNCATE);

	return SendRaw(reinterpret_cast<char*>(&pkt), pkt.size);
}

bool NetworkManager::SendMove(char inputs, float yaw, unsigned int move_time)
{
	CS_MOVE_PACKET pkt;
	ZeroMemory(&pkt, sizeof(pkt));
	pkt.size = sizeof(CS_MOVE_PACKET);
	pkt.type = CS_MOVE;
	pkt.inputs = inputs;
	pkt.yaw = yaw;
	pkt.move_time = move_time;

	return SendRaw(reinterpret_cast<char*>(&pkt), pkt.size);
}
