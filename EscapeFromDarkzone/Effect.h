// Effect.h
#pragma once

#include "Object.h"

class CEffectShader;

struct EFFECT_INFO
{
    XMFLOAT3 vPosition;
    float fProgress;
};
class CEffect : public CGameObject
{
protected:
    float m_fAge = 0.0f;
    float m_fLifeTime = 1.0f;
    bool m_bIsDead = true;

    CEffectShader* m_pEffectShader = nullptr;

public:

public:
    CEffect(float lifeTime = 1.0f);
    virtual ~CEffect();

    virtual void Animate(float fTimeElapsed) override;

    void SetEffectShader(CEffectShader* shader){ m_pEffectShader = shader; }

    float GetProgress() const { return m_fAge / m_fLifeTime; }
    XMFLOAT3 GetPosition() const { return XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43); }

    bool IsDead() const { return m_bIsDead; }
    void Play(XMFLOAT3 pos);
};