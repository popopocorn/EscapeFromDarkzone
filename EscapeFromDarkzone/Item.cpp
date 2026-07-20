#include "stdafx.h"
#include "UI.h"
#include "Item.h"
#include "Shader.h"
#include "Object.h"
#include "Network.h"	// 05.08 추가: 아이템 클릭 패킷 전송
#include"Player.h"
#include"ResourceManager.h"



CGameObject* Item::CreateModelInstance() const
{
	if (!model) return nullptr;
	return CGameObject::CreateModelInstance(model);
}

WeaponItem::WeaponItem(ItemGrade grade, ItemType category)
	: m_Grade(grade), m_Category(category)
{
	type = category;
	m_Spec = BuildSpec(category, grade);
	maxAmmo = m_Spec.magazineSize;
	CurAmmo = maxAmmo;
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
		spec.magazineSize = 15;
		spec.reloadTime = 1.2f;
		break;

	case ItemType::RIFLE:
		spec.rpm = 700.0f;
		spec.maxDistanceDamageReductionRatio = 0.10f;
		spec.magazineSize = 30;
		spec.reloadTime = 1.8f;

		switch (grade)
		{
		case ItemGrade::GRADE_1:
			spec.damage = 11.0f;
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 14.0f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 18.0f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 23.0f;
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
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 11.0f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 14.0f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 17.0f;
			break;
		default:
			break;
		}
		break;

	case ItemType::SHOTGUN:
		spec.rpm = 55.0f;
		spec.zeroDamageBeyondDistance = 20.0f;
		spec.magazineSize = 8;
		spec.reloadTime = 2.4f;

		switch (grade)
		{
		case ItemGrade::GRADE_1:
			spec.damage = 64.0f;
			break;
		case ItemGrade::GRADE_2:
			spec.damage = 72.0f;
			break;
		case ItemGrade::GRADE_3:
			spec.damage = 80.0f;
			break;
		case ItemGrade::GRADE_4:
			spec.damage = 88.0f;
			break;
		default:
			break;
		}
		break;
	}

	// DPS 일괄 계산
	spec.dps = spec.damage * spec.rpm / 60.0f;

	return spec;
}
std::shared_ptr<WeaponItem> WeaponItem::CreateDefaultPlayerRifle(CGameObject* pPrototype)
{
	if (!pPrototype) return nullptr;

	auto pItem = std::make_shared<WeaponItem>(
		ItemGrade::GRADE_1,
		ItemType::RIFLE
	);

	pItem->SetModelPrototype(pPrototype);
	return pItem;
}

//void CraftBox::Init()
//{
//
//}
//
//bool CraftBox::TryCraft(ItemID target, Inventory* playerinventory)
//{
//	auto it = recipeTable.find(target);
//	if (it == recipeTable.end())return false;
//
//	const Recipe& r = it->second;
//
//	for (const auto& req : r.ingredients)
//	{
//		if (playerinventory->GetItemCnt(req.itemID) < req.reuireCnt)
//			return false;
//	}
//	for (const auto& req : r.ingredients)
//	{
//		playerinventory->ConsumeItem(req.itemID, req.reuireCnt);
//	}
//	//playerinventory 아이템 추가
//	return true;
//}

Equip::Equip()
{
	ResetForNewRound();
}

void Equip::ResetForNewRound()
{
	helmet = ArmorItem(ItemType::ARMOR_HELMET, ItemID::NONE);
	body = ArmorItem(ItemType::ARMOR_BODY, ItemID::NONE);
	shoes = ArmorItem(ItemType::ARMOR_SHOES, ItemID::NONE);
	plate = Plate(ItemType::PLATE, ItemID::NONE);
}