#include "Server_BT.h"
#include "Server_Npc.h"
#include "Server_Room.h"

NpcBehaviorTree g_npc_bt;

void BtComposite::AddChild(std::unique_ptr<BtNode> child)
{
    if (!child) return;
    m_children.push_back(std::move(child));
}

uint8_t& BtComposite::RunningChild(NpcBtContext& ctx) const
{
    return ctx.npc->bt_running_child[m_slot];
}

void BtComposite::ResetChildren(NpcBtContext& ctx) const
{
    for (const auto& c : m_children)
        if (c) c->Reset(ctx);
}

void BtComposite::Reset(NpcBtContext& ctx) const
{
    RunningChild(ctx) = BT_INVALID_CHILD;
    ResetChildren(ctx);
}

BtStatus BtReactiveSelector::Tick(NpcBtContext& ctx) const
{
    uint8_t& running  = RunningChild(ctx);
    uint8_t  selected = BT_INVALID_CHILD;
    BtStatus result   = BtStatus::Failure;

    for (uint8_t i = 0; i < static_cast<uint8_t>(m_children.size()); ++i)
    {
        BtStatus s = m_children[i]->Tick(ctx);

        if (s == BtStatus::Failure) continue;

        selected = i;
        result = s;
        break;
    }

    if (running != BT_INVALID_CHILD && running != selected &&
        running < m_children.size())
    {
        m_children[running]->Reset(ctx);
    }

    if (result == BtStatus::Running)
    {
        running = selected;
    }
    else
    {
        running = BT_INVALID_CHILD;
        if (selected != BT_INVALID_CHILD)
            m_children[selected]->Reset(ctx);
    }

    return result;
}

BtStatus BtReactiveSequence::Tick(NpcBtContext& ctx) const
{
    uint8_t& running = RunningChild(ctx);

    for (uint8_t i = 0; i < static_cast<uint8_t>(m_children.size()); ++i)
    {
        BtStatus s = m_children[i]->Tick(ctx);

        if (s == BtStatus::Success) continue;

        if (running != BT_INVALID_CHILD && running != i &&
            running < m_children.size())
        {
            m_children[running]->Reset(ctx);
        }

        if (s == BtStatus::Running)
        {
            running = i;
            return BtStatus::Running;
        }

        running = BT_INVALID_CHILD;
        for (uint8_t j = i + 1; j < static_cast<uint8_t>(m_children.size()); ++j)
            m_children[j]->Reset(ctx);

        return BtStatus::Failure;
    }

    running = BT_INVALID_CHILD;
    return BtStatus::Success;
}

BtStatus BtCondition::Tick(NpcBtContext& ctx) const
{
    if (!m_fn) return BtStatus::Failure;
    return m_fn(ctx) ? BtStatus::Success : BtStatus::Failure;
}

BtStatus BtAction::Tick(NpcBtContext& ctx) const
{
    if (!m_fn) return BtStatus::Failure;
    return m_fn(ctx);
}

static bool CondIsDying(NpcBtContext& ctx)
{
    const SERVER_NPC& n = *ctx.npc;
    return (n.hp <= 0) || (n.state == NPC_STATE_DIE);
}

static bool CondShouldReturn(NpcBtContext& ctx)
{
    const SERVER_NPC& n = *ctx.npc;

    // 리시 이탈은 무조건 복귀
    if (n.percep.outside_leash) {
        return true;
    }

    // 이미 복귀 중이면 여기서 결론을 낸다. 아래 조건들로 내려가면 안 된다.
    //  - 스폰 도착 => false. IDLE로 빠져나가야 OnEnterNpcState(IDLE)이
    //    has_last_seen_player를 지운다. 여기서 true를 내면 영영 못 빠져나간다.
    //  - 복귀 도중 플레이어 재발견 => 즉시 재교전
    if (n.state == NPC_STATE_RETURN) {
        if (n.percep.near_spawn) {
            return false;
        }
        if (n.percep.can_see) {
            return false;
        }
        return true;
    }

    if (n.reloading) {
        return false;
    }

    // 룸에 대상이 없으면 스폰으로. 이미 스폰이면 그냥 IDLE.
    if (n.percep.target_id < 0) {
        return !n.percep.near_spawn;
    }

    // 교전했었는데 시야 기억이 만료 => 복귀
    if (n.has_last_seen_player && !n.percep.has_recent_sight) {
        return true;
    }

    return false;
}

static bool CondShouldReload(NpcBtContext& ctx)
{
    const SERVER_NPC& n = *ctx.npc;

    if (n.reloading) {
        return true;
    }
    if (n.current_ammo > 0) {
        return false;
    }

    return n.percep.can_see || n.percep.has_recent_sight;
}

static bool CondShouldAttack(NpcBtContext& ctx)
{
    const SERVER_NPC& n = *ctx.npc;

    if (n.percep.can_shoot) return true;

    if (n.state == NPC_STATE_ATTACK || n.state == NPC_STATE_RELOAD)
    {
        if (!n.percep.out_of_attack_range)
            return n.percep.can_see || n.percep.has_recent_sight;
    }
    return false;
}

static bool CondShouldChase(NpcBtContext& ctx)
{
    const SERVER_NPC& n = *ctx.npc;
    return n.percep.can_see || n.percep.has_recent_sight;
}

template <char STATE> static BtStatus ActSelectState(NpcBtContext& ctx)
{
    ChangeNpcState(*ctx.room, *ctx.npc, STATE);
    return BtStatus::Running;
}

uint8_t NpcBehaviorTree::AllocSlot()
{
    // 슬롯이 모자라면 NPC_BT_MAX_COMPOSITES를 늘릴 것
    return m_composite_count++;
}
 
// 조건 + 액션 2단 브랜치 하나를 만든다.
static std::unique_ptr<BtNode> MakeBranch(
    const char* name, uint8_t slot,
    const char* cond_name, BtConditionFn cond,
    const char* act_name, BtActionFn act)
{
    auto seq = std::make_unique<BtReactiveSequence>(name, slot);
    seq->AddChild(std::make_unique<BtCondition>(cond_name, cond));
    seq->AddChild(std::make_unique<BtAction>(act_name, act));
    return seq;
}
 
void NpcBehaviorTree::Build()
{
    m_composite_count = 0;
 
    auto root = std::make_unique<BtReactiveSelector>("NpcRoot", AllocSlot());
 
    // 위에서부터 우선순위가 높다
    root->AddChild(MakeBranch("DieBranch", AllocSlot(),
        "IsDying", &CondIsDying,
        "SelectDie", &ActSelectState<NPC_STATE_DIE>));
 
    root->AddChild(MakeBranch("ReturnBranch", AllocSlot(),
        "ShouldReturn", &CondShouldReturn,
        "SelectReturn", &ActSelectState<NPC_STATE_RETURN>));
 
    root->AddChild(MakeBranch("ReloadBranch", AllocSlot(),
        "ShouldReload", &CondShouldReload,
        "SelectReload", &ActSelectState<NPC_STATE_RELOAD>));
 
    root->AddChild(MakeBranch("AttackBranch", AllocSlot(),
        "ShouldAttack", &CondShouldAttack,
        "SelectAttack", &ActSelectState<NPC_STATE_ATTACK>));
 
    root->AddChild(MakeBranch("ChaseBranch", AllocSlot(),
        "ShouldChase", &CondShouldChase,
        "SelectChase", &ActSelectState<NPC_STATE_RUN>));
 
    // 폴백 — 조건 없음
    root->AddChild(std::make_unique<BtAction>(
        "SelectIdle", &ActSelectState<NPC_STATE_IDLE>));
 
    m_root = std::move(root);
}
 
BtStatus NpcBehaviorTree::Tick(const Room& room, SERVER_NPC& npc, float dt) const
{
    if (!m_root) return BtStatus::Failure;
 
    NpcBtContext ctx;
    ctx.room = &room;
    ctx.npc  = &npc;
    ctx.dt   = dt;
 
    return m_root->Tick(ctx);
}
 
void NpcBehaviorTree::ResetNpc(SERVER_NPC& npc) const
{
    npc.bt_running_child.fill(BT_INVALID_CHILD);
}