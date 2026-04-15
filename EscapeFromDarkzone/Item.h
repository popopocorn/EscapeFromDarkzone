#pragma once

#include "UI.h"

//아이템 선언
enum class ItemType {
	WEAPON,
	ARMOR,
	PLATE,
	CONSUMABLE,
	MATERIAL,
};

enum class ItemGrade
{
	BASIC,
	GRADE_1,
	GRADE_2,
	GRADE_3,
	GRADE_4
};

enum class WeaponCategory
{
	PISTOL,
	ASSAULT_RIFLE,
	SMG,
	SHOTGUN
};

struct WeaponSpec
{
	float damage = 0.0f;
	float rpm = 0.0f;
	float dps = 0.0f;

	float maxDistanceDamageReductionRatio = 0.0f;
	float zeroDamageBeyondDistance = 0.0f;
};

class CGameObject;
class UIObject;
class UIObjectShader;
class CShader;

//모델 원본
class ItemModelLibrary
{
public:
	static CGameObject* GetOrLoadPrototype(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		const char* modelPath,
		CShader* pShader);

	static void ReleaseAll();

private:
	static std::unordered_map<std::string, CGameObject*> s_ModelPool;
};


class Item {
public:
	Item() = default;
	virtual ~Item() = default;

	//모델 원본 관련 함수
	void SetModelPrototype(CGameObject* pPrototype) { model = pPrototype; }
	CGameObject* GetModelPrototype() const { return model; }
	CGameObject* CreateModelInstance() const;		//인스턴스 생성

	ItemType GetType() const { return type; }

protected:
	ItemType type = ItemType::MATERIAL;
	CGameObject* model = nullptr;					//모델 참조
	bool CanMake = false;
	// 제작방법
};

//아이템 슬롯
struct ItemSlot {
	std::shared_ptr<Item> item;
	int count = 0;
	UIObject* ui = nullptr;
};

const int MAX_SLOTS = 10;

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

	bool AddItem(const std::shared_ptr<Item>& item, int count = 1);
	ItemSlot* GetSlot(int idx);

	bool isOpen = false;
};

//무기 선언
class WeaponItem : public Item
{
public:
	WeaponItem(ItemGrade grade, WeaponCategory category);

	const WeaponSpec& GetSpec() const { return m_Spec; }
	ItemGrade GetGrade() const { return m_Grade; }
	WeaponCategory GetCategory() const { return m_Category; }

	static WeaponSpec BuildSpec(WeaponCategory category, ItemGrade grade);

	static std::shared_ptr<WeaponItem> CreateDefaultPlayerRifle(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pShader);

private:
	ItemGrade m_Grade = ItemGrade::BASIC;
	WeaponCategory m_Category = WeaponCategory::PISTOL;
	WeaponSpec m_Spec;
};

class ArmorItem : public Item
{
public:
	ArmorItem()
	{
		type = ItemType::ARMOR;
	}
};