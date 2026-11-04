#pragma once
#include <algorithm>
#include "Object.h"

namespace BodyYaw
{
    // 오른손 총기 자세에 맞춰 왼쪽은 더 일찍 하체가 따라오게 한다. 서버와 같은 값이다.
    inline constexpr float MAX_LEFT_UPPER_BODY_YAW_DEG = 55.0f;
    inline constexpr float MAX_RIGHT_UPPER_BODY_YAW_DEG = 70.0f;

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
