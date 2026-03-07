//Effect.h
#pragma once
#include "Object.h"
#include "EffectShader.h"

class CEffect : public CGameObject
{
protected:
    float m_fAge;
    float m_fLifeTime;
    bool  m_bIsDead;

    CEffectShader* m_pEffectShader;
public:
    CEffect(float fLifeTime = 1.0f);
    virtual ~CEffect();

    virtual void Animate(float fTimeElapsed) override;
    virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch = false, int nPipelineState = 0, CCamera* pCamera = NULL) override;
    void SetEffectShader(CEffectShader* pShader) { m_pEffectShader = pShader; }

    bool IsDead() const { return m_bIsDead; }
    float GetProgress() const { return (m_fLifeTime > 0.0f) ? (m_fAge / m_fLifeTime) : 1.0f; }
    
    void Play(XMFLOAT3 pos);
};

