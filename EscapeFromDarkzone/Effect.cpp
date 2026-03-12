// Effect.cpp
#include "stdafx.h"
#include "Effect.h"
#include "EffectShader.h"

CEffect::CEffect(float lifeTime) : CGameObject(1)
{
    m_fLifeTime = lifeTime;
    m_fAge = 0.0f;
    m_bIsDead = true;
    m_pEffectShader = nullptr;
}

CEffect::~CEffect()
{
}


void CEffect::Play(XMFLOAT3 pos)
{
    SetPosition(pos.x, pos.y, pos.z);
    m_fAge = 0.0f;
    m_bIsDead = false;

    UpdateTransform(NULL);
}

void CEffect::Animate(float dt)
{
    if (m_bIsDead) return;

    CGameObject::Animate(dt);

    m_fAge += dt;
    if (m_fAge >= m_fLifeTime)
        m_bIsDead = true;
}

void CEffect::Render(ID3D12GraphicsCommandList* cmdList, bool batch, int nPipelineState, CCamera* camera)
{
    if (m_bIsDead) return;

    if (m_pcbMappedEffectInfo)
    {
        m_pcbMappedEffectInfo->fAge = m_fAge;
        m_pcbMappedEffectInfo->fLifeTime = m_fLifeTime;
        m_pcbMappedEffectInfo->fProgress = m_fAge / m_fLifeTime;
    }

    if (m_pEffectShader)
    {
        m_pEffectShader->Render(cmdList, camera, false, nPipelineState);
    }

    if (!batch && m_pEffectShader)
    {
        m_pEffectShader->Render(cmdList, camera, false, nPipelineState);
    }

    cmdList->SetGraphicsRootConstantBufferView(17, m_d3dGpuBufferAddress);

    CGameObject::Render(cmdList, batch, nPipelineState, camera);
}