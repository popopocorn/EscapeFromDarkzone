#pragma once

#include "Item.h"

class UIObjectShader;
class CLootContainerObject;
class CPlayer;
class CEnemyObject;
class CStandardObjectsShader;
class CBoundingBoxShader;
class CShader;

class InventoryManager
{
private:
	std::unique_ptr<Inventory> m_pPlayerInventory;
	std::unique_ptr<Inventory> m_pLootInventory;
	std::unique_ptr<Inventory> m_pCraftInventory;

	CLootContainerObject* m_pOpenedLoot = nullptr;

	bool m_bTabInventoryHold = false;

	CPlayer* m_pPlayer = nullptr;
	CStandardObjectsShader* m_pLootShader = nullptr;
	CBoundingBoxShader* m_pDebugShader = nullptr;

public:
	InventoryManager() = default;
	~InventoryManager() = default;

	void Initialize(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pUIShader
	);

	void Release();

	void Update(float fTimeElapsed);

	void UpdateLootWorld(float fTimeElapsed);

	void ProcessEnemyLootSpawnRequests(CShader* pEnemyShader);

	void SubmitToShader(UIObjectShader* shader);

	bool ProcessClick(POINT mouse);

	void BindLootWorld(CPlayer* pPlayer, CStandardObjectsShader* pLootShader, CBoundingBoxShader* pDebugShader);
	void SetPlayer(CPlayer* pPlayer) { m_pPlayer = pPlayer; }

	void OpenPlayerInventory();
	void ClosePlayerInventory();
	void TogglePlayerInventory();

	void OpenLootContainer(CLootContainerObject* pLoot);
	void CloseLootInventory();

	void OpenCraftInventory();
	void CloseCraftInventory();
	void ToggleCraftInventory();

	void HandleIKeyToggle(float fLootInteractDistance);

	void HandleTabPressed(float fLootInteractDistance);
	void HandleTabReleased();

	void CloseAll();

	CLootContainerObject* FindNearestLootContainer(float fMaxDistance) const;

	void SpawnLootContainerFromEnemy(CEnemyObject* pEnemy);

	bool IsAnyInventoryOpen() const;
	bool IsPlayerInventoryOpen() const;
	bool IsLootInventoryOpen() const;
	bool IsCraftInventoryOpen() const;
	bool IsTabHold() const { return m_bTabInventoryHold; }

	Inventory* GetPlayerInventory() const { return m_pPlayerInventory.get(); }
	Inventory* GetLootInventory() const { return m_pLootInventory.get(); }
	Inventory* GetCraftInventory() const { return m_pCraftInventory.get(); }

	CLootContainerObject* GetOpenedLoot() const { return m_pOpenedLoot; }
};