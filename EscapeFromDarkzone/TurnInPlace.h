#pragma once
#include "BodyYaw.h"

// 새 애니메이션 없이, 제자리 회전할 때만 발을 번갈아 들고 내려놓는다.
namespace BodyYaw
{
    class TurnInPlace
    {
    public:
        void Cache(CGameObject* root)
        {
            m_root = root;
            m_hips = root ? root->FindFrame("mixamorig:Hips") : nullptr;
            const char* names[2][3] = {
                { "mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot" },
                { "mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot" }
            };
            for (int side = 0; side < 2; ++side)
                for (int joint = 0; joint < 3; ++joint)
                    m_bones[side][joint] = root ? root->FindFrame(names[side][joint]) : nullptr;
            Reset();
        }

        void Reset()
        {
            m_initialized = false;
            m_engaged = false;
            m_stepping = -1;
            m_progress = 0.0f;
            m_pelvisDrop = 0.0f;
        }

        // 애니메이션과 상체 회전 평가 뒤 한 번 호출한다. 이동/사망 시에는 원래 자세를 쓴다.
        void Update(float elapsed, float lowerYaw, bool standing)
        {
            if (!standing) { Reset(); return; }
            for (const auto& leg : m_bones)
                for (const auto* bone : leg)
                    if (!bone) return;
            if (!m_hips) return;
            if (!std::isfinite(elapsed) || elapsed <= 0.0f)
            {
                // 일시 정지 때도 이미 디딘 발의 보정은 유지하되 동작 시간은 진행하지 않는다.
                if (m_initialized && m_engaged)
                {
                    LowerPelvis(m_pelvisDrop);
                    for (int side = 0; side < 2; ++side) SolveLeg(side, m_display[side]);
                }
                return;
            }

            XMFLOAT4X4 authored[2] = { m_bones[0][2]->m_xmf4x4World, m_bones[1][2]->m_xmf4x4World };
            const XMFLOAT3 rootPosition = m_root->GetPosition();
            if (!m_initialized)
            {
                Capture(authored, rootPosition, lowerYaw);
                return;
            }
            const XMVECTOR drift = XMLoadFloat3(&rootPosition) - XMLoadFloat3(&m_origin);
            if (XMVectorGetX(XMVector3LengthSq(drift)) > m_legLength * m_legLength * 0.04f)
            {
                // 순간 이동/큰 서버 위치 보정 뒤 옛 월드 발 위치에 끌려가지 않게 한다.
                Capture(authored, rootPosition, lowerYaw);
                return;
            }

            const float turn = ShortestDeltaDegrees(m_lastYaw, lowerYaw);
            m_lastYaw = lowerYaw;
            if (!m_engaged)
            {
                if (fabsf(turn) < 0.02f)
                {
                    Capture(authored, rootPosition, lowerYaw);
                    return;
                }
                m_engaged = true;
                m_nextFoot = turn > 0.0f ? 1 : 0;
            }

            bool needsStep[2]{};
            for (int side = 0; side < 2; ++side)
            {
                const XMVECTOR difference = Position(authored[side]) - Position(m_planted[side]);
                const float distance = XMVectorGetX(XMVector3LengthSq(XMVectorSetY(difference, 0)));
                needsStep[side] = fabsf(ShortestDeltaDegrees(m_plantedYaw[side], lowerYaw)) > 4.0f
                    || distance > m_legLength * m_legLength * 0.0036f;
            }
            if (m_stepping < 0 && (needsStep[0] || needsStep[1]))
            {
                m_stepping = needsStep[m_nextFoot] ? m_nextFoot : 1 - m_nextFoot;
                m_start = m_planted[m_stepping];
                m_progress = 0.0f;
                const float turnSize = fabsf(ShortestDeltaDegrees(m_plantedYaw[m_stepping], lowerYaw));
                // 낮고 짧게 디뎌 발이 공중에 오래 남지 않게 한다.
                m_duration = 0.14f + 0.04f * std::min(turnSize / 90.0f, 1.0f);
            }

            m_display = m_planted;
            if (m_stepping >= 0)
            {
                m_progress = std::min(1.0f, m_progress + std::min(elapsed, 0.1f) / m_duration);
                const float t = m_progress * m_progress * (3.0f - 2.0f * m_progress);
                XMVECTOR startScale, startRotation, startPosition, endScale, endRotation, endPosition;
                XMMatrixDecompose(&startScale, &startRotation, &startPosition, XMLoadFloat4x4(&m_start));
                XMMatrixDecompose(&endScale, &endRotation, &endPosition, XMLoadFloat4x4(&authored[m_stepping]));
                XMVECTOR position = XMVectorLerp(startPosition, endPosition, t);
                const float lift = sinf(XM_PI * m_progress) * m_legLength * 0.05f;
                position = XMVectorSetY(position, XMVectorGetY(position) + std::max(lift, 0.0f));
                XMStoreFloat4x4(&m_display[m_stepping], XMMatrixAffineTransformation(endScale,
                    XMVectorZero(), XMQuaternionSlerp(startRotation, endRotation, t), position));
                if (m_progress >= 1.0f)
                {
                    m_planted[m_stepping] = m_display[m_stepping] = authored[m_stepping];
                    m_plantedYaw[m_stepping] = lowerYaw;
                    m_nextFoot = 1 - m_stepping;
                    m_stepping = -1;
                }
            }
            const float requiredDrop = RequiredPelvisDrop();
            const float settle = 1.0f - expf(-20.0f * std::min(elapsed, 0.1f));
            m_pelvisDrop = std::max(requiredDrop, m_pelvisDrop + (requiredDrop - m_pelvisDrop) * settle);
            LowerPelvis(m_pelvisDrop);
            for (int side = 0; side < 2; ++side) SolveLeg(side, m_display[side]);
        }

    private:
        static XMVECTOR Position(const XMFLOAT4X4& matrix)
        {
            return XMVectorSet(matrix._41, matrix._42, matrix._43, 0);
        }

        void Capture(const XMFLOAT4X4 (&authored)[2], const XMFLOAT3& position, float yaw)
        {
            m_legLength = XMVectorGetX(XMVector3Length(Position(m_bones[0][0]->m_xmf4x4World)
                - Position(m_bones[0][1]->m_xmf4x4World)))
                + XMVectorGetX(XMVector3Length(Position(m_bones[0][1]->m_xmf4x4World)
                    - Position(m_bones[0][2]->m_xmf4x4World)));
            if (m_legLength < 0.0001f) { Reset(); return; }
            for (int side = 0; side < 2; ++side)
            {
                m_planted[side] = m_display[side] = authored[side];
                m_plantedYaw[side] = yaw;
            }
            m_origin = position;
            m_lastYaw = yaw;
            m_initialized = true;
            m_engaged = false;
            m_stepping = -1;
            m_pelvisDrop = 0.0f;
        }

        float RequiredPelvisDrop() const
        {
            float drop = 0.0f;
            for (int side = 0; side < 2; ++side)
            {
                const XMVECTOR hip = Position(m_bones[side][0]->m_xmf4x4World);
                const XMVECTOR knee = Position(m_bones[side][1]->m_xmf4x4World);
                const XMVECTOR ankle = Position(m_bones[side][2]->m_xmf4x4World);
                const float reach = XMVectorGetX(XMVector3Length(knee - hip))
                    + XMVectorGetX(XMVector3Length(ankle - knee)) - 0.0001f;
                const XMVECTOR difference = hip - Position(m_display[side]);
                const XMVECTOR horizontal = XMVectorSetY(difference, 0);
                const float height = sqrtf(std::max(0.0f, reach * reach
                    - XMVectorGetX(XMVector3LengthSq(horizontal))));
                drop = std::max(drop, XMVectorGetY(difference) - height);
            }
            return std::clamp(drop, 0.0f, m_legLength * 0.12f);
        }

        void LowerPelvis(float drop)
        {
            // 발까지 닿는 거리만큼만 골반을 낮춰 두 발이 동시에 뜨는 현상을 막는다.
            if (drop <= 0.0f) return;
            XMMATRIX world = XMLoadFloat4x4(&m_hips->m_xmf4x4World);
            world.r[3] = XMVectorSetY(world.r[3], XMVectorGetY(world.r[3]) - drop);
            const XMMATRIX parent = m_hips->m_pParent
                ? XMLoadFloat4x4(&m_hips->m_pParent->m_xmf4x4World) : XMMatrixIdentity();
            XMStoreFloat4x4(&m_hips->m_xmf4x4ToParent, world * XMMatrixInverse(nullptr, parent));
            m_hips->UpdateTransform(m_hips->m_pParent ? &m_hips->m_pParent->m_xmf4x4World : nullptr);
        }

        static void RotateJoint(CGameObject* joint, XMVECTOR from, XMVECTOR to)
        {
            if (XMVectorGetX(XMVector3LengthSq(from)) < 1e-10f || XMVectorGetX(XMVector3LengthSq(to)) < 1e-10f) return;
            from = XMVector3Normalize(from); to = XMVector3Normalize(to);
            const float cosine = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
            XMVECTOR axis = XMVector3Cross(from, to);
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-10f)
            {
                if (cosine > 0.0f) return;
                axis = XMVector3Cross(from, XMVectorSet(1, 0, 0, 0));
                if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-10f)
                    axis = XMVector3Cross(from, XMVectorSet(0, 1, 0, 0));
            }
            XMMATRIX world = XMLoadFloat4x4(&joint->m_xmf4x4World);
            const XMVECTOR position = world.r[3];
            world.r[3] = XMVectorSet(0, 0, 0, 1);
            world = world * XMMatrixRotationAxis(XMVector3Normalize(axis), acosf(cosine));
            world.r[3] = position;
            const XMMATRIX parent = joint->m_pParent ? XMLoadFloat4x4(&joint->m_pParent->m_xmf4x4World) : XMMatrixIdentity();
            XMStoreFloat4x4(&joint->m_xmf4x4ToParent, world * XMMatrixInverse(nullptr, parent));
            joint->UpdateTransform(joint->m_pParent ? &joint->m_pParent->m_xmf4x4World : nullptr);
        }

        void SolveLeg(int side, const XMFLOAT4X4& target)
        {
            auto* hip = m_bones[side][0]; auto* knee = m_bones[side][1]; auto* ankle = m_bones[side][2];
            const XMVECTOR a = Position(hip->m_xmf4x4World), b = Position(knee->m_xmf4x4World), c = Position(ankle->m_xmf4x4World);
            const XMVECTOR goal = Position(target);
            const float upper = XMVectorGetX(XMVector3Length(b - a));
            const float lower = XMVectorGetX(XMVector3Length(c - b));
            const float distance = XMVectorGetX(XMVector3Length(goal - a));
            if (upper < 1e-5f || lower < 1e-5f || distance < 1e-5f) return;
            const XMVECTOR direction = XMVector3Normalize(goal - a);
            const float reach = std::clamp(distance, fabsf(upper - lower) + 1e-5f, upper + lower - 1e-5f);
            XMVECTOR pole = (b - a) - direction * XMVector3Dot(b - a, direction);
            if (XMVectorGetX(XMVector3LengthSq(pole)) < 1e-8f)
            {
                // 무릎이 일직선이면 캐릭터의 앞쪽으로 굽혀 뒤집힘을 막는다.
                const XMVECTOR forward = XMLoadFloat4x4(&m_root->m_xmf4x4World).r[2];
                pole = forward - direction * XMVector3Dot(forward, direction);
                if (XMVectorGetX(XMVector3LengthSq(pole)) < 1e-8f)
                    pole = XMVector3Cross(direction, XMVectorSet(1, 0, 0, 0));
            }
            const float along = (upper * upper - lower * lower + reach * reach) / (2.0f * reach);
            const XMVECTOR bentKnee = a + direction * along
                + XMVector3Normalize(pole) * sqrtf(std::max(0.0f, upper * upper - along * along));
            RotateJoint(hip, b - a, bentKnee - a);
            const XMVECTOR kneeNow = Position(knee->m_xmf4x4World), ankleNow = Position(ankle->m_xmf4x4World);
            RotateJoint(knee, ankleNow - kneeNow, a + direction * reach - kneeNow);
            // 발목 회전도 고정/보간해서 접지한 발이 바닥에서 비틀리지 않게 한다.
            XMMATRIX world = XMLoadFloat4x4(&target);
            world.r[3] = XMLoadFloat4x4(&ankle->m_xmf4x4World).r[3];
            const XMMATRIX parent = XMLoadFloat4x4(&knee->m_xmf4x4World);
            XMStoreFloat4x4(&ankle->m_xmf4x4ToParent, world * XMMatrixInverse(nullptr, parent));
            ankle->UpdateTransform(&knee->m_xmf4x4World);
        }

        CGameObject* m_root = nullptr;
        CGameObject* m_hips = nullptr;
        std::array<std::array<CGameObject*, 3>, 2> m_bones{};
        std::array<XMFLOAT4X4, 2> m_planted{}, m_display{};
        std::array<float, 2> m_plantedYaw{};
        XMFLOAT4X4 m_start{};
        XMFLOAT3 m_origin{};
        float m_lastYaw = 0.0f, m_legLength = 0.0f, m_progress = 0.0f, m_duration = 0.16f;
        float m_pelvisDrop = 0.0f;
        int m_stepping = -1, m_nextFoot = 0;
        bool m_initialized = false, m_engaged = false;
    };
}
