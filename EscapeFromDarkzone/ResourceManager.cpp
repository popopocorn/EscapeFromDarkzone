#include"stdafx.h"
#include"Object.h"
#include"ShadowMap.h"
#include"UI.h"
#include "ResourceManager.h"

void ResourceManager::CreateCbvSrvDescriptorHeaps(ID3D12Device* pd3dDevice, int nConstantBufferViews, int nShaderResourceViews)
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	d3dDescriptorHeapDesc.NumDescriptors = nConstantBufferViews + nShaderResourceViews; //CBVs + SRVs 
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dCbvSrvDescriptorHeap);

	m_d3dCbvCPUDescriptorNextHandle = m_d3dCbvCPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_d3dCbvGPUDescriptorNextHandle = m_d3dCbvGPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_d3dSrvCPUDescriptorNextHandle.ptr = m_d3dSrvCPUDescriptorStartHandle.ptr = m_d3dCbvCPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);
	m_d3dSrvGPUDescriptorNextHandle.ptr = m_d3dSrvGPUDescriptorStartHandle.ptr = m_d3dCbvGPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);

}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::CreateConstantBufferViews(ID3D12Device* pd3dDevice, int nConstantBufferViews, ID3D12Resource* pd3dConstantBuffers, UINT nStride)
{
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle = m_d3dCbvGPUDescriptorNextHandle;
	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress = pd3dConstantBuffers->GetGPUVirtualAddress();
	D3D12_CONSTANT_BUFFER_VIEW_DESC d3dCBVDesc;
	d3dCBVDesc.SizeInBytes = nStride;
	for (int j = 0; j < nConstantBufferViews; j++)
	{
		d3dCBVDesc.BufferLocation = d3dGpuVirtualAddress + (nStride * j);
		m_d3dCbvCPUDescriptorNextHandle.ptr = m_d3dCbvCPUDescriptorNextHandle.ptr + ::gnCbvSrvDescriptorIncrementSize;
		pd3dDevice->CreateConstantBufferView(&d3dCBVDesc, m_d3dCbvCPUDescriptorNextHandle);
		m_d3dCbvGPUDescriptorNextHandle.ptr = m_d3dCbvGPUDescriptorNextHandle.ptr + ::gnCbvSrvDescriptorIncrementSize;
	}
	return(d3dCbvGPUDescriptorHandle);
}

void ResourceManager::CreateShaderResourceViews(ID3D12Device* pd3dDevice, CTexture* pTexture, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex)
{
	if (!pd3dDevice || !pTexture)
		return;

	// nDescriptorHeapIndex가 0이 아니면 "시작 위치" 기준으로 강제 재배치
	if (nDescriptorHeapIndex > 0)
	{
		m_d3dSrvCPUDescriptorNextHandle.ptr =
			m_d3dSrvCPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

		m_d3dSrvGPUDescriptorNextHandle.ptr =
			m_d3dSrvGPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	}

	int nTextures = pTexture->GetTextures();
	for (int i = 0; i < nTextures; i++)
	{
		ID3D12Resource* pShaderResource = pTexture->GetResource(i);
		if (!pShaderResource)
			continue;

		D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc = pTexture->GetShaderResourceViewDesc(i);

		pd3dDevice->CreateShaderResourceView(
			pShaderResource,
			&d3dShaderResourceViewDesc,
			m_d3dSrvCPUDescriptorNextHandle
		);

		pTexture->SetGpuDescriptorHandle(i, m_d3dSrvGPUDescriptorNextHandle);

		m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}

	int nRootParameters = pTexture->GetRootParameters();
	for (int j = 0; j < nRootParameters; j++)
	{
		pTexture->SetRootParameterIndex(j, nRootParameterStartIndex + j);
	}
}

void ResourceManager::CreateshadowResourceViews(ID3D12Device * pd3dDevice, ShadowMap * shadowmap, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex)
{
	m_d3dSrvCPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	m_d3dSrvGPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	shadowmap->CreateSRV(pd3dDevice, m_d3dSrvCPUDescriptorNextHandle, m_d3dSrvGPUDescriptorNextHandle);

	m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
}

void ResourceManager::ReleaseResources()
{
	ReleaseModelPrototypes();

	if (m_pd3dCbvSrvDescriptorHeap)
	{
		m_pd3dCbvSrvDescriptorHeap->Release();
		m_pd3dCbvSrvDescriptorHeap = nullptr;
	}
}

CLoadedModelInfo* ResourceManager::GetSkinnedModel(ResourceName name)
{
	auto it = skinnedModelpool.find(name);
	if (it != skinnedModelpool.end())
	{
		return it->second.get();
	}
	else
	{
		return nullptr;
	}
}

CGameObject* ResourceManager::GetModel(ResourceName name)
{
	auto it = Modelpool.find(name);
	if (it != Modelpool.end())
	{
		return it->second.get();
	}
	else
	{
		return nullptr;
	}
}

UIMesh* ResourceManager::GetUI(ResourceName name)
{
	auto it = UIpool.find(name);
	if (it != UIpool.end())
	{
		return it->second.get();
	}
	else
	{
		return nullptr;
	}
}

bool ResourceManager::LoadAndRegisterModelPrototype(ModelName key, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, const char* modelPath, CShader* pShader)
{
	if (!pd3dDevice) return false;
	if (!pd3dCommandList) return false;
	if (!pd3dGraphicsRootSignature) return false;
	if (!modelPath) return false;
	if (!pShader) return false;

	auto it = m_ModelPrototypes.find(key);
	if (it != m_ModelPrototypes.end())
	{
		return true;
	}

	CGameObject* pPrototype = CGameObject::LoadGeometryModelByName(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, nullptr, modelPath, pShader, nullptr);

	if (!pPrototype)
		return false;

	RegisterModelPrototype(key, pPrototype);
	return true;
}

void ResourceManager::RegisterModelPrototype(ModelName key, CGameObject* pPrototype)
{
	if (!pPrototype) return;

	auto it = m_ModelPrototypes.find(key);
	if (it != m_ModelPrototypes.end())
	{
		if (it->second)
			it->second->Release();

		it->second = pPrototype;
		return;
	}

	m_ModelPrototypes.emplace(key, pPrototype);
}

CGameObject* ResourceManager::GetModelPrototype(ModelName key) const
{
	auto it = m_ModelPrototypes.find(key);
	if (it == m_ModelPrototypes.end())
		return nullptr;

	return it->second;
}

void ResourceManager::ReleaseModelPrototypes()
{
	for (auto& pair : m_ModelPrototypes)
	{
		if (pair.second)
		{
			pair.second->Release();
		}
	}
	m_ModelPrototypes.clear();
}

void ResourceManager::BuildModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pPlayerShader)
{
	if (!pPlayerShader) return;

	LoadAndRegisterModelPrototype(
		ModelName::RIFLE,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Classic_M4_1.bin",
		pPlayerShader
	);
}