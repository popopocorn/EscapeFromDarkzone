// Effect.cpp
#include "stdafx.h"
#include "Effect.h"
#include "EffectShader.h"

CEffect::CEffect(EFFECT_TYPE type, float lifeTime)
{
    m_eEffectType = type;
    m_fLifeTime = lifeTime;
    m_fAge = 0.0f;
    m_bIsDead = true;
    m_pEffectShader = nullptr;
}

CEffect::~CEffect()
{
}


void CEffect::Play(const XMFLOAT3& pos, const XMFLOAT3& right, const XMFLOAT3& up)
{
    m_xmf3Position = pos;
    m_xmf3Right = right;
    m_xmf3Up = up;

    m_fAge = 0.0f;
    m_bIsDead = false;
}

void CEffect::Animate(float dt)
{
    if (m_bIsDead) return;

    CGameObject::Animate(dt);

    m_fAge += dt;
    if (m_fAge >= m_fLifeTime)
        m_bIsDead = true;
}
