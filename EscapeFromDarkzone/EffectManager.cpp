#include "stdafx.h"
#include "Scene.h"
#include "Object.h"
#include "Camera.h"
#include "ResourceManager.h"
#include "ParticleResource.h"
#include "EffectManager.h"


EffectManager::EffectManager()
{
	m_PendingParticleSpawnRequests.reserve(MAX_PARTICLE_SPAWN_REQUESTS);
}

EffectManager::~EffectManager()
{
	Release();
}

D3D12_CPU_DESCRIPTOR_HANDLE EffectManager::GetParticleCpuDescriptorHandle(UINT descriptorIndex) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = m_d3dParticleCpuDescriptorStartHandle;
	descriptorHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * m_nParticleDescriptorIncrementSize;

	return descriptorHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE EffectManager::GetParticleGpuDescriptorHandle(UINT descriptorIndex) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle = m_d3dParticleGpuDescriptorStartHandle;
	descriptorHandle.ptr += static_cast<UINT64>(descriptorIndex) * m_nParticleDescriptorIncrementSize;

	return descriptorHandle;
}

ID3D12Resource* EffectManager::CreateParticleDefaultBuffer(ID3D12Device* pd3dDevice, UINT64 bufferSize,
	D3D12_RESOURCE_FLAGS resourceFlags, D3D12_RESOURCE_STATES initialState)
{
	if (!pd3dDevice || bufferSize == 0)
	{
		return nullptr;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = resourceFlags;

	ID3D12Resource* pd3dResource = nullptr;

	HRESULT hResult = pd3dDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		initialState, nullptr, IID_PPV_ARGS(&pd3dResource));

	if (FAILED(hResult))
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle default buffer creation failed. Size=%llu, HRESULT=0x%08X\n",
			bufferSize, static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return nullptr;
	}

	return pd3dResource;
}

ID3D12Resource* EffectManager::CreateParticleUploadBuffer(ID3D12Device* pd3dDevice, UINT64 bufferSize)
{
	if (!pd3dDevice || bufferSize == 0)
	{
		return nullptr;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* pd3dResource = nullptr;

	HRESULT hResult = pd3dDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pd3dResource));

	if (FAILED(hResult))
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle upload buffer creation failed. Size=%llu, HRESULT=0x%08X\n",
			bufferSize, static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return nullptr;
	}

	return pd3dResource;
}

bool EffectManager::CreateParticleDescriptorHeap(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice)
	{
		OutputDebugStringW(L"[EffectManager] Particle descriptor heap creation failed. Device is null.\n");
		return false;
	}

	if (m_pd3dParticleDescriptorHeap)
	{
		m_pd3dParticleDescriptorHeap->Release();
		m_pd3dParticleDescriptorHeap = nullptr;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = PARTICLE_DESCRIPTOR_COUNT;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;

	HRESULT hResult = pd3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_pd3dParticleDescriptorHeap));

	if (FAILED(hResult) || !m_pd3dParticleDescriptorHeap)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle descriptor heap creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	m_nParticleDescriptorIncrementSize = pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dParticleCpuDescriptorStartHandle = m_pd3dParticleDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_d3dParticleGpuDescriptorStartHandle = m_pd3dParticleDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	m_pd3dParticleDescriptorHeap->SetName(L"Particle CBV SRV UAV Descriptor Heap");

	OutputDebugStringW(L"[EffectManager] Particle descriptor heap created.\n");

	return true;
}

bool EffectManager::CreateParticleComputeRootSignature(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice)
	{
		OutputDebugStringW(L"[EffectManager] Particle compute root signature creation failed. Device is null.\n");
		return false;
	}

	if (m_pd3dParticleComputeRootSignature)
	{
		m_pd3dParticleComputeRootSignature->Release();
		m_pd3dParticleComputeRootSignature = nullptr;
	}

	D3D12_DESCRIPTOR_RANGE descriptorRanges[2]{};

	descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[0].NumDescriptors = 1;
	descriptorRanges[0].BaseShaderRegister = 0;
	descriptorRanges[0].RegisterSpace = 0;
	descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRanges[1].NumDescriptors = 11;
	descriptorRanges[1].BaseShaderRegister = 0;
	descriptorRanges[1].RegisterSpace = 0;
	descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[0].Constants.ShaderRegister = 0;
	rootParameters[0].Constants.RegisterSpace = 0;
	rootParameters[0].Constants.Num32BitValues = PARTICLE_COMPUTE_ROOT_CONSTANT_COUNT;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* pSignatureBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);

	if (FAILED(hResult))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
		}

		if (pSignatureBlob) pSignatureBlob->Release();
		if (pErrorBlob) pErrorBlob->Release();

		OutputDebugStringW(L"[EffectManager] Particle compute root signature serialization failed.\n");
		return false;
	}

	hResult = pd3dDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_pd3dParticleComputeRootSignature));

	if (pSignatureBlob) pSignatureBlob->Release();
	if (pErrorBlob) pErrorBlob->Release();

	if (FAILED(hResult) || !m_pd3dParticleComputeRootSignature)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle compute root signature creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	m_pd3dParticleComputeRootSignature->SetName(L"Particle Compute Root Signature");

	OutputDebugStringW(L"[EffectManager] Particle compute root signature created.\n");

	return true;
}

bool EffectManager::CreateParticleGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice)
	{
		OutputDebugStringW(L"[EffectManager] Particle graphics root signature creation failed. Device is null.\n");
		return false;
	}

	if (m_pd3dParticleGraphicsRootSignature)
	{
		m_pd3dParticleGraphicsRootSignature->Release();
		m_pd3dParticleGraphicsRootSignature = nullptr;
	}

	D3D12_DESCRIPTOR_RANGE descriptorRanges[3]{};

	descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[0].NumDescriptors = 1;
	descriptorRanges[0].BaseShaderRegister = 0;
	descriptorRanges[0].RegisterSpace = 0;
	descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[1].NumDescriptors = 1;
	descriptorRanges[1].BaseShaderRegister = 1;
	descriptorRanges[1].RegisterSpace = 0;
	descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[2].NumDescriptors = 1;
	descriptorRanges[2].BaseShaderRegister = 2;
	descriptorRanges[2].RegisterSpace = 0;
	descriptorRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[4].Constants.ShaderRegister = 1;
	rootParameters[4].Constants.RegisterSpace = 0;
	rootParameters[4].Constants.Num32BitValues = PARTICLE_GRAPHICS_ROOT_CONSTANT_COUNT;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ID3DBlob* pSignatureBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;

	HRESULT hResult = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);

	if (FAILED(hResult))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
		}

		if (pSignatureBlob) pSignatureBlob->Release();
		if (pErrorBlob) pErrorBlob->Release();

		OutputDebugStringW(L"[EffectManager] Particle graphics root signature serialization failed.\n");
		return false;
	}

	hResult = pd3dDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_pd3dParticleGraphicsRootSignature));

	if (pSignatureBlob) pSignatureBlob->Release();
	if (pErrorBlob) pErrorBlob->Release();

	if (FAILED(hResult) || !m_pd3dParticleGraphicsRootSignature)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle graphics root signature creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	m_pd3dParticleGraphicsRootSignature->SetName(L"Particle Graphics Root Signature");

	OutputDebugStringW(L"[EffectManager] Particle graphics root signature created.\n");

	return true;
}

bool EffectManager::CreateParticleDrawCommandSignature(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice)
	{
		OutputDebugStringW(L"[EffectManager] Particle draw command signature creation failed. Device is null.\n");
		return false;
	}

	if (m_pd3dParticleDrawCommandSignature)
	{
		m_pd3dParticleDrawCommandSignature->Release();
		m_pd3dParticleDrawCommandSignature = nullptr;
	}

	D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDesc{};
	indirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{};
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.pArgumentDescs = &indirectArgumentDesc;
	commandSignatureDesc.NodeMask = 0;

	HRESULT hResult = pd3dDevice->CreateCommandSignature(&commandSignatureDesc, nullptr,
		IID_PPV_ARGS(&m_pd3dParticleDrawCommandSignature));

	if (FAILED(hResult) || !m_pd3dParticleDrawCommandSignature)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle draw command signature creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	m_pd3dParticleDrawCommandSignature->SetName(L"Particle Draw Command Signature");

	OutputDebugStringW(L"[EffectManager] Particle draw command signature created.\n");

	return true;
}

bool EffectManager::CompileParticleComputeShader(const wchar_t* shaderFileName, const char* entryPoint, ID3DBlob** ppShaderBlob)
{
	if (!shaderFileName || !entryPoint || !ppShaderBlob)
	{
		return false;
	}

	*ppShaderBlob = nullptr;

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
	compileFlags |= D3DCOMPILE_DEBUG;
	compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob* pErrorBlob = nullptr;

	HRESULT hResult = D3DCompileFromFile(shaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint,
		"cs_5_1", compileFlags, 0, ppShaderBlob, &pErrorBlob);

	if (FAILED(hResult))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}

		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle compute shader compilation failed. Entry=%S, HRESULT=0x%08X\n",
			entryPoint, static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	if (pErrorBlob)
	{
		pErrorBlob->Release();
	}

	return true;
}

bool EffectManager::CompileParticleGraphicsShader(const wchar_t* shaderFileName, const char* entryPoint,
	const char* shaderProfile, ID3DBlob** ppShaderBlob)
{
	if (!shaderFileName || !entryPoint || !shaderProfile || !ppShaderBlob)
	{
		return false;
	}

	*ppShaderBlob = nullptr;

	UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
	compileFlags |= D3DCOMPILE_DEBUG;
	compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob* pErrorBlob = nullptr;

	HRESULT hResult = D3DCompileFromFile(shaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint,
		shaderProfile, compileFlags, 0, ppShaderBlob, &pErrorBlob);

	if (FAILED(hResult))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}

		wchar_t debugText[256];
		swprintf_s(debugText,
			L"[EffectManager] Particle graphics shader compilation failed. Entry=%S, Profile=%S, HRESULT=0x%08X\n",
			entryPoint, shaderProfile, static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	if (pErrorBlob)
	{
		pErrorBlob->Release();
	}

	return true;
}

bool EffectManager::CreateParticleComputePipelineStates(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice || !m_pd3dParticleComputeRootSignature)
	{
		OutputDebugStringW(L"[EffectManager] Particle compute PSO creation failed. Device or root signature is null.\n");
		return false;
	}

	if (m_pd3dParticleResetPipelineState)
	{
		m_pd3dParticleResetPipelineState->Release();
		m_pd3dParticleResetPipelineState = nullptr;
	}

	if (m_pd3dParticleUpdatePipelineState)
	{
		m_pd3dParticleUpdatePipelineState->Release();
		m_pd3dParticleUpdatePipelineState = nullptr;
	}

	if (m_pd3dParticleSpawnPipelineState)
	{
		m_pd3dParticleSpawnPipelineState->Release();
		m_pd3dParticleSpawnPipelineState = nullptr;
	}

	ID3DBlob* pResetShaderBlob = nullptr;
	ID3DBlob* pUpdateShaderBlob = nullptr;
	ID3DBlob* pSpawnShaderBlob = nullptr;

	if (!CompileParticleComputeShader(L"ParticleCompute.hlsl", "CSResetParticleFrame", &pResetShaderBlob) ||
		!CompileParticleComputeShader(L"ParticleCompute.hlsl", "CSUpdateParticles", &pUpdateShaderBlob) ||
		!CompileParticleComputeShader(L"ParticleCompute.hlsl", "CSSpawnParticles", &pSpawnShaderBlob))
	{
		if (pResetShaderBlob) pResetShaderBlob->Release();
		if (pUpdateShaderBlob) pUpdateShaderBlob->Release();
		if (pSpawnShaderBlob) pSpawnShaderBlob->Release();

		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = m_pd3dParticleComputeRootSignature;
	pipelineStateDesc.NodeMask = 0;
	pipelineStateDesc.CachedPSO = {};
	pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	pipelineStateDesc.CS.pShaderBytecode = pResetShaderBlob->GetBufferPointer();
	pipelineStateDesc.CS.BytecodeLength = pResetShaderBlob->GetBufferSize();

	HRESULT hResult = pd3dDevice->CreateComputePipelineState(&pipelineStateDesc,
		IID_PPV_ARGS(&m_pd3dParticleResetPipelineState));

	if (SUCCEEDED(hResult))
	{
		pipelineStateDesc.CS.pShaderBytecode = pUpdateShaderBlob->GetBufferPointer();
		pipelineStateDesc.CS.BytecodeLength = pUpdateShaderBlob->GetBufferSize();

		hResult = pd3dDevice->CreateComputePipelineState(&pipelineStateDesc,
			IID_PPV_ARGS(&m_pd3dParticleUpdatePipelineState));
	}

	if (SUCCEEDED(hResult))
	{
		pipelineStateDesc.CS.pShaderBytecode = pSpawnShaderBlob->GetBufferPointer();
		pipelineStateDesc.CS.BytecodeLength = pSpawnShaderBlob->GetBufferSize();

		hResult = pd3dDevice->CreateComputePipelineState(&pipelineStateDesc,
			IID_PPV_ARGS(&m_pd3dParticleSpawnPipelineState));
	}

	pResetShaderBlob->Release();
	pUpdateShaderBlob->Release();
	pSpawnShaderBlob->Release();

	if (FAILED(hResult) || !m_pd3dParticleResetPipelineState || !m_pd3dParticleUpdatePipelineState ||
		!m_pd3dParticleSpawnPipelineState)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[EffectManager] Particle compute PSO creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult));

		OutputDebugStringW(debugText);
		return false;
	}

	m_pd3dParticleResetPipelineState->SetName(L"Particle Reset Compute PSO");
	m_pd3dParticleUpdatePipelineState->SetName(L"Particle Update Compute PSO");
	m_pd3dParticleSpawnPipelineState->SetName(L"Particle Spawn Compute PSO");

	OutputDebugStringW(L"[EffectManager] Particle compute pipeline states created.\n");

	return true;
}

bool EffectManager::CreateParticleGraphicsPipelineStates(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice || !m_pd3dParticleGraphicsRootSignature)
	{
		OutputDebugStringW(L"[EffectManager] Particle graphics PSO creation failed. Device or root signature is null.\n");
		return false;
	}

	if (m_pd3dParticleAlphaPipelineState)
	{
		m_pd3dParticleAlphaPipelineState->Release();
		m_pd3dParticleAlphaPipelineState = nullptr;
	}

	if (m_pd3dParticleAdditivePipelineState)
	{
		m_pd3dParticleAdditivePipelineState->Release();
		m_pd3dParticleAdditivePipelineState = nullptr;
	}

	ID3DBlob* pVertexShaderBlob = nullptr;
	ID3DBlob* pPixelShaderBlob = nullptr;

	if (!CompileParticleGraphicsShader(L"ParticleRender.hlsl", "VSParticle", "vs_5_1", &pVertexShaderBlob) ||
		!CompileParticleGraphicsShader(L"ParticleRender.hlsl", "PSParticle", "ps_5_1", &pPixelShaderBlob))
	{
		if (pVertexShaderBlob) pVertexShaderBlob->Release();
		if (pPixelShaderBlob) pPixelShaderBlob->Release();

		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = m_pd3dParticleGraphicsRootSignature;

	pipelineStateDesc.VS.pShaderBytecode = pVertexShaderBlob->GetBufferPointer();
	pipelineStateDesc.VS.BytecodeLength = pVertexShaderBlob->GetBufferSize();

	pipelineStateDesc.PS.pShaderBytecode = pPixelShaderBlob->GetBufferPointer();
	pipelineStateDesc.PS.BytecodeLength = pPixelShaderBlob->GetBufferSize();

	pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineStateDesc.RasterizerState.FrontCounterClockwise = FALSE;
	pipelineStateDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	pipelineStateDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	pipelineStateDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	pipelineStateDesc.RasterizerState.DepthClipEnable = TRUE;
	pipelineStateDesc.RasterizerState.MultisampleEnable = FALSE;
	pipelineStateDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	pipelineStateDesc.RasterizerState.ForcedSampleCount = 0;
	pipelineStateDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	pipelineStateDesc.BlendState.AlphaToCoverageEnable = FALSE;
	pipelineStateDesc.BlendState.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC& renderTargetBlend = pipelineStateDesc.BlendState.RenderTarget[0];
	renderTargetBlend.BlendEnable = TRUE;
	renderTargetBlend.LogicOpEnable = FALSE;
	renderTargetBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	renderTargetBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	renderTargetBlend.BlendOp = D3D12_BLEND_OP_ADD;
	renderTargetBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	renderTargetBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
	renderTargetBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	renderTargetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
	renderTargetBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	pipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
	pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
	pipelineStateDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipelineStateDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
	pipelineStateDesc.InputLayout.NumElements = 0;

	pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleDesc.Quality = 0;
	pipelineStateDesc.SampleMask = UINT_MAX;
	pipelineStateDesc.NodeMask = 0;
	pipelineStateDesc.CachedPSO = {};
	pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT alphaResult = pd3dDevice->CreateGraphicsPipelineState(&pipelineStateDesc,
		IID_PPV_ARGS(&m_pd3dParticleAlphaPipelineState));

	HRESULT additiveResult = E_FAIL;

	if (SUCCEEDED(alphaResult))
	{
		renderTargetBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlend.DestBlend = D3D12_BLEND_ONE;

		additiveResult = pd3dDevice->CreateGraphicsPipelineState(&pipelineStateDesc,
			IID_PPV_ARGS(&m_pd3dParticleAdditivePipelineState));
	}

	pVertexShaderBlob->Release();
	pPixelShaderBlob->Release();

	if (FAILED(alphaResult) || FAILED(additiveResult) ||
		!m_pd3dParticleAlphaPipelineState || !m_pd3dParticleAdditivePipelineState)
	{
		wchar_t debugText[256];
		swprintf_s(debugText,
			L"[EffectManager] Particle graphics PSO creation failed. Alpha=0x%08X, Additive=0x%08X\n",
			static_cast<unsigned int>(alphaResult), static_cast<unsigned int>(additiveResult));

		OutputDebugStringW(debugText);

		if (m_pd3dParticleAlphaPipelineState)
		{
			m_pd3dParticleAlphaPipelineState->Release();
			m_pd3dParticleAlphaPipelineState = nullptr;
		}

		if (m_pd3dParticleAdditivePipelineState)
		{
			m_pd3dParticleAdditivePipelineState->Release();
			m_pd3dParticleAdditivePipelineState = nullptr;
		}

		return false;
	}

	m_pd3dParticleAlphaPipelineState->SetName(L"Particle Alpha Graphics PSO");
	m_pd3dParticleAdditivePipelineState->SetName(L"Particle Additive Graphics PSO");

	OutputDebugStringW(L"[EffectManager] Particle graphics pipeline states created.\n");

	return true;
}

bool EffectManager::CreateParticleGpuBuffers(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!pd3dDevice || !pd3dCommandList)
	{
		OutputDebugStringW(L"[EffectManager] Particle GPU buffer creation failed. Invalid argument.\n");
		return false;
	}

	ReleaseParticleGpuBuffers();

	const UINT64 particleBufferSize = sizeof(GPUParticle) * static_cast<UINT64>(MAX_GPU_PARTICLES);
	const UINT64 indexBufferSize = sizeof(UINT) * static_cast<UINT64>(MAX_GPU_PARTICLES);
	const UINT64 counterBufferSize = sizeof(UINT) * static_cast<UINT64>(PARTICLE_COUNTER_COUNT);
	const UINT64 singleFrameSpawnRequestBufferSize = sizeof(ParticleSpawnRequest) * static_cast<UINT64>(MAX_PARTICLE_SPAWN_REQUESTS);
	const UINT64 spawnRequestBufferSize = singleFrameSpawnRequestBufferSize * PARTICLE_UPLOAD_FRAME_COUNT;
	const UINT64 indirectArgumentBufferSize = sizeof(ParticleDrawArguments) * static_cast<UINT64>(ParticleRenderGroup::COUNT);

	m_pd3dParticleBuffer = CreateParticleDefaultBuffer(pd3dDevice, particleBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	m_pd3dParticleAliveIndexBuffers[0] = CreateParticleDefaultBuffer(pd3dDevice, indexBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	m_pd3dParticleAliveIndexBuffers[1] = CreateParticleDefaultBuffer(pd3dDevice, indexBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	m_pd3dParticleDeadIndexBuffer = CreateParticleDefaultBuffer(pd3dDevice, indexBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	m_pd3dParticleCounterBuffer = CreateParticleDefaultBuffer(pd3dDevice, counterBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		m_pd3dParticleRenderIndexBuffers[i] = CreateParticleDefaultBuffer(pd3dDevice, indexBufferSize,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	m_pd3dParticleIndirectArgumentBuffer = CreateParticleDefaultBuffer(pd3dDevice, indirectArgumentBufferSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

	m_pd3dParticleSpawnRequestBuffer = CreateParticleUploadBuffer(pd3dDevice, spawnRequestBufferSize);

	if (!m_pd3dParticleBuffer || !m_pd3dParticleAliveIndexBuffers[0] || !m_pd3dParticleAliveIndexBuffers[1] ||
		!m_pd3dParticleDeadIndexBuffer || !m_pd3dParticleCounterBuffer || !m_pd3dParticleIndirectArgumentBuffer ||
		!m_pd3dParticleSpawnRequestBuffer)
	{
		OutputDebugStringW(L"[EffectManager] One or more required particle GPU buffers could not be created.\n");
		ReleaseParticleGpuBuffers();
		return false;
	}

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		if (!m_pd3dParticleRenderIndexBuffers[i])
		{
			OutputDebugStringW(L"[EffectManager] Particle render index buffer creation failed.\n");
			ReleaseParticleGpuBuffers();
			return false;
		}
	}

	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	HRESULT hResult = m_pd3dParticleSpawnRequestBuffer->Map(0, &readRange,
		reinterpret_cast<void**>(&m_pMappedParticleSpawnRequests));

	if (FAILED(hResult) || !m_pMappedParticleSpawnRequests)
	{
		OutputDebugStringW(L"[EffectManager] Particle spawn request buffer mapping failed.\n");
		ReleaseParticleGpuBuffers();
		return false;
	}

	ZeroMemory(m_pMappedParticleSpawnRequests, static_cast<SIZE_T>(spawnRequestBufferSize));

	m_pd3dParticleBuffer->SetName(L"Particle Pool Buffer");
	m_pd3dParticleAliveIndexBuffers[0]->SetName(L"Particle Alive Index Buffer 0");
	m_pd3dParticleAliveIndexBuffers[1]->SetName(L"Particle Alive Index Buffer 1");
	m_pd3dParticleDeadIndexBuffer->SetName(L"Particle Dead Index Buffer");
	m_pd3dParticleCounterBuffer->SetName(L"Particle Counter Buffer");
	m_pd3dParticleIndirectArgumentBuffer->SetName(L"Particle Indirect Argument Buffer");
	m_pd3dParticleSpawnRequestBuffer->SetName(L"Particle Spawn Request Triple Upload Buffer");

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		wchar_t bufferName[128];
		swprintf_s(bufferName, L"Particle Render Index Buffer %u", i);
		m_pd3dParticleRenderIndexBuffers[i]->SetName(bufferName);
	}

	if (!InitializeParticleBufferData(pd3dDevice, pd3dCommandList))
	{
		ReleaseParticleGpuBuffers();
		return false;
	}

	if (!CreateParticleDescriptors(pd3dDevice))
	{
		ReleaseParticleGpuBuffers();
		return false;
	}

	m_nCurrentParticleAliveBufferIndex = 0;
	m_nParticleUploadFrameIndex = 0;
	m_nParticleRandomSeed = 1;

	m_PendingParticleSpawnRequests.clear();

	OutputDebugStringW(L"[EffectManager] Particle GPU buffers created.\n");

	return true;
}

bool EffectManager::InitializeParticleBufferData(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!pd3dDevice || !pd3dCommandList || !m_pd3dParticleDeadIndexBuffer ||
		!m_pd3dParticleCounterBuffer || !m_pd3dParticleIndirectArgumentBuffer)
	{
		OutputDebugStringW(L"[EffectManager] Particle buffer initialization failed. Required resource is null.\n");
		return false;
	}

	std::vector<UINT> deadIndices(MAX_GPU_PARTICLES);

	for (UINT i = 0; i < MAX_GPU_PARTICLES; ++i)
	{
		deadIndices[i] = i;
	}

	UINT particleCounters[PARTICLE_COUNTER_COUNT] = {};
	particleCounters[PARTICLE_COUNTER_DEAD] = MAX_GPU_PARTICLES;

	ParticleDrawArguments drawArguments[static_cast<UINT>(ParticleRenderGroup::COUNT)] = {};

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		drawArguments[i].vertexCountPerInstance = 6;
		drawArguments[i].instanceCount = 0;
		drawArguments[i].startVertexLocation = 0;
		drawArguments[i].startInstanceLocation = 0;
	}

	const UINT64 deadIndexBufferSize = sizeof(UINT) * static_cast<UINT64>(MAX_GPU_PARTICLES);
	const UINT64 counterBufferSize = sizeof(particleCounters);
	const UINT64 indirectArgumentBufferSize = sizeof(drawArguments);

	m_pd3dParticleDeadIndexUploadBuffer = CreateParticleUploadBuffer(pd3dDevice, deadIndexBufferSize);
	m_pd3dParticleCounterUploadBuffer = CreateParticleUploadBuffer(pd3dDevice, counterBufferSize);
	m_pd3dParticleIndirectArgumentUploadBuffer = CreateParticleUploadBuffer(pd3dDevice, indirectArgumentBufferSize);

	if (!m_pd3dParticleDeadIndexUploadBuffer || !m_pd3dParticleCounterUploadBuffer ||
		!m_pd3dParticleIndirectArgumentUploadBuffer)
	{
		OutputDebugStringW(L"[EffectManager] Particle initialization upload buffer creation failed.\n");
		return false;
	}

	void* pMappedData = nullptr;
	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	HRESULT hResult = m_pd3dParticleDeadIndexUploadBuffer->Map(0, &readRange, &pMappedData);

	if (FAILED(hResult) || !pMappedData)
	{
		OutputDebugStringW(L"[EffectManager] Dead index upload buffer mapping failed.\n");
		return false;
	}

	memcpy(pMappedData, deadIndices.data(), static_cast<SIZE_T>(deadIndexBufferSize));
	m_pd3dParticleDeadIndexUploadBuffer->Unmap(0, nullptr);

	pMappedData = nullptr;
	hResult = m_pd3dParticleCounterUploadBuffer->Map(0, &readRange, &pMappedData);

	if (FAILED(hResult) || !pMappedData)
	{
		OutputDebugStringW(L"[EffectManager] Particle counter upload buffer mapping failed.\n");
		return false;
	}

	memcpy(pMappedData, particleCounters, static_cast<SIZE_T>(counterBufferSize));
	m_pd3dParticleCounterUploadBuffer->Unmap(0, nullptr);

	pMappedData = nullptr;
	hResult = m_pd3dParticleIndirectArgumentUploadBuffer->Map(0, &readRange, &pMappedData);

	if (FAILED(hResult) || !pMappedData)
	{
		OutputDebugStringW(L"[EffectManager] Particle indirect argument upload buffer mapping failed.\n");
		return false;
	}

	memcpy(pMappedData, drawArguments, static_cast<SIZE_T>(indirectArgumentBufferSize));
	m_pd3dParticleIndirectArgumentUploadBuffer->Unmap(0, nullptr);

	pd3dCommandList->CopyBufferRegion(m_pd3dParticleDeadIndexBuffer, 0,
		m_pd3dParticleDeadIndexUploadBuffer, 0, deadIndexBufferSize);

	pd3dCommandList->CopyBufferRegion(m_pd3dParticleCounterBuffer, 0,
		m_pd3dParticleCounterUploadBuffer, 0, counterBufferSize);

	pd3dCommandList->CopyBufferRegion(m_pd3dParticleIndirectArgumentBuffer, 0,
		m_pd3dParticleIndirectArgumentUploadBuffer, 0, indirectArgumentBufferSize);

	D3D12_RESOURCE_BARRIER resourceBarriers[3]{};

	resourceBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarriers[0].Transition.pResource = m_pd3dParticleDeadIndexBuffer;
	resourceBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	resourceBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	resourceBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarriers[1].Transition.pResource = m_pd3dParticleCounterBuffer;
	resourceBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	resourceBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	resourceBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarriers[2].Transition.pResource = m_pd3dParticleIndirectArgumentBuffer;
	resourceBarriers[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceBarriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	resourceBarriers[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	pd3dCommandList->ResourceBarrier(_countof(resourceBarriers), resourceBarriers);

	OutputDebugStringW(L"[EffectManager] Particle buffer initial data uploaded.\n");

	return true;
}

bool EffectManager::CreateParticleDescriptors(ID3D12Device* pd3dDevice)
{
	if (!pd3dDevice || !m_pd3dParticleDescriptorHeap)
	{
		OutputDebugStringW(L"[EffectManager] Particle descriptor creation failed. Device or heap is null.\n");
		return false;
	}

	ParticleResource* pParticleResource = ResourceManager::Instance().GetParticleResource();

	if (!pParticleResource || !pParticleResource->IsLoaded())
	{
		OutputDebugStringW(L"[EffectManager] Particle descriptor creation failed. ParticleResource is not ready.\n");
		return false;
	}

	auto CreateStructuredBufferUav = [&](ID3D12Resource* pd3dResource, UINT descriptorIndex, UINT elementCount, UINT structureStride)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = DXGI_FORMAT_UNKNOWN;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = elementCount;
			uavDesc.Buffer.StructureByteStride = structureStride;
			uavDesc.Buffer.CounterOffsetInBytes = 0;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

			pd3dDevice->CreateUnorderedAccessView(pd3dResource, nullptr, &uavDesc,
				GetParticleCpuDescriptorHandle(descriptorIndex));
		};

	auto CreateStructuredBufferSrv = [&](ID3D12Resource* pd3dResource, UINT descriptorIndex, UINT elementCount, UINT structureStride)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = elementCount;
			srvDesc.Buffer.StructureByteStride = structureStride;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			pd3dDevice->CreateShaderResourceView(pd3dResource, &srvDesc,
				GetParticleCpuDescriptorHandle(descriptorIndex));
		};

	CreateStructuredBufferUav(m_pd3dParticleBuffer, PARTICLE_DESCRIPTOR_PARTICLE_UAV,
		MAX_GPU_PARTICLES, sizeof(GPUParticle));

	CreateStructuredBufferUav(m_pd3dParticleAliveIndexBuffers[0], PARTICLE_DESCRIPTOR_ALIVE_0_UAV,
		MAX_GPU_PARTICLES, sizeof(UINT));

	CreateStructuredBufferUav(m_pd3dParticleAliveIndexBuffers[1], PARTICLE_DESCRIPTOR_ALIVE_1_UAV,
		MAX_GPU_PARTICLES, sizeof(UINT));

	CreateStructuredBufferUav(m_pd3dParticleDeadIndexBuffer, PARTICLE_DESCRIPTOR_DEAD_UAV,
		MAX_GPU_PARTICLES, sizeof(UINT));

	CreateStructuredBufferUav(m_pd3dParticleCounterBuffer, PARTICLE_DESCRIPTOR_COUNTER_UAV,
		PARTICLE_COUNTER_COUNT, sizeof(UINT));

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		CreateStructuredBufferUav(m_pd3dParticleRenderIndexBuffers[i],
			PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ALPHA_UAV + i, MAX_GPU_PARTICLES, sizeof(UINT));
	}

	CreateStructuredBufferUav(m_pd3dParticleIndirectArgumentBuffer, PARTICLE_DESCRIPTOR_INDIRECT_ARGUMENT_UAV,
		static_cast<UINT>(ParticleRenderGroup::COUNT), sizeof(ParticleDrawArguments));

	CreateStructuredBufferSrv(m_pd3dParticleSpawnRequestBuffer, PARTICLE_DESCRIPTOR_SPAWN_REQUEST_SRV,
		MAX_PARTICLE_SPAWN_REQUESTS * PARTICLE_UPLOAD_FRAME_COUNT, sizeof(ParticleSpawnRequest));

	CreateStructuredBufferSrv(m_pd3dParticleAliveIndexBuffers[0], PARTICLE_DESCRIPTOR_ALIVE_0_SRV,
		MAX_GPU_PARTICLES, sizeof(UINT));

	CreateStructuredBufferSrv(m_pd3dParticleAliveIndexBuffers[1], PARTICLE_DESCRIPTOR_ALIVE_1_SRV,
		MAX_GPU_PARTICLES, sizeof(UINT));

	CreateStructuredBufferSrv(m_pd3dParticleBuffer, PARTICLE_DESCRIPTOR_PARTICLE_SRV,
		MAX_GPU_PARTICLES, sizeof(GPUParticle));

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		CreateStructuredBufferSrv(m_pd3dParticleRenderIndexBuffers[i],
			PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ALPHA_SRV + i, MAX_GPU_PARTICLES, sizeof(UINT));
	}

	CTexture* pExplosionTexture = pParticleResource->GetTexture(ParticleTextureID::EXPLOSION);
	CTexture* pRifleSparkTexture = pParticleResource->GetTexture(ParticleTextureID::SPARK_RIFLE_SMG);
	CTexture* pShotgunSparkTexture = pParticleResource->GetTexture(ParticleTextureID::SPARK_SHOTGUN);

	if (!pExplosionTexture || !pRifleSparkTexture || !pShotgunSparkTexture ||
		!pExplosionTexture->GetResource(0) || !pRifleSparkTexture->GetResource(0) ||
		!pShotgunSparkTexture->GetResource(0))
	{
		OutputDebugStringW(L"[EffectManager] Particle texture descriptor creation failed. Texture is null.\n");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC explosionSrvDesc = pExplosionTexture->GetShaderResourceViewDesc(0);
	D3D12_SHADER_RESOURCE_VIEW_DESC rifleSparkSrvDesc = pRifleSparkTexture->GetShaderResourceViewDesc(0);
	D3D12_SHADER_RESOURCE_VIEW_DESC shotgunSparkSrvDesc = pShotgunSparkTexture->GetShaderResourceViewDesc(0);

	pd3dDevice->CreateShaderResourceView(pExplosionTexture->GetResource(0), &explosionSrvDesc,
		GetParticleCpuDescriptorHandle(PARTICLE_DESCRIPTOR_TEXTURE_EXPLOSION_SRV));

	pd3dDevice->CreateShaderResourceView(pRifleSparkTexture->GetResource(0), &rifleSparkSrvDesc,
		GetParticleCpuDescriptorHandle(PARTICLE_DESCRIPTOR_TEXTURE_SPARK_RIFLE_SMG_SRV));

	pd3dDevice->CreateShaderResourceView(pShotgunSparkTexture->GetResource(0), &shotgunSparkSrvDesc,
		GetParticleCpuDescriptorHandle(PARTICLE_DESCRIPTOR_TEXTURE_SPARK_SHOTGUN_SRV));

	OutputDebugStringW(L"[EffectManager] Particle SRV and UAV descriptors created.\n");

	return true;
}

ParticleRenderGroup EffectManager::ResolveParticleRenderGroup(ParticleTextureID textureId, ParticleBlendMode blendMode) const
{
	switch (textureId)
	{
	case ParticleTextureID::EXPLOSION:
		return (blendMode == ParticleBlendMode::ADDITIVE) ?
			ParticleRenderGroup::EXPLOSION_ADDITIVE : ParticleRenderGroup::EXPLOSION_ALPHA;

	case ParticleTextureID::SPARK_SHOTGUN:
		return ParticleRenderGroup::SHOTGUN_ADDITIVE;

	case ParticleTextureID::SPARK_RIFLE_SMG:
		return (blendMode == ParticleBlendMode::ADDITIVE) ?
			ParticleRenderGroup::RIFLE_ADDITIVE : ParticleRenderGroup::RIFLE_ALPHA;

	default:
		return ParticleRenderGroup::EXPLOSION_ALPHA;
	}
}

bool EffectManager::QueueParticleEffect(const EffectSpawnDesc& desc)
{
	if (!m_bParticleSystemResourcesReady || !m_pMappedParticleSpawnRequests)
	{
		return false;
	}

	ParticleResource* pParticleResource = ResourceManager::Instance().GetParticleResource();

	if (!pParticleResource || !pParticleResource->IsLoaded())
	{
		return false;
	}

	const ParticleEffectDesc* pEffectDesc = pParticleResource->GetEffectDesc(desc.id);

	if (!pEffectDesc)
	{
		return false;
	}

	XMFLOAT3 effectDirection = desc.direction;

	if (Vector3::Length(effectDirection) < 0.0001f)
	{
		effectDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		effectDirection = Vector3::Normalize(effectDirection);
	}

	const bool isMuzzleSpark =
		desc.id == EffectID::SPARK ||
		desc.id == EffectID::SPARK_SHOTGUN ||
		desc.id == EffectID::SPARK_PISTOL;

	bool queuedAnyRequest = false;

	for (const ParticleEmitterDesc& emitterDesc : pEffectDesc->emitters)
	{
		if (m_PendingParticleSpawnRequests.size() >= MAX_PARTICLE_SPAWN_REQUESTS)
		{
			if (!m_bParticleRequestOverflowLogged)
			{
				OutputDebugStringW(L"[EffectManager] Particle spawn request capacity exceeded.\n");
				m_bParticleRequestOverflowLogged = true;
			}

			break;
		}

		ParticleSpawnRequest request{};

		request.position = desc.position;

		// 총구 Spark는 호출부에서 전달된 Socket_Muzzle 위치를 그대로 사용한다.
		if (!isMuzzleSpark && fabsf(emitterDesc.positionOffsetAlongDirection) > 0.0001f)
		{
			request.position.x += effectDirection.x * emitterDesc.positionOffsetAlongDirection;
			request.position.y += effectDirection.y * emitterDesc.positionOffsetAlongDirection;
			request.position.z += effectDirection.z * emitterDesc.positionOffsetAlongDirection;
		}

		// 수류탄의 불꽃과 불씨는 폭발 중심 높이에서 생성한다.
		if (desc.id == EffectID::GRENADE_EXPLOSION &&
			emitterDesc.billboardMode == ParticleBillboardMode::VELOCITY_ALIGNED)
		{
			request.position.y += 0.9f;
		}

		request.burstCount = emitterDesc.burstCount;
		request.direction = isMuzzleSpark ? effectDirection : emitterDesc.direction;
		request.coneAngleDegrees = emitterDesc.coneAngleDegrees;

		request.acceleration = emitterDesc.acceleration;
		request.speedMin = emitterDesc.speedMin;
		request.speedMax = emitterDesc.speedMax;

		request.lifeTimeMin = emitterDesc.lifeTimeMin;
		request.lifeTimeMax = emitterDesc.lifeTimeMax;

		request.spawnDelayMin = emitterDesc.spawnDelayMin;
		request.spawnDelayMax = emitterDesc.spawnDelayMax;

		request.rotationMin = emitterDesc.rotationMin;
		request.rotationMax = emitterDesc.rotationMax;

		request.angularVelocityMin = emitterDesc.angularVelocityMin;
		request.angularVelocityMax = emitterDesc.angularVelocityMax;

		request.sizeScaleMin = emitterDesc.sizeScaleMin;
		request.sizeScaleMax = emitterDesc.sizeScaleMax;

		request.textureId = static_cast<UINT>(emitterDesc.textureId);

		request.startSize = emitterDesc.startSize;
		request.endSize = emitterDesc.endSize;

		request.startColor = emitterDesc.startColor;
		request.endColor = emitterDesc.endColor;

		request.frameMode = static_cast<UINT>(emitterDesc.frameMode);
		request.firstFrame = emitterDesc.firstFrame;
		request.frameCount = emitterDesc.frameCount;
		request.loopAnimation = emitterDesc.loopAnimation;

		request.selectedFrameCount = min(emitterDesc.selectedFrameCount, PARTICLE_SELECTED_FRAME_CAPACITY);
		request.billboardMode = static_cast<UINT>(emitterDesc.billboardMode);
		request.renderGroup = static_cast<UINT>(
			ResolveParticleRenderGroup(emitterDesc.textureId, emitterDesc.blendMode));
		request.blendMode = static_cast<UINT>(emitterDesc.blendMode);

		for (UINT i = 0; i < request.selectedFrameCount; ++i)
		{
			request.selectedFrames[i] = emitterDesc.selectedFrames[i];
		}

		m_PendingParticleSpawnRequests.push_back(request);
		queuedAnyRequest = true;
	}

	return queuedAnyRequest;
}

UINT EffectManager::UploadPendingParticleSpawnRequests()
{
	if (!m_pMappedParticleSpawnRequests)
	{
		return 0;
	}

	UINT requestCount = static_cast<UINT>(m_PendingParticleSpawnRequests.size());
	requestCount = min(requestCount, MAX_PARTICLE_SPAWN_REQUESTS);

	if (requestCount == 0)
	{
		return 0;
	}

	UINT spawnRequestBaseIndex = m_nParticleUploadFrameIndex * MAX_PARTICLE_SPAWN_REQUESTS;
	ParticleSpawnRequest* pFrameSpawnRequests = m_pMappedParticleSpawnRequests + spawnRequestBaseIndex;

	memcpy(pFrameSpawnRequests, m_PendingParticleSpawnRequests.data(),
		static_cast<size_t>(requestCount) * sizeof(ParticleSpawnRequest));

	return requestCount;
}

void EffectManager::ExecuteParticleCompute(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!pd3dCommandList || !m_bParticleSystemResourcesReady || m_bParticleComputeExecutedThisFrame)
	{
		return;
	}

	if (!m_pd3dParticleDescriptorHeap || !m_pd3dParticleComputeRootSignature ||
		!m_pd3dParticleResetPipelineState || !m_pd3dParticleUpdatePipelineState ||
		!m_pd3dParticleSpawnPipelineState)
	{
		return;
	}

	UINT spawnRequestCount = UploadPendingParticleSpawnRequests();
	UINT spawnRequestBaseIndex = m_nParticleUploadFrameIndex * MAX_PARTICLE_SPAWN_REQUESTS;
	UINT inputAliveBufferIndex = m_nCurrentParticleAliveBufferIndex;
	UINT outputAliveBufferIndex = 1 - inputAliveBufferIndex;

	ParticleComputeConstants computeConstants{};
	computeConstants.deltaTime = m_fParticleDeltaTime;
	computeConstants.totalTime = m_fParticleTotalTime;
	computeConstants.spawnRequestCount = spawnRequestCount;
	computeConstants.inputAliveBufferIndex = inputAliveBufferIndex;
	computeConstants.outputAliveBufferIndex = outputAliveBufferIndex;
	computeConstants.randomSeed = m_nParticleRandomSeed++;
	computeConstants.maxParticles = MAX_GPU_PARTICLES;
	computeConstants.spawnRequestBaseIndex = spawnRequestBaseIndex;

	ID3D12DescriptorHeap* particleDescriptorHeaps[] = { m_pd3dParticleDescriptorHeap };

	pd3dCommandList->SetDescriptorHeaps(_countof(particleDescriptorHeaps), particleDescriptorHeaps);
	pd3dCommandList->SetComputeRootSignature(m_pd3dParticleComputeRootSignature);

	pd3dCommandList->SetComputeRoot32BitConstants(0, PARTICLE_COMPUTE_ROOT_CONSTANT_COUNT, &computeConstants, 0);
	pd3dCommandList->SetComputeRootDescriptorTable(1, GetParticleGpuDescriptorHandle(PARTICLE_DESCRIPTOR_SPAWN_REQUEST_SRV));
	pd3dCommandList->SetComputeRootDescriptorTable(2, GetParticleGpuDescriptorHandle(PARTICLE_DESCRIPTOR_PARTICLE_UAV));

	pd3dCommandList->SetPipelineState(m_pd3dParticleResetPipelineState);
	pd3dCommandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	uavBarrier.UAV.pResource = nullptr;

	pd3dCommandList->ResourceBarrier(1, &uavBarrier);

	pd3dCommandList->SetPipelineState(m_pd3dParticleUpdatePipelineState);

	const UINT updateThreadGroupCount = (MAX_GPU_PARTICLES + 255) / 256;
	pd3dCommandList->Dispatch(updateThreadGroupCount, 1, 1);

	pd3dCommandList->ResourceBarrier(1, &uavBarrier);

	if (spawnRequestCount > 0)
	{
		pd3dCommandList->SetPipelineState(m_pd3dParticleSpawnPipelineState);

		const UINT spawnThreadGroupCount = (spawnRequestCount + 63) / 64;
		pd3dCommandList->Dispatch(spawnThreadGroupCount, 1, 1);

		pd3dCommandList->ResourceBarrier(1, &uavBarrier);
	}

	m_nCurrentParticleAliveBufferIndex = outputAliveBufferIndex;
	m_nParticleUploadFrameIndex = (m_nParticleUploadFrameIndex + 1) % PARTICLE_UPLOAD_FRAME_COUNT;

	m_PendingParticleSpawnRequests.clear();
	m_bParticleRequestOverflowLogged = false;
	m_bParticleComputeExecutedThisFrame = true;

	ID3D12DescriptorHeap* pResourceDescriptorHeap = ResourceManager::Instance().GetDescriptorHeap();

	if (pResourceDescriptorHeap)
	{
		pd3dCommandList->SetDescriptorHeaps(1, &pResourceDescriptorHeap);
	}
}

bool EffectManager::InitializeParticleSystemResources(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!pd3dDevice || !pd3dCommandList)
	{
		OutputDebugStringW(L"[EffectManager] Particle system resource initialization failed. Invalid argument.\n");
		return false;
	}

	ReleaseParticleSystemResources();

	if (!ResourceManager::Instance().BuildParticleResource(pd3dDevice, pd3dCommandList))
	{
		OutputDebugStringW(L"[EffectManager] ParticleResource initialization failed.\n");
		return false;
	}

	ParticleResource* pParticleResource = ResourceManager::Instance().GetParticleResource();

	if (!pParticleResource || !pParticleResource->IsLoaded())
	{
		OutputDebugStringW(L"[EffectManager] ParticleResource is not ready.\n");
		return false;
	}

	if (!CreateParticleDescriptorHeap(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleComputeRootSignature(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleGraphicsRootSignature(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleDrawCommandSignature(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleComputePipelineStates(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleGraphicsPipelineStates(pd3dDevice))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	if (!CreateParticleGpuBuffers(pd3dDevice, pd3dCommandList))
	{
		ReleaseParticleSystemResources();
		return false;
	}

	m_bParticleSystemResourcesReady = true;

	OutputDebugStringW(L"[EffectManager] Particle system base resources ready.\n");

	return true;
}

void EffectManager::ReleaseParticleGpuBuffers()
{
	m_nCurrentParticleAliveBufferIndex = 0;
	m_nParticleUploadFrameIndex = 0;
	m_nParticleRandomSeed = 1;

	m_fParticleDeltaTime = 0.0f;
	m_fParticleTotalTime = 0.0f;

	m_bParticleComputeExecutedThisFrame = false;
	m_bParticleRequestOverflowLogged = false;

	m_PendingParticleSpawnRequests.clear();

	if (m_pd3dParticleSpawnRequestBuffer)
	{
		if (m_pMappedParticleSpawnRequests)
		{
			m_pd3dParticleSpawnRequestBuffer->Unmap(0, nullptr);
		}

		m_pd3dParticleSpawnRequestBuffer->Release();
		m_pd3dParticleSpawnRequestBuffer = nullptr;
	}

	m_pMappedParticleSpawnRequests = nullptr;

	if (m_pd3dParticleIndirectArgumentUploadBuffer)
	{
		m_pd3dParticleIndirectArgumentUploadBuffer->Release();
		m_pd3dParticleIndirectArgumentUploadBuffer = nullptr;
	}

	if (m_pd3dParticleCounterUploadBuffer)
	{
		m_pd3dParticleCounterUploadBuffer->Release();
		m_pd3dParticleCounterUploadBuffer = nullptr;
	}

	if (m_pd3dParticleDeadIndexUploadBuffer)
	{
		m_pd3dParticleDeadIndexUploadBuffer->Release();
		m_pd3dParticleDeadIndexUploadBuffer = nullptr;
	}

	if (m_pd3dParticleIndirectArgumentBuffer)
	{
		m_pd3dParticleIndirectArgumentBuffer->Release();
		m_pd3dParticleIndirectArgumentBuffer = nullptr;
	}

	for (UINT i = 0; i < static_cast<UINT>(ParticleRenderGroup::COUNT); ++i)
	{
		if (m_pd3dParticleRenderIndexBuffers[i])
		{
			m_pd3dParticleRenderIndexBuffers[i]->Release();
			m_pd3dParticleRenderIndexBuffers[i] = nullptr;
		}
	}

	if (m_pd3dParticleCounterBuffer)
	{
		m_pd3dParticleCounterBuffer->Release();
		m_pd3dParticleCounterBuffer = nullptr;
	}

	if (m_pd3dParticleDeadIndexBuffer)
	{
		m_pd3dParticleDeadIndexBuffer->Release();
		m_pd3dParticleDeadIndexBuffer = nullptr;
	}

	for (UINT i = 0; i < 2; ++i)
	{
		if (m_pd3dParticleAliveIndexBuffers[i])
		{
			m_pd3dParticleAliveIndexBuffers[i]->Release();
			m_pd3dParticleAliveIndexBuffers[i] = nullptr;
		}
	}

	if (m_pd3dParticleBuffer)
	{
		m_pd3dParticleBuffer->Release();
		m_pd3dParticleBuffer = nullptr;
	}
}

void EffectManager::ReleaseParticleSystemResources()
{
	m_bParticleSystemResourcesReady = false;

	ReleaseParticleGpuBuffers();

	if (m_pd3dParticleAdditivePipelineState)
	{
		m_pd3dParticleAdditivePipelineState->Release();
		m_pd3dParticleAdditivePipelineState = nullptr;
	}

	if (m_pd3dParticleAlphaPipelineState)
	{
		m_pd3dParticleAlphaPipelineState->Release();
		m_pd3dParticleAlphaPipelineState = nullptr;
	}

	if (m_pd3dParticleSpawnPipelineState)
	{
		m_pd3dParticleSpawnPipelineState->Release();
		m_pd3dParticleSpawnPipelineState = nullptr;
	}

	if (m_pd3dParticleUpdatePipelineState)
	{
		m_pd3dParticleUpdatePipelineState->Release();
		m_pd3dParticleUpdatePipelineState = nullptr;
	}

	if (m_pd3dParticleResetPipelineState)
	{
		m_pd3dParticleResetPipelineState->Release();
		m_pd3dParticleResetPipelineState = nullptr;
	}

	if (m_pd3dParticleDrawCommandSignature)
	{
		m_pd3dParticleDrawCommandSignature->Release();
		m_pd3dParticleDrawCommandSignature = nullptr;
	}

	if (m_pd3dParticleGraphicsRootSignature)
	{
		m_pd3dParticleGraphicsRootSignature->Release();
		m_pd3dParticleGraphicsRootSignature = nullptr;
	}

	if (m_pd3dParticleComputeRootSignature)
	{
		m_pd3dParticleComputeRootSignature->Release();
		m_pd3dParticleComputeRootSignature = nullptr;
	}

	if (m_pd3dParticleDescriptorHeap)
	{
		m_pd3dParticleDescriptorHeap->Release();
		m_pd3dParticleDescriptorHeap = nullptr;
	}

	m_nParticleDescriptorIncrementSize = 0;
	m_d3dParticleCpuDescriptorStartHandle = {};
	m_d3dParticleGpuDescriptorStartHandle = {};
}

//리소스 초기화
void EffectManager::Initialize(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	m_pd3dDevice = pd3dDevice;
	m_pd3dCommandList = pd3dCommandList;
	m_pd3dGraphicsRootSignature = pd3dGraphicsRootSignature;

	if (!InitializeParticleSystemResources(pd3dDevice, pd3dCommandList))
	{
		OutputDebugStringW(L"[EffectManager] Particle system initialization failed.\n");
	}

	// 레이저 쉐이더 생성
	m_pLaserShader = new CLaserShader();
	m_pLaserShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	m_pLaserShader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);

	//{
	//	CGameObject* pLaserObject = new CGameObject();
	//	pLaserObject->SetMesh(new CLaserMesh(pd3dDevice, pd3dCommandList));
	//	//pLaserObject->SetShader(m_pLaserShader);
	//
	//	m_pLaserShader->addObjects(pLaserObject);
	//
	//	XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
	//	pLaserObject->UpdateTransform(NULL);
	//
	//	m_LaserObjects.emplace(0, pLaserObject);
	//}
}

void EffectManager::Release()
{
	ReleaseParticleSystemResources();

	m_LaserObjects.clear();

	if (m_pLaserShader)
	{
		delete m_pLaserShader;
		m_pLaserShader = nullptr;
	}

	m_pd3dDevice = nullptr;
	m_pd3dCommandList = nullptr;
	m_pd3dGraphicsRootSignature = nullptr;
}

void EffectManager::ReleaseUploadBuffers()
{
	if (m_pLaserShader)
	{
		m_pLaserShader->ReleaseUploadBuffers();
	}

	if (m_pd3dParticleDeadIndexUploadBuffer)
	{
		m_pd3dParticleDeadIndexUploadBuffer->Release();
		m_pd3dParticleDeadIndexUploadBuffer = nullptr;
	}

	if (m_pd3dParticleCounterUploadBuffer)
	{
		m_pd3dParticleCounterUploadBuffer->Release();
		m_pd3dParticleCounterUploadBuffer = nullptr;
	}

	if (m_pd3dParticleIndirectArgumentUploadBuffer)
	{
		m_pd3dParticleIndirectArgumentUploadBuffer->Release();
		m_pd3dParticleIndirectArgumentUploadBuffer = nullptr;
	}
}

void EffectManager::RequestPlayEffect(EFFECT_TYPE type, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 up)
{
	if (type < 0 || type >= EFFECT_MAX)
	{
		return;
	}

	if (type == EFFECT_BOMB)
	{
		OutputDebugStringW(L"[EffectManager] Legacy EFFECT_BOMB request ignored. Use GRENADE_EXPLOSION.\n");
		return;
	}

	EffectID gpuEffectId = EffectID::NONE;

	switch (type)
	{
	case EFFECT_SPARK_RIFLE_SMG:
		gpuEffectId = EffectID::SPARK;
		break;

	case EFFECT_SPARK_SHOTGUN:
		gpuEffectId = EffectID::SPARK_SHOTGUN;
		break;

	case EFFECT_SPARK_PISTOL:
		gpuEffectId = EffectID::SPARK_PISTOL;
		break;

	case EFFECT_BLOOD:
		return;

	default:
		return;
	}

	XMVECTOR vRight = XMLoadFloat3(&right);
	XMVECTOR vUp = XMLoadFloat3(&up);
	XMVECTOR vDirection = XMVector3Cross(vRight, vUp);

	if (XMVectorGetX(XMVector3LengthSq(vDirection)) < 0.0001f)
	{
		vDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}
	else
	{
		vDirection = XMVector3Normalize(vDirection);
	}

	EffectSpawnDesc desc{};
	desc.id = gpuEffectId;
	desc.position = pos;
	XMStoreFloat3(&desc.direction, vDirection);
	desc.ownerId = 0;
	desc.value = 0.0f;

	if (!QueueParticleEffect(desc))
	{
		switch (type)
		{
		case EFFECT_SPARK_RIFLE_SMG:
			OutputDebugStringW(L"[EffectManager] GPU Rifle/SMG Spark request failed.\n");
			break;

		case EFFECT_SPARK_SHOTGUN:
			OutputDebugStringW(L"[EffectManager] GPU Shotgun Spark request failed.\n");
			break;

		case EFFECT_SPARK_PISTOL:
			OutputDebugStringW(L"[EffectManager] GPU Pistol Spark request failed.\n");
			break;

		default:
			break;
		}
	}
}

void EffectManager::PlayEffectByID(const EffectSpawnDesc& desc)
{
	if (desc.id == EffectID::NONE)
	{
		return;
	}

	switch (desc.id)
	{
	case EffectID::GRENADE_EXPLOSION:
	case EffectID::SPARK:
	case EffectID::SPARK_SHOTGUN:
	case EffectID::SPARK_PISTOL:
	{
		QueueParticleEffect(desc);
		break;
	}

	case EffectID::HIT:
	{
		EffectSpawnDesc sparkDesc = desc;
		sparkDesc.id = EffectID::SPARK;
		QueueParticleEffect(sparkDesc);
		break;
	}

	case EffectID::BLOOD:
	{
		// BLOOD는 현재 구현되어 있지 않은 기존 상태를 유지한다.
		break;
	}

	default:
		break;
	}
}

void EffectManager::Update(float fTimeElapsed)
{
	m_fParticleDeltaTime = max(fTimeElapsed, 0.0f);
	m_fParticleTotalTime += m_fParticleDeltaTime;
	m_bParticleComputeExecutedThisFrame = false;
}

CGameObject* EffectManager::GetOrCreateLaserObject(int ownerId)
{
	auto it = m_LaserObjects.find(ownerId);

	if (it != m_LaserObjects.end())
	{
		return it->second;
	}

	return nullptr;
}

void EffectManager::UpdateLaser(int ownerId, const XMFLOAT3& origin, const XMFLOAT3& right,
	const XMFLOAT3& up, const XMFLOAT3& dir, float fLength)
{
	CGameObject* pLaserObject = GetOrCreateLaserObject(ownerId);
	if (!pLaserObject) return;

	XMVECTOR vOrigin = XMLoadFloat3(&origin);
	XMVECTOR vRight = XMVector3Normalize(XMLoadFloat3(&right));
	XMVECTOR vUp = XMVector3Normalize(XMLoadFloat3(&up));
	XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&dir));

	XMMATRIX matScale = XMMatrixScaling(0.05f, 0.05f, fLength);

	XMMATRIX matRotation = XMMatrixIdentity();
	matRotation.r[0] = XMVectorSetW(vRight, 0.0f);
	matRotation.r[1] = XMVectorSetW(vUp, 0.0f);
	matRotation.r[2] = XMVectorSetW(vDir, 0.0f);
	matRotation.r[3] = XMVectorSet(0, 0, 0, 1);

	XMMATRIX matTranslation = XMMatrixTranslationFromVector(vOrigin);
	XMMATRIX matWorld = matScale * matRotation * matTranslation;

	XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, matWorld);
	pLaserObject->UpdateTransform(NULL);
}

void EffectManager::HideLaser(int ownerId)
{
	auto it = m_LaserObjects.find(ownerId);
	if (it == m_LaserObjects.end()) return;

	CGameObject* pLaserObject = it->second;
	if (!pLaserObject) return;

	XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
	pLaserObject->UpdateTransform(NULL);
}

void EffectManager::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState)
{
	if (nPipelineState == SHADOW)
	{
		return;
	}

	ExecuteParticleCompute(pd3dCommandList);

	if (m_pLaserShader)
	{
		m_pLaserShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}
}

void EffectManager::RenderGpuParticles(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	if (!pd3dCommandList || !pCamera || !m_bParticleSystemResourcesReady)
	{
		return;
	}

	if (!m_pd3dParticleDescriptorHeap || !m_pd3dParticleGraphicsRootSignature ||
		!m_pd3dParticleAlphaPipelineState || !m_pd3dParticleAdditivePipelineState ||
		!m_pd3dParticleDrawCommandSignature || !m_pd3dParticleBuffer ||
		!m_pd3dParticleIndirectArgumentBuffer)
	{
		return;
	}

	ParticleResource* pParticleResource = ResourceManager::Instance().GetParticleResource();

	if (!pParticleResource || !pParticleResource->IsLoaded())
	{
		return;
	}

	constexpr UINT renderGroupCount = static_cast<UINT>(ParticleRenderGroup::COUNT);
	constexpr UINT transitionBarrierCount = renderGroupCount + 2;

	D3D12_RESOURCE_BARRIER toGraphicsBarriers[transitionBarrierCount]{};

	auto SetTransitionBarrier = [](D3D12_RESOURCE_BARRIER& barrier, ID3D12Resource* pd3dResource,
		D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
		{
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = pd3dResource;
			barrier.Transition.StateBefore = stateBefore;
			barrier.Transition.StateAfter = stateAfter;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		};

	SetTransitionBarrier(toGraphicsBarriers[0], m_pd3dParticleBuffer,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	for (UINT i = 0; i < renderGroupCount; ++i)
	{
		if (!m_pd3dParticleRenderIndexBuffers[i])
		{
			return;
		}

		SetTransitionBarrier(toGraphicsBarriers[i + 1], m_pd3dParticleRenderIndexBuffers[i],
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	SetTransitionBarrier(toGraphicsBarriers[renderGroupCount + 1], m_pd3dParticleIndirectArgumentBuffer,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

	pd3dCommandList->ResourceBarrier(transitionBarrierCount, toGraphicsBarriers);

	ID3D12DescriptorHeap* particleDescriptorHeaps[] = { m_pd3dParticleDescriptorHeap };

	pd3dCommandList->SetDescriptorHeaps(_countof(particleDescriptorHeaps), particleDescriptorHeaps);
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dParticleGraphicsRootSignature);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);

	pd3dCommandList->SetGraphicsRootDescriptorTable(1,
		GetParticleGpuDescriptorHandle(PARTICLE_DESCRIPTOR_PARTICLE_SRV));

	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dCommandList->IASetVertexBuffers(0, 0, nullptr);
	pd3dCommandList->IASetIndexBuffer(nullptr);

	for (UINT renderGroupIndex = 0; renderGroupIndex < renderGroupCount; ++renderGroupIndex)
	{
		ParticleRenderGroup renderGroup = static_cast<ParticleRenderGroup>(renderGroupIndex);
		ParticleTextureID textureId = ParticleTextureID::EXPLOSION;
		bool additiveBlend = false;

		switch (renderGroup)
		{
		case ParticleRenderGroup::EXPLOSION_ALPHA:
			textureId = ParticleTextureID::EXPLOSION;
			additiveBlend = false;
			break;

		case ParticleRenderGroup::EXPLOSION_ADDITIVE:
			textureId = ParticleTextureID::EXPLOSION;
			additiveBlend = true;
			break;

		case ParticleRenderGroup::SHOTGUN_ADDITIVE:
			textureId = ParticleTextureID::SPARK_SHOTGUN;
			additiveBlend = true;
			break;

		case ParticleRenderGroup::RIFLE_ADDITIVE:
			textureId = ParticleTextureID::SPARK_RIFLE_SMG;
			additiveBlend = true;
			break;

		case ParticleRenderGroup::RIFLE_ALPHA:
			textureId = ParticleTextureID::SPARK_RIFLE_SMG;
			additiveBlend = false;
			break;

		default:
			continue;
		}

		const ParticleAtlasDesc* pAtlasDesc = pParticleResource->GetAtlasDesc(textureId);

		if (!pAtlasDesc)
		{
			continue;
		}

		ParticleGraphicsConstants graphicsConstants{};
		graphicsConstants.textureWidth = pAtlasDesc->textureWidth;
		graphicsConstants.textureHeight = pAtlasDesc->textureHeight;
		graphicsConstants.columns = pAtlasDesc->columns;
		graphicsConstants.rows = pAtlasDesc->rows;

		graphicsConstants.frameWidth = pAtlasDesc->frameWidth;
		graphicsConstants.frameHeight = pAtlasDesc->frameHeight;
		graphicsConstants.borderX = pAtlasDesc->borderX;
		graphicsConstants.borderY = pAtlasDesc->borderY;

		graphicsConstants.spacingX = pAtlasDesc->spacingX;
		graphicsConstants.spacingY = pAtlasDesc->spacingY;
		graphicsConstants.validFrameCount = pAtlasDesc->validFrameCount;
		graphicsConstants.renderGroup = renderGroupIndex;

		graphicsConstants.inverseTextureWidth = (pAtlasDesc->textureWidth > 0) ?
			1.0f / static_cast<float>(pAtlasDesc->textureWidth) : 1.0f;

		graphicsConstants.inverseTextureHeight = (pAtlasDesc->textureHeight > 0) ?
			1.0f / static_cast<float>(pAtlasDesc->textureHeight) : 1.0f;

		pd3dCommandList->SetPipelineState(additiveBlend ?
			m_pd3dParticleAdditivePipelineState : m_pd3dParticleAlphaPipelineState);

		pd3dCommandList->SetGraphicsRootDescriptorTable(2,
			GetParticleGpuDescriptorHandle(PARTICLE_DESCRIPTOR_RENDER_EXPLOSION_ALPHA_SRV + renderGroupIndex));

		pd3dCommandList->SetGraphicsRootDescriptorTable(3,
			GetParticleGpuDescriptorHandle(PARTICLE_DESCRIPTOR_TEXTURE_EXPLOSION_SRV +
				static_cast<UINT>(textureId)));

		pd3dCommandList->SetGraphicsRoot32BitConstants(4, PARTICLE_GRAPHICS_ROOT_CONSTANT_COUNT,
			&graphicsConstants, 0);

		UINT64 indirectArgumentOffset =
			sizeof(D3D12_DRAW_ARGUMENTS) * static_cast<UINT64>(renderGroupIndex);

		pd3dCommandList->ExecuteIndirect(m_pd3dParticleDrawCommandSignature, 1,
			m_pd3dParticleIndirectArgumentBuffer, indirectArgumentOffset, nullptr, 0);
	}

	D3D12_RESOURCE_BARRIER toComputeBarriers[transitionBarrierCount]{};

	SetTransitionBarrier(toComputeBarriers[0], m_pd3dParticleBuffer,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	for (UINT i = 0; i < renderGroupCount; ++i)
	{
		SetTransitionBarrier(toComputeBarriers[i + 1], m_pd3dParticleRenderIndexBuffers[i],
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	SetTransitionBarrier(toComputeBarriers[renderGroupCount + 1], m_pd3dParticleIndirectArgumentBuffer,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	pd3dCommandList->ResourceBarrier(transitionBarrierCount, toComputeBarriers);
}