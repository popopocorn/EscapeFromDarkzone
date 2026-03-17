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
