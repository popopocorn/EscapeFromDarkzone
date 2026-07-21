#include "stdafx.h"
#include "Object.h"
#include "ShadowMap.h"
#include "UI.h"
#include "ResourceManager.h"


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
	for (auto& obj : textureMap)
	{
		obj.second->ReleaseUploadBuffers();
	}
	for (auto& obj : m_ModelPrototypes)
	{
		obj.second->ReleaseUploadBuffers();
	}
	for (auto& obj : m_UIPrototypes)
	{
		obj.second->ReleaseUploadBuffers();
	}
}

void ResourceManager::ReleaseResources()
{
	m_ModelPrototypes.clear();

	m_SkinnedModelPrototypes.clear();

	m_AnimationSetOwners.clear();

	m_UIPrototypes.clear();

	textureMap.clear();
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

	ModelResource* pPrototype = LoadGeometryModelByName(
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
		LoadGeometryAndAnimationFromFile(
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
	m_AnimationSetOwners.emplace(key, unique_ptr<CAnimationSets>(pLoadedModel->m_pAnimationSets));

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

	m_AnimationSetOwners.erase(targetKey);

	pTargetInfo->m_pAnimationSets = pSourceInfo->m_pAnimationSets;

	return true;
}

void ResourceManager::BuildPlayerModelPrototypes(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* PlayerShader)
{
	if (!PlayerShader) return;

	LoadAndRegisterSkinnedModelPrototype(ModelName::PLAYER_01, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/SM_Soldier_03_Complete_Reduced_black.bin", PlayerShader);

}

void ResourceManager::BuildSkinnedModelPrototypes(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* SkinnedShader)
{
	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_1.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_2.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_01_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster1_3.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_1.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_2.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_02_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster2_3.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_1,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_1.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_2,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_2.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::ENEMY_03_3,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/SM_Gangster3_3.bin",
		SkinnedShader
	);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::PLAYER_02, 
		pd3dDevice, 
		pd3dCommandList, 
		pd3dGraphicsRootSignature, 
		"Model/SM_Soldier_03_Complete_Reduced_yellow.bin", SkinnedShader);

	LoadAndRegisterSkinnedModelPrototype(
		ModelName::PLAYER_03, 
		pd3dDevice, 
		pd3dCommandList, 
		pd3dGraphicsRootSignature, 
		"Model/SM_Soldier_03_Complete_Reduced_green.bin", SkinnedShader);

	ShareSkinnedAnimationSets(ModelName::PLAYER_02, ModelName::PLAYER_01);
	ShareSkinnedAnimationSets(ModelName::PLAYER_03, ModelName::PLAYER_01);
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
		"Model/bullet2.bin",
		Standardshader
	);

	//데칼
	LoadAndRegisterModelPrototype(
		ModelName::BULLET_DECAL,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/bullet_decal.bin",
		Standardshader
	);
	LoadAndRegisterModelPrototype(
		ModelName::BLOOD_DECAL,
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/blood_decal.bin",
		Standardshader
	);
	//view

	ModelResource* viewCircle = new ModelResource();
	viewCircle->SetMesh(new CViewCircleMesh(pd3dDevice, pd3dCommandList, 3.0f, 72));
	m_ModelPrototypes[ModelName::VIEW_CIRCLE] = unique_ptr<ModelResource>(viewCircle);

	ModelResource* viewCone = new ModelResource();
	viewCone->SetMesh(new CViewConeMesh(pd3dDevice, pd3dCommandList, 20.0f, 75.0f, 72));
	m_ModelPrototypes[ModelName::VIEW_CONE] = unique_ptr<ModelResource>(viewCone);
}

void ResourceManager::LoadUIMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, UIName name, const wchar_t* path)
{
	auto it = m_UIPrototypes.find(name);
	if (it != m_UIPrototypes.end())return;
	m_UIPrototypes[name] = make_unique<UIMesh>(pd3dDevice, pd3dCommandList);
	m_UIPrototypes[name]->LoadTexture(pd3dDevice, pd3dCommandList, path);

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
		UIName::CROSSHAIR,
		L"UI/Crosshair_03.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::UI_WAIT,
		L"UI/wait.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::UI_SUCCESS,
		L"UI/Success.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::UI_FAIL,
		L"UI/Fail.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::UI_RETURNTOLOBBY,
		L"UI/ReturnToLobby.dds"
	);

	LoadUIMesh(
		pd3dDevice,
		pd3dCommandList,
		UIName::ICON_KEY,
		L"UI/key.dds"
	);

}

CGameObject* ResourceManager::GetModelInstance(ModelName key) const
{
	auto it = m_ModelPrototypes.find(key);
	if (it == m_ModelPrototypes.end())
		return nullptr;

	return CGameObject::CreateModelInstance(it->second.get());
}

ModelInstance* ResourceManager::CreateSkinnedModelInstance(ModelName key)
{
	auto it = m_SkinnedModelPrototypes.find(key);
	if (it == m_SkinnedModelPrototypes.end())
		return nullptr;

	CLoadedModelInfo* pPrototypeInfo = it->second.get();
	if (!pPrototypeInfo) return nullptr;
	if (!pPrototypeInfo->m_pModelRootObject) return nullptr;
	if (!pPrototypeInfo->m_pAnimationSets) return nullptr;

	CGameObject* pRootInstance = CGameObject::CreateModelInstance(pPrototypeInfo->m_pModelRootObject.get());

	if (!pRootInstance)
		return nullptr;

	ModelInstance* pInstanceInfo = new ModelInstance();

	pInstanceInfo->m_pRootObject = unique_ptr<CGameObject>(pRootInstance);
	pInstanceInfo->m_nSkinnedMeshes = pPrototypeInfo->m_nSkinnedMeshes;

	pInstanceInfo->m_pAnimationSets = pPrototypeInfo->m_pAnimationSets;

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

void ResourceManager::SaveTexture(string name, CTexture* tex)
{
	textureMap[name] = unique_ptr<CTexture>(tex);
}

CTexture* ResourceManager::GetTexture(string name)
{
	auto it = textureMap.find(name);
	if (it != textureMap.end())return it->second.get();
	return nullptr;
}

void ResourceManager::LoadMaterialsFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ModelResource* pParent, FILE* pInFile, CShader* pShader)
{
	char pstrToken[260] = { '\0' };
	int nMaterial = 0;
	UINT nReads = 0;

	pParent->m_nMaterials = ReadIntegerFromFile(pInFile);

	pParent->m_ppMaterials.resize(pParent->m_nMaterials);

	CMaterial* pMaterial = NULL;

	for (; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));

		if (!strcmp(pstrToken, "<Material>:"))
		{
			nMaterial = ReadIntegerFromFile(pInFile);

			pMaterial = new CMaterial(7); //0:Albedo, 1:Specular, 2:Metallic, 3:Normal, 4:Emission, 5:DetailAlbedo, 6:DetailNormal

			UINT nMeshType = pParent->GetMeshType();

			if (nMeshType & VERTEXT_NORMAL_TANGENT_TEXTURE)
			{
				if (nMeshType & VERTEXT_BONE_INDEX_WEIGHT)
				{
					if (pShader)
						pMaterial->SetShader(pShader);
					else
						pMaterial->SetSkinnedAnimationShader();
				}
				else
				{
					pMaterial->SetStandardShader();
				}
			}
			pParent->SetMaterial(nMaterial, pMaterial);
		}
		else if (!strcmp(pstrToken, "<AlbedoColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4AlbedoColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<EmissiveColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4EmissiveColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<SpecularColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4SpecularColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<Glossiness>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fGlossiness), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Smoothness>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fSmoothness), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Metallic>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fSpecularHighlight), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<SpecularHighlight>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fMetallic), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<GlossyReflection>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fGlossyReflection), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<AlbedoMap>:"))
		{
			pMaterial->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_ALBEDO_MAP, 3, pMaterial->m_ppstrTextureNames[0], &(pMaterial->m_ppTextures[0]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<SpecularMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_SPECULAR_MAP, 4, pMaterial->m_ppstrTextureNames[1], &(pMaterial->m_ppTextures[1]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<NormalMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_NORMAL_MAP, 5, pMaterial->m_ppstrTextureNames[2], &(pMaterial->m_ppTextures[2]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<MetallicMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_METALLIC_MAP, 6, pMaterial->m_ppstrTextureNames[3], &(pMaterial->m_ppTextures[3]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<EmissionMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_EMISSION_MAP, 7, pMaterial->m_ppstrTextureNames[4], &(pMaterial->m_ppTextures[4]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<DetailAlbedoMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_ALBEDO_MAP, 8, pMaterial->m_ppstrTextureNames[5], &(pMaterial->m_ppTextures[5]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<DetailNormalMap>:"))
		{
			pParent->m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_NORMAL_MAP, 9, pMaterial->m_ppstrTextureNames[6], &(pMaterial->m_ppTextures[6]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "</Materials>"))
		{
			break;
		}
	}
}

void ResourceManager::LoadAnimationFromFile(FILE * pInFile, CLoadedModelInfo * pLoadedModel)
{
	char pstrToken[260] = { '\0' };
	UINT nReads = 0;

	int nAnimationSets = 0;

	for (; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
		if (!strcmp(pstrToken, "<AnimationSets>:"))
		{
			nAnimationSets = ::ReadIntegerFromFile(pInFile);
			pLoadedModel->m_pAnimationSets = new CAnimationSets(nAnimationSets);
		}
		else if (!strcmp(pstrToken, "<FrameNames>:"))
		{
			pLoadedModel->m_pAnimationSets->m_nBoneFrames = ::ReadIntegerFromFile(pInFile);

			for (int j = 0; j < pLoadedModel->m_pAnimationSets->m_nBoneFrames; j++)
			{
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));

				pLoadedModel->m_pAnimationSets->m_vBoneNames.push_back(pstrToken);

#ifdef _WITH_DEBUG_SKINNING_BONE
				TCHAR pstrDebug[256] = { 0 };
				TCHAR pwstrAnimationBoneName[260] = { 0 };
				TCHAR pwstrBoneCacheName[260] = { 0 };
				size_t nConverted = 0;
				mbstowcs_s(&nConverted, pwstrAnimationBoneName, _countof(pwstrAnimationBoneName), pstrToken, _TRUNCATE);
				mbstowcs_s(&nConverted, pwstrBoneCacheName, _countof(pwstrBoneCacheName), pLoadedModel->m_pAnimationSets->m_ppBoneFrameCaches[j]->m_pstrFrameName, _TRUNCATE);
				_stprintf_s(pstrDebug, 256, _T("AnimationBoneFrame:: Cache(%s) AnimationBone(%s)\n"), pwstrBoneCacheName, pwstrAnimationBoneName);
				OutputDebugString(pstrDebug);
#endif
			}
		}
		else if (!strcmp(pstrToken, "<AnimationSet>:"))
		{
			int nAnimationSet = ::ReadIntegerFromFile(pInFile);

			::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken)); //Animation Set Name

			float fLength = ::ReadFloatFromFile(pInFile);
			int nFramesPerSecond = ::ReadIntegerFromFile(pInFile);
			int nKeyFrames = ::ReadIntegerFromFile(pInFile);

			CAnimationSet* pNewAnimationSet = new CAnimationSet(fLength, nFramesPerSecond, nKeyFrames, pLoadedModel->m_pAnimationSets->m_nBoneFrames, pstrToken);
			pLoadedModel->m_pAnimationSets->m_vAnimationSets.push_back(pNewAnimationSet);

			for (int i = 0; i < nKeyFrames; i++)
			{
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
				if (!strcmp(pstrToken, "<Transforms>:"))
				{
					int nKey = ::ReadIntegerFromFile(pInFile);
					float fKeyTime = ::ReadFloatFromFile(pInFile);

#ifdef _WITH_ANIMATION_SRT
					// 주의: 원본 코드의 변수명(pAnimationSet) 오류 가능성을 pNewAnimationSet으로 맞춰둠
					pNewAnimationSet->m_pfKeyFrameScaleTimes[i] = fKeyTime;
					pNewAnimationSet->m_pfKeyFrameRotationTimes[i] = fKeyTime;
					pNewAnimationSet->m_pfKeyFrameTranslationTimes[i] = fKeyTime;
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf3KeyFrameScales[i], sizeof(XMFLOAT3), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf4KeyFrameRotations[i], sizeof(XMFLOAT4), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf3KeyFrameTranslations[i], sizeof(XMFLOAT3), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
#else
					pNewAnimationSet->m_pfKeyFrameTimes[i] = fKeyTime;
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf4x4KeyFrameTransforms[i], sizeof(XMFLOAT4X4), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
#endif
				}
			}
		}
		else if (!strcmp(pstrToken, "</AnimationSets>"))
		{
			break;
		}
	}
}

ModelResource* ResourceManager::LoadFrameHierarchyFromFile(ID3D12Device * pd3dDevice, ID3D12GraphicsCommandList * pd3dCommandList, ID3D12RootSignature * pd3dGraphicsRootSignature, ModelResource * pParent, FILE * pInFile, CShader * pShader, int* pnSkinnedMeshes, const char* pstrFileName)
{
	char pstrToken[260] = { '\0' };
	UINT nReads = 0;

	int nFrame = 0, nTextures = 0;

	ModelResource* pGameObject = new ModelResource();

	for (; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
		if (!strcmp(pstrToken, "<Frame>:"))
		{
			nFrame = ::ReadIntegerFromFile(pInFile);
			nTextures = ::ReadIntegerFromFile(pInFile);

			::ReadStringFromFile(pInFile, pGameObject->m_pstrFrameName);
		}
		else if (!strcmp(pstrToken, "<Transform>:"))
		{
			XMFLOAT3 xmf3Position, xmf3Rotation, xmf3Scale;
			XMFLOAT4 xmf4Rotation;
			nReads = (UINT)::fread(&xmf3Position, sizeof(float), 3, pInFile);
			nReads = (UINT)::fread(&xmf3Rotation, sizeof(float), 3, pInFile); //Euler Angle
			nReads = (UINT)::fread(&xmf3Scale, sizeof(float), 3, pInFile);
			nReads = (UINT)::fread(&xmf4Rotation, sizeof(float), 4, pInFile); //Quaternion
		}
		else if (!strcmp(pstrToken, "<TransformMatrix>:"))
		{
			nReads = (UINT)::fread(&pGameObject->m_xmf4x4ToParent, sizeof(float), 16, pInFile);
		}
		else if (!strcmp(pstrToken, "<Mesh>:"))
		{
			CStandardMesh* pMesh = new CStandardMesh(pd3dDevice, pd3dCommandList);
			pMesh->LoadMeshFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);
			pGameObject->SetMesh(pMesh);
		}
		else if (!strcmp(pstrToken, "<SkinningInfo>:"))
		{
			if (pnSkinnedMeshes) (*pnSkinnedMeshes)++;

			CSkinnedMesh* pSkinnedMesh = new CSkinnedMesh(pd3dDevice, pd3dCommandList);
			pSkinnedMesh->LoadSkinInfoFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);
			pSkinnedMesh->CreateShaderVariables(pd3dDevice, pd3dCommandList);

			::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));//<Mesh>:
			if (!strcmp(pstrToken, "<Mesh>:")) pSkinnedMesh->LoadMeshFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);

			pGameObject->SetMesh(pSkinnedMesh);
		}
		else if (!strcmp(pstrToken, "<Materials>:"))
		{
			LoadMaterialsFromFile(pd3dDevice, pd3dCommandList, pGameObject, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<Children>:"))
		{
			int nChilds = ::ReadIntegerFromFile(pInFile);
			if (nChilds > 0)
			{
				for (int i = 0; i < nChilds; i++)
				{
					ModelResource* pChild = LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pGameObject, pInFile, pShader, pnSkinnedMeshes, pstrFileName);
					if (pChild) pGameObject->SetChild(pChild);
#ifdef _WITH_DEBUG_FRAME_HIERARCHY
					TCHAR pstrDebug[256] = { 0 };
					_stprintf_s(pstrDebug, 256, _T("(Frame: %p) (Parent: %p)\n"), pChild, pGameObject);
					OutputDebugString(pstrDebug);
#endif
				}
			}
		}
		else if (!strcmp(pstrToken, "</Frame>"))
		{
			break;
		}
	}
	return(pGameObject);
}

ModelResource* ResourceManager::LoadGeometryModelByName(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, ModelResource* pParent, const char* name, CShader* pShader, int* pnSkinnedMeshes)
{
	ModelResource* model = NULL;
	FILE* pInFile;
	::fopen_s(&pInFile, name, "rb");
	if (pInFile)
	{
		::rewind(pInFile);
		model = LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pParent, pInFile, pShader, 0, name);
		::fclose(pInFile);
	}
	else
	{
		OutputDebugString(L"Error: file not found.\n");
	}
	return model;
}

CLoadedModelInfo* ResourceManager::LoadGeometryAndAnimationFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, const char* pstrFileName, CShader* pShader)
{
	FILE* pInFile = NULL;
	::fopen_s(&pInFile, pstrFileName, "rb");
	::rewind(pInFile);

	CLoadedModelInfo* pLoadedModel = new CLoadedModelInfo();

	char pstrToken[260] = { '\0' };

	for (; ; )
	{
		if (::ReadStringFromFile(pInFile, pstrToken))
		{
			if (!strcmp(pstrToken, "<Hierarchy>:"))
			{
				pLoadedModel->m_pModelRootObject = unique_ptr<ModelResource>(LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, NULL, pInFile, pShader, &pLoadedModel->m_nSkinnedMeshes, pstrFileName));
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken)); //"</Hierarchy>"
			}
			else if (!strcmp(pstrToken, "<Animation>:"))
			{
				LoadAnimationFromFile(pInFile, pLoadedModel);
				int nSkinnedMesh = 0;
				pLoadedModel->m_ppSkinnedMeshes = new CSkinnedMesh * [pLoadedModel->m_nSkinnedMeshes];
				pLoadedModel->m_pModelRootObject->FindAndSetSkinnedMesh(pLoadedModel->m_ppSkinnedMeshes, &nSkinnedMesh);
			}
			else if (!strcmp(pstrToken, "</Animation>:"))
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

#ifdef _WITH_DEBUG_FRAME_HIERARCHY
	TCHAR pstrDebug[256] = { 0 };
	_stprintf_s(pstrDebug, 256, _T("Frame Hierarchy\n"));
	OutputDebugString(pstrDebug);

	CGameObject::PrintFrameInfo(pGameObject, NULL);
#endif

	return(pLoadedModel);
}
