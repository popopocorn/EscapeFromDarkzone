// Effect.h
#pragma once

#include "Object.h"

class CEffectShader;

enum EFFECT_TYPE
{
    EFFECT_BOMB = 0,

    EFFECT_SPARK_RIFLE_SMG,
    EFFECT_SPARK_SHOTGUN,
    EFFECT_SPARK_PISTOL,

    EFFECT_BLOOD,
    EFFECT_MAX,

    EFFECT_SPARK = EFFECT_SPARK_RIFLE_SMG
};
enum class EffectID : unsigned char
{
    NONE = 0,

    GRENADE_EXPLOSION,
    SPARK,

    BLOOD,
    HIT,

    SPARK_SHOTGUN,
    SPARK_PISTOL
};
struct EffectSpawnDesc
{
    EffectID id = EffectID::NONE;

    XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 direction = XMFLOAT3(0.0f, 0.0f, 1.0f);

    int ownerId = 0;
    float value = 0.0f;
};

struct EFFECT_INFO
{
    XMFLOAT3 vPosition;
    float    fProgress;

    XMFLOAT2 vSize;
    float    padding0[2];

    XMFLOAT3 vRight;
    float    padding1;

    XMFLOAT3 vUp;
    float    padding2;

    XMFLOAT4 vColor;
};
class CEffect : public CGameObject
{
protected:
    EFFECT_TYPE m_eEffectType;

    float m_fAge = 0.0f;
    float m_fLifeTime = 1.0f;
    bool m_bIsDead = true;

    CEffectShader* m_pEffectShader = nullptr;

private:
    XMFLOAT3 m_xmf3Position = XMFLOAT3(0, 0, 0);
    XMFLOAT3 m_xmf3Right = XMFLOAT3(1, 0, 0);
    XMFLOAT3 m_xmf3Up = XMFLOAT3(0, 1, 0);

public:
    CEffect(EFFECT_TYPE type, float lifeTime = 1.0f);
    virtual ~CEffect();

    virtual void Animate(float fTimeElapsed) override;

    void SetEffectShader(CEffectShader* shader){ m_pEffectShader = shader; }

    float GetProgress() const { return m_fAge / m_fLifeTime; }

    XMFLOAT3 GetPosition() const { return m_xmf3Position; }
    const XMFLOAT3& GetRight() const { return m_xmf3Right; }
    const XMFLOAT3& GetUp() const { return m_xmf3Up; }

    bool IsDead() const { return m_bIsDead; }
    void Play(const XMFLOAT3& pos, const XMFLOAT3& right, const XMFLOAT3& up);
};