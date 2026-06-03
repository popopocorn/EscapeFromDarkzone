#include "stdafx.h"
#include "Object.h"
#include "ShadowMap.h"
#include "UI.h"
#include "ResourceManager.h"


static CAnimationSets* CreateAnimationSetsInstanceCache(
	CAnimationSets* pPrototypeAnimationSets,
	CGameObject* pInstanceRoot)
{
	if (!pPrototypeAnimationSets || !pInstanceRoot)
		return nullptr;

	CAnimationSets* pInstanceAnimationSets =
		new CAnimationSets(pPrototypeAnimationSets->m_nAnimationSets);

	// 애니메이션 키프레임 데이터는 원본과 공유
	pInstanceAnimationSets->m_vAnimationSets =
		pPrototypeAnimationSets->m_vAnimationSets;

	pInstanceAnimationSets->m_nAnimationSets =
		pPrototypeAnimationSets->m_nAnimationSets;

	pInstanceAnimationSets->m_nBoneFrames =
		pPrototypeAnimationSets->m_nBoneFrames;

	// 중요:
	// m_vAnimationSets는 원본과 공유하므로 인스턴스 쪽에서 delete하면 안 됨
	pInstanceAnimationSets->m_bOwnAnimationSets = false;

	if (pInstanceAnimationSets->m_nBoneFrames > 0)
	{
		pInstanceAnimationSets->m_ppBoneFrameCaches =
			new CGameObject * [pInstanceAnimationSets->m_nBoneFrames];

		for (int i = 0; i < pInstanceAnimationSets->m_nBoneFrames; ++i)
		{
			pInstanceAnimationSets->m_ppBoneFrameCaches[i] = nullptr;

			CGameObject* pPrototypeBone =
				pPrototypeAnimationSets->m_ppBoneFrameCaches[i];

			if (!pPrototypeBone)
				continue;

			// 원본 bone 이름과 같은 frame을 인스턴스 계층에서 찾음
			pInstanceAnimationSets->m_ppBoneFrameCaches[i] =
				pInstanceRoot->FindFrame(pPrototypeBone->m_pstrFrameName);
		}
	}

	return pInstanceAnimationSets;
}


void ResourceManager::CreateCbvSrvDescriptorHeaps(
	ID3D12Device* pd3dDevice,
	int nConstantBufferViews,
	int nShaderResourceViews)
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc{};
	d3dDescriptorHeapDesc.NumDescriptors = nConstantBufferViews + nShaderResourceViews;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap),
		(void**)&m_pd3dCbvSrvDescriptorHeap
	);

	m_d3dCbvCPUDescriptorNextHandle =
		m_d3dCbvCPUDescriptorStartHandle =
		m_pd3dCbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	m_d3dCbvGPUDescriptorNextHandle =
		m_d3dCbvGPUDescriptorStartHandle =
		m_pd3dCbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	m_d3dSrvCPUDescriptorNextHandle.ptr =
		m_d3dSrvCPUDescriptorStartHandle.ptr =
		m_d3dCbvCPUDescriptorStartHandle.ptr +
		(::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);

	m_d3dSrvGPUDescriptorNextHandle.ptr =
		m_d3dSrvGPUDescriptorStartHandle.ptr =
		m_d3dCbvGPUDescriptorStartHandle.ptr +
		(::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::CreateConstantBufferViews(
	ID3D12Device* pd3dDevice,
	int nConstantBufferViews,
	ID3D12Resource* pd3dConstantBuffers,
	UINT nStride)
{
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle =
		m_d3dCbvGPUDescriptorNextHandle;

	D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress =
		pd3dConstantBuffers->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC d3dCBVDesc{};
	d3dCBVDesc.SizeInBytes = nStride;

	for (int j = 0; j < nConstantBufferViews; j++)
	{
		d3dCBVDesc.BufferLocation = d3dGpuVirtualAddress + (nStride * j);



		pd3dDevice->CreateConstantBufferView(
			&d3dCBVDesc,
			m_d3dCbvCPUDescriptorNextHandle
		);

		m_d3dCbvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		m_d3dCbvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}

	return d3dCbvGPUDescriptorHandle;
}

void ResourceManager::CreateShaderResourceViews(
	ID3D12Device* pd3dDevice,
	CTexture* pTexture,
	UINT nDescriptorHeapIndex,
	UINT nRootParameterStartIndex)
{
	if (!pd3dDevice || !pTexture)
		return;

	// nDescriptorHeapIndex가 0이 아니면 SRV 시작 위치 기준으로 이동
	if (nDescriptorHeapIndex > 0)
	{
		m_d3dSrvCPUDescriptorNextHandle.ptr =
			m_d3dSrvCPUDescriptorStartHandle.ptr +
			(::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

		m_d3dSrvGPUDescriptorNextHandle.ptr =
			m_d3dSrvGPUDescriptorStartHandle.ptr +
			(::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	}

	int nTextures = pTexture->GetTextures();

	for (int i = 0; i < nTextures; i++)
	{
		ID3D12Resource* pShaderResource = pTexture->GetResource(i);
		if (!pShaderResource)
			continue;

		D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc =
			pTexture->GetShaderResourceViewDesc(i);

		pd3dDevice->CreateShaderResourceView(
			pShaderResource,
			&d3dShaderResourceViewDesc,
			m_d3dSrvCPUDescriptorNextHandle
		);

		pTexture->SetGpuDescriptorHandle(
			i,
			m_d3dSrvGPUDescriptorNextHandle
		);

		m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	}

	int nRootParameters = pTexture->GetRootParameters();

	for (int j = 0; j < nRootParameters; j++)
	{
		pTexture->SetRootParameterIndex(
			j,
			nRootParameterStartIndex + j
		);
	}
}

void ResourceManager::CreateshadowResourceViews(
	ID3D12Device* pd3dDevice,
	ShadowMap* shadowmap,
	UINT nDescriptorHeapIndex,
	UINT nRootParameterStartIndex)
{
	UNREFERENCED_PARAMETER(nRootParameterStartIndex);

	if (!pd3dDevice || !shadowmap)
		return;

	m_d3dSrvCPUDescriptorNextHandle.ptr +=
		(::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	m_d3dSrvGPUDescriptorNextHandle.ptr +=
		(::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	shadowmap->CreateSRV(
		pd3dDevice,
		m_d3dSrvCPUDescriptorNextHandle,
		m_d3dSrvGPUDescriptorNextHandle
	);

	m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
}

void ResourceManager::ReleaseUploadBuffers()
{
	for (auto& obj : m_SkinnedModelPrototypes)
	{
		obj.second->m_pModelRootObject->ReleaseUploadBuffers();
	}

	for (auto& obj : m_ModelPrototypes)
	{
		obj.second->ReleaseUploadBuffers();
	}
}

void ResourceManager::ReleaseResources()
{
	m_ModelPrototypes.clear();
	ReleaseSkinnedModelPrototypes();

	if (m_pd3dCbvSrvDescriptorHeap)
	{
		m_pd3dCbvSrvDescriptorHeap->Release();
		m_pd3dCbvSrvDescriptorHeap = nullptr;
	}
}

bool ResourceManager::LoadAndRegisterModelPrototype(
	ModelName key,
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	const char* modelPath,
	CShader* pShader)
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

	CGameObject* pPrototype = CGameObject::LoadGeometryModelByName(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		nullptr,
		modelPath,
		pShader,
		nullptr
	);

	if (!pPrototype)
		return false;

	m_ModelPrototypes.emplace(key, pPrototype);
	return true;
}

bool ResourceManager::LoadAndRegisterSkinnedModelPrototype(
	ModelName key,
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	const char* modelPath,
	CShader* pShader)
{
	if (!pd3dDevice) return false;
	if (!pd3dCommandList) return false;
	if (!pd3dGraphicsRootSignature) return false;
	if (!modelPath) return false;

	auto it = m_SkinnedModelPrototypes.find(key);
	if (it != m_SkinnedModelPrototypes.end())
	{
		return true;
	}

	CLoadedModelInfo* pLoadedModel =
		CGameObject::LoadGeometryAndAnimationFromFile(
			pd3dDevice,
			pd3dCommandList,
			pd3dGraphicsRootSignature,
			modelPath,
			pShader
		);

	if (!pLoadedModel)
		return false;

	if (!pLoadedModel->m_pAnimationSets)
	{
		pLoadedModel->m_pAnimationSets = new CAnimationSets(0);
	}

	m_SkinnedModelPrototypes.emplace(key, pLoadedModel);
	return true;
}

void ResourceManager::BuildPlayerModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* PlayerShader)
{
	// 플레이어 모델
	LoadAndRegisterSkinnedModelPrototype(
		ModelName::PLAYER_01,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Soldier_03_Complete_Reduced.bin",
		PlayerShader
	);


}

void ResourceManager::BuildSkinnedModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* SkinnedShader)
{

	// 나중에 플레이어 모델 추가 시
	/*
	LoadAndRegisterSkinnedModelPrototype(
		ModelName::PLAYER_02,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Player_02.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::PLAYER_03,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Player_03.bin",
		SkinnedShader
	);
	*/

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Ganster4.bin",
		SkinnedShader
	);

	// 나중에 적 모델 추가 시 여기만 열면 됨
	/*
	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Enemy_02.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Enemy_03.bin",
		SkinnedShader
	);
	*/
}

void ResourceManager::BuildModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* Standardshader)
{
	if (!Standardshader) return;

	// 무기 / 정적 모델
	LoadAndRegisterModelPrototype(
		ModelName::RIFLE,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Classic_M4.bin",
		Standardshader
	);

	LoadAndRegisterModelPrototype(
		ModelName::PISTOL,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Pistol_2.bin",
		Standardshader
	);
	LoadAndRegisterModelPrototype(
		ModelName::SMG,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_MP5.bin",
		Standardshader
	);
	LoadAndRegisterModelPrototype(
		ModelName::SHOTGUN,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Shotgun.bin",
		Standardshader
	);

	ModelName name = ModelName::MAP_FLOOR;
	for (const string& s : s_mapFiles)
	{
		LoadAndRegisterModelPrototype(
			name,
			pd3dDevice,
			pd3dCommandList,
			pd3dGraphicsRootSignature,
			s.c_str(),
			Standardshader
		);
		++name;
	}

	//루팅 아이템 모델
	LoadAndRegisterModelPrototype(
		ModelName::LOOT_BOX,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/LootBox.bin",
		Standardshader
	);
}


void ResourceManager::BuildUIMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	m_UIPrototypes[UIName::LOBBY_BACKGROUND] = make_unique<UIMesh>(pd3dDevice, pd3dCommandList);
	//m_UIPrototypes[UIName::LOBBY_BACKGROUND]->LoadTexture(pd3dDevice, pd3dCommandList, L"");

	m_UIPrototypes[UIName::LOBBY_START_BUTTON] = make_unique<UIMesh>(pd3dDevice, pd3dCommandList);
	m_UIPrototypes[UIName::LOBBY_START_BUTTON]->LoadTexture(pd3dDevice, pd3dCommandList, L"Model/Textures/Start_BTN.dds");
}

CGameObject* ResourceManager::GetModelPrototype(ModelName key) const
{
	auto it = m_ModelPrototypes.find(key);
	if (it == m_ModelPrototypes.end())
		return nullptr;

	return it->second.get();
}

CLoadedModelInfo* ResourceManager::CreateSkinnedModelInstance(ModelName key)
{
	auto it = m_SkinnedModelPrototypes.find(key);
	if (it == m_SkinnedModelPrototypes.end())
		return nullptr;

	CLoadedModelInfo* pPrototypeInfo = it->second.get();
	if (!pPrototypeInfo) return nullptr;
	if (!pPrototypeInfo->m_pModelRootObject) return nullptr;
	if (!pPrototypeInfo->m_pAnimationSets) return nullptr;

	CGameObject* pRootInstance =
		CGameObject::CreateModelInstance(pPrototypeInfo->m_pModelRootObject);

	if (!pRootInstance)
		return nullptr;

	CLoadedModelInfo* pInstanceInfo = new CLoadedModelInfo();

	pInstanceInfo->m_pModelRootObject = pRootInstance;
	pInstanceInfo->m_nSkinnedMeshes = pPrototypeInfo->m_nSkinnedMeshes;

	pInstanceInfo->m_pAnimationSets =
		CreateAnimationSetsInstanceCache(
			pPrototypeInfo->m_pAnimationSets,
			pRootInstance
		);

	if (!pInstanceInfo->m_pAnimationSets)
	{
		delete pInstanceInfo;
		return nullptr;
	}

	if (pInstanceInfo->m_nSkinnedMeshes > 0)
	{
		pInstanceInfo->m_ppSkinnedMeshes =
			new CSkinnedMesh * [pInstanceInfo->m_nSkinnedMeshes];

		int nSkinnedMesh = 0;

		pRootInstance->FindAndSetSkinnedMesh(
			pInstanceInfo->m_ppSkinnedMeshes,
			&nSkinnedMesh
		);
	}

	return pInstanceInfo;
}

UIMesh* ResourceManager::GetUIMesh(UIName name)
{
	auto it = m_UIPrototypes.find(name);
	if (it != m_UIPrototypes.end())return it->second.get();
	else return nullptr;
}

void ResourceManager::ReleaseSkinnedModelPrototypes()
{
	for (auto& pair : m_SkinnedModelPrototypes)
	{
		CLoadedModelInfo* pInfo = pair.second.get();
		if (!pInfo) continue;

		if (pInfo->m_pAnimationSets)
		{
			//pInfo->m_pAnimationSets->ReleaseUploadBuffers();
			pInfo->m_pAnimationSets = nullptr;
		}

		delete pInfo;
	}

	m_SkinnedModelPrototypes.clear();
}