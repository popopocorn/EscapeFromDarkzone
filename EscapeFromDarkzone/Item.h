#pragma once

enum class ItemType {
	WEAPON,
	ARMOR,
	PLATE,
	CONSUMABLE,
	MATERIAL,
};

class CGameObject;
class UIObject;

class Item{
private:
	ItemType type;
	CGameObject* model = nullptr;
	bool CanMake = false;
	//제작방법

};

struct ItemSlot {
	Item* item = nullptr;
	int count = 0;
	UIObject* ui = nullptr;

};

const int MAX_SLOTS = 10;

class UIObjectShader;

class Inventory {
private:
	std::array<ItemSlot, MAX_SLOTS> slots;
	CheckBox box;
public:
	Inventory(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pShader);
	void SubmitToShader(UIObjectShader* shader);
	void SlotClicked(int slotidx);
	void ProcessClick(POINT mouse);
	bool isOpen = false;
};

