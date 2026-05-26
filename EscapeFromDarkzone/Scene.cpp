//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "Player.h"
#include "EnemyObject.h"
#include "Shader.h"
#include "InputManager.h"
#include "ShadowMap.h"
#include "EffectShader.h"
#include "Collision.h"
#include "UI.h"
#include "Item.h"
#include "AI.h"
#include "EffectManager.h"
#include "InventoryManager.h"
#include "ResourceManager.h"

#include "Network.h"

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
static CGameObject* FindWeaponMuzzleFrame(CGameObject* pWeapon)
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
static void GatherVisionBlockersFromShader(CShader* pShader, std::vector<CGameObject*>& outBlockers)
{
	if (!pShader) return;

	auto* objs = pShader->GetObj();
	if (!objs) return;

	for (auto& obj : *objs)
	{
		if (!obj) continue;
		outBlockers.push_back(obj.get());
	}
}
static void GatherVisionBlockersNearPlayer(CShader* pShader, const XMFLOAT3& playerPos, float maxDistance, std::vector<CGameObject*>& outBlockers)
{
	if (!pShader) return;

	auto* objs = pShader->GetObj();
	if (!objs) return;

	const float maxDistSq = maxDistance * maxDistance;

	for (auto& obj : *objs)
	{
		if (!obj) continue;

		XMFLOAT3 objPos = obj->GetPosition();
		float dx = objPos.x - playerPos.x;
		float dz = objPos.z - playerPos.z;
		float distSq = dx * dx + dz * dz;

		if (distSq <= maxDistSq)
		{
			outBlockers.push_back(obj.get());
		}
	}
}
static bool IntersectsVisionQueryRect2D(BoundingOrientedBox* pOOBB, const XMFLOAT3& playerPos, float halfExtent)
{
	if (!pOOBB) return false;

	const XMFLOAT3& c = pOOBB->Center;

	float radiusXZ = sqrtf(
		pOOBB->Extents.x * pOOBB->Extents.x +
		pOOBB->Extents.z * pOOBB->Extents.z
	);

	if (fabsf(c.x - playerPos.x) > (halfExtent + radiusXZ)) return false;
	if (fabsf(c.z - playerPos.z) > (halfExtent + radiusXZ)) return false;

	return true;
}
static void GatherVisionMapBlockersInRectFromList(const std::vector<CGameObject*>& mapChunks, const XMFLOAT3& playerPos, float halfExtent, std::vector<CGameObject*>& outBlockers)
{
	for (CGameObject* pObj : mapChunks)
	{
		if (!pObj) continue;

		bool intersects = false;

		const auto& oobbs = pObj->GetOOBB();
		for (BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			if (IntersectsVisionQueryRect2D(pOOBB, playerPos, halfExtent))
			{
				intersects = true;
				break;
			}
		}

		if (intersects)
		{
			outBlockers.push_back(pObj);
		}
	}
}

CScene::CScene()
{
	

	m_pPlayer = nullptr;

	m_pd3dGraphicsRootSignature = nullptr;
	m_pd3dcbLights = nullptr;
	m_pcbMappedLights = nullptr;

	m_pSkyBox = nullptr;
	
	UIShader = nullptr;

	m_nLights = 0;
	m_fElapsedTime = 0.0f;
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


void CScene::DumpMapOOBBToCSV(const char* filename)
{
	if (m_ppShaders.size() <= SHADERIDX::MAP) return;
	if (!m_ppShaders[SHADERIDX::MAP]) return;

	auto* objs = m_ppShaders[SHADERIDX::MAP]->GetObj();
	if (!objs) return;

	FILE* fp = nullptr;
	fopen_s(&fp, filename, "w");
	if (!fp)
	{
		OutputDebugStringA("DumpMapOOBBToCSV: failed to open file\n");
		return;
	}

	// 헤더 주석
	fprintf(fp, "# center_x,center_y,center_z,ext_x,ext_y,ext_z,quat_x,quat_y,quat_z,quat_w\n");

	int count = 0;
	for (const auto& obj : *objs)
	{
		if (!obj) continue;

		const auto& oobbs = obj->GetOOBB();
		for (const BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			// 쿼터니언 정규화 (단위 쿼터니언 보장)
			XMVECTOR q = XMLoadFloat4(&pOOBB->Orientation);
			q = XMQuaternionNormalize(q);
			XMFLOAT4 quat;
			XMStoreFloat4(&quat, q);

			fprintf(fp, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
				pOOBB->Center.x, pOOBB->Center.y, pOOBB->Center.z,
				pOOBB->Extents.x, pOOBB->Extents.y, pOOBB->Extents.z,
				quat.x, quat.y, quat.z, quat.w);
			++count;
		}
	}

	fclose(fp);

	char msg[128];
	sprintf_s(msg, "DumpMapOOBBToCSV: dumped %d OOBBs to %s\n", count, filename);
	OutputDebugStringA(msg);
}

bool CScene::ProcessInput(UCHAR *pKeysBuffer)
{
	return(false);
}

MainScene::MainScene() : CScene()
{
	colManager = std::make_unique<CollisionManager>();
	m_pFogOverlayShader = nullptr;
	m_pDebugShader = nullptr;

	m_pLaserMuzzle = nullptr;
	m_pWeaponMuzzle = nullptr;

	m_pEffectManager = nullptr;
	m_pInventoryManager = nullptr;

	m_bLaserActive = false;
	m_bSparkFireActive = false;
	m_fSparkSpawnTimer = 0.0f;
	m_fSparkSpawnInterval = 0.03f;
	m_fLaserLength = 15.0f;
}

MainScene::~MainScene()
{

}

void MainScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	{
		if (IsAnyInventoryOpen())
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->ProcessClick(InputManager::Instance().GetMousePos());
			}

			m_bSparkFireActive = false;
			m_bLaserActive = false;
			m_fSparkSpawnTimer = 0.0f;

			if (m_pEffectManager)
			{
				m_pEffectManager->HideLaser(0);
			}
			return;
		}

		if (!m_pPlayer) return;

		m_bSparkFireActive = true;
		m_bLaserActive = true;
		m_fSparkSpawnTimer = 0.0f;
		m_fSparkSpawnInterval = m_pPlayer->GetWeaponShotInterval();

		if (!m_pWeaponMuzzle && m_pPlayer->GetWeapon())
		{
			m_pWeaponMuzzle = FindWeaponMuzzleFrame(m_pPlayer->GetWeapon());
		}

		if (!m_pPlayer->TryFireWeapon())
		{
			return;
		}

		m_pPlayer->NotifyWeaponFired();

		XMFLOAT3 sparkPos;
		XMFLOAT3 sparkRight;
		XMFLOAT3 sparkUp;

		if (m_pWeaponMuzzle)
		{
			XMFLOAT3 muzzlePos = m_pWeaponMuzzle->GetPosition();
			XMFLOAT3 muzzleLook = m_pWeaponMuzzle->GetLook();
			XMFLOAT3 muzzleRight = m_pWeaponMuzzle->GetRight();

			XMFLOAT3 flatLook = muzzleLook;
			flatLook.y = 0.0f;
			flatLook = Vector3::Normalize(flatLook);

			sparkPos.x = muzzlePos.x + muzzleLook.x * 0.1f;
			sparkPos.y = muzzlePos.y + muzzleLook.y * 0.1f;
			sparkPos.z = muzzlePos.z + muzzleLook.z * 0.1f;

			sparkRight = muzzleRight;
			sparkUp = flatLook;
		}
		else
		{
			XMFLOAT3 pos = m_pPlayer->GetPosition();
			XMFLOAT3 look = m_pPlayer->GetLookVector();
			XMFLOAT3 right = m_pPlayer->GetRightVector();
			XMFLOAT3 up = m_pPlayer->GetUpVector();

			XMFLOAT3 flatLook = look;
			flatLook.y = 0.0f;
			flatLook = Vector3::Normalize(flatLook);

			sparkPos.x = pos.x + look.x * 0.8f + right.x * 0.25f + up.x * 1.2f;
			sparkPos.y = pos.y + look.y * 0.8f + right.y * 0.25f + up.y * 1.2f;
			sparkPos.z = pos.z + look.z * 0.8f + right.z * 0.25f + up.z * 1.2f;

			sparkRight = right;
			sparkUp = flatLook;
		}

		if (m_pEffectManager)
		{
			m_pEffectManager->RequestPlayEffect(EFFECT_SPARK, sparkPos, sparkRight, sparkUp);
		}

		if (m_ppShaders[SHADERIDX::ENEMY] && !m_ppShaders[SHADERIDX::ENEMY]->GetObj()->empty())
		{
			if (NetworkManager::Instance().IsConnected()) {
				XMFLOAT3 rayOrigin = m_pPlayer->GetPosition();
				XMFLOAT3 lookVec = m_pPlayer->GetLookVector();

				XMVECTOR dirVec = XMVector3Normalize(XMLoadFloat3(&lookVec));
				XMFLOAT3 rayDirection;
				XMStoreFloat3(&rayDirection, dirVec);

				NetworkManager::Instance().SendHitNpc(rayOrigin, rayDirection, 0);
			}
			else {
				XMFLOAT3 playerPos = m_pPlayer->GetPosition();
				XMVECTOR rayOrigin = XMLoadFloat3(&playerPos);
				XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetLookVector()));

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
								pEnemy->HandleHP(m_pPlayer->GetWeaponDamage());
							}
							break;
						}
					}
				}
			}

		}
		break;
	}

	case WM_LBUTTONUP:
		m_bSparkFireActive = false;
		m_bLaserActive = false;
		m_fSparkSpawnTimer = 0.0f;

		if (m_pEffectManager)
		{
			m_pEffectManager->HideLaser(0);
		}
		break;

	default:
		break;
	}
}

bool MainScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (!m_pPlayer) return false;

	auto SendPlayerKeyEvent = [this](WPARAM keyCode, KEY_STATE state) -> bool
		{
			INPUT_KEY key;
			bool validKey = true;

			switch (keyCode)
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
			case 'R': key = INPUT_KEY::R; break;

			case '1': key = INPUT_KEY::KEY_1; break;
			case '2': key = INPUT_KEY::KEY_2; break;
			case '3': key = INPUT_KEY::KEY_3; break;
			case '4': key = INPUT_KEY::KEY_4; break;

			case VK_SHIFT:   key = INPUT_KEY::SHIFT; break;
			case VK_SPACE:   key = INPUT_KEY::SPACE; break;

			case VK_LBUTTON: key = INPUT_KEY::LBUTTON; break;
			case VK_RBUTTON: key = INPUT_KEY::RBUTTON; break;

			default:
				validKey = false;
				break;
			}

			if (!validKey) return false;

			GameEvent e;
			e.type = EventType::Input;
			e.keyEvent = { key, state };

			m_pPlayer->AddEvent(e);
			return true;
		};

	switch (nMessageID)
	{
	case WM_KEYDOWN:
	{
		bool wasDownBefore = (lParam & (1 << 30)) != 0;

		switch (wParam)
		{
		case 'I':
		{
			if (wasDownBefore) return true;

			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleIKeyToggle(m_fLootInteractDistance);
			}
			return true;
		}

		case VK_TAB:
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleTabPressed(m_fLootInteractDistance);
			}
			return true;
		}

		default:
			break;
		case VK_F11:		// 서버 충돌처리용 OOBB CSV 파일 생성/업데이트
			DumpMapOOBBToCSV("map_oobb.csv");
			break;
		}

		if (IsAnyInventoryOpen())
			return true;

		if (wasDownBefore)
			return true;

		return SendPlayerKeyEvent(wParam, KEY_STATE::DOWN);
	}

	case WM_KEYUP:
	{
		if (wParam == VK_TAB)
		{
			if (m_pInventoryManager)
			{
				m_pInventoryManager->HandleTabReleased();
			}
			return true;
		}

		if (IsAnyInventoryOpen())
			return true;

		return SendPlayerKeyEvent(wParam, KEY_STATE::UP);
	}
	}

	return false;
}

void MainScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	CMaterial::PrepareShaders(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	BuildDefaultLightsAndMaterials();

	m_pSkyBox = std::make_unique<CSkyBox>(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	UIShader = make_unique<UIObjectShader>();
	UIShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_pFogOverlayShader = std::make_unique<CFogOverlayShader>();
	m_pFogOverlayShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_pInventoryManager = new InventoryManager();
	m_pInventoryManager->Initialize(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		UIShader.get()
	);

	// 맵 쉐이더
	std::unique_ptr<CStandardObjectsShader> stdshader = std::make_unique<CStandardObjectsShader>();
	stdshader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	stdshader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	stdshader->CreateShadowShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	m_vVisionMapChunks.clear();
	m_vVisionMapChunks.reserve(64);

	{
		std::unique_ptr<CGameObject> floorObj(
			CGameObject::LoadGeometryModelByName(
				pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature,
				NULL, "Model/floor.bin", stdshader.get(), 0
			)
		);
		floorObj->SetPosition(-150, -0.5f, -150);
		floorObj->SetOOBB(NULL);
		stdshader->addObjects(std::move(floorObj));

		static const char* s_mapFiles[] = {
			"Model/block1.bin",
			"Model/block3.bin",
			"Model/block4.bin",
			"Model/block5.bin",
			"Model/block6.bin",
			"Model/block10.bin",
			"Model/block11.bin",
			"Model/block12.bin",
			"Model/block13.bin",
			"Model/block14.bin",
			"Model/block15.bin",
			"Model/block16.bin",
			"Model/block17.bin",
			"Model/block18.bin",
			"Model/block19.bin",
			"Model/block20.bin",
			"Model/block21.bin",
			"Model/block22.bin",
			"Model/block23.bin",
			"Model/block24.bin",
			"Model/block30.bin",
			"Model/block31.bin",
			"Model/block32.bin",
			"Model/block33.bin",
			"Model/block34.bin",
			"Model/block35.bin",
			"Model/block36.bin",
			"Model/block37.bin",
			"Model/block38.bin",
			"Model/block40.bin"
		};

		for (const char* fileName : s_mapFiles)
		{
			std::unique_ptr<CGameObject> map(
				CGameObject::LoadGeometryModelByName(
					pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature,
					NULL, fileName, stdshader.get(), 0
				)
			);
			map->SetPosition(-150, 0.0f, -150);
			map->SetOOBB(NULL);

			m_vVisionMapChunks.push_back(map.get());

			stdshader->addObjects(std::move(map));
		}
	}
	m_ppShaders.push_back(std::move(stdshader));

	// 시야 객체 생성
	std::unique_ptr<ViewShader> view = make_unique<ViewShader>();
	view->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	view->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	view->CreateThroughShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	{
		std::unique_ptr<ViewObject> viewobj = make_unique<ViewObject>();
		viewobj->SetPosition(0.0f, 0.03f, 0.0f);

		auto pCircleObj = std::make_unique<CGameObject>();
		strcpy_s(pCircleObj->m_pstrFrameName, 64, "ViewCircle");
		pCircleObj->SetMesh(new CViewCircleMesh(pd3dDevice, pd3dCommandList, 3.0f, 72));
		pCircleObj->SetShader(view.get());
		pCircleObj->SetPosition(0.0f, 0.7f, 0.0f);

		auto pConeObj = std::make_unique<CGameObject>();
		strcpy_s(pConeObj->m_pstrFrameName, 64, "ViewCone");
		pConeObj->SetMesh(new CViewConeMesh(pd3dDevice, pd3dCommandList, 20.0f, 75.0f, 72));
		pConeObj->SetShader(view.get());
		pConeObj->SetPosition(0.0f, 0.7f, 0.0f);

		viewobj->SetCircleObject(pCircleObj.get());
		viewobj->SetConeObject(pConeObj.get());

		viewobj->SetChild(pCircleObj.get());
		viewobj->SetChild(pConeObj.get());

		pCircleObj.release();
		pConeObj.release();

		view->addObjects(std::move(viewobj));
	}
	m_ppShaders.push_back(std::move(view));

	// 디버그 쉐이더
	m_pDebugShader = std::make_unique<CBoundingBoxShader>(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		false
	);

	// 시체박스 전용 솔리드 큐브
	m_pLootBoxShader = std::make_unique<CBoundingBoxShader>(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		true
	);

	// 적 쉐이더
	auto pSkinnedShader = std::make_unique<CSkinnedAnimationObjectsShader>();

	pSkinnedShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	pSkinnedShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pSkinnedShader->CreateShadowShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	AStarNav = make_unique<AstarNavigation>();
	AStarNav->LoadNavMeshFromFile("Model/NavMeshData.bin");


	//적 오브젝트 - 네트워크가 안 될 때에만
	//CEnemyObject* pEnemy = new CEnemyObject(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature, NULL);
	//pEnemy->SetPosition(0.0f, 0.0f, 0.0f);
	//pEnemy->SetScale(1.0f, 1.0f, 1.0f);
	//pEnemy->SetOOBB(NULL);
	//pEnemy->setNav(AStarNav.get());
	//pSkinnedShader->addObjects(std::unique_ptr<CGameObject>(pEnemy));

	m_ppShaders.push_back(std::move(pSkinnedShader));

	// 루팅 전용 쉐이더
	auto pLootShader = std::make_unique<CStandardObjectsShader>();
	pLootShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	pLootShader->CreateShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);
	pLootShader->CreateShadowShader(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	CStandardObjectsShader* pLootShaderRaw = pLootShader.get();

	m_ppShaders.push_back(std::move(pLootShader));

	if (m_pInventoryManager)
	{
		m_pInventoryManager->BindLootWorld(
			nullptr,
			pLootShaderRaw,
			m_pDebugShader.get(),
			m_pLootBoxShader.get()
		);
	}

	// EffectManager 생성
	m_pEffectManager = new EffectManager();
	m_pEffectManager->Initialize(pd3dDevice, pd3dCommandList, m_pd3dGraphicsRootSignature);

	for (const auto& shader : m_ppShaders)
	{
		auto* objs = shader->GetObj();
		if (objs != nullptr && !objs->empty())
		{
			for (auto& obj : *objs)
			{
				m_pDebugShader->AddObject(obj.get());
			}
		}
	}

	ShadowCameraManager.CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void MainScene::ReleaseObjects()
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->Release();
		delete m_pInventoryManager;
		m_pInventoryManager = nullptr;
	}

	if (m_pEffectManager)
	{
		m_pEffectManager->Release();
		delete m_pEffectManager;
		m_pEffectManager = nullptr;
	}

	for (auto& s : m_ppShaders)
	{
		s->ReleaseObjects();
	}
	m_ppShaders.clear();

	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature = nullptr;

	ReleaseModelPrototypes();
	ReleaseShaderVariables();

	m_pLights.clear();
}

bool MainScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return false;
}

void MainScene::AnimateObjects(float fTimeElapsed)
{
	m_fElapsedTime = fTimeElapsed;

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i]) m_ppShaders[i]->AnimateObjects(fTimeElapsed);
	}

	if (m_pInventoryManager)
	{
		//m_pInventoryManager->UpdateLootWorld(fTimeElapsed);	// 루트박스 만료 서버로 옮겼음

		if (m_ppShaders.size() > SHADERIDX::ENEMY && m_ppShaders[SHADERIDX::ENEMY])
		{
			//m_pInventoryManager->ProcessEnemyLootSpawnRequests(m_ppShaders[SHADERIDX::ENEMY].get());	// 루트박스를 서버 권위로 바꾸면서 자체스폰 삭제
		}

		m_pInventoryManager->Update(fTimeElapsed);
	}

	if (m_pPlayer && m_pLights.size() > 1)
	{
		m_pLights[1].m_xmf3Position = m_pPlayer->GetPosition();
		m_pLights[1].m_xmf3Direction = m_pPlayer->GetLookVector();
	}
	// 시야 객체 blocker 갱신
	{
		if (m_pPlayer && m_ppShaders.size() > SHADERIDX::VIEW && m_ppShaders[SHADERIDX::VIEW])
		{
			std::vector<CGameObject*> visionBlockers;
			visionBlockers.reserve(32);

			XMFLOAT3 playerPos = m_pPlayer->GetPosition();

			if (!m_vVisionMapChunks.empty())
			{
				GatherVisionMapBlockersInRectFromList(m_vVisionMapChunks, playerPos, 18.0f, visionBlockers);
			}
			else if (m_ppShaders.size() > SHADERIDX::MAP && m_ppShaders[SHADERIDX::MAP])
			{
				GatherVisionBlockersFromShader(m_ppShaders[SHADERIDX::MAP].get(), visionBlockers);
			}

			auto* viewObjs = m_ppShaders[SHADERIDX::VIEW]->GetObj();
			if (viewObjs && !viewObjs->empty())
			{
				ViewObject* pViewObj = dynamic_cast<ViewObject*>(viewObjs->at(0).get());
				if (pViewObj)
				{
					pViewObj->UpdateClippedMeshes(visionBlockers);
				}
			}
		}
	}

	// 연사 처리
	if (m_bSparkFireActive && m_pPlayer && !IsAnyInventoryOpen())
	{
		if (!m_pWeaponMuzzle && m_pPlayer->GetWeapon())
		{
			m_pWeaponMuzzle = FindWeaponMuzzleFrame(m_pPlayer->GetWeapon());
		}

		m_fSparkSpawnInterval = m_pPlayer->GetWeaponShotInterval();
		m_fSparkSpawnTimer += fTimeElapsed;

		while (m_fSparkSpawnTimer >= m_fSparkSpawnInterval)
		{
			if (!m_pPlayer->TryFireWeapon())
			{
				m_fSparkSpawnTimer = 0.0f;
				break;
			}

			m_pPlayer->NotifyWeaponFired();

			XMFLOAT3 sparkPos;
			XMFLOAT3 sparkRight;
			XMFLOAT3 sparkUp;

			if (m_pWeaponMuzzle)
			{
				XMFLOAT3 muzzlePos = m_pWeaponMuzzle->GetPosition();
				XMFLOAT3 muzzleLook = m_pWeaponMuzzle->GetLook();
				XMFLOAT3 muzzleRight = m_pWeaponMuzzle->GetRight();

				XMFLOAT3 flatLook = muzzleLook;
				flatLook.y = 0.0f;
				flatLook = Vector3::Normalize(flatLook);

				sparkPos.x = muzzlePos.x + muzzleLook.x * 0.1f;
				sparkPos.y = muzzlePos.y + muzzleLook.y * 0.1f;
				sparkPos.z = muzzlePos.z + muzzleLook.z * 0.1f;

				sparkRight = muzzleRight;
				sparkUp = flatLook;
			}
			else
			{
				XMFLOAT3 pos = m_pPlayer->GetPosition();
				XMFLOAT3 look = m_pPlayer->GetLookVector();
				XMFLOAT3 right = m_pPlayer->GetRightVector();
				XMFLOAT3 up = m_pPlayer->GetUpVector();

				XMFLOAT3 flatLook = look;
				flatLook.y = 0.0f;
				flatLook = Vector3::Normalize(flatLook);

				sparkPos.x = pos.x + look.x * 0.8f + right.x * 0.25f + up.x * 1.2f;
				sparkPos.y = pos.y + look.y * 0.8f + right.y * 0.25f + up.y * 1.2f;
				sparkPos.z = pos.z + look.z * 0.8f + right.z * 0.25f + up.z * 1.2f;

				sparkRight = right;
				sparkUp = flatLook;
			}

			if (m_pEffectManager)
			{
				m_pEffectManager->RequestPlayEffect(EFFECT_SPARK, sparkPos, sparkRight, sparkUp);
			}

			if (m_ppShaders[SHADERIDX::ENEMY] && !m_ppShaders[SHADERIDX::ENEMY]->GetObj()->empty())
			{
				if (NetworkManager::Instance().IsConnected()) {
					XMFLOAT3 rayOrigin = m_pPlayer->GetPosition();
					XMFLOAT3 lookVec = m_pPlayer->GetLookVector();

					XMVECTOR dirVec = XMVector3Normalize(XMLoadFloat3(&lookVec));
					XMFLOAT3 rayDirection;
					XMStoreFloat3(&rayDirection, dirVec);

					NetworkManager::Instance().SendHitNpc(rayOrigin, rayDirection, 0);
				}
				else {
					XMFLOAT3 playerPos = m_pPlayer->GetPosition();
					XMVECTOR rayOrigin = XMLoadFloat3(&playerPos);
					XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->GetLookVector()));

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
									pEnemy->HandleHP(m_pPlayer->GetWeaponDamage());
								}
								break;
							}
						}
					}
				}

			}

			m_fSparkSpawnTimer -= m_fSparkSpawnInterval;
		}
	}
	else
	{
		m_fSparkSpawnTimer = 0.0f;
	}

	if (m_pEffectManager)
	{
		m_pEffectManager->Update(fTimeElapsed);
	}

	// 레이저 transform 갱신
	if (m_bLaserActive && m_pPlayer && !IsAnyInventoryOpen() && m_pEffectManager)
	{
		if (!m_pLaserMuzzle && m_pPlayer->GetWeapon())
		{
			m_pLaserMuzzle = FindLaserMuzzleFrame(m_pPlayer->GetWeapon());
		}

		XMFLOAT3 origin;
		XMFLOAT3 right;
		XMFLOAT3 up;
		XMFLOAT3 dir;

		if (m_pLaserMuzzle)
		{
			origin = m_pLaserMuzzle->GetPosition();
			dir = Vector3::Normalize(m_pLaserMuzzle->GetLook());
			right = Vector3::Normalize(m_pLaserMuzzle->GetRight());
			up = Vector3::Normalize(m_pLaserMuzzle->GetUp());
		}
		else
		{
			XMFLOAT3 pos = m_pPlayer->GetPosition();
			XMFLOAT3 look = Vector3::Normalize(m_pPlayer->GetLookVector());
			XMFLOAT3 rightVec = Vector3::Normalize(m_pPlayer->GetRightVector());
			XMFLOAT3 upVec = Vector3::Normalize(m_pPlayer->GetUpVector());

			origin.x = pos.x + upVec.x * 1.2f + rightVec.x * 0.3f + look.x * 0.5f;
			origin.y = pos.y + upVec.y * 1.2f + rightVec.y * 0.3f + look.y * 0.5f;
			origin.z = pos.z + upVec.z * 1.2f + rightVec.z * 0.3f + look.z * 0.5f;

			dir = look;
			right = rightVec;
			up = upVec;
		}

		m_pEffectManager->UpdateLaser(0, origin, right, up, dir, m_fLaserLength);
	}
	else if (m_pEffectManager)
	{
		m_pEffectManager->HideLaser(0);
	}

	ShadowCameraManager.Update();

	if (m_pInventoryManager)
	{
		m_pInventoryManager->SubmitToShader(UIShader.get());
	}

	colManager->DoCollision(m_pPlayer, m_ppShaders[SHADERIDX::MAP]->GetObj());	// 서버 충돌처리 확인을 위한 주석처리

}

void MainScene::Render(ID3D12GraphicsCommandList * pd3dCommandList, int nPipelineState, CCamera * pCamera)
{
	if (m_pd3dGraphicsRootSignature) pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
	ID3D12DescriptorHeap* heap = ResourceManager::Instance().GetDescriptorHeap();
	pd3dCommandList->SetDescriptorHeaps(1, &heap);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(2, d3dcbLightsGpuVirtualAddress);

	pd3dCommandList->OMSetStencilRef(0xff);

	if (nPipelineState == SHADOW)
	{
		for (int i = 0; i < m_ppShaders.size(); i++)
		{
			if (m_ppShaders[i] && m_ppShaders[i]->DoShadow())
				m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
		}
		return;
	}

	// 1. 월드
	if (m_pSkyBox) m_pSkyBox->Render(pd3dCommandList, false, nPipelineState, pCamera);

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i])
			m_ppShaders[i]->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}

	// 2. 이펙트 매니저 렌더
	if (m_pEffectManager)
	{
		m_pEffectManager->Render(pd3dCommandList, pCamera, nPipelineState);
	}

	// 3. 시야 바깥
	if (m_pFogOverlayShader)
	{
		pd3dCommandList->OMSetStencilRef(0x00);
		m_pFogOverlayShader->Render(pd3dCommandList, pCamera, true, nPipelineState);

		pd3dCommandList->OMSetStencilRef(0xff);
	}

	// 4. UI
	if (UIShader)
	{
		UIShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}

#ifdef _DEBUG
	if (m_pDebugShader)
	{
		m_pDebugShader->Render(pd3dCommandList, pCamera, nPipelineState);
	}
#endif

	if (m_pLootBoxShader)
	{
		m_pLootBoxShader->Render(pd3dCommandList, pCamera, nPipelineState);
	}
}

void MainScene::ThroughRender(ID3D12GraphicsCommandList * pd3dCommandList, CCamera * pCamera)
{
	if (!m_pPlayer || !pCamera) return;
	m_pPlayer->Render(pd3dCommandList, THROUGH, pCamera);
}

void MainScene::ReleaseUploadBuffers()
{
	if (m_pSkyBox) m_pSkyBox->ReleaseUploadBuffers();

	for (int i = 0; i < m_ppShaders.size(); i++)
	{
		if (m_ppShaders[i])
			m_ppShaders[i]->ReleaseUploadBuffers();
	}

	if (m_pEffectManager)
	{
		m_pEffectManager->ReleaseUploadBuffers();
	}
}

void MainScene::SetPlayer(CPlayer* p)
{
	m_pPlayer = p;

	std::vector<std::unique_ptr<CGameObject>>* pVector = m_ppShaders[SHADERIDX::VIEW]->GetObj();
	for (auto& obj : *pVector)
	{
		ViewObject* pViewObj = static_cast<ViewObject*>(obj.get());
		pViewObj->setPlayer(p);
	}

	if (m_pPlayer) m_pDebugShader->AddObject(m_pPlayer);

	ShadowCameraManager.SetPlayer(p->GetCamera());
	ShadowCameraManager.SetDir(m_pLights[0].m_xmf3Direction);

	if (m_ppShaders[SHADERIDX::ENEMY])
	{
		auto* objs = m_ppShaders[SHADERIDX::ENEMY]->GetObj();
		for (auto& obj : *objs)
		{
			CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj.get());
			if (pEnemy)
			{
				pEnemy->SetPlayer(m_pPlayer);
			}
		}
	}

	m_pWeaponMuzzle = nullptr;
	if (m_pPlayer && m_pPlayer->GetWeapon())
	{
		m_pWeaponMuzzle = FindWeaponMuzzleFrame(m_pPlayer->GetWeapon());
	}

	m_pLaserMuzzle = nullptr;
	if (m_pPlayer && m_pPlayer->GetWeapon())
	{
		m_pLaserMuzzle = FindLaserMuzzleFrame(m_pPlayer->GetWeapon());
	}

	if (m_pInventoryManager)
	{
		m_pInventoryManager->SetPlayer(m_pPlayer);
	}
}


bool MainScene::IsAnyInventoryOpen() const
{
	return (m_pInventoryManager) ? m_pInventoryManager->IsAnyInventoryOpen() : false;
}

void MainScene::CloseCorpseInventory()
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->CloseLootInventory();
	}
}

void MainScene::OpenLootContainer(CLootContainerObject* pLoot)
{
	if (m_pInventoryManager)
	{
		m_pInventoryManager->OpenLootContainer(pLoot);
	}
}
bool MainScene::LoadAndRegisterModelPrototype(ModelName key, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const char* modelPath, CShader* pShader)
{
	if (!pd3dDevice) return false;
	if (!pd3dCommandList) return false;
	if (!modelPath) return false;
	if (!pShader) return false;
	if (!m_pd3dGraphicsRootSignature) return false;

	auto it = m_ModelPrototypes.find(key);
	if (it != m_ModelPrototypes.end())
	{
		return true;
	}

	CGameObject* pPrototype = CGameObject::LoadGeometryModelByName(
		pd3dDevice,
		pd3dCommandList,
		m_pd3dGraphicsRootSignature,
		nullptr,
		modelPath,
		pShader,
		nullptr
	);

	if (!pPrototype)
		return false;

	RegisterModelPrototype(key, pPrototype);
	return true;
}

void MainScene::RegisterModelPrototype(ModelName key, CGameObject* pPrototype)
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

CGameObject* MainScene::GetModelPrototype(ModelName key) const
{
	auto it = m_ModelPrototypes.find(key);
	if (it == m_ModelPrototypes.end())
		return nullptr;

	return it->second;
}

void MainScene::ReleaseModelPrototypes()
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

void MainScene::BuildModelPrototypes(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, CShader* pPlayerShader)
{
	if (!pPlayerShader) return;

	LoadAndRegisterModelPrototype(
		ModelName::RIFLE,
		pd3dDevice,
		pd3dCommandList,
		"Model/Classic_M4_1.bin",
		pPlayerShader
	);
}

void LobbyScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	{

		break;
	}
	default:
		break;
	}
}

bool LobbyScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return false;
}

void LobbyScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	
}

void LobbyScene::ReleaseObjects()
{
	
}

void LobbyScene::Render(ID3D12GraphicsCommandList * pd3dCommandList, int nPipelineState, CCamera * pCamera)
{
	
}
