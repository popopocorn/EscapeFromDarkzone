#include "stdafx.h"
#include "Object.h"
#include "ParticleResource.h"

ParticleResource::~ParticleResource()
{
	Release();
}

bool ParticleResource::Load(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!pd3dDevice || !pd3dCommandList)
	{
		OutputDebugStringW(L"[ParticleResource] Load failed. Device or command list is null.\n");
		return false;
	}

	if (m_bLoaded)
	{
		OutputDebugStringW(L"[ParticleResource] Resources are already loaded.\n");
		return true;
	}

	Release();

	ParticleAtlasDesc explosionAtlasDesc;
	explosionAtlasDesc.textureWidth = 3584;
	explosionAtlasDesc.textureHeight = 3072;
	explosionAtlasDesc.columns = 7;
	explosionAtlasDesc.rows = 6;
	explosionAtlasDesc.frameWidth = 512;
	explosionAtlasDesc.frameHeight = 512;
	explosionAtlasDesc.borderX = 0;
	explosionAtlasDesc.borderY = 0;
	explosionAtlasDesc.spacingX = 0;
	explosionAtlasDesc.spacingY = 0;
	explosionAtlasDesc.validFrameCount = 38;

	ParticleAtlasDesc rifleSparkAtlasDesc;
	rifleSparkAtlasDesc.textureWidth = 14352;
	rifleSparkAtlasDesc.textureHeight = 6926;
	rifleSparkAtlasDesc.columns = 7;
	rifleSparkAtlasDesc.rows = 6;
	rifleSparkAtlasDesc.frameWidth = 2048;
	rifleSparkAtlasDesc.frameHeight = 1152;
	rifleSparkAtlasDesc.borderX = 2;
	rifleSparkAtlasDesc.borderY = 2;
	rifleSparkAtlasDesc.spacingX = 2;
	rifleSparkAtlasDesc.spacingY = 2;
	rifleSparkAtlasDesc.validFrameCount = 42;

	ParticleAtlasDesc shotgunSparkAtlasDesc;
	shotgunSparkAtlasDesc.textureWidth = 8080;
	shotgunSparkAtlasDesc.textureHeight = 12302;
	shotgunSparkAtlasDesc.columns = 7;
	shotgunSparkAtlasDesc.rows = 6;
	shotgunSparkAtlasDesc.frameWidth = 1152;
	shotgunSparkAtlasDesc.frameHeight = 2048;
	shotgunSparkAtlasDesc.borderX = 2;
	shotgunSparkAtlasDesc.borderY = 2;
	shotgunSparkAtlasDesc.spacingX = 2;
	shotgunSparkAtlasDesc.spacingY = 2;
	shotgunSparkAtlasDesc.validFrameCount = 42;

	if (!LoadTexture(ParticleTextureID::EXPLOSION, pd3dDevice, pd3dCommandList, L"Model/Explosion3.dds", explosionAtlasDesc))
	{
		Release();
		return false;
	}

	if (!LoadTexture(ParticleTextureID::SPARK_RIFLE_SMG, pd3dDevice, pd3dCommandList, L"Model/Spark_Rifle_SMG.dds", rifleSparkAtlasDesc))
	{
		Release();
		return false;
	}

	if (!LoadTexture(ParticleTextureID::SPARK_SHOTGUN, pd3dDevice, pd3dCommandList, L"Model/Spark_Shotgun.dds", shotgunSparkAtlasDesc))
	{
		Release();
		return false;
	}

	BuildGrenadeEffectDesc();

	m_bLoaded = true;

	OutputDebugStringW(L"[ParticleResource] Particle resources loaded.\n");

	return true;
}

bool ParticleResource::LoadTexture(ParticleTextureID textureId, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const wchar_t* texturePath, const ParticleAtlasDesc& atlasDesc)
{
	if (!pd3dDevice || !pd3dCommandList || !texturePath)
	{
		OutputDebugStringW(L"[ParticleResource] Texture load failed. Invalid argument.\n");
		return false;
	}

	auto textureIt = m_Textures.find(textureId);

	if (textureIt != m_Textures.end() && textureIt->second)
	{
		return true;
	}

	auto pTexture = std::make_unique<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
	pTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, texturePath, RESOURCE_TEXTURE2D, 0);

	if (!pTexture->GetResource(0))
	{
		OutputDebugStringW(L"[ParticleResource] DDS resource creation failed.\n");
		return false;
	}

	m_AtlasDescs[textureId] = atlasDesc;
	m_Textures[textureId] = std::move(pTexture);

	wchar_t debugText[256];

	swprintf_s(debugText, L"[ParticleResource] Texture loaded. ID=%u, Size=%ux%u, Atlas=%ux%u, Frames=%u\n",
		static_cast<UINT>(textureId), atlasDesc.textureWidth, atlasDesc.textureHeight,
		atlasDesc.columns, atlasDesc.rows, atlasDesc.validFrameCount);

	OutputDebugStringW(debugText);

	return true;
}

void ParticleResource::BuildGrenadeEffectDesc()
{
	ParticleEffectDesc grenadeDesc;
	grenadeDesc.id = EffectID::GRENADE_EXPLOSION;
	grenadeDesc.emitters.reserve(5);

	// ¼ø°£ ¼¶±¤
	ParticleEmitterDesc flashDesc;
	flashDesc.textureId = ParticleTextureID::EXPLOSION;
	flashDesc.blendMode = ParticleBlendMode::ADDITIVE;
	flashDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	flashDesc.frameMode = ParticleFrameMode::FIXED_FRAME;
	flashDesc.burstCount = 1;
	flashDesc.lifeTimeMin = 0.06f;
	flashDesc.lifeTimeMax = 0.08f;
	flashDesc.startSize = XMFLOAT2(2.5f, 2.5f);
	flashDesc.endSize = XMFLOAT2(7.5f, 7.5f);
	flashDesc.startColor = XMFLOAT4(1.0f, 0.92f, 0.62f, 1.0f);
	flashDesc.endColor = XMFLOAT4(1.0f, 0.28f, 0.03f, 0.0f);
	flashDesc.rotationMin = 0.0f;
	flashDesc.rotationMax = XM_2PI;
	flashDesc.firstFrame = 5;
	flashDesc.frameCount = 1;
	grenadeDesc.emitters.push_back(flashDesc);

	// Áß¾Ó Æø¹ß º»Ã¼
	ParticleEmitterDesc coreDesc;
	coreDesc.textureId = ParticleTextureID::EXPLOSION;
	coreDesc.blendMode = ParticleBlendMode::ALPHA;
	coreDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	coreDesc.frameMode = ParticleFrameMode::SEQUENTIAL;
	coreDesc.burstCount = 1;
	coreDesc.lifeTimeMin = 0.85f;
	coreDesc.lifeTimeMax = 0.95f;
	coreDesc.startSize = XMFLOAT2(2.8f, 2.8f);
	coreDesc.endSize = XMFLOAT2(7.5f, 7.5f);
	coreDesc.startColor = XMFLOAT4(1.0f, 0.88f, 0.68f, 0.90f);
	coreDesc.endColor = XMFLOAT4(0.26f, 0.22f, 0.20f, 0.0f);
	coreDesc.rotationMin = 0.0f;
	coreDesc.rotationMax = XM_2PI;
	coreDesc.angularVelocityMin = -0.35f;
	coreDesc.angularVelocityMax = 0.35f;
	coreDesc.firstFrame = 1;
	coreDesc.frameCount = 37;
	grenadeDesc.emitters.push_back(coreDesc);

	// ¹æ»çÇü È­¿° ÁÙ±â
	ParticleEmitterDesc flameStreakDesc;
	flameStreakDesc.textureId = ParticleTextureID::SPARK_SHOTGUN;
	flameStreakDesc.blendMode = ParticleBlendMode::ADDITIVE;
	flameStreakDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	flameStreakDesc.frameMode = ParticleFrameMode::SEQUENTIAL;
	flameStreakDesc.burstCount = 6;
	flameStreakDesc.spawnDelayMin = 0.0f;
	flameStreakDesc.spawnDelayMax = 0.03f;
	flameStreakDesc.lifeTimeMin = 0.18f;
	flameStreakDesc.lifeTimeMax = 0.28f;
	flameStreakDesc.speedMin = 4.0f;
	flameStreakDesc.speedMax = 8.0f;
	flameStreakDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	flameStreakDesc.coneAngleDegrees = 180.0f;
	flameStreakDesc.acceleration = XMFLOAT3(0.0f, -6.0f, 0.0f);
	flameStreakDesc.startSize = XMFLOAT2(0.55f, 2.2f);
	flameStreakDesc.endSize = XMFLOAT2(0.12f, 0.40f);
	flameStreakDesc.sizeScaleMin = 0.75f;
	flameStreakDesc.sizeScaleMax = 1.25f;
	flameStreakDesc.startColor = XMFLOAT4(1.0f, 0.62f, 0.16f, 1.0f);
	flameStreakDesc.endColor = XMFLOAT4(1.0f, 0.18f, 0.02f, 0.0f);
	flameStreakDesc.firstFrame = 0;
	flameStreakDesc.frameCount = 7;
	grenadeDesc.emitters.push_back(flameStreakDesc);

	// ºÒ¾¾¿Í ÀÛÀº È­¿° Á¶°¢
	ParticleEmitterDesc sparkDesc;
	sparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	sparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	sparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	sparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	sparkDesc.burstCount = 32;
	sparkDesc.spawnDelayMin = 0.0f;
	sparkDesc.spawnDelayMax = 0.04f;
	sparkDesc.lifeTimeMin = 0.35f;
	sparkDesc.lifeTimeMax = 0.75f;
	sparkDesc.speedMin = 6.0f;
	sparkDesc.speedMax = 14.0f;
	sparkDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	sparkDesc.coneAngleDegrees = 180.0f;
	sparkDesc.acceleration = XMFLOAT3(0.0f, -9.8f, 0.0f);
	sparkDesc.startSize = XMFLOAT2(0.12f, 0.35f);
	sparkDesc.endSize = XMFLOAT2(0.02f, 0.06f);
	sparkDesc.sizeScaleMin = 0.65f;
	sparkDesc.sizeScaleMax = 1.35f;
	sparkDesc.startColor = XMFLOAT4(1.0f, 0.68f, 0.18f, 1.0f);
	sparkDesc.endColor = XMFLOAT4(1.0f, 0.08f, 0.01f, 0.0f);

	const UINT sparkFrames[] = { 0, 3, 6, 9, 15, 21, 22, 24, 29 };
	sparkDesc.selectedFrameCount = static_cast<UINT>(_countof(sparkFrames));

	for (UINT i = 0; i < sparkDesc.selectedFrameCount; ++i)
	{
		sparkDesc.selectedFrames[i] = sparkFrames[i];
	}

	grenadeDesc.emitters.push_back(sparkDesc);

	// »ó½ÂÇÏ´Â ¿¬±â
	ParticleEmitterDesc smokeDesc;
	smokeDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	smokeDesc.blendMode = ParticleBlendMode::ALPHA;
	smokeDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	smokeDesc.frameMode = ParticleFrameMode::SEQUENTIAL;
	smokeDesc.burstCount = 12;
	smokeDesc.spawnDelayMin = 0.05f;
	smokeDesc.spawnDelayMax = 0.15f;
	smokeDesc.lifeTimeMin = 1.2f;
	smokeDesc.lifeTimeMax = 2.0f;
	smokeDesc.speedMin = 0.5f;
	smokeDesc.speedMax = 1.8f;
	smokeDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	smokeDesc.coneAngleDegrees = 65.0f;
	smokeDesc.acceleration = XMFLOAT3(0.0f, 0.45f, 0.0f);
	smokeDesc.startSize = XMFLOAT2(0.8f, 0.8f);
	smokeDesc.endSize = XMFLOAT2(3.8f, 3.8f);
	smokeDesc.sizeScaleMin = 0.75f;
	smokeDesc.sizeScaleMax = 1.35f;
	smokeDesc.startColor = XMFLOAT4(0.30f, 0.27f, 0.24f, 0.58f);
	smokeDesc.endColor = XMFLOAT4(0.14f, 0.14f, 0.14f, 0.0f);
	smokeDesc.rotationMin = 0.0f;
	smokeDesc.rotationMax = XM_2PI;
	smokeDesc.angularVelocityMin = -1.2f;
	smokeDesc.angularVelocityMax = 1.2f;
	smokeDesc.firstFrame = 35;
	smokeDesc.frameCount = 7;
	grenadeDesc.emitters.push_back(smokeDesc);

	m_EffectDescs[grenadeDesc.id] = std::move(grenadeDesc);
}

void ParticleResource::ReleaseUploadBuffers()
{
	for (auto& texturePair : m_Textures)
	{
		if (texturePair.second)
		{
			texturePair.second->ReleaseUploadBuffers();
		}
	}
}

void ParticleResource::Release()
{
	ReleaseUploadBuffers();

	m_EffectDescs.clear();
	m_AtlasDescs.clear();
	m_Textures.clear();

	m_bLoaded = false;
}

const ParticleEffectDesc* ParticleResource::GetEffectDesc(EffectID effectId) const
{
	auto it = m_EffectDescs.find(effectId);

	if (it == m_EffectDescs.end())
	{
		return nullptr;
	}

	return &it->second;
}

CTexture* ParticleResource::GetTexture(ParticleTextureID textureId) const
{
	auto it = m_Textures.find(textureId);

	if (it == m_Textures.end())
	{
		return nullptr;
	}

	return it->second.get();
}

const ParticleAtlasDesc* ParticleResource::GetAtlasDesc(ParticleTextureID textureId) const
{
	auto it = m_AtlasDescs.find(textureId);

	if (it == m_AtlasDescs.end())
	{
		return nullptr;
	}

	return &it->second;
}