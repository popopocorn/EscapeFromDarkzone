#pragma once
#include <algorithm>
#include "Object.h"

namespace BodyYaw
{
    // 오른손 총기 자세에 맞춰 왼쪽은 더 일찍 하체가 따라오게 한다. 서버와 같은 값이다.
    inline constexpr float MAX_LEFT_UPPER_BODY_YAW_DEG = 55.0f;
    inline constexpr float MAX_RIGHT_UPPER_BODY_YAW_DEG = 70.0f;
    // 정지 회전에서 하체가 상체를 늦게 따라오는 느낌을 줄인다.
    inline constexpr float LOWER_BODY_YAW_RESPONSE = 18.0f;

    inline float NormalizeDegrees(float angle)
    {
        angle = fmodf(angle, 360.0f);
        if (angle < 0.0f) angle += 360.0f;
        if (angle >= 360.0f) angle -= 360.0f;
        return angle;
    }

    inline float ShortestDeltaDegrees(float from, float to)
    {
        float delta = fmodf(to - from, 360.0f);
        if (delta > 180.0f) delta -= 360.0f;
        if (delta < -180.0f) delta += 360.0f;
        return delta;
    }

    inline float ClampUpperYaw(float delta)
    {
        // yaw가 음수면 왼쪽, 양수면 오른쪽이다.
        return std::clamp(delta, -MAX_LEFT_UPPER_BODY_YAW_DEG, MAX_RIGHT_UPPER_BODY_YAW_DEG);
    }

    inline float ConstrainLowerYaw(float upperYaw, float lowerYaw)
    {
        return NormalizeDegrees(upperYaw - ClampUpperYaw(ShortestDeltaDegrees(lowerYaw, upperYaw)));
    }

    // 서버의 상/하체 방향은 유지하고, 화면에서는 한계를 넘기 전에 한 번에 발을 돌린다.
    // 최대 비틀림 각도와 회전 시작 각도를 분리해 한계 자세에서 계속 미끄러지지 않게 한다.
    class StandingTurn
    {
    public:
        void Reset(float lowerYaw)
        {
            m_target = NormalizeDegrees(lowerYaw);
            m_initialized = true;
            m_turning = false;
        }

        float Update(float upperYaw, float lowerYaw, float elapsed)
        {
            if (!std::isfinite(elapsed) || elapsed <= 0.0f) return lowerYaw;
            if (!m_initialized) Reset(lowerYaw);
            const float offset = ShortestDeltaDegrees(lowerYaw, upperYaw);
            if (!m_turning && (offset < -40.0f || offset > 55.0f))
            {
                m_turning = true;
                m_direction = std::copysign(1.0f, offset);
            }
            if (m_turning)
            {
                // 회전 도중에는 조준을 계속 따라가고, 10도까지 풀리면 그 자리에 발을 고정한다.
                if (offset * m_direction <= 10.5f)
                {
                    m_target = lowerYaw;
                    m_turning = false;
                }
                else m_target = NormalizeDegrees(upperYaw - 10.0f * m_direction);
            }
            const float delta = ShortestDeltaDegrees(lowerYaw, m_target);
            const float blend = 1.0f - expf(-LOWER_BODY_YAW_RESPONSE * std::min(elapsed, 0.1f));
            const float next = fabsf(delta) < 0.05f ? m_target : NormalizeDegrees(lowerYaw + delta * blend);
            // 마우스를 아주 빠르게 돌려도 허리가 서버와 같은 최대 각도를 넘지 않게 한다.
            return ConstrainLowerYaw(upperYaw, next);
        }

    private:
        float m_target = 0.0f;
        bool m_initialized = false;
        bool m_turning = false;
        float m_direction = 1.0f;
    };

    inline std::array<CGameObject*, 3> CacheBones(CGameObject* root)
    {
        const char* names[] = { "mixamorig:Spine", "mixamorig:Spine1", "mixamorig:Spine2" };
        std::array<CGameObject*, 3> bones{};
        if (root)
            for (size_t i = 0; i < bones.size(); ++i) bones[i] = root->FindFrame(names[i]);
        return bones;
    }

    // 애니메이션의 로컬 행렬을 평가한 뒤 프레임마다 한 번 적용한다.
    inline void ApplyTorsoYaw(const std::array<CGameObject*, 3>& bones, float upperYaw, float lowerYaw)
    {
        const float delta = ClampUpperYaw(ShortestDeltaDegrees(lowerYaw, upperYaw));
        if (fabsf(delta) < 0.0001f) return;
        constexpr float weights[] = { 0.4f, 0.35f, 0.25f };
        float totalWeight = 0.0f;
        for (size_t i = 0; i < bones.size(); ++i)
            if (bones[i]) totalWeight += weights[i];
        if (totalWeight <= 0.0f) return;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            CGameObject* bone = bones[i];
            if (!bone) continue;
            // 원래 본의 축과 스케일을 보존하고 월드 수직축으로 회전한다.
            XMMATRIX world = XMLoadFloat4x4(&bone->m_xmf4x4World);
            const XMVECTOR position = world.r[3];
            world.r[3] = XMVectorSet(0, 0, 0, 1);
            world = world * XMMatrixRotationY(XMConvertToRadians(delta * weights[i] / totalWeight));
            world.r[3] = position;
            const XMMATRIX parent = bone->m_pParent
                ? XMLoadFloat4x4(&bone->m_pParent->m_xmf4x4World) : XMMatrixIdentity();
            XMStoreFloat4x4(&bone->m_xmf4x4ToParent, world * XMMatrixInverse(nullptr, parent));
            bone->UpdateTransform(bone->m_pParent ? &bone->m_pParent->m_xmf4x4World : nullptr);
        }
    }
}
