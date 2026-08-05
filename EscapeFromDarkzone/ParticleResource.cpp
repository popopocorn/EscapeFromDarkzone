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

	if (!LoadTexture(ParticleTextureID::EXPLOSION, pd3dDevice, pd3dCommandList,
		L"Model/Explosion3.dds", explosionAtlasDesc))
	{
		Release();
		return false;
	}

	if (!LoadTexture(ParticleTextureID::SPARK_RIFLE_SMG, pd3dDevice, pd3dCommandList,
		L"Model/Spark_Rifle_SMG.dds", rifleSparkAtlasDesc))
	{
		Release();
		return false;
	}

	if (!LoadTexture(ParticleTextureID::SPARK_SHOTGUN, pd3dDevice, pd3dCommandList,
		L"Model/Spark_Shotgun.dds", shotgunSparkAtlasDesc))
	{
		Release();
		return false;
	}

	BuildGrenadeEffectDesc();
	BuildRifleSparkEffectDesc();
	BuildShotgunSparkEffectDesc();
	BuildPistolSparkEffectDesc();

	m_bLoaded = true;

	OutputDebugStringW(L"[ParticleResource] Particle textures and effect descriptions loaded.\n");

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

	// 순간 섬광
	ParticleEmitterDesc flashDesc;
	flashDesc.textureId = ParticleTextureID::EXPLOSION;
	flashDesc.blendMode = ParticleBlendMode::ADDITIVE;
	flashDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	flashDesc.frameMode = ParticleFrameMode::FIXED_FRAME;
	flashDesc.burstCount = 1;
	flashDesc.lifeTimeMin = 0.035f;
	flashDesc.lifeTimeMax = 0.05f;
	flashDesc.startSize = XMFLOAT2(1.5f, 1.5f);
	flashDesc.endSize = XMFLOAT2(3.6f, 3.6f);
	flashDesc.startColor = XMFLOAT4(1.0f, 0.78f, 0.35f, 1.0f);
	flashDesc.endColor = XMFLOAT4(1.0f, 0.18f, 0.02f, 0.0f);
	flashDesc.rotationMin = 0.0f;
	flashDesc.rotationMax = 0.0f;
	flashDesc.firstFrame = 2;
	flashDesc.frameCount = 1;
	grenadeDesc.emitters.push_back(flashDesc);

	// 중앙 폭발 본체
	ParticleEmitterDesc coreDesc;
	coreDesc.textureId = ParticleTextureID::EXPLOSION;
	coreDesc.blendMode = ParticleBlendMode::ALPHA;
	coreDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	coreDesc.frameMode = ParticleFrameMode::SEQUENTIAL;
	coreDesc.burstCount = 1;
	coreDesc.lifeTimeMin = 0.58f;
	coreDesc.lifeTimeMax = 0.68f;
	coreDesc.startSize = XMFLOAT2(1.8f, 1.8f);
	coreDesc.endSize = XMFLOAT2(4.6f, 4.6f);
	coreDesc.startColor = XMFLOAT4(1.0f, 0.95f, 0.82f, 0.95f);
	coreDesc.endColor = XMFLOAT4(0.58f, 0.54f, 0.52f, 0.0f);
	coreDesc.rotationMin = -0.08f;
	coreDesc.rotationMax = 0.08f;
	coreDesc.angularVelocityMin = -0.12f;
	coreDesc.angularVelocityMax = 0.12f;
	coreDesc.firstFrame = 0;
	coreDesc.frameCount = 35;
	grenadeDesc.emitters.push_back(coreDesc);

	// 방사형 화염 줄기
	ParticleEmitterDesc flameStreakDesc;
	flameStreakDesc.textureId = ParticleTextureID::SPARK_SHOTGUN;
	flameStreakDesc.blendMode = ParticleBlendMode::ADDITIVE;
	flameStreakDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;

	// Shotgun 애니메이션은 0번 이후 빠르게 회색으로 사라지므로
	// 가장 밝은 0번 프레임을 고정하고 색상 알파로 사라지게 한다.
	flameStreakDesc.frameMode = ParticleFrameMode::FIXED_FRAME;

	flameStreakDesc.burstCount = 8;
	flameStreakDesc.spawnDelayMin = 0.0f;
	flameStreakDesc.spawnDelayMax = 0.015f;
	flameStreakDesc.lifeTimeMin = 0.12f;
	flameStreakDesc.lifeTimeMax = 0.19f;
	flameStreakDesc.speedMin = 5.5f;
	flameStreakDesc.speedMax = 9.5f;
	flameStreakDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	flameStreakDesc.coneAngleDegrees = 78.0f;
	flameStreakDesc.acceleration = XMFLOAT3(0.0f, -3.0f, 0.0f);
	flameStreakDesc.startSize = XMFLOAT2(0.24f, 1.45f);
	flameStreakDesc.endSize = XMFLOAT2(0.05f, 0.30f);
	flameStreakDesc.sizeScaleMin = 0.85f;
	flameStreakDesc.sizeScaleMax = 1.20f;
	flameStreakDesc.startColor = XMFLOAT4(1.0f, 0.68f, 0.18f, 1.0f);
	flameStreakDesc.endColor = XMFLOAT4(1.0f, 0.08f, 0.01f, 0.0f);
	flameStreakDesc.firstFrame = 0;
	flameStreakDesc.frameCount = 1;
	grenadeDesc.emitters.push_back(flameStreakDesc);

	// 불씨와 작은 화염 조각
	ParticleEmitterDesc sparkDesc;
	sparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	sparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	sparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	sparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	sparkDesc.burstCount = 36;
	sparkDesc.spawnDelayMin = 0.0f;
	sparkDesc.spawnDelayMax = 0.04f;
	sparkDesc.lifeTimeMin = 0.28f;
	sparkDesc.lifeTimeMax = 0.62f;
	sparkDesc.speedMin = 5.5f;
	sparkDesc.speedMax = 11.5f;
	sparkDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	sparkDesc.coneAngleDegrees = 86.0f;
	sparkDesc.acceleration = XMFLOAT3(0.0f, -7.5f, 0.0f);
	sparkDesc.startSize = XMFLOAT2(0.08f, 0.38f);
	sparkDesc.endSize = XMFLOAT2(0.015f, 0.06f);
	sparkDesc.sizeScaleMin = 0.75f;
	sparkDesc.sizeScaleMax = 1.35f;
	sparkDesc.startColor = XMFLOAT4(1.0f, 0.82f, 0.28f, 1.0f);
	sparkDesc.endColor = XMFLOAT4(1.0f, 0.06f, 0.01f, 0.0f);

	const UINT sparkFrames[] = { 0, 3, 6, 9, 15, 21, 22, 24, 29 };
	sparkDesc.selectedFrameCount = static_cast<UINT>(_countof(sparkFrames));

	for (UINT i = 0; i < sparkDesc.selectedFrameCount; ++i)
	{
		sparkDesc.selectedFrames[i] = sparkFrames[i];
	}

	grenadeDesc.emitters.push_back(sparkDesc);

	// 중앙 폭발 이후 천천히 올라오는 연기
	ParticleEmitterDesc smokeDesc;
	smokeDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	smokeDesc.blendMode = ParticleBlendMode::ALPHA;
	smokeDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	smokeDesc.frameMode = ParticleFrameMode::SEQUENTIAL;
	smokeDesc.burstCount = 8;

	// 폭발 초반에는 코어와 불꽃이 보이도록 연기 출현을 늦춘다.
	smokeDesc.spawnDelayMin = 0.22f;
	smokeDesc.spawnDelayMax = 0.38f;

	smokeDesc.lifeTimeMin = 1.0f;
	smokeDesc.lifeTimeMax = 1.45f;
	smokeDesc.speedMin = 0.25f;
	smokeDesc.speedMax = 0.80f;
	smokeDesc.direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	smokeDesc.coneAngleDegrees = 42.0f;
	smokeDesc.acceleration = XMFLOAT3(0.0f, 0.25f, 0.0f);
	smokeDesc.startSize = XMFLOAT2(0.8f, 0.8f);
	smokeDesc.endSize = XMFLOAT2(3.6f, 3.6f);
	smokeDesc.sizeScaleMin = 0.80f;
	smokeDesc.sizeScaleMax = 1.25f;
	smokeDesc.startColor = XMFLOAT4(0.65f, 0.62f, 0.58f, 0.38f);
	smokeDesc.endColor = XMFLOAT4(0.35f, 0.35f, 0.35f, 0.0f);
	smokeDesc.rotationMin = 0.0f;
	smokeDesc.rotationMax = XM_2PI;
	smokeDesc.angularVelocityMin = -0.65f;
	smokeDesc.angularVelocityMax = 0.65f;
	smokeDesc.firstFrame = 35;
	smokeDesc.frameCount = 7;
	grenadeDesc.emitters.push_back(smokeDesc);

	m_EffectDescs[grenadeDesc.id] = std::move(grenadeDesc);
}

void ParticleResource::BuildRifleSparkEffectDesc()
{
	ParticleEffectDesc sparkEffectDesc;
	sparkEffectDesc.id = EffectID::SPARK;
	sparkEffectDesc.emitters.reserve(4);

	const UINT brightFrames[] = { 0, 3, 6, 9, 15, 21, 22, 24, 29 };

	// 총구 근처를 순간적으로 밝게 만드는 짧은 코어 플래시
	ParticleEmitterDesc muzzleFlashCoreDesc;
	muzzleFlashCoreDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	muzzleFlashCoreDesc.blendMode = ParticleBlendMode::ADDITIVE;
	muzzleFlashCoreDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	muzzleFlashCoreDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	muzzleFlashCoreDesc.burstCount = 1;
	muzzleFlashCoreDesc.spawnDelayMin = 0.0f;
	muzzleFlashCoreDesc.spawnDelayMax = 0.0f;
	muzzleFlashCoreDesc.lifeTimeMin = 0.025f;
	muzzleFlashCoreDesc.lifeTimeMax = 0.040f;
	muzzleFlashCoreDesc.speedMin = 0.0f;
	muzzleFlashCoreDesc.speedMax = 0.0f;
	muzzleFlashCoreDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	muzzleFlashCoreDesc.coneAngleDegrees = 0.0f;

	// 코어의 밝은 부분이 총구 바로 앞에 붙도록 약간 뒤로 이동한다.
	muzzleFlashCoreDesc.positionOffsetAlongDirection = -0.20f;

	muzzleFlashCoreDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	muzzleFlashCoreDesc.startSize = XMFLOAT2(0.75f, 0.75f);
	muzzleFlashCoreDesc.endSize = XMFLOAT2(0.20f, 0.20f);
	muzzleFlashCoreDesc.sizeScaleMin = 0.95f;
	muzzleFlashCoreDesc.sizeScaleMax = 1.10f;
	muzzleFlashCoreDesc.startColor = XMFLOAT4(1.8f, 1.45f, 0.75f, 1.0f);
	muzzleFlashCoreDesc.endColor = XMFLOAT4(1.2f, 0.35f, 0.04f, 0.0f);
	muzzleFlashCoreDesc.rotationMin = 0.0f;
	muzzleFlashCoreDesc.rotationMax = XM_2PI;
	muzzleFlashCoreDesc.angularVelocityMin = 0.0f;
	muzzleFlashCoreDesc.angularVelocityMax = 0.0f;
	muzzleFlashCoreDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < muzzleFlashCoreDesc.selectedFrameCount; ++i)
	{
		muzzleFlashCoreDesc.selectedFrames[i] = brightFrames[i];
	}

	sparkEffectDesc.emitters.push_back(muzzleFlashCoreDesc);

	// 총구에서 앞으로 길게 뻗는 메인 불씨
	ParticleEmitterDesc muzzleFlameDesc;
	muzzleFlameDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	muzzleFlameDesc.blendMode = ParticleBlendMode::ADDITIVE;
	muzzleFlameDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	muzzleFlameDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	muzzleFlameDesc.burstCount = 1;
	muzzleFlameDesc.spawnDelayMin = 0.0f;
	muzzleFlameDesc.spawnDelayMax = 0.0f;
	muzzleFlameDesc.lifeTimeMin = 0.050f;
	muzzleFlameDesc.lifeTimeMax = 0.070f;

	// 이동량보다 방향 정렬이 목적
	muzzleFlameDesc.speedMin = 0.20f;
	muzzleFlameDesc.speedMax = 0.28f;
	muzzleFlameDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	muzzleFlameDesc.coneAngleDegrees = 2.0f;

	// 영상에서 총구와 가장 크게 떨어져 있던 메인 불씨를 뒤로 당긴다.
	muzzleFlameDesc.positionOffsetAlongDirection = -0.52f;

	muzzleFlameDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	muzzleFlameDesc.startSize = XMFLOAT2(0.95f, 1.95f);
	muzzleFlameDesc.endSize = XMFLOAT2(0.22f, 0.60f);
	muzzleFlameDesc.sizeScaleMin = 0.95f;
	muzzleFlameDesc.sizeScaleMax = 1.15f;
	muzzleFlameDesc.startColor = XMFLOAT4(1.7f, 1.30f, 0.52f, 1.0f);
	muzzleFlameDesc.endColor = XMFLOAT4(1.15f, 0.20f, 0.02f, 0.0f);
	muzzleFlameDesc.rotationMin = -0.05f;
	muzzleFlameDesc.rotationMax = 0.05f;
	muzzleFlameDesc.angularVelocityMin = 0.0f;
	muzzleFlameDesc.angularVelocityMax = 0.0f;
	muzzleFlameDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < muzzleFlameDesc.selectedFrameCount; ++i)
	{
		muzzleFlameDesc.selectedFrames[i] = brightFrames[i];
	}

	sparkEffectDesc.emitters.push_back(muzzleFlameDesc);

	// 총구 앞쪽으로 튀는 주 불씨
	ParticleEmitterDesc muzzleSparkDesc;
	muzzleSparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	muzzleSparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	muzzleSparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	muzzleSparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	muzzleSparkDesc.burstCount = 5;
	muzzleSparkDesc.spawnDelayMin = 0.0f;
	muzzleSparkDesc.spawnDelayMax = 0.010f;
	muzzleSparkDesc.lifeTimeMin = 0.070f;
	muzzleSparkDesc.lifeTimeMax = 0.125f;
	muzzleSparkDesc.speedMin = 3.6f;
	muzzleSparkDesc.speedMax = 6.2f;
	muzzleSparkDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	muzzleSparkDesc.coneAngleDegrees = 12.0f;

	// 긴 보조 불씨도 총구에서 출발하는 것처럼 소폭 뒤로 당긴다.
	muzzleSparkDesc.positionOffsetAlongDirection = -0.10f;

	muzzleSparkDesc.acceleration = XMFLOAT3(0.0f, -1.2f, 0.0f);
	muzzleSparkDesc.startSize = XMFLOAT2(0.070f, 0.55f);
	muzzleSparkDesc.endSize = XMFLOAT2(0.012f, 0.085f);
	muzzleSparkDesc.sizeScaleMin = 0.85f;
	muzzleSparkDesc.sizeScaleMax = 1.25f;
	muzzleSparkDesc.startColor = XMFLOAT4(1.6f, 1.10f, 0.42f, 1.0f);
	muzzleSparkDesc.endColor = XMFLOAT4(1.0f, 0.10f, 0.01f, 0.0f);
	muzzleSparkDesc.rotationMin = -0.10f;
	muzzleSparkDesc.rotationMax = 0.10f;
	muzzleSparkDesc.angularVelocityMin = 0.0f;
	muzzleSparkDesc.angularVelocityMax = 0.0f;
	muzzleSparkDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < muzzleSparkDesc.selectedFrameCount; ++i)
	{
		muzzleSparkDesc.selectedFrames[i] = brightFrames[i];
	}

	sparkEffectDesc.emitters.push_back(muzzleSparkDesc);

	// 메인 불씨 주변으로 좌우 / 위아래에 퍼지는 더 작은 보조 불씨
	ParticleEmitterDesc scatteredSparkDesc;
	scatteredSparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	scatteredSparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	scatteredSparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	scatteredSparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	scatteredSparkDesc.burstCount = 12;
	scatteredSparkDesc.spawnDelayMin = 0.0f;
	scatteredSparkDesc.spawnDelayMax = 0.010f;
	scatteredSparkDesc.lifeTimeMin = 0.045f;
	scatteredSparkDesc.lifeTimeMax = 0.090f;
	scatteredSparkDesc.speedMin = 2.6f;
	scatteredSparkDesc.speedMax = 5.0f;
	scatteredSparkDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);

	// 발사 방향을 중심으로 넓게 퍼지게 해서 좌우/상하 보조 불씨를 만든다.
	scatteredSparkDesc.coneAngleDegrees = 82.0f;

	// 작은 불씨의 출발점도 총구에 모이도록 조금만 뒤로 당긴다.
	scatteredSparkDesc.positionOffsetAlongDirection = -0.06f;

	scatteredSparkDesc.acceleration = XMFLOAT3(0.0f, -0.6f, 0.0f);
	scatteredSparkDesc.startSize = XMFLOAT2(0.032f, 0.24f);
	scatteredSparkDesc.endSize = XMFLOAT2(0.006f, 0.040f);
	scatteredSparkDesc.sizeScaleMin = 0.80f;
	scatteredSparkDesc.sizeScaleMax = 1.20f;
	scatteredSparkDesc.startColor = XMFLOAT4(1.45f, 1.00f, 0.36f, 0.95f);
	scatteredSparkDesc.endColor = XMFLOAT4(1.0f, 0.08f, 0.01f, 0.0f);
	scatteredSparkDesc.rotationMin = -0.18f;
	scatteredSparkDesc.rotationMax = 0.18f;
	scatteredSparkDesc.angularVelocityMin = 0.0f;
	scatteredSparkDesc.angularVelocityMax = 0.0f;
	scatteredSparkDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < scatteredSparkDesc.selectedFrameCount; ++i)
	{
		scatteredSparkDesc.selectedFrames[i] = brightFrames[i];
	}

	sparkEffectDesc.emitters.push_back(scatteredSparkDesc);

	m_EffectDescs[sparkEffectDesc.id] = std::move(sparkEffectDesc);
}

void ParticleResource::BuildShotgunSparkEffectDesc()
{
	ParticleEffectDesc shotgunSparkEffectDesc;
	shotgunSparkEffectDesc.id = EffectID::SPARK_SHOTGUN;
	shotgunSparkEffectDesc.emitters.reserve(4);

	const UINT brightRifleFrames[] = { 0, 3, 6, 9, 15, 21, 22, 24, 29 };

	// 총구 중심에서 순간적으로 강하게 빛나는 섬광
	ParticleEmitterDesc muzzleCoreDesc;
	muzzleCoreDesc.textureId = ParticleTextureID::SPARK_SHOTGUN;
	muzzleCoreDesc.blendMode = ParticleBlendMode::ADDITIVE;
	muzzleCoreDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	muzzleCoreDesc.frameMode = ParticleFrameMode::FIXED_FRAME;
	muzzleCoreDesc.burstCount = 1;
	muzzleCoreDesc.spawnDelayMin = 0.0f;
	muzzleCoreDesc.spawnDelayMax = 0.0f;
	muzzleCoreDesc.lifeTimeMin = 0.030f;
	muzzleCoreDesc.lifeTimeMax = 0.045f;
	muzzleCoreDesc.speedMin = 0.0f;
	muzzleCoreDesc.speedMax = 0.0f;
	muzzleCoreDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	muzzleCoreDesc.coneAngleDegrees = 0.0f;
	muzzleCoreDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	muzzleCoreDesc.startSize = XMFLOAT2(1.10f, 1.10f);
	muzzleCoreDesc.endSize = XMFLOAT2(0.30f, 0.30f);
	muzzleCoreDesc.sizeScaleMin = 0.95f;
	muzzleCoreDesc.sizeScaleMax = 1.10f;
	muzzleCoreDesc.startColor = XMFLOAT4(2.0f, 1.55f, 0.72f, 1.0f);
	muzzleCoreDesc.endColor = XMFLOAT4(1.2f, 0.24f, 0.02f, 0.0f);
	muzzleCoreDesc.rotationMin = 0.0f;
	muzzleCoreDesc.rotationMax = XM_2PI;
	muzzleCoreDesc.angularVelocityMin = 0.0f;
	muzzleCoreDesc.angularVelocityMax = 0.0f;
	muzzleCoreDesc.firstFrame = 0;
	muzzleCoreDesc.frameCount = 1;

	shotgunSparkEffectDesc.emitters.push_back(muzzleCoreDesc);

	// 총구 정면으로 굵고 길게 뻗는 주 화염
	ParticleEmitterDesc mainFlameDesc;
	mainFlameDesc.textureId = ParticleTextureID::SPARK_SHOTGUN;
	mainFlameDesc.blendMode = ParticleBlendMode::ADDITIVE;
	mainFlameDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	mainFlameDesc.frameMode = ParticleFrameMode::FIXED_FRAME;
	mainFlameDesc.burstCount = 1;
	mainFlameDesc.spawnDelayMin = 0.0f;
	mainFlameDesc.spawnDelayMax = 0.0f;
	mainFlameDesc.lifeTimeMin = 0.055f;
	mainFlameDesc.lifeTimeMax = 0.075f;
	mainFlameDesc.speedMin = 0.18f;
	mainFlameDesc.speedMax = 0.26f;
	mainFlameDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	mainFlameDesc.coneAngleDegrees = 3.0f;
	mainFlameDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mainFlameDesc.startSize = XMFLOAT2(1.15f, 2.35f);
	mainFlameDesc.endSize = XMFLOAT2(0.30f, 0.70f);
	mainFlameDesc.sizeScaleMin = 0.95f;
	mainFlameDesc.sizeScaleMax = 1.15f;
	mainFlameDesc.startColor = XMFLOAT4(1.9f, 1.35f, 0.50f, 1.0f);
	mainFlameDesc.endColor = XMFLOAT4(1.1f, 0.16f, 0.01f, 0.0f);
	mainFlameDesc.rotationMin = -0.05f;
	mainFlameDesc.rotationMax = 0.05f;
	mainFlameDesc.angularVelocityMin = 0.0f;
	mainFlameDesc.angularVelocityMax = 0.0f;
	mainFlameDesc.firstFrame = 0;
	mainFlameDesc.frameCount = 1;

	shotgunSparkEffectDesc.emitters.push_back(mainFlameDesc);

	// 샷건 특유의 넓은 범위로 퍼지는 짧은 화염
	ParticleEmitterDesc wideFlameDesc;
	wideFlameDesc.textureId = ParticleTextureID::SPARK_SHOTGUN;
	wideFlameDesc.blendMode = ParticleBlendMode::ADDITIVE;
	wideFlameDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	wideFlameDesc.frameMode = ParticleFrameMode::FIXED_FRAME;
	wideFlameDesc.burstCount = 5;
	wideFlameDesc.spawnDelayMin = 0.0f;
	wideFlameDesc.spawnDelayMax = 0.008f;
	wideFlameDesc.lifeTimeMin = 0.040f;
	wideFlameDesc.lifeTimeMax = 0.070f;
	wideFlameDesc.speedMin = 0.8f;
	wideFlameDesc.speedMax = 1.7f;
	wideFlameDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	wideFlameDesc.coneAngleDegrees = 38.0f;
	wideFlameDesc.acceleration = XMFLOAT3(0.0f, -0.4f, 0.0f);
	wideFlameDesc.startSize = XMFLOAT2(0.28f, 0.90f);
	wideFlameDesc.endSize = XMFLOAT2(0.05f, 0.16f);
	wideFlameDesc.sizeScaleMin = 0.80f;
	wideFlameDesc.sizeScaleMax = 1.25f;
	wideFlameDesc.startColor = XMFLOAT4(1.7f, 1.05f, 0.30f, 1.0f);
	wideFlameDesc.endColor = XMFLOAT4(1.0f, 0.08f, 0.01f, 0.0f);
	wideFlameDesc.rotationMin = -0.10f;
	wideFlameDesc.rotationMax = 0.10f;
	wideFlameDesc.angularVelocityMin = 0.0f;
	wideFlameDesc.angularVelocityMax = 0.0f;
	wideFlameDesc.firstFrame = 0;
	wideFlameDesc.frameCount = 1;

	shotgunSparkEffectDesc.emitters.push_back(wideFlameDesc);

	// 총구 주변과 전방으로 넓게 튀는 작은 불씨
	ParticleEmitterDesc scatteredSparkDesc;
	scatteredSparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	scatteredSparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	scatteredSparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	scatteredSparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	scatteredSparkDesc.burstCount = 18;
	scatteredSparkDesc.spawnDelayMin = 0.0f;
	scatteredSparkDesc.spawnDelayMax = 0.015f;
	scatteredSparkDesc.lifeTimeMin = 0.060f;
	scatteredSparkDesc.lifeTimeMax = 0.140f;
	scatteredSparkDesc.speedMin = 3.5f;
	scatteredSparkDesc.speedMax = 7.0f;
	scatteredSparkDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	scatteredSparkDesc.coneAngleDegrees = 58.0f;
	scatteredSparkDesc.acceleration = XMFLOAT3(0.0f, -1.8f, 0.0f);
	scatteredSparkDesc.startSize = XMFLOAT2(0.040f, 0.32f);
	scatteredSparkDesc.endSize = XMFLOAT2(0.006f, 0.050f);
	scatteredSparkDesc.sizeScaleMin = 0.75f;
	scatteredSparkDesc.sizeScaleMax = 1.30f;
	scatteredSparkDesc.startColor = XMFLOAT4(1.65f, 1.10f, 0.38f, 1.0f);
	scatteredSparkDesc.endColor = XMFLOAT4(1.0f, 0.06f, 0.01f, 0.0f);
	scatteredSparkDesc.rotationMin = -0.15f;
	scatteredSparkDesc.rotationMax = 0.15f;
	scatteredSparkDesc.angularVelocityMin = 0.0f;
	scatteredSparkDesc.angularVelocityMax = 0.0f;
	scatteredSparkDesc.selectedFrameCount = static_cast<UINT>(_countof(brightRifleFrames));

	for (UINT i = 0; i < scatteredSparkDesc.selectedFrameCount; ++i)
	{
		scatteredSparkDesc.selectedFrames[i] = brightRifleFrames[i];
	}

	shotgunSparkEffectDesc.emitters.push_back(scatteredSparkDesc);

	m_EffectDescs[shotgunSparkEffectDesc.id] = std::move(shotgunSparkEffectDesc);
}

void ParticleResource::BuildPistolSparkEffectDesc()
{
	ParticleEffectDesc pistolSparkEffectDesc;
	pistolSparkEffectDesc.id = EffectID::SPARK_PISTOL;
	pistolSparkEffectDesc.emitters.reserve(3);

	const UINT brightFrames[] = { 0, 3, 6, 9, 15, 21, 22, 24, 29 };

	// 피스톨 총구 근처의 짧은 중심 섬광
	ParticleEmitterDesc muzzleCoreDesc;
	muzzleCoreDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	muzzleCoreDesc.blendMode = ParticleBlendMode::ADDITIVE;
	muzzleCoreDesc.billboardMode = ParticleBillboardMode::CAMERA_FACING;
	muzzleCoreDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	muzzleCoreDesc.burstCount = 1;
	muzzleCoreDesc.spawnDelayMin = 0.0f;
	muzzleCoreDesc.spawnDelayMax = 0.0f;
	muzzleCoreDesc.lifeTimeMin = 0.020f;
	muzzleCoreDesc.lifeTimeMax = 0.035f;
	muzzleCoreDesc.speedMin = 0.0f;
	muzzleCoreDesc.speedMax = 0.0f;
	muzzleCoreDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	muzzleCoreDesc.coneAngleDegrees = 0.0f;
	muzzleCoreDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	muzzleCoreDesc.startSize = XMFLOAT2(0.38f, 0.38f);
	muzzleCoreDesc.endSize = XMFLOAT2(0.10f, 0.10f);
	muzzleCoreDesc.sizeScaleMin = 0.90f;
	muzzleCoreDesc.sizeScaleMax = 1.10f;
	muzzleCoreDesc.startColor = XMFLOAT4(1.65f, 1.20f, 0.48f, 1.0f);
	muzzleCoreDesc.endColor = XMFLOAT4(1.0f, 0.15f, 0.01f, 0.0f);
	muzzleCoreDesc.rotationMin = 0.0f;
	muzzleCoreDesc.rotationMax = XM_2PI;
	muzzleCoreDesc.angularVelocityMin = 0.0f;
	muzzleCoreDesc.angularVelocityMax = 0.0f;
	muzzleCoreDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < muzzleCoreDesc.selectedFrameCount; ++i)
	{
		muzzleCoreDesc.selectedFrames[i] = brightFrames[i];
	}

	pistolSparkEffectDesc.emitters.push_back(muzzleCoreDesc);

	// 피스톨 총구에서 짧게 앞으로 뻗는 주 화염
	ParticleEmitterDesc mainFlameDesc;
	mainFlameDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	mainFlameDesc.blendMode = ParticleBlendMode::ADDITIVE;
	mainFlameDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	mainFlameDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	mainFlameDesc.burstCount = 1;
	mainFlameDesc.spawnDelayMin = 0.0f;
	mainFlameDesc.spawnDelayMax = 0.0f;
	mainFlameDesc.lifeTimeMin = 0.032f;
	mainFlameDesc.lifeTimeMax = 0.050f;
	mainFlameDesc.speedMin = 0.12f;
	mainFlameDesc.speedMax = 0.18f;
	mainFlameDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	mainFlameDesc.coneAngleDegrees = 3.0f;
	mainFlameDesc.acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mainFlameDesc.startSize = XMFLOAT2(0.32f, 0.75f);
	mainFlameDesc.endSize = XMFLOAT2(0.08f, 0.22f);
	mainFlameDesc.sizeScaleMin = 0.90f;
	mainFlameDesc.sizeScaleMax = 1.10f;
	mainFlameDesc.startColor = XMFLOAT4(1.65f, 1.12f, 0.40f, 1.0f);
	mainFlameDesc.endColor = XMFLOAT4(1.0f, 0.10f, 0.01f, 0.0f);
	mainFlameDesc.rotationMin = -0.06f;
	mainFlameDesc.rotationMax = 0.06f;
	mainFlameDesc.angularVelocityMin = 0.0f;
	mainFlameDesc.angularVelocityMax = 0.0f;
	mainFlameDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < mainFlameDesc.selectedFrameCount; ++i)
	{
		mainFlameDesc.selectedFrames[i] = brightFrames[i];
	}

	pistolSparkEffectDesc.emitters.push_back(mainFlameDesc);

	// 피스톨 총구 주변으로 적게 튀는 작은 불씨
	ParticleEmitterDesc scatteredSparkDesc;
	scatteredSparkDesc.textureId = ParticleTextureID::SPARK_RIFLE_SMG;
	scatteredSparkDesc.blendMode = ParticleBlendMode::ADDITIVE;
	scatteredSparkDesc.billboardMode = ParticleBillboardMode::VELOCITY_ALIGNED;
	scatteredSparkDesc.frameMode = ParticleFrameMode::RANDOM_SELECTED;
	scatteredSparkDesc.burstCount = 4;
	scatteredSparkDesc.spawnDelayMin = 0.0f;
	scatteredSparkDesc.spawnDelayMax = 0.008f;
	scatteredSparkDesc.lifeTimeMin = 0.040f;
	scatteredSparkDesc.lifeTimeMax = 0.080f;
	scatteredSparkDesc.speedMin = 2.0f;
	scatteredSparkDesc.speedMax = 4.2f;
	scatteredSparkDesc.direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	scatteredSparkDesc.coneAngleDegrees = 38.0f;
	scatteredSparkDesc.acceleration = XMFLOAT3(0.0f, -0.7f, 0.0f);
	scatteredSparkDesc.startSize = XMFLOAT2(0.018f, 0.14f);
	scatteredSparkDesc.endSize = XMFLOAT2(0.003f, 0.020f);
	scatteredSparkDesc.sizeScaleMin = 0.75f;
	scatteredSparkDesc.sizeScaleMax = 1.20f;
	scatteredSparkDesc.startColor = XMFLOAT4(1.45f, 0.95f, 0.30f, 0.95f);
	scatteredSparkDesc.endColor = XMFLOAT4(1.0f, 0.05f, 0.01f, 0.0f);
	scatteredSparkDesc.rotationMin = -0.12f;
	scatteredSparkDesc.rotationMax = 0.12f;
	scatteredSparkDesc.angularVelocityMin = 0.0f;
	scatteredSparkDesc.angularVelocityMax = 0.0f;
	scatteredSparkDesc.selectedFrameCount = static_cast<UINT>(_countof(brightFrames));

	for (UINT i = 0; i < scatteredSparkDesc.selectedFrameCount; ++i)
	{
		scatteredSparkDesc.selectedFrames[i] = brightFrames[i];
	}

	pistolSparkEffectDesc.emitters.push_back(scatteredSparkDesc);

	m_EffectDescs[pistolSparkEffectDesc.id] = std::move(pistolSparkEffectDesc);
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