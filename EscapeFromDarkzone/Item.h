#pragma once

#include<algorithm>
#include "ItemDef.h"



struct WeaponSpec
{
	float damage = 0.0f;
	float rpm = 0.0f;
	float dps = 0.0f;

	float maxDistanceDamageReductionRatio = 0.0f;
	float zeroDamageBeyondDistance = 0.0f;

	int magazineSize = 0;
	float reloadTime = 0.0f;
};


struct RecipeElement {
	ItemID itemID;
	int reuireCnt;
};

struct Recipe {
	ItemID result;
	int resultCnt;
	vector<RecipeElement>ingredients;
};



class CGameObject;
class UIObject;
class UIObjectShader;
class CShader;



class Item {
public:
	Item() = default;
	virtual ~Item() = default;
	Item(ItemType t, ItemID i) { type = t; id = i; }

	//모델 원본 관련 함수
	void SetModelPrototype(CGameObject* pPrototype) { model = pPrototype; }
	CGameObject* GetModelPrototype() const { return model; }
	CGameObject* CreateModelInstance() const;		//인스턴스 생성

	ItemType GetType() const { return type; }
	ItemID GetID()const { return id; }

protected:
	ItemType type = ItemType::MATERIAL;
	CGameObject* model = nullptr;					//모델 참조 포인터
	ItemID id = ItemID::NONE;
	// 제작방법
};

//아이템 데이터와 출력 구조체로 나눠서 관리
//아이템 데이터 슬롯
/*struct ItemSlot {
	ItemID item;
	int count = 0;
};*/

//아이템 출력 슬롯
struct ItemSlotView {
	std::unique_ptr<UIObject> hitBox;
	std::unique_ptr<UIObject> iconCell;
	std::unique_ptr<UIObject> textCell;
	std::unique_ptr<UIObject> countCell;
};

//아이템 제작시스템
//class CraftBox {
//private:
//	map<ItemID, Recipe> recipeTable;
//public:
//	void Init();
//
//	bool TryCraft(ItemID target, Inventory* playerinventory);
//};


//무기 선언
class WeaponItem : public Item
{
public:
	WeaponItem(ItemGrade grade, ItemType category);

	const WeaponSpec& GetSpec() const { return m_Spec; }
	ItemGrade GetGrade() const { return m_Grade; }

	static WeaponSpec BuildSpec(ItemType category, ItemGrade grade);

	static std::shared_ptr<WeaponItem> CreateDefaultPlayerRifle(CGameObject* pPrototype);
	int maxAmmo;
	int CurAmmo;
private:
	ItemGrade m_Grade = ItemGrade::BASIC;
	ItemType m_Category = ItemType::PISTOL;
	WeaponSpec m_Spec;
};

class Plate : public Item {
private:
	short hp = 100;
	bool broken = false;
public:
	Plate() {};
	Plate(ItemType t, ItemID i) : Item(t, i) {};
	short GetHp() { return hp; }
	short DoDamage(short d) { hp -= d; if (hp < 0)broken = true; }
};
class ArmorItem : public Item
{
public:
	ArmorItem() = default;
	ArmorItem(ItemType t, ItemID i) : Item(t, i) {};
};

struct Equip {
	Equip();
	ArmorItem helmet;
	ArmorItem body;
	ArmorItem shoes;
	Plate     plate;
};

