#include"stdafx.h"
#include"UI.h"
#include "Item.h"

void Inventory::initSlots()
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		slots[i].ui->SetFunc([this, i]() {
			this->SlotClicked(i);
			});
	}
}

void Inventory::SlotClicked(int slotidx)
{
	
}