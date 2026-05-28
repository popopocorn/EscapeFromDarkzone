#include "stdafx.h"
#include "InventoryManager.h"
#include "Shader.h"
#include "EnemyObject.h"
#include "Player.h"
#include "Object.h"

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
	UNREFERENCED_PARAMETER(fTimeElapsed);

	if (m_pOpenedLoot && !m_pOpenedLoot->IsAlive())
	{
		CloseLootInventory();
	}
}

void InventoryManager::UpdateLootWorld(float fTimeElapsed)		// 서버 권위 구조로 바꾸면서 안씀
{
	if (!m_pLootShader) return;

	auto* lootObjs = m_pLootShader->GetObj();
	if (!lootObjs) return;

	for (auto& obj : *lootObjs)
	{
		CLootContainerObject* pLoot = dynamic_cast<CLootContainerObject*>(obj.get());
		if (pLoot)
		{
			pLoot->UpdateLifetime(fTimeElapsed);
		}
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
	if (!m_pLootShader) return nullptr;

	auto* objs = m_pLootShader->GetObj();
	if (!objs) return nullptr;

	const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	const float maxDistSq = fMaxDistance * fMaxDistance;

	CLootContainerObject* pNearest = nullptr;
	float nearestDistSq = maxDistSq;

	for (const auto& obj : *objs)
	{
		if (!obj) continue;

		CLootContainerObject* pLoot = dynamic_cast<CLootContainerObject*>(obj.get());
		if (!pLoot) continue;
		if (!pLoot->IsAlive()) continue;

		float distSq = pLoot->GetDistanceSq(playerPos);
		if (distSq <= nearestDistSq)
		{
			nearestDistSq = distSq;
			pNearest = pLoot;
		}
	}

	return pNearest;
}

/*void InventoryManager::SpawnLootContainerFromEnemy(CEnemyObject* pEnemy)
{
	if (!pEnemy) return;
	if (!m_pLootShader) return;

	CLootContainerObject* pLoot = new CLootContainerObject(30.0f);

	XMFLOAT3 pos = pEnemy->GetPosition();
	pLoot->SetPosition(pos);

	BoundingOrientedBox obb;
	obb.Center = XMFLOAT3(0.0f, 0.6f, 0.0f);
	obb.Extents = XMFLOAT3(0.35f, 0.6f, 0.35f);
	obb.Orientation = XMFLOAT4(0, 0, 0, 1);
	pLoot->SetOOBB(obb);

	// 임시 기본 루팅 아이템
	//pLoot->AddLoot(std::make_shared<WeaponItem>(ItemGrade::GRADE_1, WeaponCategory::PISTOL), 1);
	//pLoot->AddLoot(std::make_shared<ArmorItem>(), 1);

	m_pLootShader->addObjects(std::unique_ptr<CGameObject>(pLoot));

	if (m_pLootBoxShader)
	{
		m_pLootBoxShader->AddObject(pLoot);
	}
}*/
void InventoryManager::SpawnLootContainer(short npc_id, const XMFLOAT3& pos, const ItemID* items, const int* counts, int slotCount)
{
	if (!m_pLootShader) return;

	CLootContainerObject* pLoot = new CLootContainerObject(30.0f);
	pLoot->SetBoxId(npc_id);
	pLoot->SetPosition(pos);

	BoundingOrientedBox obb;
	obb.Center = XMFLOAT3(0.0f, 0.6f, 0.0f);
	obb.Extents = XMFLOAT3(0.35f, 0.6f, 0.35f);
	obb.Orientation = XMFLOAT4(0, 0, 0, 1);
	pLoot->SetOOBB(obb);

	for (int i = 0; i < slotCount; ++i) {
		if (items[i] != ItemID::NONE && counts[i] > 0) {
			pLoot->SetSlotData(i, items[i], counts[i]);
		}
	}

	m_pLootShader->addObjects(std::unique_ptr<CGameObject>(pLoot));

	//if (m_pDebugShader) {
	//	m_pDebugShader->AddObject(pLoot);
	//}
	if (m_pLootBoxShader)
	{
		m_pLootBoxShader->AddObject(pLoot);
	}
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
	if (!m_pLootShader) return nullptr;
	auto* objs = m_pLootShader->GetObj();
	if (!objs) return nullptr;

	for (const auto& obj : *objs) {
		if (!obj) continue;
		CLootContainerObject* pLoot = dynamic_cast<CLootContainerObject*>(obj.get());
		if (!pLoot) continue;
		if (pLoot->GetBoxId() == box_id) return pLoot;
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

	// 박스가 열려있다면 UI 닫기
	if (m_pOpenedLoot == pLoot) {
		m_pOpenedLoot = nullptr;
		if (m_pLootInventory) {
			m_pLootInventory->ClearItems();
			m_pLootInventory->isOpen = false;
			m_pLootInventory->SetId(-1);
		}
	}

	// 박스 객체 시각 표시 종료 (이거 이렇게 처리해도 되는 건가요?)
	pLoot->Kill();
}