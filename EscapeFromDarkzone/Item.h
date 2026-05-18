#pragma once

#include "UI.h"
#include<algorithm>

//아이템 선언
enum class ItemType {
	PISTOL,
	ASSAULT_RIFLE,
	SMG,
	SHOTGUN,
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

//재료를 위한 구조
enum class ItemID {
	NONE = 0,
	MAT_1,
	MAT_2,
	MAT_3,
	MAT_4,
	MAT_5,
	MAT_6,

	WEAPON_UPGRADE_1,
	WEAPON_UPGRADE_2,
	WEAPON_UPGRADE_3,
	WEAPON_UPGRADE_4,
	ARMOR_PLATE,
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
struct ItemSlot {
	ItemID item;
	int count = 0;
};

//아이템 출력 슬롯
struct ItemSlotView {
	std::unique_ptr<UIObject> hitBox;
	std::unique_ptr<UIObject> iconCell;
	std::unique_ptr<UIObject> textCell;
	std::unique_ptr<UIObject> countCell;
};

const int MAX_SLOTS = 10;

class Inventory {
private:
	std::array<ItemSlot, MAX_SLOTS> slots;
	std::array<ItemSlotView, MAX_SLOTS> slotViews;
	CheckBox box;

	float m_baseX = 0.0f;
	float m_baseY = 0.0f;
	float m_slotW = 0.0f;
	float m_slotH = 0.0f;
	float m_slotGap = 0.025f;

	float m_iconRatio = 0.20f;
	float m_textRatio = 0.60f;
	float m_countRatio = 0.20f;

	int ID;

	std::unique_ptr<UIMesh> m_pSharedMesh; // UI 공용 메쉬

private:
	void BuildSlotViews();  // 3칸 UI 생성
	void LayoutSlotViews(); // 3칸 위치 배치

public:
	Inventory(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pShader);

	void SubmitToShader(UIObjectShader* shader);
	void SlotClicked(int slotidx);
	bool ProcessClick(POINT mouse);
	//드래그 앤 드롭 처리 함수 추가
	bool AddItem(ItemID item, int count = 1);
	ItemSlot* GetSlot(int idx);

	void ClearItems();                    // 인벤토리 슬롯 내용 초기화
	void SetPosition(float x, float y);   // 인벤토리 전체 위치 이동
	void SetId(int x) { ID = x; }
	int GetItemCnt(ItemID id)const;
	void ConsumeItem(ItemID id, int cnt);

	bool isOpen = false;
};

//아이템 제작시스템
class CraftBox {
private:
	map<ItemID, Recipe> recipeTable;
public:
	void Init();

	bool TryCraft(ItemID target, Inventory* playerinventory);
};


//무기 선언
class WeaponItem : public Item
{
public:
	WeaponItem(ItemGrade grade, ItemType category);

	const WeaponSpec& GetSpec() const { return m_Spec; }
	ItemGrade GetGrade() const { return m_Grade; }

	static WeaponSpec BuildSpec(ItemType category, ItemGrade grade);

	static std::shared_ptr<WeaponItem> CreateDefaultPlayerRifle(CGameObject* pPrototype);

private:
	ItemGrade m_Grade = ItemGrade::BASIC;
	ItemType m_Category = ItemType::PISTOL;
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

class EquipUI {
private:
	CPlayer* player;



public:
};