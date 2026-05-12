#include "stdafx.h"
#include "UI.h"
#include "Item.h"
#include "Shader.h"
#include "Object.h"

//모델 원본 풀
std::unordered_map<std::string, CGameObject*> ItemModelLibrary::s_ModelPool;

CGameObject* ItemModelLibrary::GetOrLoadPrototype(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	const char* modelPath,
	CShader* pShader)
{
	if (!modelPath) return nullptr;

	auto it = s_ModelPool.find(modelPath);
	if (it != s_ModelPool.end())
	{
		return it->second;
	}

	CGameObject* pPrototype = CGameObject::LoadGeometryModelByName(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		nullptr,
		modelPath,
		pShader,
		nullptr
	);

	if (!pPrototype)
		return nullptr;

	pPrototype->AddRef();
	s_ModelPool.emplace(modelPath, pPrototype);

	return pPrototype;
}

void ItemModelLibrary::ReleaseAll()
{
	for (auto& pair : s_ModelPool)
	{
		if (pair.second)
		{
			pair.second->Release();
		}
	}
	s_ModelPool.clear();
}

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

std::shared_ptr<WeaponItem> WeaponItem::CreateDefaultPlayerRifle(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pShader)
{
	auto pItem = std::make_shared<WeaponItem>(
		ItemGrade::GRADE_1,
		ItemType::ASSAULT_RIFLE
	);

	pItem->SetModelPrototype(
		ItemModelLibrary::GetOrLoadPrototype(
			pd3dDevice,
			pd3dCommandList,
			pd3dGraphicsRootSignature,
			"Model/Classic_M4_1.bin",
			pShader
		)
	);

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

	UIMesh* m = new UIMesh(pd3dDevice, pd3dCommandList);

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (!slots[i].ui)
		{
			slots[i].ui = std::make_unique<UIObject>();
		}

		auto* ui = slots[i].ui.get();
		ui->SetFunc([this, i]() {
			this->SlotClicked(i);
			});
		ui->SetUIMesh(m);
		ui->SetScale(m_slotW, m_slotH - m_slotGap, 1.0f);
	}

	SetPosition(0.0f, 0.0f);
}

void Inventory::SubmitToShader(UIObjectShader* shader)
{
	if (!isOpen) return;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (slots[i].ui)
		{
			shader->addObjects(slots[i].ui.get());
		}
	}
}

void Inventory::SlotClicked(int slotidx)
{
	wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"Slot %d clicked\n", slotidx);
	OutputDebugStringW(debugBuf);
}

void Inventory::ProcessClick(POINT mouse)
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		auto* ui = slots[i].ui.get();
		if (!ui) continue;

		if (ui->GetBox().Intersects(mouse))
		{
			ui->HandleClick();
		}
		//여기에 드래그 앤 드롭 처리 추가


	}
}

void Inventory::SetPosition(float x, float y)
{
	m_baseX = x;
	m_baseY = y;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (!slots[i].ui) continue;

		float posY = m_baseY + (0.5f - (m_slotH * 0.5f) - (i * m_slotH));
		slots[i].ui->SetLocate(m_baseX, posY, 0.5f);
		slots[i].ui->setAABB();
	}
}

int Inventory::GetItemCnt(ItemID id) const
{
	
	for (const auto& slot : slots)
	{
		if (slot.item != nullptr && slot.item->GetID() == id)
		{
			return slot.count;
		}
	}
	
}

void Inventory::ConsumeItem(ItemID id, int cnt)
{
	for (auto& s : slots)
	{
		if (s.item != nullptr && s.item->GetID() == id)
		{
			s.count -= cnt;
			if (s.count == 0)
			{
				s.item.reset();
			}
			break;
		}
	}
}

bool Inventory::AddItem(std::unique_ptr<Item> item, int count)
{
	if (!item || count <= 0) return false;

	for (auto& slot : slots)
	{
		if (slot.item!=nullptr && slot.item->GetID() == item->GetID())
		{
			slot.count += count;
			return true;
		}
		
	}
	for (auto& slot : slots)
	{
		if (slot.item == nullptr)
		{
			slot.item = std::move(item);
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
		slot.item.reset();
		slot.count = 0;
	}
}

ItemSlot* Inventory::GetSlot(int idx)
{
	if (idx < 0 || idx >= MAX_SLOTS) return nullptr;
	return &slots[idx];
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

}
