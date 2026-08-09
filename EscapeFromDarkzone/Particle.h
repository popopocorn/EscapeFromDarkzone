#pragma once

#include "ParticleResource.h"

constexpr UINT MAX_GPU_PARTICLES = 4096;
constexpr UINT MAX_PARTICLE_SPAWN_REQUESTS = 64;
constexpr UINT PARTICLE_COMPUTE_ROOT_CONSTANT_COUNT = 8;
constexpr UINT PARTICLE_GRAPHICS_ROOT_CONSTANT_COUNT = 16;
constexpr UINT PARTICLE_UPLOAD_FRAME_COUNT = 3;

enum class ParticleRenderGroup : UINT
{
	EXPLOSION_ALPHA = 0,
	EXPLOSION_ADDITIVE,
	SHOTGUN_ADDITIVE,
	RIFLE_ADDITIVE,
	RIFLE_ALPHA,

	COUNT
};

enum ParticleCounterIndex : UINT
{
	PARTICLE_COUNTER_ALIVE_0 = 0,
	PARTICLE_COUNTER_ALIVE_1,
	PARTICLE_COUNTER_DEAD,
	PARTICLE_COUNTER_SPAWN_REQUEST,

	PARTICLE_COUNTER_RENDER_EXPLOSION_ALPHA,
	PARTICLE_COUNTER_RENDER_EXPLOSION_ADDITIVE,
	PARTICLE_COUNTER_RENDER_SHOTGUN_ADDITIVE,
	PARTICLE_COUNTER_RENDER_RIFLE_ADDITIVE,
	PARTICLE_COUNTER_RENDER_RIFLE_ALPHA,

	PARTICLE_COUNTER_COUNT
};

enum ParticleDescriptorIndex : UINT
{
	PARTICLE_DESCRIPTOR_PARTICLE_UAV = 0,
	PARTICLE_DESCRIPTOR_ALIVE_0_UAV,
	PARTICLE_DESCRIPTOR_ALIVE_1_UAV,
	PARTICLE_DESCRIPTOR_DEAD_UAV,
	PARTICLE_DESCRIPTOR_COUNTER_UAV,

	PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ALPHA_UAV,
	PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ADDITIVE_UAV,
	PARTICLE_DESCRIPTOR_RENDER_SHOTGUN_ADDITIVE_UAV,
	PARTICLE_DESCRIPTOR_RENDER_RIFLE_ADDITIVE_UAV,
	PARTICLE_DESCRIPTOR_RENDER_RIFLE_ALPHA_UAV,

	PARTICLE_DESCRIPTOR_INDIRECT_ARGUMENT_UAV,

	PARTICLE_DESCRIPTOR_SPAWN_REQUEST_SRV,
	PARTICLE_DESCRIPTOR_ALIVE_0_SRV,
	PARTICLE_DESCRIPTOR_ALIVE_1_SRV,

	PARTICLE_DESCRIPTOR_PARTICLE_SRV,

	PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ALPHA_SRV,
	PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ADDITIVE_SRV,
	PARTICLE_DESCRIPTOR_RENDER_SHOTGUN_ADDITIVE_SRV,
	PARTICLE_DESCRIPTOR_RENDER_RIFLE_ADDITIVE_SRV,
	PARTICLE_DESCRIPTOR_RENDER_RIFLE_ALPHA_SRV,

	PARTICLE_DESCRIPTOR_TEXTURE_EXPLOSION_SRV,
	PARTICLE_DESCRIPTOR_TEXTURE_SPARK_RIFLE_SMG_SRV,
	PARTICLE_DESCRIPTOR_TEXTURE_SPARK_SHOTGUN_SRV,

	PARTICLE_DESCRIPTOR_COUNT
};

//GPU풀에 저장되는 개별 파티클 구조체(파티클 시스템에서 사용)
struct GPUParticle
{
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float age = 0.0f;

	XMFLOAT3 velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float lifeTime = 0.0f;

	XMFLOAT3 acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float spawnDelay = 0.0f;

	XMFLOAT2 startSize = XMFLOAT2(1.0f, 1.0f);
	XMFLOAT2 endSize = XMFLOAT2(1.0f, 1.0f);

	XMFLOAT4 startColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4 endColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

	float rotation = 0.0f;
	float angularVelocity = 0.0f;
	float sizeScale = 1.0f;
	UINT textureId = 0;

	UINT billboardMode = 0;
	UINT frameMode = 0;
	UINT firstFrame = 0;
	UINT frameCount = 1;

	UINT renderGroup = 0;
	UINT alive = 0;
	UINT padding0 = 0;
	UINT padding1 = 0;
};

//이미터에서 GPU로 전달되는 파티클 방출 요청 구조체(이미터당 하나)
struct ParticleSpawnRequest
{
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	UINT burstCount = 0;

	XMFLOAT3 direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
	float coneAngleDegrees = 0.0f;

	XMFLOAT3 acceleration = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float speedMin = 0.0f;

	float speedMax = 0.0f;
	float lifeTimeMin = 1.0f;
	float lifeTimeMax = 1.0f;
	float spawnDelayMin = 0.0f;

	float spawnDelayMax = 0.0f;
	float rotationMin = 0.0f;
	float rotationMax = 0.0f;
	float angularVelocityMin = 0.0f;

	float angularVelocityMax = 0.0f;
	float sizeScaleMin = 1.0f;
	float sizeScaleMax = 1.0f;
	UINT textureId = 0;

	XMFLOAT2 startSize = XMFLOAT2(1.0f, 1.0f);
	XMFLOAT2 endSize = XMFLOAT2(1.0f, 1.0f);

	XMFLOAT4 startColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4 endColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

	UINT frameMode = 0;
	UINT firstFrame = 0;
	UINT frameCount = 1;
	UINT loopAnimation = 0;

	UINT selectedFrameCount = 0;
	UINT billboardMode = 0;
	UINT renderGroup = 0;
	UINT blendMode = 0;

	UINT selectedFrames[PARTICLE_SELECTED_FRAME_CAPACITY] = {};
};

struct ParticleDrawArguments
{
	UINT vertexCountPerInstance = 6;
	UINT instanceCount = 0;
	UINT startVertexLocation = 0;
	UINT startInstanceLocation = 0;
};

struct ParticleComputeConstants
{
	float deltaTime = 0.0f;
	float totalTime = 0.0f;
	UINT spawnRequestCount = 0;
	UINT inputAliveBufferIndex = 0;

	UINT outputAliveBufferIndex = 1;
	UINT randomSeed = 0;
	UINT maxParticles = MAX_GPU_PARTICLES;
	UINT spawnRequestBaseIndex = 0;
};

struct ParticleGraphicsConstants
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
	UINT renderGroup = 0;

	float inverseTextureWidth = 1.0f;
	float inverseTextureHeight = 1.0f;
	UINT padding0 = 0;
	UINT padding1 = 0;
};

static_assert(sizeof(ParticleComputeConstants) == sizeof(UINT) * PARTICLE_COMPUTE_ROOT_CONSTANT_COUNT,
	"ParticleComputeConstants must match the compute root constants.");

static_assert(sizeof(ParticleGraphicsConstants) == sizeof(UINT) * PARTICLE_GRAPHICS_ROOT_CONSTANT_COUNT,
	"ParticleGraphicsConstants must match the graphics root constants.");

static_assert(sizeof(GPUParticle) == 144, "GPUParticle size must match the HLSL structure.");
static_assert(sizeof(ParticleSpawnRequest) == 240, "ParticleSpawnRequest size must match the HLSL structure.");
static_assert(sizeof(ParticleDrawArguments) == sizeof(D3D12_DRAW_ARGUMENTS),
	"ParticleDrawArguments must match D3D12_DRAW_ARGUMENTS.");