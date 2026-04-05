//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "Player.h"
#include "EnemyObject.h"
#include "Shader.h"
#include "InputManager.h"
#include"ShadowMap.h"
#include "EffectShader.h"
#include"Collision.h"

ID3D12DescriptorHeap *CScene::m_pd3dCbvSrvDescriptorHeap = NULL;

D3D12_CPU_DESCRIPTOR_HANDLE	CScene::m_d3dCbvCPUDescriptorStartHandle;
D3D12_GPU_DESCRIPTOR_HANDLE	CScene::m_d3dCbvGPUDescriptorStartHandle;
D3D12_CPU_DESCRIPTOR_HANDLE	CScene::m_d3dSrvCPUDescriptorStartHandle;
D3D12_GPU_DESCRIPTOR_HANDLE	CScene::m_d3dSrvGPUDescriptorStartHandle;

D3D12_CPU_DESCRIPTOR_HANDLE	CScene::m_d3dCbvCPUDescriptorNextHandle;
D3D12_GPU_DESCRIPTOR_HANDLE	CScene::m_d3dCbvGPUDescriptorNextHandle;
D3D12_CPU_DESCRIPTOR_HANDLE	CScene::m_d3dSrvCPUDescriptorNextHandle;
D3D12_GPU_DESCRIPTOR_HANDLE	CScene::m_d3dSrvGPUDescriptorNextHandle;

static CGameObject* FindLaserMuzzleFrame(CGameObject* pWeapon)
{
	if (!pWeapon) return nullptr;

	static const char* s_ppMuzzleNames[] =
	{
		"Muzzle",
		"muzzle",
		"MuzzleFlash",
		"muzzleflash",
		"FirePos",
		"firepos",
		"BarrelEnd",
		"barrel_end",
		"Socket_Muzzle",
		"socket_muzzle"
	};

	for (const char* name : s_ppMuzzleNames)
	{
		CGameObject* pFrame = pWeapon->FindFrame(name);
		if (pFrame) return pFrame;
	}

	return nullptr;
}

CScene::CScene()
{
	colManager = std::make_unique<CollisionManager>();
}

CScene::~CScene()
{
}

void CScene::BuildDefaultLightsAndMaterials()
{
	m_xmf4GlobalAmbient = XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);

	LIGHT directional;
	directional.m_bEnable = true;
	directional.m_nType = DIRECTIONAL_LIGHT;
	directional.m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	directional.m_xmf4Diffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	directional.m_xmf4Specular = XMFLOAT4(0.4f, 0.4f, 0.4f, 0.0f);
	directional.m_xmf3Direction = XMFLOAT3(1.0f, -1.0f, 0.0f);
	m_pLights.push_back(directional);

	m_nLights = m_pLights.size();
}

void tempfunc() {
	OutputDebugString(L"fds\n");
}

void CScene::BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	CreateCbvSrvDescriptorHeaps(pd3dDevice, 0, 29 + 4); //SuperCobra(17), Gunship(2), Player:Mi24(1), Angrybot()

	CMaterial::PrepareShaders(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature); 

	BuildDefaultLightsAndMaterials();

	m_pSkyBox = std::make_unique<CSkyBox>(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	XMFLOAT3 xmf3Scale(8.0f, 2.0f, 8.0f);
	XMFLOAT4 xmf4Color(0.0f, 0.3f, 0.0f, 0.0f);

	UIShader = make_unique<UIObjectShader>();
	UIShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	
	UIObject* UItemp = new UIObject();
	UItemp->SetChild(CGameObject::LoadGeometryModelByName(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, "Model/pannel.bin", UIShader.get(), 0));
	UItemp->SetPosition(0.5, 0, 0.5);
	UItemp->SetScale(0.5, 0.5, 1);

	UItemp->setAABB();
	UItemp->SetFunc(tempfunc);
	UIShader->addObjects(std::unique_ptr<UIObject>(UItemp));


	std::unique_ptr<CStandardObjectsShader> stdshader = std::make_unique<CStandardObjectsShader>();
	stdshader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	stdshader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	stdshader->CreateShadowShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	std::unique_ptr<CGameObject> map(CGameObject::LoadGeometryModelByName(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, "Model/map0310_dds.bin", stdshader.get(), 0));
	map->SetPosition(-150, -0.5, -150);
	map->SetOOBB(NULL);
	stdshader->addObjects(std::move(map));
	m_ppShaders.push_back(std::move(stdshader));


	std::unique_ptr<ViewShader> view = make_unique<ViewShader>();
	view->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	view->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	view->CreateThroughShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	FILE* viewFile = NULL;

	::fopen_s(&viewFile, "Model/r.bin", "rb");
	if (viewFile)
	{
		::rewind(viewFile);
		CGameObject* viewmodel = CGameObject::LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL, viewFile, view.get(), 0);
		viewmodel->SetPosition(0, 0, 0);
		std::unique_ptr<ViewObject> viewobj = make_unique<ViewObject>();
		viewobj->SetPosition(0, 0, 0);
		viewobj->SetChild(viewmodel);
		view->addObjects(std::move(viewobj));
		::fclose(viewFile);
	}
	
	m_ppShaders.push_back(std::move(view));

	// 레이저 오브젝트
	auto pLaserShader = std::make_unique<CLaserShader>();

	pLaserShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	pLaserShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_pLaserObject = new CGameObject();
	m_pLaserObject->SetMesh(new CLaserMesh(pd3dDevice, pd3dCommandList));
	m_pLaserObject->SetShader(pLaserShader.get());

	pLaserShader->addObjects(std::unique_ptr<CGameObject>(m_pLaserObject));

	XMStoreFloat4x4(&m_pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
	m_pLaserObject->UpdateTransform(NULL);

	m_ppShaders.push_back(std::move(pLaserShader));

	//디버그 쉐이더
	m_pDebugShader = new CBoundingBoxShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	//오브젝트 쉐이더(스탠다드, 스킨드)
	auto pSkinnedShader = std::make_unique<CSkinnedAnimationObjectsShader>();

	pSkinnedShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	pSkinnedShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pSkinnedShader->CreateShadowShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);


	//적 오브젝트
	CEnemyObject* pEnemy = new CEnemyObject(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pEnemy->SetPosition(0.0f, 0.0f, 10.0f);
	pEnemy->SetScale(1.0f, 1.0f, 1.0f);
	pEnemy->SetOOBB(NULL);
	pSkinnedShader->addObjects(std::unique_ptr<CGameObject>(pEnemy));

	//여기에 otherplayer 생성하고 위처럼 집어 넣으면 됨


	m_ppShaders.push_back(std::move(pSkinnedShader));

	//이펙트	쉐이더
	auto pEffectShader = std::make_unique<CEffectShader>();
	CEffectShader* pRawEffectShader = pEffectShader.get();

	m_pEffectShader = pRawEffectShader;

	pRawEffectShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	pRawEffectShader->CreateGraphicsPipelineState(pd3dDevice, m_pd3dGraphicsRootSignature, 0);

	m_ppShaders.push_back(std::move(pEffectShader));

	float effectWidth = 5.0f;
	float effectHeight = 5.0f * (180.0f / 182.0f);

	m_pEffectMesh = new CParticleMesh(pd3dDevice, pd3dCommandList, effectWidth, effectHeight);

	//bomb effect
	CTexture* pBombTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
	pBombTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Explosion1.dds", RESOURCE_TEXTURE2D, 0);
	CScene::CreateShaderResourceViews(pd3dDevice, pBombTexture, 0, 3);

	m_pEffectMaterials[EFFECT_BOMB] = new CMaterial(1);
	m_pEffectMaterials[EFFECT_BOMB]->SetTexture(pBombTexture);
	m_pEffectMaterials[EFFECT_BOMB]->SetShader(pRawEffectShader);

	//spark effect
	CTexture* pSparkTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
	pSparkTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Explosion1.dds", RESOURCE_TEXTURE2D, 0);
	CScene::CreateShaderResourceViews(pd3dDevice, pSparkTexture, 0, 3);

	m_pEffectMaterials[EFFECT_SPARK] = new CMaterial(1);
	m_pEffectMaterials[EFFECT_SPARK]->SetTexture(pSparkTexture);
	m_pEffectMaterials[EFFECT_SPARK]->SetShader(pRawEffectShader);

	//blood effect
	CTexture* pBloodTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
	pBloodTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Explosion1.dds", RESOURCE_TEXTURE2D, 0);
	CScene::CreateShaderResourceViews(pd3dDevice, pBloodTexture, 0, 3);

	m_pEffectMaterials[EFFECT_BLOOD] = new CMaterial(1);
	m_pEffectMaterials[EFFECT_BLOOD]->SetTexture(pBloodTexture);
	m_pEffectMaterials[EFFECT_BLOOD]->SetShader(pRawEffectShader);

	for (int i = 0; i < EFFECT_MAX; i++)
	{
		// 인스턴스 버퍼	크기
		UINT nBufferSize = sizeof(EFFECT_INFO) * 100;

		// 버퍼	생성
		m_pd3dInstBufferEffect[i] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL,
			nBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
		m_pd3dInstBufferEffect[i]->Map(0, NULL, (void**)&m_pMappedInstBufferEffect[i]);

		// 인스턴 버퍼 뷰 생성
		m_d3dInstBufferViewEffect[i].BufferLocation = m_pd3dInstBufferEffect[i]->GetGPUVirtualAddress();
		m_d3dInstBufferViewEffect[i].StrideInBytes = sizeof(EFFECT_INFO);
		m_d3dInstBufferViewEffect[i].SizeInBytes = nBufferSize;
	}

	for (int i = 0; i < MAX_BOMB_EFFECTS; i++)
	{
		m_vEffectPools[EFFECT_BOMB].push_back(new CEffect(EFFECT_BOMB, 1.0f));
		m_vEffectPools[EFFECT_SPARK].push_back(new CEffect(EFFECT_SPARK, 0.5f));
		m_vEffectPools[EFFECT_BLOOD].push_back(new CEffect(EFFECT_BLOOD, 0.8f));
	}
	
	for (const auto& shader : m_ppShaders) {
		auto* objs = shader->GetObj();
		if (objs != nullptr && !objs->empty()) {
			for (auto& obj : *objs) {
				m_pDebugShader->AddObject(obj.get());
			}
		}
	}

	ShadowCameraManager.CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature->Release();
	if (m_pd3dCbvSrvDescriptorHeap) m_pd3dCbvSrvDescriptorHeap->Release();

	m_ppShaders.clear();

	ReleaseShaderVariables();

	m_pLights.clear();
}

ID3D12RootSignature *CScene::CreateGraphicsRootSignature(ID3D12Device *pd3dDevice)
{
	ID3D12RootSignature *pd3dGraphicsRootSignature = NULL;

	D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[11];

	pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[0].NumDescriptors = 1;
	pd3dDescriptorRanges[0].BaseShaderRegister = 6; //t6: gtxtAlbedoTexture
	pd3dDescriptorRanges[0].RegisterSpace = 0;
	pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[1].NumDescriptors = 1;
	pd3dDescriptorRanges[1].BaseShaderRegister = 7; //t7: gtxtSpecularTexture
	pd3dDescriptorRanges[1].RegisterSpace = 0;
	pd3dDescriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[2].NumDescriptors = 1;
	pd3dDescriptorRanges[2].BaseShaderRegister = 8; //t8: gtxtNormalTexture
	pd3dDescriptorRanges[2].RegisterSpace = 0;
	pd3dDescriptorRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[3].NumDescriptors = 1;
	pd3dDescriptorRanges[3].BaseShaderRegister = 9; //t9: gtxtMetallicTexture
	pd3dDescriptorRanges[3].RegisterSpace = 0;
	pd3dDescriptorRanges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[4].NumDescriptors = 1;
	pd3dDescriptorRanges[4].BaseShaderRegister = 10; //t10: gtxtEmissionTexture
	pd3dDescriptorRanges[4].RegisterSpace = 0;
	pd3dDescriptorRanges[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[5].NumDescriptors = 1;
	pd3dDescriptorRanges[5].BaseShaderRegister = 11; //t11: gtxtEmissionTexture
	pd3dDescriptorRanges[5].RegisterSpace = 0;
	pd3dDescriptorRanges[5].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[6].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[6].NumDescriptors = 1;
	pd3dDescriptorRanges[6].BaseShaderRegister = 12; //t12: gtxtEmissionTexture
	pd3dDescriptorRanges[6].RegisterSpace = 0;
	pd3dDescriptorRanges[6].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[7].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[7].NumDescriptors = 1;
	pd3dDescriptorRanges[7].BaseShaderRegister = 13; //t13: gtxtSkyBoxTexture
	pd3dDescriptorRanges[7].RegisterSpace = 0;
	pd3dDescriptorRanges[7].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[8].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[8].NumDescriptors = 1;
	pd3dDescriptorRanges[8].BaseShaderRegister = 1; //t1: gtxtTerrainBaseTexture
	pd3dDescriptorRanges[8].RegisterSpace = 0;
	pd3dDescriptorRanges[8].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[9].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[9].NumDescriptors = 1;
	pd3dDescriptorRanges[9].BaseShaderRegister = 2; //t2: gtxtTerrainDetailTexture
	pd3dDescriptorRanges[9].RegisterSpace = 0;
	pd3dDescriptorRanges[9].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	pd3dDescriptorRanges[10].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[10].NumDescriptors = 1;
	pd3dDescriptorRanges[10].BaseShaderRegister = 14; //t2: gtxtTerrainDetailTexture
	pd3dDescriptorRanges[10].RegisterSpace = 0;
	pd3dDescriptorRanges[10].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	D3D12_ROOT_PARAMETER pd3dRootParameters[17 + 1];

	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[0].Descriptor.ShaderRegister = 1; //Camera
	pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 33;
	pd3dRootParameters[1].Constants.ShaderRegister = 2; //GameObject
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[2].Descriptor.ShaderRegister = 4; //Lights
	pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[3].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[0]);
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[4].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[1]);
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[5].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[2]);
	pd3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[6].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[3]);
	pd3dRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[7].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[7].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[4]);
	pd3dRootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[8].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[8].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[5]);
	pd3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[9].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[9].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[6]);
	pd3dRootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[10].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[10].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[7]);
	pd3dRootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[11].Descriptor.ShaderRegister = 7; //Skinned Bone Offsets
	pd3dRootParameters[11].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	pd3dRootParameters[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[12].Descriptor.ShaderRegister = 8; //Skinned Bone Transforms
	pd3dRootParameters[12].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	pd3dRootParameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[13].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[13].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[8]);
	pd3dRootParameters[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[14].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[14].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[9]);
	pd3dRootParameters[14].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[15].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[15].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[15].DescriptorTable.pDescriptorRanges = &(pd3dDescriptorRanges[10]);
	pd3dRootParameters[15].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[16].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[16].Descriptor.ShaderRegister = 9; // b5
	pd3dRootParameters[16].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[16].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[17].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[17].Descriptor.ShaderRegister = 10; // b10
	pd3dRootParameters[17].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[17].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC pd3dSamplerDescs[3];

	pd3dSamplerDescs[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	pd3dSamplerDescs[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	pd3dSamplerDescs[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	pd3dSamplerDescs[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	pd3dSamplerDescs[0].MipLODBias = 0;
	pd3dSamplerDescs[0].MaxAnisotropy = 1;
	pd3dSamplerDescs[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pd3dSamplerDescs[0].MinLOD = 0;
	pd3dSamplerDescs[0].MaxLOD = D3D12_FLOAT32_MAX;
	pd3dSamplerDescs[0].ShaderRegister = 0;
	pd3dSamplerDescs[0].RegisterSpace = 0;
	pd3dSamplerDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dSamplerDescs[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	pd3dSamplerDescs[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	pd3dSamplerDescs[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	pd3dSamplerDescs[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	pd3dSamplerDescs[1].MipLODBias = 0;
	pd3dSamplerDescs[1].MaxAnisotropy = 1;
	pd3dSamplerDescs[1].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pd3dSamplerDescs[1].MinLOD = 0;
	pd3dSamplerDescs[1].MaxLOD = D3D12_FLOAT32_MAX;
	pd3dSamplerDescs[1].ShaderRegister = 1;
	pd3dSamplerDescs[1].RegisterSpace = 0;
	pd3dSamplerDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dSamplerDescs[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	pd3dSamplerDescs[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	pd3dSamplerDescs[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	pd3dSamplerDescs[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	pd3dSamplerDescs[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; 
	pd3dSamplerDescs[2].MipLODBias = 0;
	pd3dSamplerDescs[2].MaxAnisotropy = 1;
	pd3dSamplerDescs[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; 
	pd3dSamplerDescs[2].MinLOD = 0;
	pd3dSamplerDescs[2].MaxLOD = D3D12_FLOAT32_MAX;
	pd3dSamplerDescs[2].ShaderRegister = 2; 
	pd3dSamplerDescs[2].RegisterSpace = 0;
	pd3dSamplerDescs[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = _countof(pd3dSamplerDescs);
	d3dRootSignatureDesc.pStaticSamplers = pd3dSamplerDescs;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ID3DBlob *pd3dSignatureBlob = NULL;
	ID3DBlob *pd3dErrorBlob = NULL;
	D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
	pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&pd3dGraphicsRootSignature);
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
	return(pd3dGraphicsRootSignature);
}

void CScene::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); //256의 배수
	m_pd3dcbLights = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);

	m_pd3dcbLights->Map(0, NULL, (void **)&m_pcbMappedLights);
}
void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	::memcpy(m_pcbMappedLights->m_pLights, m_pLights.data(), sizeof(LIGHT) * m_nLights);
	::memcpy(&m_pcbMappedLights->m_xmf4GlobalAmbient, &m_xmf4GlobalAmbient, sizeof(XMFLOAT4));
	::memcpy(&m_pcbMappedLights->m_nLights, &m_nLights, sizeof(int));
}
void CScene::ReleaseShaderVariables()
{
	if (m_pd3dcbLights)
	{
		m_pd3dcbLights->Unmap(0, NULL);
		m_pd3dcbLights->Release();
	}
}
void CScene::ReleaseUploadBuffers()
{
	if (m_pSkyBox) m_pSkyBox->ReleaseUploadBuffers();


	for (int i = 0; i < m_ppShaders.size(); i++) 
		if (m_ppShaders[i])
			m_ppShaders[i]->ReleaseUploadBuffers();
}

void CScene::CreateCbvSrvDescriptorHeaps(ID3D12Device *pd3dDevice, int nConstantBufferViews, int nShaderResourceViews)
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	d3dDescriptorHeapDesc.NumDescriptors = nConstantBufferViews + nShaderResourceViews; //CBVs + SRVs 
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_pd3dCbvSrvDescriptorHeap);

	m_d3dCbvCPUDescriptorNextHandle = m_d3dCbvCPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_d3dCbvGPUDescriptorNextHandle = m_d3dCbvGPUDescriptorStartHandle = m_pd3dCbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_d3dSrvCPUDescriptorNextHandle.ptr = m_d3dSrvCPUDescriptorStartHandle.ptr = m_d3dCbvCPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);
	m_d3dSrvGPUDescriptorNextHandle.ptr = m_d3dSrvGPUDescriptorStartHandle.ptr = m_d3dCbvGPUDescriptorStartHandle.ptr + (::gnCbvSrvDescriptorIncrementSize * nConstantBufferViews);
}
D3D12_GPU_DESCRIPTOR_HANDLE CScene::CreateConstantBufferViews(ID3D12Device *pd3dDevice, int nConstantBufferViews, ID3D12Resource *pd3dConstantBuffers, UINT nStride)
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
void CScene::CreateShaderResourceViews(ID3D12Device* pd3dDevice, CTexture* pTexture, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex)
{
	m_d3dSrvCPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	m_d3dSrvGPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	if (pTexture)
	{
		int nTextures = pTexture->GetTextures();
		for (int i = 0; i < nTextures; i++)
		{
			ID3D12Resource* pShaderResource = pTexture->GetResource(i);
			D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc = pTexture->GetShaderResourceViewDesc(i);
			pd3dDevice->CreateShaderResourceView(pShaderResource, &d3dShaderResourceViewDesc, m_d3dSrvCPUDescriptorNextHandle);
			m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
			pTexture->SetGpuDescriptorHandle(i, m_d3dSrvGPUDescriptorNextHandle);
			m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
		}
	}
	int nRootParameters = pTexture->GetRootParameters();
	for (int j = 0; j < nRootParameters; j++) pTexture->SetRootParameterIndex(j, nRootParameterStartIndex + j);
}

void CScene::CreateshadowResourceViews(ID3D12Device* pd3dDevice, ShadowMap* shadowmap, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex)
{
	m_d3dSrvCPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);
	m_d3dSrvGPUDescriptorNextHandle.ptr += (::gnCbvSrvDescriptorIncrementSize * nDescriptorHeapIndex);

	shadowmap->CreateSRV(pd3dDevice, m_d3dSrvCPUDescriptorNextHandle, m_d3dSrvGPUDescriptorNextHandle);

	m_d3dSrvCPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
	m_d3dSrvGPUDescriptorNextHandle.ptr += ::gnCbvSrvDescriptorIncrementSize;
}


void CScene::PlayEffect(EFFECT_TYPE type, XMFLOAT3 pos)
{
	for (CEffect* pEffect : m_vEffectPools[type])
	{
		if (pEffect->IsDead())
		{
			pEffect->Play(pos);
			return;
		}
	}

	float lifeTime = (type == EFFECT_BOMB) ? 1.0f : 0.5f;
	CEffect* pNewEffect = new CEffect(type, lifeTime);
	pNewEffect->Play(pos);
	m_vEffectPools[type].push_back(pNewEffect);
}

void CScene::SetPlayer(CPlayer* p)
{
	m_pPlayer = p;
	std::vector<std::unique_ptr<CGameObject>>* pVector = m_ppShaders[SHADERIDX::VIEW]->GetObj();
	CGameObject* pGameObj = (*pVector)[0].get();
	ViewObject* pViewObj = static_cast<ViewObject*>(pGameObj);
	pViewObj->setPlayer(p);
	if (m_pPlayer) m_pDebugShader->AddObject(m_pPlayer);

	ShadowCameraManager.SetPlayer(p->GetCamera());
	ShadowCameraManager.SetDir(m_pLights[0].m_xmf3Direction);
	if (m_ppShaders[SHADERIDX::ENEMY]) {
		auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();
		for (auto& obj : *objs) {
			CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj.get());
			if (pEnemy) {
				pEnemy->SetPlayer(m_pPlayer);
			}
		}
	}
	m_pLaserMuzzle = nullptr;
	if (m_pPlayer && m_pPlayer->GetWeapon())
	{
		m_pLaserMuzzle = FindLaserMuzzleFrame(m_pPlayer->GetWeapon());
	}
}

CCamera* CScene::GetLightCamera(int idx)
{
	return &ShadowCameraManager.GetCameras()[idx];
}

void CScene::DeleteDeadObject(UINT64 Fence)
{
	for (auto& shader : m_ppShaders) {
		shader->DeleteObject(Fence);
	}
}

void CScene::DeleteTrash(UINT64 Fence)
{
	for (auto& shader : m_ppShaders) {
		shader->ProcessingGarbageQueue(Fence);
	}
}

void CScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	//handlehp, handlecollision 함수 통일해야함
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	{
		m_bLaserActive = true;

		if (UIShader) UIShader->ProcessClick(InputManager::Instance().GetMousePos());
		if (!m_pPlayer || m_ppShaders[SHADERIDX::ENEMY]->GetObj()->empty()) return;

		XMFLOAT3 playerPos = m_pPlayer->GetPosition();
		XMVECTOR rayOrigin = XMLoadFloat3(&playerPos);

		XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetLookVector()));
		if (m_ppShaders[SHADERIDX::ENEMY])
		{
			auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();
			for (auto& obj : *objs)
			{
				const auto& oobbs = obj->GetOOBB();

				for (BoundingOrientedBox* pOOBB : oobbs)
				{
					float fDist = 0.0f;

					if (pOOBB->Intersects(rayOrigin, rayDir, fDist))
					{
						CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj.get());
						if (pEnemy)
						{
							pEnemy->HandleHP(10.0f);
						}
						break;
					}
				}
			}
		}
		break;
	}

	case WM_LBUTTONUP:
		m_bLaserActive = false;
		break;

	default:
		break;
	}
}

bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (!m_pPlayer) return false;

	switch (nMessageID)
	{
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		INPUT_KEY key;
		bool validKey = true;

		switch (wParam)
		{
		case VK_UP:     key = INPUT_KEY::UP; break;
		case VK_DOWN:   key = INPUT_KEY::DOWN; break;
		case VK_LEFT:   key = INPUT_KEY::LEFT; break;
		case VK_RIGHT:  key = INPUT_KEY::RIGHT; break;

		case 'W': key = INPUT_KEY::W; break;
		case 'A': key = INPUT_KEY::A; break;
		case 'S': key = INPUT_KEY::S; break;
		case 'D': key = INPUT_KEY::D; break;

		case 'E': key = INPUT_KEY::E; break;
		case 'G': key = INPUT_KEY::G; break;
		case 'I': key = INPUT_KEY::I; break;

		case '1': key = INPUT_KEY::KEY_1; break;
		case '2': key = INPUT_KEY::KEY_2; break;
		case '3': key = INPUT_KEY::KEY_3; break;
		case '4': key = INPUT_KEY::KEY_4; break;

		case VK_SHIFT:   key = INPUT_KEY::SHIFT; break;
		case VK_SPACE:   key = INPUT_KEY::SPACE; break;

		case VK_LBUTTON: key = INPUT_KEY::LBUTTON; break;
		case VK_RBUTTON: key = INPUT_KEY::RBUTTON; break;
			break;
		default:
			validKey = false;
			break;
		}

		if (!validKey)
			break;
		
		if (nMessageID == WM_KEYDOWN)
		{
			// lParam의 30번째 비트가 1이면, 이전에 이미 눌려있던 키가 다시 들어온 것입니다.
			bool wasDownBefore = (lParam & (1 << 30)) != 0;
			if (wasDownBefore)
				break;
		}
		KEY_STATE state =
			(nMessageID == WM_KEYDOWN) ? KEY_STATE::DOWN : KEY_STATE::UP;

		GameEvent e;
		e.type = EventType::Input;
		e.keyEvent = { key, state };

		m_pPlayer->AddEvent(e);
		return true;
	}
	}

	return false;
}
bool CScene::ProcessInput(UCHAR *pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{

	m_fElapsedTime = fTimeElapsed;
	for (int i = 0; i < m_ppShaders.size(); i++) 
		if (m_ppShaders[i]) m_ppShaders[i]->AnimateObjects(fTimeElapsed);

	/*for (int i = 0; i < m_ppGameObjects.size(); i++)
		if (m_ppGameObjects[i]) m_ppGameObjects[i]->Animate(fTimeElapsed);*/

	//if (m_pLights.size() > 1)
	//{
	//	m_pLights[1].m_xmf3Position = m_pPlayer->GetPosition();
	//	m_pLights[1].m_xmf3Direction = m_pPlayer->GetLookVector();
	//}

	if (m_pPlayer && m_pLights.size() > 1)
	{
		m_pLights[1].m_xmf3Position = m_pPlayer->GetPosition();
		m_pLights[1].m_xmf3Direction = m_pPlayer->GetLookVector();
	}

	//이펙트 풀 순회하고 살아있는 이펙트들만 Animate() 호출
	for (int type = 0; type < EFFECT_MAX; type++)
	{
		for (CEffect* pEffect : m_vEffectPools[type])
		{
			if (!pEffect->IsDead())
			{
				pEffect->Animate(fTimeElapsed);
			}
		}
	}
	
	// 레이저 표시/숨김 처리
	if (m_pLaserObject)
	{
		if (m_bLaserActive && m_pPlayer)
		{
			if (!m_pLaserMuzzle && m_pPlayer->GetWeapon())
			{
				m_pLaserMuzzle = FindLaserMuzzleFrame(m_pPlayer->GetWeapon());
			}

			XMVECTOR rayOrigin;
			XMVECTOR rayDir;
			XMVECTOR vUp;
			XMVECTOR vRight;

			if (m_pLaserMuzzle)
			{
				XMFLOAT3 muzzlePos = m_pLaserMuzzle->GetPosition();
				XMFLOAT3 muzzleLook = m_pLaserMuzzle->GetLook();
				XMFLOAT3 muzzleUp = m_pLaserMuzzle->GetUp();
				XMFLOAT3 muzzleRight = m_pLaserMuzzle->GetRight();

				rayOrigin = XMLoadFloat3(&muzzlePos);
				rayDir = XMVector3Normalize(XMLoadFloat3(&muzzleLook));
				vUp = XMVector3Normalize(XMLoadFloat3(&muzzleUp));
				vRight = XMVector3Normalize(XMLoadFloat3(&muzzleRight));
			}
			else
			{
				// 총구 프레임을 못 찾은 경우 fallback
				XMVECTOR vPos = XMLoadFloat3(&m_pPlayer->GetPosition());
				XMVECTOR vLook = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetLookVector()));
				vRight = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetRightVector()));
				vUp = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetUpVector()));
				vUp = XMVector3Normalize(XMVector3Cross(vLook, vRight));

				rayOrigin = vPos + vUp * 1.2f + vRight * 0.3f + vLook * 0.5f;
				rayDir = vLook;
			}

			XMMATRIX matScale = XMMatrixScaling(0.05f, 0.05f, m_fLaserLength);

			XMMATRIX matRotation = XMMatrixIdentity();
			matRotation.r[0] = XMVectorSetW(vRight, 0.0f);
			matRotation.r[1] = XMVectorSetW(vUp, 0.0f);
			matRotation.r[2] = XMVectorSetW(rayDir, 0.0f);
			matRotation.r[3] = XMVectorSet(0, 0, 0, 1);

			XMMATRIX matTranslation = XMMatrixTranslationFromVector(rayOrigin);
			XMMATRIX matWorld = matScale * matRotation * matTranslation;

			XMStoreFloat4x4(&m_pLaserObject->m_xmf4x4ToParent, matWorld);
			m_pLaserObject->UpdateTransform(NULL);
		}
		else
		{
			XMStoreFloat4x4(&m_pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
			m_pLaserObject->UpdateTransform(NULL);
		}
	}
	ShadowCameraManager.Update();

	colManager->DoCollision(m_pPlayer, m_ppShaders[SHADERIDX::MAP]->GetObj());
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera)
{
	if (m_pd3dGraphicsRootSignature) pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
	if (m_pd3dCbvSrvDescriptorHeap) pd3dCommandList->SetDescriptorHeaps(1, &m_pd3dCbvSrvDescriptorHeap);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dcbLightsGpuVirtualAddress);


	pd3dCommandList->OMSetStencilRef(0xff);

	if (nPipelineState == SHADOW) {
		for (int i = 0; i < m_ppShaders.size(); i++)
		{
			if (m_ppShaders[i] && m_ppShaders[i]->DoShadow())
				m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
		}
	}
	else {
		if (m_pSkyBox) m_pSkyBox->Render(pd3dCommandList, false, nPipelineState, pCamera);
		for (int i = 0; i < m_ppShaders.size(); i++) if (m_ppShaders[i]) m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
		m_pDebugShader->Render(pd3dCommandList, pCamera, nPipelineState);
		UIShader->Render(pd3dCommandList, pCamera, true ,nPipelineState);
	}

	if (m_pEffectShader) m_pEffectShader->Render(pd3dCommandList, pCamera, false, nPipelineState);

	for (int type = 0; type < EFFECT_MAX; type++)
	{
		int activeCount = 0;

		for (CEffect* pEffect : m_vEffectPools[type])
		{
			if (!pEffect->IsDead())
			{
				m_pMappedInstBufferEffect[type][activeCount].vPosition = pEffect->GetPosition();
				m_pMappedInstBufferEffect[type][activeCount].fProgress = pEffect->GetProgress();

				activeCount++;
				if (activeCount >= 100) break;
			}
		}

		if (activeCount > 0 && m_pEffectMesh && m_pEffectMaterials[type])
		{
			m_pEffectMaterials[type]->UpdateShaderVariables(pd3dCommandList);

			pd3dCommandList->IASetVertexBuffers(1, 1, &m_d3dInstBufferViewEffect[type]);

			m_pEffectMesh->Render(pd3dCommandList, activeCount);
		}
	}
}

void CScene::ThroughRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	if (m_ppShaders[1]) m_ppShaders[SHADERIDX::VIEW]->Render(pd3dCommandList, pCamera, true, THROUGH);
}

