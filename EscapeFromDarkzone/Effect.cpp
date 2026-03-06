//Effect.cpp
#include "stdafx.h"
#include "Effect.h"

CEffect::CEffect(float fLifeTime) : CGameObject(1)
{
    m_fLifeTime = fLifeTime;
    m_fAge = 0.0f;
    m_bIsDead = false;
    m_pEffectShader = nullptr;
}

CEffect::~CEffect()
{
}

void CEffect::Animate(float fTimeElapsed)
{
    CGameObject::Animate(fTimeElapsed);

    m_fAge += fTimeElapsed;
    if (m_fAge >= m_fLifeTime)
    {
        m_bIsDead = true;
    }
}

void CEffect::Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera)
{
    if (m_bIsDead) return;

    if (nPipelineState != 0) return;
    if (m_pEffectShader == nullptr) __debugbreak();
    if (m_pEffectShader)
    {
        m_pEffectShader->m_fAge = this->m_fAge;
        m_pEffectShader->m_fLifeTime = this->m_fLifeTime;

        m_pEffectShader->UpdateShaderVariables(pd3dCommandList);

        m_pEffectShader->Render(pd3dCommandList, pCamera, batch, nPipelineState);
    }

    CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);
}