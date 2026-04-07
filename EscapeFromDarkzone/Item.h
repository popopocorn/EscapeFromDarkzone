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
	UIObject* ui;

};

const int MAX_SLOTS = 32;

class Inventory {
private:
	std::array<ItemSlot, MAX_SLOTS> slots;
public:
	void initSlots();

	void SlotClicked(int slotidx);
};

