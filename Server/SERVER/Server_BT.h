#pragma once
#include <array>
#include <vector>
#include <memory>
#include <cstdint>

struct Room;
struct SERVER_NPC;

enum class BtStatus : uint8_t
{
    Success,
    Failure,
    Running
};

struct NpcBtContext
{
    const Room* room = nullptr;
    SERVER_NPC* npc  = nullptr;
    float       dt   = 0.0f;
};

using BtConditionFn = bool     (*)(NpcBtContext&);
using BtActionFn    = BtStatus (*)(NpcBtContext&);

constexpr uint8_t BT_INVALID_CHILD = 0xFF;

class BtNode
{
public:
    virtual ~BtNode() = default;

    virtual BtStatus Tick(NpcBtContext& ctx) const = 0;
    virtual void     Reset(NpcBtContext& ctx) const { (void)ctx; }

    const char* Name() const { return m_name; }

protected:
    explicit BtNode(const char* name) : m_name(name ? name : "BtNode") {}

    const char* m_name;
};

class BtComposite : public BtNode
{
public:
    void AddChild(std::unique_ptr<BtNode> child);
    void Reset(NpcBtContext& ctx) const override;

protected:
    BtComposite(const char* name, uint8_t slot) : BtNode(name), m_slot(slot) {}

    uint8_t& RunningChild(NpcBtContext& ctx) const;
    void     ResetChildren(NpcBtContext& ctx) const;

    std::vector<std::unique_ptr<BtNode>> m_children;
    uint8_t                              m_slot;
};

class BtReactiveSelector : public BtComposite
{
public:
    BtReactiveSelector(const char* name, uint8_t slot) : BtComposite(name, slot) {}
    BtStatus Tick(NpcBtContext& ctx) const override;
};

class BtReactiveSequence : public BtComposite
{
public:
    BtReactiveSequence(const char* name, uint8_t slot) : BtComposite(name, slot) {}
    BtStatus Tick(NpcBtContext& ctx) const override;
};

class BtCondition : public BtNode
{
public:
    BtCondition(const char* name, BtConditionFn fn) : BtNode(name), m_fn(fn) {}
    BtStatus Tick(NpcBtContext& ctx) const override;

private:
    BtConditionFn m_fn;
};

class BtAction : public BtNode
{
public:
    BtAction(const char* name, BtActionFn fn) : BtNode(name), m_fn(fn) {}
    BtStatus Tick(NpcBtContext& ctx) const override;

private:
    BtActionFn m_fn;
};

class NpcBehaviorTree
{
public:
    void     Build();                                                  // 서버 부팅 시 1회
    BtStatus Tick(const Room& room, SERVER_NPC& npc, float dt) const;  // 매 틱
    void     ResetNpc(SERVER_NPC& npc) const;                          // 스폰/리스폰 시

    uint8_t CompositeCount() const { return m_composite_count; }

private:
    uint8_t AllocSlot();

    std::unique_ptr<BtNode> m_root;
    uint8_t                 m_composite_count = 0;
};

extern NpcBehaviorTree g_npc_bt;

void ChangeNpcState(const Room& r, SERVER_NPC& npc, char new_state);
