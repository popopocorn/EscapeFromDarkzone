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
    D3D12_GPU_VIRTUAL_ADDRESS m_d3dGpuBufferAddress = 0;
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

    void SetConstantBufferInfo(D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, EFFECT_INFO* mappedPointer)
    {
        m_d3dGpuBufferAddress = gpuAddress;
        m_pcbMappedEffectInfo = mappedPointer;
    }

    bool IsDead() const { return m_bIsDead; }
    void Play(XMFLOAT3 pos);
};