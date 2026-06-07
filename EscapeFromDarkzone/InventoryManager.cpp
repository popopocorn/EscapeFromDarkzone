#include "stdafx.h"
#include "InventoryManager.h"
#include "Shader.h"
#include "EnemyObject.h"
#include "Player.h"
#include "Object.h"
#include "ResourceManager.h"


InventoryManager::~InventoryManager()
{
	Release();
}
//인벤토리 3개(플레이어, 루팅, 제작) 초기화
void InventoryManager::Initialize(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pUIShader)
{
	Release();

	m_pLootInventory = std::make_unique<Inventory>(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		pUIShader
	);

	m_pCraftInventory = std::make_unique<Inventory>(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		pUIShader
	);

	m_pLootInventory->SetPosition(-0.7f, 0.0f);
	m_pCraftInventory->SetPosition(0.0f, 0.0f);

	m_pLootInventory->isOpen = false;
	m_pCraftInventory->isOpen = false;

	m_pOpenedLoot = nullptr;
	m_bTabInventoryHold = false;

	m_pPlayer = nullptr;
	m_pLootShader = nullptr;
	m_pLootBoxShader = nullptr;
	m_pDebugShader = nullptr;
}

void InventoryManager::Release()
{
	m_pOpenedLoot = nullptr;
	m_bTabInventoryHold = false;

	m_vLootContainers.clear();

	m_pPlayer = nullptr;
	m_pLootShader = nullptr;
	m_pLootBoxShader = nullptr;
	m_pDebugShader = nullptr;

	m_pCraftInventory.reset();
	m_pLootInventory.reset();
}

void InventoryManager::BindLootWorld(
	CPlayer* pPlayer,
	CStandardObjectsShader* pLootShader,
	CBoundingBoxShader* pDebugShader,
	CBoundingBoxShader* pLootBoxShader)
{
	m_pPlayer = pPlayer;
	m_pLootShader = pLootShader;
	m_pDebugShader = pDebugShader;
	m_pLootBoxShader = pLootBoxShader;
}

//열린 루팅창 갱신
void InventoryManager::Update(float fTimeElapsed)
{
	UpdateLootWorld(fTimeElapsed);

	if (m_pOpenedLoot && !m_pOpenedLoot->IsAlive())
	{
		CloseLootInventory();
	}

	std::erase_if(m_vLootContainers, [](std::unique_ptr<CLootContainerObject>& loot) {
		return (!loot || !loot->IsAlive());
		});
}
void InventoryManager::UpdateLootWorld(float fTimeElapsed)
{
	for (auto& loot : m_vLootContainers)
	{
		if (loot)
		{
			loot->UpdateLifetime(fTimeElapsed);
		}
	}
}

void InventoryManager::RenderLootWorld(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState)
{
	if (!m_pLootShader) return;

	pd3dCommandList->OMSetStencilRef(0xff);
	m_pLootShader->OnPrepareRender(pd3dCommandList, 0);

	for (auto& loot : m_vLootContainers)
	{
		if (!loot) continue;
		if (!loot->IsAlive()) continue;

		loot->UpdateTransform(NULL);
		loot->Render(pd3dCommandList, true, 0, pCamera);
	}
}
/*void InventoryManager::ProcessEnemyLootSpawnRequests(CShader* pEnemyShader)
{
	if (!pEnemyShader) return;

	auto* enemyObjs = pEnemyShader->GetObj();
	if (!enemyObjs) return;

	for (auto& obj : *enemyObjs)
	{
		CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj.get());
		if (!pEnemy) continue;

		if (pEnemy->ConsumeLootSpawnRequest())
		{
			SpawnLootContainerFromEnemy(pEnemy);
			pEnemy->MarkDeadForRemoval();
		}
	}
}*/

Inventory* InventoryManager::GetPlayerInventoryPtr() const
{
	return (m_pPlayer) ? m_pPlayer->GetInventory() : nullptr;
}

//패킷으로 플레이어 인벤토리 슬롯 업데이트
bool InventoryManager::ApplyPlayerInventorySlotUpdate(ItemID itemId, int count, int slotIndex)
{
	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (!pPlayerInventory) return false;

	return pPlayerInventory->ApplyServerSlotUpdate(slotIndex, itemId, count);
}

void InventoryManager::SubmitToShader(UIObjectShader* shader)
{
	if (!shader) return;

	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (pPlayerInventory)
		pPlayerInventory->SubmitToShader(shader);

	if (m_pLootInventory)
		m_pLootInventory->SubmitToShader(shader);

	if (m_pCraftInventory)
		m_pCraftInventory->SubmitToShader(shader);
}

bool InventoryManager::ProcessClick(POINT mouse)
{
	if (m_pLootInventory && m_pLootInventory->isOpen)
	{
		if (m_pLootInventory->ProcessClick(mouse))
			return true;
	}

	if (m_pCraftInventory && m_pCraftInventory->isOpen)
	{
		if (m_pCraftInventory->ProcessClick(mouse))
			return true;
	}

	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (pPlayerInventory && pPlayerInventory->isOpen)
	{
		if (pPlayerInventory->ProcessClick(mouse))
			return true;
	}

	return false;
}

void InventoryManager::OpenPlayerInventory()
{
	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (pPlayerInventory)
		pPlayerInventory->isOpen = true;
}

void InventoryManager::ClosePlayerInventory()
{
	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (pPlayerInventory)
		pPlayerInventory->isOpen = false;
}

void InventoryManager::TogglePlayerInventory()
{
	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	if (!pPlayerInventory) return;

	pPlayerInventory->isOpen = !pPlayerInventory->isOpen;
}

void InventoryManager::OpenLootContainer(CLootContainerObject* pLoot)
{
	if (!pLoot || !m_pLootInventory) return;

	m_pLootInventory->ClearItems();
	pLoot->FillInventoryUI(m_pLootInventory.get());
	m_pLootInventory->isOpen = true;
	m_pLootInventory->SetId(pLoot->GetBoxId());
	m_pOpenedLoot = pLoot;
}

void InventoryManager::CloseLootInventory()
{
	if (m_pLootInventory)
	{
		m_pLootInventory->isOpen = false;
		m_pLootInventory->ClearItems();
	}
	m_pOpenedLoot = nullptr;
}

void InventoryManager::OpenCraftInventory()
{
	if (m_pCraftInventory)
		m_pCraftInventory->isOpen = true;
}

void InventoryManager::CloseCraftInventory()
{
	if (m_pCraftInventory)
	{
		m_pCraftInventory->isOpen = false;
		m_pCraftInventory->ClearItems();
	}
}

void InventoryManager::ToggleCraftInventory()
{
	if (!m_pCraftInventory) return;
	m_pCraftInventory->isOpen = !m_pCraftInventory->isOpen;
}

CLootContainerObject* InventoryManager::FindNearestLootContainer(float fMaxDistance) const
{
	if (!m_pPlayer) return nullptr;

	const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	const float maxDistSq = fMaxDistance * fMaxDistance;

	CLootContainerObject* pNearest = nullptr;
	float nearestDistSq = maxDistSq;

	for (const auto& loot : m_vLootContainers)
	{
		if (!loot) continue;
		if (!loot->IsAlive()) continue;

		float distSq = loot->GetDistanceSq(playerPos);
		if (distSq <= nearestDistSq)
		{
			nearestDistSq = distSq;
			pNearest = loot.get();
		}
	}

	return pNearest;
}

void InventoryManager::SpawnLootContainerFromEnemy(CEnemyObject* pEnemy)
{
	if (!pEnemy) return;

	static short s_LocalLootBoxId = -1000;

	XMFLOAT3 pos = pEnemy->GetPosition();

	SpawnLootContainer(s_LocalLootBoxId--, pos, nullptr, nullptr, 0);
}

void InventoryManager::SpawnLootContainer(short npc_id, const XMFLOAT3& pos, const ItemID* items, const int* counts, int slotCount)
{
	if (!m_pLootShader) return;

	CLootContainerObject* pLoot = new CLootContainerObject(30.0f);

	XMFLOAT3 spawnPos = pos;
	spawnPos.y += 0.05f;

	pLoot->SetBoxId(npc_id);
	pLoot->SetPosition(spawnPos);

	CGameObject* pLootPrototype = ResourceManager::Instance().GetModelPrototype(ModelName::LOOT_BOX);

	if (pLootPrototype)
	{
		CGameObject* pLootBoxModel = CGameObject::CreateModelInstance(pLootPrototype);

		if (pLootBoxModel)
		{
			pLootBoxModel->SetScale(3.0f, 3.0f, 3.0f);
			pLootBoxModel->SetPosition(0.0f, 0.55f, 0.0f);
			pLoot->SetVisualModel(pLootBoxModel);
		}
	}
	
	if (pLoot->GetOOBB().empty())
	{
		BoundingOrientedBox obb;
		obb.Center = XMFLOAT3(0.0f, 0.6f, 0.0f);
		obb.Extents = XMFLOAT3(0.35f, 0.6f, 0.35f);
		obb.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		pLoot->SetOOBB(obb);
	}

	if (items && counts && slotCount > 0)
	{
		for (int i = 0; i < slotCount && i < MAX_LOOT_SLOTS; ++i)
		{
			if (items[i] != ItemID::NONE && counts[i] > 0)
			{
				pLoot->SetSlotData(i, items[i], counts[i]);
			}
		}
	}

	pLoot->UpdateTransform(NULL);
	m_vLootContainers.push_back(std::unique_ptr<CLootContainerObject>(pLoot));
}

void InventoryManager::HandleIKeyToggle(float fLootInteractDistance)
{
	if (IsAnyInventoryOpen())
	{
		CloseAll();
		return;
	}

	OpenPlayerInventory();

	CLootContainerObject* pNearestLoot = FindNearestLootContainer(fLootInteractDistance);
	if (pNearestLoot)
	{
		OpenLootContainer(pNearestLoot);
	}
	else
	{
		CloseLootInventory();
	}
}

void InventoryManager::HandleTabPressed(float fLootInteractDistance)
{
	if (m_bTabInventoryHold)
		return;

	OpenPlayerInventory();

	CLootContainerObject* pNearestLoot = FindNearestLootContainer(fLootInteractDistance);
	if (pNearestLoot)
	{
		OpenLootContainer(pNearestLoot);
	}
	else
	{
		CloseLootInventory();
	}

	m_bTabInventoryHold = true;
}

void InventoryManager::HandleTabReleased()
{
	if (!m_bTabInventoryHold)
		return;

	ClosePlayerInventory();
	CloseLootInventory();
	m_bTabInventoryHold = false;
}

void InventoryManager::CloseAll()
{
	ClosePlayerInventory();
	CloseLootInventory();
	CloseCraftInventory();
	m_bTabInventoryHold = false;
}

bool InventoryManager::IsAnyInventoryOpen() const
{
	return IsPlayerInventoryOpen() || IsLootInventoryOpen() || IsCraftInventoryOpen();
}

bool InventoryManager::IsPlayerInventoryOpen() const
{
	Inventory* pPlayerInventory = GetPlayerInventoryPtr();
	return (pPlayerInventory && pPlayerInventory->isOpen);
}

bool InventoryManager::IsLootInventoryOpen() const
{
	return (m_pLootInventory && m_pLootInventory->isOpen);
}

bool InventoryManager::IsCraftInventoryOpen() const
{
	return (m_pCraftInventory && m_pCraftInventory->isOpen);
}

CLootContainerObject* InventoryManager::FindLootBoxById(short box_id)
{
	for (const auto& loot : m_vLootContainers)
	{
		if (!loot) continue;
		if (loot->GetBoxId() == box_id) return loot.get();
	}

	return nullptr;
}

void InventoryManager::ApplyLootBoxSlotUpdate(short box_id, int slotidx,
	ItemID item, int count)
{
	CLootContainerObject* pLoot = FindLootBoxById(box_id);
	if (!pLoot) return;

	pLoot->SetSlotData(slotidx, item, count);

	if (m_pOpenedLoot == pLoot && m_pLootInventory) {
		m_pLootInventory->ApplyServerSlotUpdate(slotidx, item, count);
	}
}

void InventoryManager::DeactivateLootBox(short box_id)
{
	CLootContainerObject* pLoot = FindLootBoxById(box_id);
	if (!pLoot) return;

	if (m_pOpenedLoot == pLoot)
	{
		m_pOpenedLoot = nullptr;

		if (m_pLootInventory)
		{
			m_pLootInventory->ClearItems();
			m_pLootInventory->isOpen = false;
			m_pLootInventory->SetId(-1);
		}
	}

	pLoot->Kill();
}