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

	int magazineSize = 0;
	float reloadTime = 0.0f;
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
	CGameObject* model = nullptr;					//모델 참조 포인터
	bool CanMake = false;

	//나중에 제작 시스템 추가
};

//아이템 데이터와 출력 구조체로 나눠서 관리
//아이템 데이터 슬롯
struct ItemSlot {
	std::unique_ptr<Item> item = nullptr;
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
	//드래그 앤 드롭 처리 함수 추가, 지금은 클릭 처리만 구현

	bool AddItem(std::unique_ptr<Item> item, int count = 1);
	ItemSlot* GetSlot(int idx);

	void ClearItems();                    // 인벤토리 슬롯 내용 초기화
	void SetPosition(float x, float y);   // 인벤토리 전체 위치 이동
	void SetId(int x) { ID = x; }

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