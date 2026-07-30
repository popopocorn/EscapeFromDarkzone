#pragma once

#include "stdafx.h"
#include "Effect.h"

class CTexture;

constexpr UINT PARTICLE_SELECTED_FRAME_CAPACITY = 16;

enum class ParticleTextureID : UINT
{
	EXPLOSION = 0,
	SPARK_RIFLE_SMG,
	SPARK_SHOTGUN,

	COUNT
};

enum class ParticleBlendMode : UINT
{
	ALPHA = 0,
	ADDITIVE
};

enum class ParticleBillboardMode : UINT
{
	CAMERA_FACING = 0,
	VELOCITY_ALIGNED
};

enum class ParticleFrameMode : UINT
{
	FIXED_FRAME = 0,
	SEQUENTIAL,
	RANDOM_SELECTED
};

struct ParticleAtlasDesc
{
	UINT textureWidth = 1;
	UINT textureHeight = 1;

	UINT columns = 1;
	UINT rows = 1;

	UINT frameWidth = 1;
	UINT frameHeight = 1;

	UINT borderX = 0;
	UINT borderY = 0;

	UINT spacingX = 0;
	UINT spacingY = 0;

	UINT validFrameCount = 1;
};

struct ParticleEmitterDesc
{
	ParticleTextureID textureId = ParticleTextureID::EXPLOSION;
	ParticleBlendMode blendMode = ParticleBlendMode::ALPHA;
	ParticleBillboardMode billboardMode = ParticleBillboardMode::CAMERA_FACING;
	ParticleFrameMode frameMode = ParticleFrameMode::FIXED_FRAME;

	UINT burstCount = 0;

	float spawnDelayMin = 0.0f;
	float spawnDelayMax = 0.0f;

	float lifeTimeMin = 1.0f;
	float lifeTimeMax = 1.0f;

	float speedMin = 0.0f;
	float speedMax = 0.0f;

	XMFLOAT3 direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	float coneAngleDegrees = 0.0f;

	XMFLOAT3 acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);

	XMFLOAT2 startSize = XMFLOAT2(1.0f, 1.0f);
	XMFLOAT2 endSize = XMFLOAT2(1.0f, 1.0f);

	float sizeScaleMin = 1.0f;
	float sizeScaleMax = 1.0f;

	XMFLOAT4 startColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4 endColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

	float rotationMin = 0.0f;
	float rotationMax = 0.0f;

	float angularVelocityMin = 0.0f;
	float angularVelocityMax = 0.0f;

	UINT firstFrame = 0;
	UINT frameCount = 1;
	UINT loopAnimation = 0;

	UINT selectedFrameCount = 0;
	UINT selectedFrames[PARTICLE_SELECTED_FRAME_CAPACITY] = {};
};

struct ParticleEffectDesc
{
	EffectID id = EffectID::NONE;
	std::vector<ParticleEmitterDesc> emitters;
};

class ParticleResource
{
public:
	ParticleResource() = default;
	~ParticleResource();

	bool Load(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	void ReleaseUploadBuffers();
	void Release();

	bool IsLoaded() const { return m_bLoaded; }

	const ParticleEffectDesc* GetEffectDesc(EffectID effectId) const;
	CTexture* GetTexture(ParticleTextureID textureId) const;
	const ParticleAtlasDesc* GetAtlasDesc(ParticleTextureID textureId) const;

private:
	bool LoadTexture(ParticleTextureID textureId, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const wchar_t* texturePath, const ParticleAtlasDesc& atlasDesc);

	void BuildGrenadeEffectDesc();

private:
	bool m_bLoaded = false;

	std::unordered_map<ParticleTextureID, std::unique_ptr<CTexture>> m_Textures;
	std::unordered_map<ParticleTextureID, ParticleAtlasDesc> m_AtlasDescs;
	std::unordered_map<EffectID, ParticleEffectDesc> m_EffectDescs;
};