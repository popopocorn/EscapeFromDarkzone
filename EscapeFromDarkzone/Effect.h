// Effect.h
#pragma once

#include "Object.h"

class CEffectShader;

struct EFFECT_INFO
{
    float fAge;
    float fLifeTime;
    float fProgress;
    float padding;
};

class CEffect : public CGameObject
{
protected:
    float m_fAge = 0.0f;
    float m_fLifeTime = 1.0f;
    bool m_bIsDead = true;

    CEffectShader* m_pEffectShader = nullptr;

public:
    ID3D12Resource* m_pd3dcbEffectInfo = nullptr;
    EFFECT_INFO* m_pcbMappedEffectInfo = nullptr;

public:
    CEffect(float lifeTime = 1.0f);
    virtual ~CEffect();

    virtual void Animate(float fTimeElapsed) override;

    virtual void Render(
        ID3D12GraphicsCommandList* cmdList,
        bool batch = false,
        int nPipelineState = 0,
        CCamera* camera = NULL) override;

    void SetEffectShader(CEffectShader* shader)
    {
        m_pEffectShader = shader;
    }

    void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

    bool IsDead() const { return m_bIsDead; }
    void Play(XMFLOAT3 pos);
};