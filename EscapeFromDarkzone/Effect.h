#pragma once
#include "Object.h"

class CEffect : public CGameObject
{
protected:
    float m_fAge;
    float m_fLifeTime;
    bool  m_bIsDead;

public:
    CEffect(float fLifeTime = 1.0f);
    virtual ~CEffect();

    virtual void Animate(float fTimeElapsed) override;

    bool IsDead() const { return m_bIsDead; }

    float GetProgress() const { return (m_fLifeTime > 0.0f) ? (m_fAge / m_fLifeTime) : 1.0f; }
};

