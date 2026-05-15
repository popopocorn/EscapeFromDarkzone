#include "stdafx.h"
#include "UI.h"
#include "Item.h"
#include "Shader.h"
#include "Object.h"



CGameObject* Item::CreateModelInstance() const
{
	if (!model) return nullptr;
	return CGameObject::CreateModelInstance(model);
}

WeaponItem::WeaponItem(ItemGrade grade, ItemType category)
	: m_Grade(grade), m_Category(category)
{
	type = ItemType::PISTOL;
	m_Spec = BuildSpec(category, grade);
}

//무기 수치
WeaponSpec WeaponItem::BuildSpec(ItemType category, ItemGrade grade)
{
	WeaponSpec spec;

	switch (category)
	{
	case ItemType::PISTOL:
		spec.damage = 7.0f;
		spec.rpm = 550.0f;
		spec.dps = 64.166667f;
		spec.magazineSize = 15;
		spec.reloadTime = 1.2f;
		break;

	case ItemType::ASSAULT_RIFLE:
		spec.rpm = 700.0f;
		spec.maxDistanceDamageReductionRatio = 0.10f;
		spec.magazineSize = 30;
		spec.reloadTime = 1.8f;

		switch (grade)
		{
		case ItemGrade::GRADE_1:
			spec.damage = 11.0f;
			spec.dps = 128.33333f;
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 14.0f;
			spec.dps = 163.33333f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 18.0f;
			spec.dps = 210.0f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 23.0f;
			spec.dps = 268.33333f;
			break;
		default:
			break;
		}
		break;

	case ItemType::SMG:
		spec.rpm = 900.0f;
		spec.maxDistanceDamageReductionRatio = 0.50f;
		spec.magazineSize = 35;
		spec.reloadTime = 1.6f;

		switch (grade)
		{
		case ItemGrade::GRADE_1:
			spec.damage = 9.0f;
			spec.dps = 135.0f;
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 11.0f;
			spec.dps = 165.0f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 14.0f;
			spec.dps = 210.0f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 17.0f;
			spec.dps = 255.0f;
			break;
		default:
			break;
		}
		break;

	case ItemType::SHOTGUN:
		spec.rpm = 200.0f;
		spec.zeroDamageBeyondDistance = 20.0f;
		spec.magazineSize = 8;
		spec.reloadTime = 2.4f;

		switch (grade)
		{
		case ItemGrade::GRADE_1:
			spec.damage = 64.0f;
			spec.dps = 213.33333f;
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 72.0f;
			spec.dps = 240.0f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 80.0f;
			spec.dps = 266.66667f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 88.0f;
			spec.dps = 293.33333f;
			break;
		default:
			break;
		}
		break;
	}

	return spec;
}

std::shared_ptr<WeaponItem> WeaponItem::CreateDefaultPlayerRifle(CGameObject* pPrototype)
{
	if (!pPrototype) return nullptr;

	auto pItem = std::make_shared<WeaponItem>(
		ItemGrade::GRADE_1,
		ItemType::ASSAULT_RIFLE
	);

	pItem->SetModelPrototype(pPrototype);
	return pItem;
}

Inventory::Inventory(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* pShader)
{
	box.maxY = 0.5f;
	box.minY = -0.5f;
	box.minX = -0.5f;
	box.maxX = 0.0f;

	float totalW = box.maxX - box.minX;
	float totalH = box.maxY - box.minY;

	m_slotW = totalW;
	m_slotH = totalH / static_cast<float>(MAX_SLOTS);
	m_slotGap = 0.025f;

	m_pSharedMesh = std::make_unique<UIMesh>(pd3dDevice, pd3dCommandList);

	BuildSlotViews();
	SetPosition(0.0f, 0.0f);
}

void Inventory::BuildSlotViews()
{
	const float rowH = m_slotH - m_slotGap;

	const float iconW = m_slotW * m_iconRatio;
	const float textW = m_slotW * m_textRatio;
	const float countW = m_slotW * m_countRatio;

	const float innerGap = 0.01f;

	const float iconRenderW = iconW - innerGap;
	const float textRenderW = textW - innerGap;
	const float countRenderW = countW - innerGap;
	const float renderH = rowH - 0.005f;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (!slotViews[i].hitBox)
		{
			slotViews[i].hitBox = std::make_unique<UIObject>();
			slotViews[i].hitBox->SetFunc([this, i]() {
				this->SlotClicked(i);
				});
			slotViews[i].hitBox->SetScale(m_slotW, rowH, 1.0f);
		}

		if (!slotViews[i].iconCell)
		{
			slotViews[i].iconCell = std::make_unique<UIObject>();
			slotViews[i].iconCell->SetUIMesh(m_pSharedMesh.get());
			slotViews[i].iconCell->SetScale(iconRenderW, renderH, 1.0f);
		}

		if (!slotViews[i].textCell)
		{
			slotViews[i].textCell = std::make_unique<UIObject>();
			slotViews[i].textCell->SetUIMesh(m_pSharedMesh.get());
			slotViews[i].textCell->SetScale(textRenderW, renderH, 1.0f);
		}

		if (!slotViews[i].countCell)
		{
			slotViews[i].countCell = std::make_unique<UIObject>();
			slotViews[i].countCell->SetUIMesh(m_pSharedMesh.get());
			slotViews[i].countCell->SetScale(countRenderW, renderH, 1.0f);
		}
	}
}

void Inventory::LayoutSlotViews()
{
	const float iconW = m_slotW * m_iconRatio;
	const float textW = m_slotW * m_textRatio;
	const float countW = m_slotW * m_countRatio;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		float posY = m_baseY + (0.5f - (m_slotH * 0.5f) - (i * m_slotH));

		float rowLeft = m_baseX - (m_slotW * 0.5f);

		float iconCenterX = rowLeft + (iconW * 0.5f);
		float textCenterX = rowLeft + iconW + (textW * 0.5f);
		float countCenterX = rowLeft + iconW + textW + (countW * 0.5f);

		if (slotViews[i].hitBox)
		{
			slotViews[i].hitBox->SetLocate(m_baseX, posY, 0.5f);
			slotViews[i].hitBox->setAABB();
		}

		if (slotViews[i].iconCell)
		{
			slotViews[i].iconCell->SetLocate(iconCenterX, posY, 0.5f);
			slotViews[i].iconCell->setAABB();
		}

		if (slotViews[i].textCell)
		{
			slotViews[i].textCell->SetLocate(textCenterX, posY, 0.5f);
			slotViews[i].textCell->setAABB();
		}

		if (slotViews[i].countCell)
		{
			slotViews[i].countCell->SetLocate(countCenterX, posY, 0.5f);
			slotViews[i].countCell->setAABB();
		}
	}
}

void Inventory::SubmitToShader(UIObjectShader* shader)
{
	if (!isOpen) return;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (slotViews[i].iconCell)
		{
			shader->addObjects(slotViews[i].iconCell.get());
		}
		if (slotViews[i].textCell)
		{
			shader->addObjects(slotViews[i].textCell.get());
		}
		if (slotViews[i].countCell)
		{
			shader->addObjects(slotViews[i].countCell.get());
		}
	}
}

void Inventory::SlotClicked(int slotidx)
{
	wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"Slot %d clicked\n", slotidx);
	OutputDebugStringW(debugBuf);
}

bool Inventory::ProcessClick(POINT mouse)
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		auto* hitBox = slotViews[i].hitBox.get();
		if (!hitBox) continue;

		if (hitBox->GetBox().Intersects(mouse))
		{
			hitBox->HandleClick();
			return true; // UI 클릭 소비됨
		}
	}

	return false; // UI 클릭 아님
}

void Inventory::SetPosition(float x, float y)
{
	m_baseX = x;
	m_baseY = y;

	LayoutSlotViews();
}

int Inventory::GetItemCnt(ItemID id) const
{
	for (const auto& slot : slots)
	{
		if (slot.item != ItemID::NONE && slot.item == id)
		{
			return slot.count;
		}
	}

	return 0;
}

void Inventory::ConsumeItem(ItemID id, int cnt)
{
	for (auto& s : slots)
	{
		if (s.item != ItemID::NONE && s.item == id)
		{
			s.count -= cnt;
			if (s.count == 0)
			{
				s.item = ItemID::NONE;
			}
			break;
		}
	}
}

bool Inventory::AddItem(ItemID item, int count)
{
	if (item == ItemID::NONE || count <= 0) return false;

	for (auto& slot : slots)
	{
		if (slot.item != ItemID::NONE && slot.item == item)
		{
			slot.count += count;
			return true;
		}
	}

	for (auto& slot : slots)
	{
		if (slot.item == ItemID::NONE)
		{
			slot.item = item;
			slot.count = count;
			return true;
		}
	}

	return false;
}

void Inventory::ClearItems()
{
	for (auto& slot : slots)
	{
		slot.item = ItemID::NONE;
		slot.count = 0;
	}
}

ItemSlot* Inventory::GetSlot(int idx)
{
	if (idx < 0 || idx >= MAX_SLOTS) return nullptr;
	return &slots[idx];
}

//슬롯 규칙
bool Inventory::ApplyServerSlotUpdate(int slotIndex, ItemID itemId, int count)
{
	ItemSlot* pSlot = GetSlot(slotIndex);
	if (!pSlot) return false;

	// 서버가 이름 NONE이나 수량 0이면 슬롯 비우기
	if (itemId == ItemID::NONE || count <= 0)
	{
		pSlot->item = ItemID::NONE;
		pSlot->count = 0;
		return true;
	}

	// 슬롯에 이미 뭐가 있으면 수정하지 않음
	if (pSlot->item != ItemID::NONE || pSlot->count > 0)
	{
		return false;
	}

	// 빈 슬롯일 때만 서버 값으로 채움
	pSlot->item = itemId;
	pSlot->count = count;
	return true;
}

void CraftBox::Init()
{

}

bool CraftBox::TryCraft(ItemID target, Inventory* playerinventory)
{
	auto it = recipeTable.find(target);
	if (it == recipeTable.end())return false;

	const Recipe& r = it->second;

	for (const auto& req : r.ingredients)
	{
		if (playerinventory->GetItemCnt(req.itemID) < req.reuireCnt)
			return false;
	}
	for (const auto& req : r.ingredients)
	{
		playerinventory->ConsumeItem(req.itemID, req.reuireCnt);
	}
	//playerinventory 아이템 추가
	return true;
}
