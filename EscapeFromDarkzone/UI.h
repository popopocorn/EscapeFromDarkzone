#pragma once
#include "Object.h"
#include"Item.h"


class UIMesh {
private:
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
	vector<XMFLOAT3>				m_pxmf3Positions;
	vector<XMFLOAT2>				UVs;

	ID3D12Resource*					m_pd3dPositionBuffer = NULL;
	ID3D12Resource*					m_pd3dPositionUploadBuffer = NULL;
	ID3D12Resource*					UVBuffer = NULL;
	ID3D12Resource*					UVUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dPositionBufferView;
	D3D12_VERTEX_BUFFER_VIEW		UVBufferView;
	unique_ptr<CTexture>			texture;
public:	
	UIMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist);
	~UIMesh();
	virtual void ReleaseUploadBuffers();
	virtual void LoadTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName);
	virtual void OnPreRender(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nSubSet = 0, int nInstances = 1);

};

struct CheckBox {
	float minX;
	float minY;
	float maxX;
	float maxY;
	bool Intersects(POINT p)
	{
		float ndcX = (2.0f * static_cast<float>(p.x)) / FRAME_BUFFER_WIDTH - 1.0f;
		float ndcY = 1.0f - (2.0f * static_cast<float>(p.y)) / FRAME_BUFFER_HEIGHT; // DirectX 기준 Y축 상하 반전
		if (ndcX < minX) return false;
		if (ndcX > maxX) return false;
		if (ndcY < minY) return false;
		if (ndcY > maxY) return false;
		return true;

	}
};


class UIObject {
protected:
	UIMesh*					object;
	XMFLOAT4X4				world;
	std::function<void()>	Task = nullptr;
	CheckBox				CollisionBox;
public:
	UIObject();

	void SetUIMesh(UIMesh* mesh) { object = mesh; }
	void SetScale(float x, float y, float z);
	void SetLocate(float x, float y, float z);
	void HandleClick() { if (Task) Task(); }
	void SetFunc(std::function<void()> func) { Task = func; }
	void setAABB();
	CheckBox GetBox() { return CollisionBox; }


	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera = NULL);
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

	int ID;		// 이 친구를 써서 인벤토리가 플레이어 것인지 루트박스 것인지 구분하도록 만들기 (-1이면 플레이어, 0 이상이면 npc_id 루트박스)

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

	bool ApplyServerSlotUpdate(int slotIndex, ItemID itemId, int count);

	void ClearItems();                    // 인벤토리 슬롯 내용 초기화
	void SetPosition(float x, float y);   // 인벤토리 전체 위치 이동
	void SetId(int x) { ID = x; }
	int GetItemCnt(ItemID id)const;
	void ConsumeItem(ItemID id, int cnt);

	bool isOpen = false;
};
class UIPannel {
public:
	bool isOpen = false;
	virtual ~UIPannel() = default;
	virtual bool ProcessClick(POINT mouse) = 0;
	virtual void SubmitToShader(UIObjectShader* shader) = 0;
	virtual void ToggleOpen() { isOpen = !isOpen; }
};
class EquipUI :public UIPannel{
private:
	Equip* player;
	ItemID		helmet = ItemID::NONE;
	ItemID		body = ItemID::NONE;
	ItemID		shoes = ItemID::NONE;
	ItemID		plate = ItemID::NONE;
	unordered_map<ItemType, unique_ptr<UIObject>> UIs;
	unique_ptr<UIObject>base;
public:
	EquipUI() = default;
	EquipUI(CPlayer* player);
	void Init(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void EquipItem(ItemID item);
	bool ProcessClick(POINT mouse);
	void SubmitToShader(UIObjectShader* shader);
	
};

class UIObjectShader;

class HUDManager {
private:
	vector<unique_ptr<UIObject>> objs;
	vector<unique_ptr<UIPannel>> pannels;
public:
	bool ProcessClick(POINT mouse);
	void SubmitToShader(UIObjectShader* shader);
	void release();
	void AddToManager(UIObject* obj) { objs.push_back(unique_ptr<UIObject>(obj)); }
};