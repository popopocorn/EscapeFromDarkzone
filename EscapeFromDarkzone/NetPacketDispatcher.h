#pragma once
#include <vector>
#include <cassert>
#include <cstring>
#include <type_traits>
#include "protocol.h"

class CGameFramework;   // 전방 선언 (순환 include 방지)

class NetPacketDispatcher
{
public:
    explicit NetPacketDispatcher(CGameFramework* pFramework)
        : m_pFramework(pFramework) {
    }

    void Handle(const std::vector<char>& packet);

    static bool IsValidPacket(const std::vector<char>& packet)
    {
        if (packet.size() < 2 || static_cast<unsigned char>(packet[0]) != packet.size())
            return false;

        std::size_t expectedSize = 0;
        switch (packet[1])
        {
        case SC_LOGIN_INFO: expectedSize = sizeof(SC_LOGIN_INFO_PACKET); break;
        case SC_ADD_PLAYER: expectedSize = sizeof(SC_ADD_PLAYER_PACKET); break;
        case SC_REMOVE_PLAYER: expectedSize = sizeof(SC_REMOVE_PLAYER_PACKET); break;
        case SC_MOVE_PLAYER: expectedSize = sizeof(SC_MOVE_PLAYER_PACKET); break;
        case SC_PLAYER_STATE_CHANGE: expectedSize = sizeof(SC_PLAYER_STATE_CHANGE_PACKET); break;
        case SC_CHANGE_WEAPON: expectedSize = sizeof(SC_CHANGE_WEAPON_PACKET); break;
        case SC_ROUND_START: expectedSize = sizeof(SC_ROUND_START_PACKET); break;
        case SC_ESCAPE_SUCCESS: expectedSize = sizeof(SC_ESCAPE_SUCCESS_PACKET); break;
        case SC_ESCAPE_PROGRESS: expectedSize = sizeof(SC_ESCAPE_PROGRESS_PACKET); break;
        case SC_GAME_OVER: expectedSize = sizeof(SC_GAME_OVER_PACKET); break;
        case SC_ROUND_RESET: expectedSize = sizeof(SC_ROUND_RESET_PACKET); break;
        case SC_ADD_NPC: expectedSize = sizeof(SC_ADD_NPC_PACKET); break;
        case SC_REMOVE_NPC: expectedSize = sizeof(SC_REMOVE_NPC_PACKET); break;
        case SC_MOVE_NPC: expectedSize = sizeof(SC_MOVE_NPC_PACKET); break;
        case SC_NPC_STATE_CHANGE: expectedSize = sizeof(SC_NPC_STATE_CHANGE_PACKET); break;
        case SC_AMMO_STATE: expectedSize = sizeof(SC_AMMO_STATE_PACKET); break;
        case SC_GRENADE_COUNT: expectedSize = sizeof(SC_GRENADE_COUNT_PACKET); break;
        case SC_INVENTORY_UPDATE: expectedSize = sizeof(SC_INVENTORY_UPDATE_PACKET); break;
        case SC_EQUIPMENT_UPDATE: expectedSize = sizeof(SC_EQUIPMENT_UPDATE_PACKET); break;
        case SC_ADD_LOOT_BOX: expectedSize = sizeof(SC_ADD_LOOT_BOX_PACKET); break;
        case SC_LOOT_BOX_SLOT_UPDATE: expectedSize = sizeof(SC_LOOT_BOX_SLOT_UPDATE_PACKET); break;
        case SC_DEACTIVATE_LOOT_BOX: expectedSize = sizeof(SC_DEACTIVATE_LOOT_BOX_PACKET); break;
        case SC_PLAYER_HP_UPDATE: expectedSize = sizeof(SC_PLAYER_HP_UPDATE_PACKET); break;
        case SC_FIRE_TRACER: expectedSize = sizeof(SC_FIRE_TRACER_PACKET); break;
        case SC_PLAY_EFFECT_ATTACHED: expectedSize = sizeof(SC_PLAY_EFFECT_ATTACHED_PACKET); break;
        case SC_PLAY_EFFECT_WORLD: expectedSize = sizeof(SC_PLAY_EFFECT_WORLD_PACKET); break;
        default: return false;
        }
        return packet.size() == expectedSize;
    }

private:
    template<typename T>
    static T ReadPacket(const std::vector<char>& packet)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        assert(packet.size() == sizeof(T));
        T result{};
        std::memcpy(&result, packet.data(), sizeof(T));
        return result;
    }
    CGameFramework* m_pFramework = nullptr;    // non-owning, 임시
};
