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
    if (m_pd3dcbEffectInfo)
    {
        m_pd3dcbEffectInfo->Unmap(0, NULL);
        m_pd3dcbEffectInfo->Release();
    }
}

void CEffect::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
    UINT ncbElementBytes = ((sizeof(EFFECT_INFO) + 255) & ~255);
    m_pd3dcbEffectInfo = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
    m_pd3dcbEffectInfo->Map(0, NULL, (void**)&m_pcbMappedEffectInfo);
}

void CEffect::Play(XMFLOAT3 pos)
{
    SetPosition(pos.x, pos.y + 0.5f, pos.z);
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
    if (nPipelineState != 0) return;

    if (m_pcbMappedEffectInfo)
    {
        m_pcbMappedEffectInfo->fAge = m_fAge;
        m_pcbMappedEffectInfo->fLifeTime = m_fLifeTime;
        m_pcbMappedEffectInfo->fProgress = m_fAge / m_fLifeTime;
    }

    if (m_pEffectShader)
    {
        m_pEffectShader->Render(cmdList, camera, batch, nPipelineState);
        cmdList->SetGraphicsRootConstantBufferView(17, m_pd3dcbEffectInfo->GetGPUVirtualAddress());
    }

    CGameObject::Render(cmdList, batch, nPipelineState, camera);
}