#pragma once
#include "Object.h"
#include "ResourceManager.h"

// Grip points measured in the exported model's bind-world space (metres).
// Convert through the imported root: M4/shotgun carry a 100x export scale.
namespace WeaponAttachment
{
    struct Profile
    {
        XMFLOAT3 rotation;
        float scale;
        XMFLOAT3 modelGrip;
        XMFLOAT3 handGrip;
    };

    inline Profile GetProfile(ModelName model)
    {
        const XMFLOAT3 riflePalm(0.00201356f, 0.16223830f, -0.00476833f);
        switch (model)
        {
        case ModelName::PISTOL:
            // Pistol barrel is +Y in export space, unlike the long guns (-X).
            // Fit against pistol-idle hand rotation, not the rifle's 90-degree offset.
            return { {-0.41187356f,10.19121844f,179.60951064f}, 1.0f,
                {0.0673f,-0.0429f,0}, {0.00266981f,0.15691298f,0.00521841f} };
        case ModelName::SMG:
            return { {90,0,0}, 0.99f, {0.028f,-0.045f,0}, riflePalm };
        case ModelName::SHOTGUN:
            // Grip the wrist of the stock behind the trigger, not the butt.
            // The old rearward point also put the pump beyond the support arm's reach.
            return { {180,0,90}, 1.1f, {0.12f,-0.005f,0}, riflePalm };
        default:
            return { {90,0,0}, 1.1f, {0.157f,0.055f,0}, riflePalm };
        }
    }

    inline XMFLOAT4X4 BuildLocal(const XMFLOAT4X4& importedRoot, ModelName model)
    {
        const Profile profile = GetProfile(model);
        const XMMATRIX imported = XMLoadFloat4x4(&importedRoot);
        const XMVECTOR gripInMesh = XMVector3TransformCoord(XMLoadFloat3(&profile.modelGrip),
            XMMatrixInverse(nullptr, imported));
        XMMATRIX local = XMMatrixRotationRollPitchYaw(XMConvertToRadians(profile.rotation.x),
            XMConvertToRadians(profile.rotation.y), XMConvertToRadians(profile.rotation.z))
            * XMMatrixScaling(profile.scale, profile.scale, profile.scale) * imported;
        local.r[3] = XMVectorSet(0,0,0,1);
        local.r[3] = XMVectorSetW(XMVectorSubtract(XMLoadFloat3(&profile.handGrip),
            XMVector3TransformCoord(gripInMesh, local)), 1.0f);
        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, local);
        return result;
    }

    inline void RotateJoint(CGameObject* joint, XMVECTOR from, XMVECTOR to)
    {
        if (XMVectorGetX(XMVector3LengthSq(from)) < 1e-10f ||
            XMVectorGetX(XMVector3LengthSq(to)) < 1e-10f) return;
        from = XMVector3Normalize(from);
        to = XMVector3Normalize(to);
        const float cosine = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
        XMVECTOR axis = XMVector3Cross(from, to);
        if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-10f)
        {
            if (cosine > 0.0f) return;
            axis = XMVector3Cross(from, XMVectorSet(1,0,0,0));
            if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-10f)
                axis = XMVector3Cross(from, XMVectorSet(0,1,0,0));
        }
        XMMATRIX world = XMLoadFloat4x4(&joint->m_xmf4x4World);
        const XMVECTOR position = world.r[3];
        world.r[3] = XMVectorSet(0,0,0,1);
        world = world * XMMatrixRotationAxis(XMVector3Normalize(axis), acosf(cosine));
        world.r[3] = position;
        const XMMATRIX parent = joint->m_pParent
            ? XMLoadFloat4x4(&joint->m_pParent->m_xmf4x4World) : XMMatrixIdentity();
        XMStoreFloat4x4(&joint->m_xmf4x4ToParent, world * XMMatrixInverse(nullptr, parent));
        joint->UpdateTransform(joint->m_pParent ? &joint->m_pParent->m_xmf4x4World : nullptr);
    }

    // Analytic two-bone IK: keep the authored elbow side and hand orientation.
    // Unreachable targets are clamped; arm length and finger animation never change.
    inline void SolveSupportHand(CGameObject* upper, CGameObject* forearm,
        CGameObject* hand, CGameObject* grip, bool pistol = false)
    {
        if (!upper || !forearm || !hand || !grip) return;
        const XMMATRIX handWorld = XMLoadFloat4x4(&hand->m_xmf4x4World);
        const XMVECTOR a = XMLoadFloat4x4(&upper->m_xmf4x4World).r[3];
        const XMVECTOR b = XMLoadFloat4x4(&forearm->m_xmf4x4World).r[3];
        const XMVECTOR c = handWorld.r[3];
        const XMVECTOR palm = pistol ? XMVectorSet(0.00025833f,0.16026240f,0.00349642f,0)
            : XMVectorSet(-0.00035033f,0.16371045f,-0.00199699f,0);
        const XMMATRIX gripWorld = XMLoadFloat4x4(&grip->m_xmf4x4World);
        // The pistol support palm cups the outside of the firing hand, not
        // the grip centre. Its local +X points toward the character's left.
        const XMVECTOR supportOffset = pistol ? XMVector3TransformNormal(XMVectorSet(0.025f,0,0,0),gripWorld) : XMVectorZero();
        const XMVECTOR target = XMVectorSubtract(gripWorld.r[3] + supportOffset,
            XMVector3TransformNormal(palm, handWorld));
        const float upperLength = XMVectorGetX(XMVector3Length(b-a));
        const float lowerLength = XMVectorGetX(XMVector3Length(c-b));
        const float distance = XMVectorGetX(XMVector3Length(target-a));
        if (upperLength < 1e-5f || lowerLength < 1e-5f || distance < 1e-5f) return;
        const float reach = std::clamp(distance, fabsf(upperLength-lowerLength)+1e-5f,
            upperLength+lowerLength-1e-5f);
        const XMVECTOR direction = XMVector3Normalize(target-a);
        XMVECTOR pole = (b-a) - direction * XMVector3Dot(b-a,direction);
        if (XMVectorGetX(XMVector3LengthSq(pole)) < 1e-10f)
        {
            pole = XMVector3Cross(direction, XMVectorSet(0,1,0,0));
            if (XMVectorGetX(XMVector3LengthSq(pole)) < 1e-10f)
                pole = XMVector3Cross(direction, XMVectorSet(1,0,0,0));
        }
        const float along = (upperLength*upperLength-lowerLength*lowerLength+reach*reach)/(2*reach);
        const float height = sqrtf(std::max(0.0f,upperLength*upperLength-along*along));
        const XMVECTOR elbow = a + direction*along + XMVector3Normalize(pole)*height;
        const XMVECTOR wrist = a + direction*reach;
        RotateJoint(upper,b-a,elbow-a);
        const XMVECTOR currentElbow = XMLoadFloat4x4(&forearm->m_xmf4x4World).r[3];
        const XMVECTOR currentWrist = XMLoadFloat4x4(&hand->m_xmf4x4World).r[3];
        RotateJoint(forearm,currentWrist-currentElbow,wrist-currentElbow);
        XMMATRIX finalHand = handWorld;
        finalHand.r[3] = XMLoadFloat4x4(&hand->m_xmf4x4World).r[3];
        const XMMATRIX parent = hand->m_pParent
            ? XMLoadFloat4x4(&hand->m_pParent->m_xmf4x4World) : XMMatrixIdentity();
        XMStoreFloat4x4(&hand->m_xmf4x4ToParent, finalHand * XMMatrixInverse(nullptr,parent));
        hand->UpdateTransform(hand->m_pParent ? &hand->m_pParent->m_xmf4x4World : nullptr);
    }
}
