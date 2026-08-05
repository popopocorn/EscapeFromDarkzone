#pragma once

#include "stdafx.h"

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