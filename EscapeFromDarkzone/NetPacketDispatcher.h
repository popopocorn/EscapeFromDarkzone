#pragma once
#include <vector>

class CGameFramework;   // 전방 선언 (순환 include 방지)

class NetPacketDispatcher
{
public:
    explicit NetPacketDispatcher(CGameFramework* pFramework)
        : m_pFramework(pFramework) {
    }

    void Handle(std::vector<char>& packet);   // 패킷 1개 처리

private:
    CGameFramework* m_pFramework = nullptr;    // non-owning, 임시
};