#pragma once

#include "stdafx.h"
#include "Effect.h"
#include "Particle.h"
#include "Shader.h"

class CCamera;
class CGameObject;
class CLaserShader;

class EffectManager
{
public:
	EffectManager();
	~EffectManager();

	void Initialize(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature);

	void Release();
	void ReleaseUploadBuffers();

	void Update(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState);
	void RenderGpuParticles(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	void RequestPlayEffect(EFFECT_TYPE type, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 up);
	void PlayEffectByID(const EffectSpawnDesc& desc);

	void UpdateLaser(int ownerId, const XMFLOAT3& origin, const XMFLOAT3& right, const XMFLOAT3& up,
		const XMFLOAT3& dir, float fLength);

	void HideLaser(int ownerId);

private:
	CGameObject* GetOrCreateLaserObject(int ownerId);

	void BuildEffectBasis(const XMFLOAT3& direction, XMFLOAT3& outRight, XMFLOAT3& outUp);

	bool InitializeParticleSystemResources(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	bool CreateParticleDescriptorHeap(ID3D12Device* pd3dDevice);
	bool CreateParticleComputeRootSignature(ID3D12Device* pd3dDevice);
	bool CreateParticleGraphicsRootSignature(ID3D12Device* pd3dDevice);
	bool CreateParticleDrawCommandSignature(ID3D12Device* pd3dDevice);
	bool CreateParticleComputePipelineStates(ID3D12Device* pd3dDevice);
	bool CreateParticleGraphicsPipelineStates(ID3D12Device* pd3dDevice);

	bool CompileParticleComputeShader(const wchar_t* shaderFileName, const char* entryPoint, ID3DBlob** ppShaderBlob);
	bool CompileParticleGraphicsShader(const wchar_t* shaderFileName, const char* entryPoint,
		const char* shaderProfile, ID3DBlob** ppShaderBlob);

	bool CreateParticleGpuBuffers(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	bool InitializeParticleBufferData(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	bool CreateParticleDescriptors(ID3D12Device* pd3dDevice);

	ID3D12Resource* CreateParticleDefaultBuffer(ID3D12Device* pd3dDevice, UINT64 bufferSize,
		D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState);

	ID3D12Resource* CreateParticleUploadBuffer(ID3D12Device* pd3dDevice, UINT64 bufferSize);

	bool QueueParticleEffect(const EffectSpawnDesc& desc);

	void ExecuteParticleCompute(ID3D12GraphicsCommandList* pd3dCommandList);
	UINT UploadPendingParticleSpawnRequests();

	void ReleaseParticleGpuBuffers();
	void ReleaseParticleSystemResources();

	D3D12_CPU_DESCRIPTOR_HANDLE GetParticleCpuDescriptorHandle(UINT descriptorIndex) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetParticleGpuDescriptorHandle(UINT descriptorIndex) const;

private:
	ID3D12Device* m_pd3dDevice = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCommandList = nullptr;
	ID3D12RootSignature* m_pd3dGraphicsRootSignature = nullptr;

	CLaserShader* m_pLaserShader = nullptr;
	std::unordered_map<int, CGameObject*> m_LaserObjects;

	ID3D12RootSignature* m_pd3dParticleComputeRootSignature = nullptr;
	ID3D12RootSignature* m_pd3dParticleGraphicsRootSignature = nullptr;
	ID3D12CommandSignature* m_pd3dParticleDrawCommandSignature = nullptr;

	ID3D12PipelineState* m_pd3dParticleResetPipelineState = nullptr;
	ID3D12PipelineState* m_pd3dParticleUpdatePipelineState = nullptr;
	ID3D12PipelineState* m_pd3dParticleSpawnPipelineState = nullptr;

	ID3D12PipelineState* m_pd3dParticleAlphaPipelineState = nullptr;
	ID3D12PipelineState* m_pd3dParticleAdditivePipelineState = nullptr;

	ID3D12DescriptorHeap* m_pd3dParticleDescriptorHeap = nullptr;
	UINT m_nParticleDescriptorIncrementSize = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_d3dParticleCpuDescriptorStartHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dParticleGpuDescriptorStartHandle{};

	ID3D12Resource* m_pd3dParticleBuffer = nullptr;
	ID3D12Resource* m_pd3dParticleAliveIndexBuffers[2] = {};
	ID3D12Resource* m_pd3dParticleDeadIndexBuffer = nullptr;
	ID3D12Resource* m_pd3dParticleCounterBuffer = nullptr;

	ID3D12Resource* m_pd3dParticleRenderIndexBuffers[static_cast<UINT>(ParticleRenderGroup::COUNT)] = {};
	ID3D12Resource* m_pd3dParticleIndirectArgumentBuffer = nullptr;

	ID3D12Resource* m_pd3dParticleSpawnRequestBuffer = nullptr;
	ParticleSpawnRequest* m_pMappedParticleSpawnRequests = nullptr;

	ID3D12Resource* m_pd3dParticleDeadIndexUploadBuffer = nullptr;
	ID3D12Resource* m_pd3dParticleCounterUploadBuffer = nullptr;
	ID3D12Resource* m_pd3dParticleIndirectArgumentUploadBuffer = nullptr;

	std::vector<ParticleSpawnRequest> m_PendingParticleSpawnRequests;

	UINT m_nCurrentParticleAliveBufferIndex = 0;
	UINT m_nParticleUploadFrameIndex = 0;
	UINT m_nParticleRandomSeed = 1;

	float m_fParticleDeltaTime = 0.0f;
	float m_fParticleTotalTime = 0.0f;

	bool m_bParticleComputeExecutedThisFrame = false;
	bool m_bParticleRequestOverflowLogged = false;
	bool m_bParticleSystemResourcesReady = false;

};