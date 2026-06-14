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

	pInstanceAnimationSets->m_vAnimationSets =
		pPrototypeAnimationSets->m_vAnimationSets;

	pInstanceAnimationSets->m_nAnimationSets =
		pPrototypeAnimationSets->m_nAnimationSets;

	pInstanceAnimationSets->m_nBoneFrames =
		pPrototypeAnimationSets->m_nBoneFrames;

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



bool ResourceManager::ShareSkinnedAnimationSets(ModelName targetKey, ModelName sourceKey)
{
	auto targetIt = m_SkinnedModelPrototypes.find(targetKey);
	if (targetIt == m_SkinnedModelPrototypes.end()) return false;

	auto sourceIt = m_SkinnedModelPrototypes.find(sourceKey);
	if (sourceIt == m_SkinnedModelPrototypes.end()) return false;

	CLoadedModelInfo* pTargetInfo = targetIt->second.get();
	CLoadedModelInfo* pSourceInfo = sourceIt->second.get();

	if (!pTargetInfo) return false;
	if (!pSourceInfo) return false;
	if (!pSourceInfo->m_pAnimationSets) return false;

	if (pTargetInfo->m_pAnimationSets && pTargetInfo->m_pAnimationSets != pSourceInfo->m_pAnimationSets)
	{
		delete pTargetInfo->m_pAnimationSets;
		pTargetInfo->m_pAnimationSets = nullptr;
	}

	pTargetInfo->m_pAnimationSets = pSourceInfo->m_pAnimationSets;

	return true;
}

void ResourceManager::BuildPlayerModelPrototypes(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* PlayerShader)
{
	if (!PlayerShader) return;

	LoadAndRegisterSkinnedModelPrototype(ModelName::PLAYER_01, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/SM_Soldier_03_Complete_Reduced_black.bin", PlayerShader);

	LoadAndRegisterSkinnedModelPrototype(ModelName::PLAYER_02, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/SM_Soldier_03_Complete_Reduced_yellow.bin", PlayerShader);

	LoadAndRegisterSkinnedModelPrototype(ModelName::PLAYER_03, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/SM_Soldier_03_Complete_Reduced_green.bin", PlayerShader);

	ShareSkinnedAnimationSets(ModelName::PLAYER_02, ModelName::PLAYER_01);
	ShareSkinnedAnimationSets(ModelName::PLAYER_03, ModelName::PLAYER_01);
}

void ResourceManager::BuildSkinnedModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* SkinnedShader)
{
	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_1.bin",
		SkinnedShader
	);*/

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_2.bin",
		SkinnedShader
	);

	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_3.bin",
		SkinnedShader
	);*/

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_1.bin",
		SkinnedShader
	);

	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_2.bin",
		SkinnedShader
	);*/

	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_3.bin",
		SkinnedShader
	);*/

	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_1.bin",
		SkinnedShader
	);*/



	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_2.bin",
		SkinnedShader
	);*/

	/*LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_3.bin",
		SkinnedShader
		);*/
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

	LoadAndRegisterModelPrototype(
		ModelName::GRENADE,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/grenade.bin",
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

	//총알
	LoadAndRegisterModelPrototype(
		ModelName::BULLET,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/bullet.bin",
		Standardshader
	);
}

void ResourceManager::LoadUIMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, UIName name, const wchar_t* path)
{
	m_UIPrototypes[name] = make_unique<UIMesh>(pd3dDevice, pd3dCommandList);
	m_UIPrototypes[name]->LoadTexture(pd3dDevice, pd3dCommandList,path);

}
void ResourceManager::BuildUIMesh(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::LOBBY_BACKGROUND,
		L"UI/Lobby.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::LOBBY_START_BUTTON,
		L"UI/Start_BTN.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::TABLE_VERTICAL,
		L"UI/Table_02.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::WINDOW_BASE,
		L"UI/Window.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::PANEL_001,
		L"UI/panel001.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::DIVIDER_001,
		L"UI/divider-002.dds"
	);
	//icon
	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_FIBER,
		L"UI/yarn.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_NEEDLE,
		L"UI/needle.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_METAL,
		L"UI/metalbar.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_HELMET_01,
		L"UI/Icon_Helmet_01.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_HELMET_02,
		L"UI/Icon_Helmet_02.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_HELMET_03,
		L"UI/Icon_Helmet_03.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_HELMET_04,
		L"UI/Icon_Helmet_04.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_BODY_01,
		L"UI/Icon_Body_01.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_BODY_02,
		L"UI/Icon_Body_02.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_BODY_03,
		L"UI/Icon_Body_03.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_BODY_04,
		L"UI/Icon_Body_04.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_SHOES_01,
		L"UI/Icon_Shoes_01.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_SHOES_02,
		L"UI/Icon_Shoes_02.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_SHOES_03,
		L"UI/Icon_Shoes_03.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_SHOES_04,
		L"UI/Icon_Shoes_04.dds"
	);

	//status
	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_HEALTH_BAR,
		L"UI/Health_Bar_Table.dds"
	);
	
	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_HEALTH_DOT,
		L"UI/Health_Dot.dds"
	);
	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_RIFLE_BULLET,
		L"UI/Rifle_Bullet2.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_SMG_BULLET,
		L"UI/SMG_Bullet.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_SHOTGUN_BULLET,
		L"UI/Shotgun_Bullet.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_PISTOL_BULLET,
		L"UI/PistolBullet.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_BULLET_DOT,
		L"UI/BlueDot.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_BULLET_DOT,
		L"UI/BlueDot.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::STATUS_BULLET_DOT,
		L"UI/BlueDot.dds"
	);
	




}

CGameObject* ResourceManager::GetModelPrototype(ModelName key) const
{
	auto it = m_ModelPrototypes.find(key);
	if (it == m_ModelPrototypes.end())
		return nullptr;

	return CGameObject::CreateModelInstance(it->second.get());
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

	CGameObject* pRootInstance = CGameObject::CreateModelInstance(pPrototypeInfo->m_pModelRootObject);

	if (!pRootInstance)
		return nullptr;

	CLoadedModelInfo* pInstanceInfo = new CLoadedModelInfo();

	pInstanceInfo->m_pModelRootObject = pRootInstance;
	pInstanceInfo->m_nSkinnedMeshes = pPrototypeInfo->m_nSkinnedMeshes;

	pInstanceInfo->m_pAnimationSets = CreateAnimationSetsInstanceCache(pPrototypeInfo->m_pAnimationSets, pRootInstance);

	if (!pInstanceInfo->m_pAnimationSets)
	{
		delete pInstanceInfo;
		return nullptr;
	}

	pInstanceInfo->m_ppSkinnedMeshes = nullptr;

	if (pInstanceInfo->m_nSkinnedMeshes > 0)
	{
		pInstanceInfo->m_ppSkinnedMeshes = new CSkinnedMesh * [pInstanceInfo->m_nSkinnedMeshes];

		for (int i = 0; i < pInstanceInfo->m_nSkinnedMeshes; ++i)
		{
			pInstanceInfo->m_ppSkinnedMeshes[i] = nullptr;
		}

		int nSkinnedMesh = 0;
		pRootInstance->FindAndSetSkinnedMesh(pInstanceInfo->m_ppSkinnedMeshes, &nSkinnedMesh);

		if (nSkinnedMesh != pInstanceInfo->m_nSkinnedMeshes)
		{
			pInstanceInfo->m_nSkinnedMeshes = nSkinnedMesh;
		}

		if (pInstanceInfo->m_nSkinnedMeshes <= 0)
		{
			delete[] pInstanceInfo->m_ppSkinnedMeshes;
			pInstanceInfo->m_ppSkinnedMeshes = nullptr;
		}
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