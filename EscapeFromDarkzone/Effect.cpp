#include "stdafx.h"
#include "Effect.h"

CEffect::CEffect(float fLifeTime) : CGameObject()
{
    m_fAge = 0.0f;
    m_fLifeTime = fLifeTime;
    m_bIsDead = false;
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