#include "stdafx.h"
#include "UI.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Network.h"
#include "Player.h"
#include "TextRenderer.h"
#include "ItemTextData.h"

UIName MapItemIDToUIName(ItemID id);

UIMesh::UIMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist)
{
	m_pxmf3Positions.resize(6);
	m_pxmf3Positions[0] = XMFLOAT3(-0.5f, -0.5f, 0.0f); // LB
	m_pxmf3Positions[1] = XMFLOAT3(-0.5f, 0.5f, 0.0f); // LT
	m_pxmf3Positions[2] = XMFLOAT3(0.5f, 0.5f, 0.0f); // RT

	m_pxmf3Positions[3] = XMFLOAT3(-0.5f, -0.5f, 0.0f); // LB
	m_pxmf3Positions[4] = XMFLOAT3(0.5f, 0.5f, 0.0f); // RT
	m_pxmf3Positions[5] = XMFLOAT3(0.5f, -0.5f, 0.0f); // RB

	UVs.resize(6);
	UVs[0] = XMFLOAT2(0.0f, 1.0f); // LB
	UVs[1] = XMFLOAT2(0.0f, 0.0f); // LT
	UVs[2] = XMFLOAT2(1.0f, 0.0f); // RT

	UVs[3] = XMFLOAT2(0.0f, 1.0f); // LB
	UVs[4] = XMFLOAT2(1.0f, 0.0f); // RT
	UVs[5] = XMFLOAT2(1.0f, 1.0f); // RB

	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


	m_pd3dPositionBuffer = ::CreateBufferResource(
		device,
		commandlist,
		m_pxmf3Positions.data(),
		sizeof(XMFLOAT3) * m_pxmf3Positions.size(),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dPositionUploadBuffer);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_pxmf3Positions.size();

	UVBuffer = ::CreateBufferResource(
		device,
		commandlist,
		UVs.data(),
		sizeof(XMFLOAT2) * UVs.size(),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&UVUploadBuffer
	);

	UVBufferView.BufferLocation = UVBuffer->GetGPUVirtualAddress();
	UVBufferView.StrideInBytes = sizeof(XMFLOAT2);
	UVBufferView.SizeInBytes = sizeof(XMFLOAT2) * UVs.size();

	/*m_pd3dPositionBuffer->SetName(L"uivbuffer");
	m_pd3dPositionBuffer->SetName(L"uivupuffer");
	UVBuffer->SetName(L"uiuvbuffer");
	UVUploadBuffer->SetName(L"uiuvupbuffer");*/
	//LoadTexture(device, commandlist, L"./Model/Textures/Asphalt_texture1.dds");
}

UIMesh::~UIMesh()
{

	if (m_pd3dPositionBuffer)
	{
		m_pd3dPositionBuffer->Release();
		m_pd3dPositionBuffer = nullptr;
	}

	if (UVBuffer)
	{
		UVBuffer->Release();
		UVBuffer = nullptr;
	}
}

void UIMesh::ReleaseUploadBuffers()
{
	m_pd3dPositionUploadBuffer->Release();
	UVUploadBuffer->Release();

	texture->ReleaseUploadBuffers();
}

void UIMesh::LoadTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName)
{
	texture = make_unique<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);
	texture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, pszFileName, RESOURCE_TEXTURE2D, 0);
	ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, texture.get(), 0, 3);

}

void UIMesh::OnPreRender(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	D3D12_VERTEX_BUFFER_VIEW b[] = { m_d3dPositionBufferView, UVBufferView };
	pd3dCommandList->IASetVertexBuffers(0, 2, b);
	if (texture)texture->UpdateShaderVariables(pd3dCommandList);
}

void UIMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nSubSet, int nInstances)
{
	OnPreRender(pd3dCommandList, NULL);

	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);

	pd3dCommandList->DrawInstanced(m_pxmf3Positions.size(), nInstances, 0, 0);
}



UIObject::UIObject()
{
	XMStoreFloat4x4(&world, XMMatrixIdentity());
}

void UIObject::SetScale(float x, float y, float z)
{
	world._11 = x;
	world._22 = y;
	world._33 = z;
}

void UIObject::SetLocate(float x, float y, float z)
{
	world._41 = x;
	world._42 = y;
	world._43 = z;
}

void UIObject::setAABB()
{
	float cx = world._41;
	float cy = world._42;

	float sx = sqrtf(world._11 * world._11 + world._12 * world._12 + world._13 * world._13);
	float sy = sqrtf(world._21 * world._21 + world._22 * world._22 + world._23 * world._23);

	CollisionBox.minX = cx - sx * 0.5f;
	CollisionBox.maxX = cx + sx * 0.5f;
	CollisionBox.minY = cy - sy * 0.5f;
	CollisionBox.maxY = cy + sy * 0.5f;
}

void UIObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera)
{
	if (not object)return;
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&world)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
	object->Render(pd3dCommandList, 0, 1);
}


Inventory::Inventory(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pShader)
{
	UNREFERENCED_PARAMETER(pd3dDevice);
	UNREFERENCED_PARAMETER(pd3dCommandList);
	UNREFERENCED_PARAMETER(pd3dGraphicsRootSignature);
	UNREFERENCED_PARAMETER(pShader);

	box.maxY = 0.55f;
	box.minY = -0.75f;
	box.minX = -0.4f;
	box.maxX = 0.0f;

	float totalW = box.maxX - box.minX;
	float totalH = box.maxY - box.minY;

	m_slotW = totalW;
	m_slotH = totalH / static_cast<float>(MAX_SLOTS);
	m_slotGap = 0.025f;

	m_pSharedMesh = ResourceManager::Instance().GetUIMesh(UIName::TABLE_VERTICAL);
	m_pSharedMiddleMesh = ResourceManager::Instance().GetUIMesh(UIName::DIVIDER_001);

	BuildSlotViews();
	BuildNameTexts();
	BuildCountTexts();

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
			slotViews[i].hitBox->SetUIMesh(m_pSharedMesh);
			slotViews[i].hitBox->SetScale(m_slotW, rowH, 1.0f);
		}

		if (!slotViews[i].iconCell)
		{
			slotViews[i].iconCell = std::make_unique<UIObject>();
			//slotViews[i].iconCell->SetUIMesh(m_pSharedMesh);
			slotViews[i].iconCell->SetScale(iconRenderW, renderH, 1.0f);
		}

		if (!slotViews[i].textCell)
		{
			slotViews[i].textCell = std::make_unique<UIObject>();
			//slotViews[i].textCell->SetUIMesh(m_pSharedMiddleMesh);
			slotViews[i].textCell->SetScale(textRenderW, renderH, 1.0f);
		}

		if (!slotViews[i].countCell)
		{
			slotViews[i].countCell = std::make_unique<UIObject>();
			//slotViews[i].countCell->SetUIMesh(m_pSharedMesh);
			slotViews[i].countCell->SetScale(countRenderW, renderH, 1.0f);
		}
	}
}

void Inventory::BuildNameTexts()
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		m_nameTexts[i] = std::make_unique<UIText>(
			L"",
			XMFLOAT2(0.0f, 0.0f),
			UITextAlign::CENTER
		);

		m_nameTexts[i]->SetColor(
			1.0f,
			1.0f,
			1.0f,
			1.0f
		);

		m_nameTexts[i]->SetVisible(false);
	}
}

void Inventory::LayoutSlotViews()
{
	const float iconW = m_slotW * m_iconRatio;
	const float textW = m_slotW * m_textRatio;
	const float countW = m_slotW * m_countRatio;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		float posY =
			m_baseY +
			(0.5f - (m_slotH * 0.5f) - (i * m_slotH));

		float rowLeft =
			m_baseX -
			(m_slotW * 0.5f);

		float iconCenterX =
			rowLeft +
			(iconW * 0.5f);

		float textCenterX =
			rowLeft +
			iconW +
			(textW * 0.5f);

		float countCenterX =
			rowLeft +
			iconW +
			textW +
			(countW * 0.5f);

		if (slotViews[i].hitBox)
		{
			slotViews[i].hitBox->SetLocate(
				m_baseX,
				posY,
				0.5f
			);

			slotViews[i].hitBox->setAABB();
		}

		if (slotViews[i].iconCell)
		{
			slotViews[i].iconCell->SetLocate(
				iconCenterX,
				posY,
				0.5f
			);

			slotViews[i].iconCell->setAABB();
		}

		if (slotViews[i].textCell)
		{
			slotViews[i].textCell->SetLocate(
				textCenterX,
				posY,
				0.5f
			);

			slotViews[i].textCell->setAABB();
		}

		if (slotViews[i].countCell)
		{
			slotViews[i].countCell->SetLocate(
				countCenterX,
				posY,
				0.5f
			);

			slotViews[i].countCell->setAABB();
		}

		if (m_nameTexts[i])
		{
			XMFLOAT2 namePosition =
				ConvertNdcToPixel(
					textCenterX,
					posY
				);

			namePosition.y -= 16.0f;

			m_nameTexts[i]->SetPosition(
				namePosition
			);
		}

		if (m_countTexts[i])
		{
			XMFLOAT2 countPosition =
				ConvertNdcToPixel(
					countCenterX,
					posY
				);

			countPosition.y -= 16.0f;

			m_countTexts[i]->SetPosition(
				countPosition
			);
		}
	}
}

void Inventory::BuildCountTexts()
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		m_countTexts[i] = std::make_unique<UIText>(
			L"",
			XMFLOAT2(0.0f, 0.0f),
			UITextAlign::CENTER
		);

		m_countTexts[i]->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		m_countTexts[i]->SetVisible(false);
	}
}

void Inventory::UpdateSlotNameText(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= MAX_SLOTS)
		return;

	UIText* pNameText =
		m_nameTexts[slotIndex].get();

	if (!pNameText)
		return;

	const ItemSlot& slot =
		slots[slotIndex];

	if (slot.item == ItemID::NONE ||
		slot.count <= 0)
	{
		pNameText->SetText(L"");
		pNameText->SetVisible(false);
		return;
	}

	const wchar_t* pDisplayName =
		GetItemDisplayName(slot.item);

	if (!pDisplayName ||
		pDisplayName[0] == L'\0')
	{
		pNameText->SetText(L"");
		pNameText->SetVisible(false);
		return;
	}

	pNameText->SetText(pDisplayName);
	pNameText->SetVisible(true);
}

void Inventory::UpdateSlotCountText(int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= MAX_SLOTS)
		return;

	UIText* pCountText = m_countTexts[slotIndex].get();

	if (!pCountText)
		return;

	const ItemSlot& slot = slots[slotIndex];

	if (slot.item == ItemID::NONE || slot.count <= 0)
	{
		pCountText->SetText(L"");
		pCountText->SetVisible(false);
		return;
	}

	pCountText->SetText(std::to_wstring(slot.count));
	pCountText->SetVisible(true);
}

void Inventory::UpdateSlotTexts(int slotIndex)
{
	UpdateSlotNameText(slotIndex);
	UpdateSlotCountText(slotIndex);
}

XMFLOAT2 Inventory::ConvertNdcToPixel(float ndcX, float ndcY)
{
	float pixelX =
		(ndcX + 1.0f) *
		0.5f *
		static_cast<float>(FRAME_BUFFER_WIDTH);

	float pixelY =
		(1.0f - ndcY) *
		0.5f *
		static_cast<float>(FRAME_BUFFER_HEIGHT);

	return XMFLOAT2(pixelX, pixelY);
}

void Inventory::SubmitToShader(UIObjectShader* shader)
{
	if (!isOpen) return;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (slotViews[i].hitBox)
		{
			shader->addObjects(slotViews[i].hitBox.get());
		}
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

void Inventory::SubmitText(TextRenderer* renderer)
{
	if (!renderer)
		return;

	if (!isOpen)
		return;

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		UpdateSlotTexts(i);

		if (m_nameTexts[i])
		{
			renderer->SubmitText(m_nameTexts[i].get());
		}

		if (m_countTexts[i])
		{
			renderer->SubmitText(m_countTexts[i].get());
		}
	}
}

void Inventory::SlotClicked(int slotidx)
{
	/*wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"Slot %d clicked\n", slotidx);
	OutputDebugStringW(debugBuf);*/

	if (false == NetworkManager::Instance().IsConnected()) {
		return;
	}

	if (ID >= 0) {
		NetworkManager::Instance().SendLootPickup(
			static_cast<short>(ID),
			static_cast<short>(slotidx));
	}
	else {
		NetworkManager::Instance().SendInventoryClick(
			INV_ACTION_CLICK,
			static_cast<short>(slotidx));
	}
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
	if (id == ItemID::NONE ||
		cnt <= 0)
	{
		return;
	}

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		ItemSlot& slot =
			slots[i];

		if (slot.item == ItemID::NONE ||
			slot.item != id)
		{
			continue;
		}

		slot.count -= cnt;

		if (slot.count <= 0)
		{
			slot.item = ItemID::NONE;
			slot.count = 0;

			if (slotViews[i].iconCell)
			{
				slotViews[i].iconCell->SetUIMesh(
					nullptr
				);
			}
		}

		UpdateSlotTexts(i);

		break;
	}
}

bool Inventory::AddItem(ItemID item, int count)
{
	if (item == ItemID::NONE ||
		count <= 0)
	{
		return false;
	}

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (slots[i].item != ItemID::NONE &&
			slots[i].item == item)
		{
			slots[i].count += count;

			UpdateSlotTexts(i);

			return true;
		}
	}

	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		if (slots[i].item != ItemID::NONE)
			continue;

		slots[i].item = item;
		slots[i].count = count;

		UIName uiName =
			MapItemIDToUIName(item);

		UIMesh* pMesh =
			ResourceManager::Instance().GetUIMesh(
				uiName
			);

		if (slotViews[i].iconCell)
		{
			slotViews[i].iconCell->SetUIMesh(
				pMesh
			);
		}

		UpdateSlotTexts(i);

		return true;
	}

	return false;
}

void Inventory::ClearItems()
{
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		slots[i].item = ItemID::NONE;
		slots[i].count = 0;

		if (slotViews[i].iconCell)
		{
			slotViews[i].iconCell->SetUIMesh(
				nullptr
			);
		}

		UpdateSlotTexts(i);
	}
}

UIName MapItemIDToUIName(ItemID id)
{
	static const std::unordered_map<ItemID, UIName> itemToUIMap = {
		{ ItemID::MAT_1_FIBER,        UIName::ICON_FIBER },
		{ ItemID::MAT_2_METAL_PLATE,  UIName::ICON_METAL },
		{ ItemID::MAT_3_BOLT_AND_NUT, UIName::ICON_NEEDLE },
		{ ItemID::ESCAPE_KEY, UIName::ICON_KEY },

		{ ItemID::ARMOR_HELMET_01,    UIName::ICON_HELMET_01 },
		{ ItemID::ARMOR_HELMET_02,    UIName::ICON_HELMET_02 },
		{ ItemID::ARMOR_HELMET_03,    UIName::ICON_HELMET_03 },
		{ ItemID::ARMOR_HELMET_04,    UIName::ICON_HELMET_04 },

		// --- 방어구: 바디 ---
		{ ItemID::ARMOR_BODY_01,      UIName::ICON_BODY_01 },
		{ ItemID::ARMOR_BODY_02,      UIName::ICON_BODY_02 },
		{ ItemID::ARMOR_BODY_03,      UIName::ICON_BODY_03 },
		{ ItemID::ARMOR_BODY_04,      UIName::ICON_BODY_04 },

		// --- 방어구: 신발 ---
		{ ItemID::ARMOR_SHOES_01,     UIName::ICON_SHOES_01 },
		{ ItemID::ARMOR_SHOES_02,     UIName::ICON_SHOES_02 },
		{ ItemID::ARMOR_SHOES_03,     UIName::ICON_SHOES_03 },
		{ ItemID::ARMOR_SHOES_04,     UIName::ICON_SHOES_04 }

	};

	auto it = itemToUIMap.find(id);
	if (it != itemToUIMap.end())
	{
		return it->second;
	}

	return UIName::UI_NONE_IMAGE; // 매핑된 아이콘이 없을 경우의 기본 이미지
}

ItemSlot* Inventory::GetSlot(int idx)
{
	if (idx < 0 || idx >= MAX_SLOTS) return nullptr;
	return &slots[idx];
}

bool Inventory::ApplyServerSlotUpdate(
	int slotIndex,
	ItemID itemId,
	int count)
{
	ItemSlot* pSlot =
		GetSlot(slotIndex);

	if (!pSlot)
		return false;

	if (itemId == ItemID::NONE ||
		count <= 0)
	{
		pSlot->item = ItemID::NONE;
		pSlot->count = 0;

		if (slotViews[slotIndex].iconCell)
		{
			slotViews[slotIndex].iconCell->SetUIMesh(
				nullptr
			);
		}

		UpdateSlotTexts(slotIndex);

		return true;
	}

	pSlot->item = itemId;
	pSlot->count = count;

	UIName uiName =
		MapItemIDToUIName(itemId);

	UIMesh* pMesh =
		ResourceManager::Instance().GetUIMesh(
			uiName
		);

	if (slotViews[slotIndex].iconCell)
	{
		slotViews[slotIndex].iconCell->SetUIMesh(
			pMesh
		);
	}

	UpdateSlotTexts(slotIndex);

	return true;
}

EquipUI::EquipUI(CPlayer* p)
{
	player = p->GetEquips();
}

XMFLOAT3 CalcPixelByRatio(float ratio)
{
	float screenAspectRatio = static_cast<float>(FRAME_BUFFER_WIDTH) / static_cast<float>(FRAME_BUFFER_HEIGHT);

	float correctedScaleX = ratio / screenAspectRatio;

	return XMFLOAT3(correctedScaleX, 1.0f, 1.0f);
}
XMFLOAT3 CalcPixelByRatio(int width, int height)
{
	if (height == 0)
		return XMFLOAT3(1.0f, 1.0f, 1.0f);
	float ratio = static_cast<float>(width) / static_cast<float>(height);
	return CalcPixelByRatio(ratio);
}
void EquipUI::Init(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	base = make_unique<UIObject>();
	base->SetLocate(0.5, 0.0, 0.6);
	base->SetScale(0.5f, 1.0f, 1.0f);
	base->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::WINDOW_BASE));

	float BtnSize = 0.15f;
	XMFLOAT3 scale = CalcPixelByRatio(1.0f);
	UIs[ItemType::ARMOR_HELMET] = make_unique<UIObject>();
	UIs[ItemType::ARMOR_HELMET]->SetLocate(0.38, 0.25, 0.5);
	UIs[ItemType::ARMOR_HELMET]->SetScale(scale.x * BtnSize, scale.y * BtnSize, 1.0f);
	UIs[ItemType::ARMOR_HELMET]->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::PANEL_001));
	UIs[ItemType::ARMOR_HELMET]->SetFunc([this]() {
		ItemID i = this->helmet;
		switch (i)
		{
		case ItemID::NONE:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(ItemID::ARMOR_HELMET_01);
			}
			break;
		case ItemID::ARMOR_HELMET_01:
		case ItemID::ARMOR_HELMET_02:
		case ItemID::ARMOR_HELMET_03:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(++i);
			}
			break;
		}

		});
	UIs[ItemType::ARMOR_HELMET]->setAABB();


	UIs[ItemType::ARMOR_BODY] = make_unique<UIObject>();
	UIs[ItemType::ARMOR_BODY]->SetLocate(0.38, 0.08, 0.5);
	UIs[ItemType::ARMOR_BODY]->SetScale(scale.x * BtnSize, scale.y * BtnSize, 1.0f);
	UIs[ItemType::ARMOR_BODY]->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::PANEL_001));
	UIs[ItemType::ARMOR_BODY]->SetFunc([this]() {
		ItemID i = this->body;
		switch (i)
		{
		case ItemID::NONE:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(ItemID::ARMOR_BODY_01);
			}
			break;
		case ItemID::ARMOR_BODY_01:
		case ItemID::ARMOR_BODY_02:
		case ItemID::ARMOR_BODY_03:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(++i);
			}
			break;
		}

		});
	UIs[ItemType::ARMOR_BODY]->setAABB();


	UIs[ItemType::ARMOR_SHOES] = make_unique<UIObject>();
	UIs[ItemType::ARMOR_SHOES]->SetLocate(0.38, -0.09, 0.5);
	UIs[ItemType::ARMOR_SHOES]->SetScale(scale.x * BtnSize, scale.y * BtnSize, 1.0f);
	UIs[ItemType::ARMOR_SHOES]->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::PANEL_001));
	UIs[ItemType::ARMOR_SHOES]->SetFunc([this]() {
		ItemID i = this->shoes;
		switch (i)
		{
		case ItemID::NONE:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(ItemID::ARMOR_SHOES_01);
			}
			break;
		case ItemID::ARMOR_SHOES_01:
		case ItemID::ARMOR_SHOES_02:
		case ItemID::ARMOR_SHOES_03:
			if (NetworkManager::Instance().IsConnected()) {
				NetworkManager::Instance().SendCraftRequest(++i);
			}
			break;
		}

		});
	UIs[ItemType::ARMOR_SHOES]->setAABB();

}

void EquipUI::EquipItem(ItemID item)
{
	if (!player)
		return;

	switch (item)
	{
	case ItemID::ARMOR_HELMET_01:
	case ItemID::ARMOR_HELMET_02:
	case ItemID::ARMOR_HELMET_03:
	case ItemID::ARMOR_HELMET_04:
	{
		UIName n = MapItemIDToUIName(item);

		auto ui = UIs.find(ItemType::ARMOR_HELMET);
		if (ui != UIs.end() && ui->second)
		{
			ui->second->SetUIMesh(ResourceManager::Instance().GetUIMesh(n));
		}

		player->helmet = ArmorItem(ItemType::ARMOR_HELMET, item);
		helmet = item;
		break;
	}

	case ItemID::ARMOR_BODY_01:
	case ItemID::ARMOR_BODY_02:
	case ItemID::ARMOR_BODY_03:
	case ItemID::ARMOR_BODY_04:
	{
		UIName n = MapItemIDToUIName(item);

		auto ui = UIs.find(ItemType::ARMOR_BODY);
		if (ui != UIs.end() && ui->second)
		{
			ui->second->SetUIMesh(ResourceManager::Instance().GetUIMesh(n));
		}

		player->body = ArmorItem(ItemType::ARMOR_BODY, item);
		body = item;
		break;
	}

	case ItemID::ARMOR_SHOES_01:
	case ItemID::ARMOR_SHOES_02:
	case ItemID::ARMOR_SHOES_03:
	case ItemID::ARMOR_SHOES_04:
	{
		UIName n = MapItemIDToUIName(item);

		auto ui = UIs.find(ItemType::ARMOR_SHOES);
		if (ui != UIs.end() && ui->second)
		{
			ui->second->SetUIMesh(ResourceManager::Instance().GetUIMesh(n));
		}

		player->shoes = ArmorItem(ItemType::ARMOR_SHOES, item);
		shoes = item;
		break;
	}

	case ItemID::ARMOR_PLATE:
	{
		player->plate = Plate(ItemType::PLATE, item);
		plate = item;
		break;
	}

	default:
		break;
	}
}

void EquipUI::ResetForNewRound()
{
	if (player)
	{
		player->ResetForNewRound();
	}

	helmet = ItemID::NONE;
	body = ItemID::NONE;
	shoes = ItemID::NONE;
	plate = ItemID::NONE;

	UIMesh* pEmptyMesh = ResourceManager::Instance().GetUIMesh(UIName::PANEL_001);

	auto helmetUI = UIs.find(ItemType::ARMOR_HELMET);
	if (helmetUI != UIs.end() && helmetUI->second)
	{
		helmetUI->second->SetUIMesh(pEmptyMesh);
	}

	auto bodyUI = UIs.find(ItemType::ARMOR_BODY);
	if (bodyUI != UIs.end() && bodyUI->second)
	{
		bodyUI->second->SetUIMesh(pEmptyMesh);
	}

	auto shoesUI = UIs.find(ItemType::ARMOR_SHOES);
	if (shoesUI != UIs.end() && shoesUI->second)
	{
		shoesUI->second->SetUIMesh(pEmptyMesh);
	}

	isOpen = false;

	OutputDebugString(L"[RoundReset] EquipUI 초기화 완료\n");
}

bool EquipUI::ProcessClick(POINT mouse)
{
	for (auto& p : UIs)
	{
		if (p.second->GetBox().Intersects(mouse))
		{
			p.second->HandleClick();
		}
	}
}

void EquipUI::SubmitToShader(UIObjectShader* shader)
{
	if (not isOpen)return;
	shader->addObjects(base.get());
	for (auto& p : UIs)
	{
		shader->addObjects(p.second.get());
	}

}


PlayerStatus::PlayerStatus(CPlayer* p)
{
	player = p;
	FullHp = p->GetHP();
	hp = p->GetHP();
	isOpen = true;
}
void PlayerStatus::Init(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	Rifle = ResourceManager::Instance().GetUIMesh(UIName::STATUS_RIFLE_BULLET);
	SMG = ResourceManager::Instance().GetUIMesh(UIName::STATUS_SMG_BULLET);
	Shotgun = ResourceManager::Instance().GetUIMesh(UIName::STATUS_SHOTGUN_BULLET);
	Pistol = ResourceManager::Instance().GetUIMesh(UIName::STATUS_PISTOL_BULLET);
	bullet = ResourceManager::Instance().GetUIMesh(UIName::STATUS_BULLET_DOT);

	curammo = player ? player->GetCurrentAmmo() : 0;

	UIs[HP_BASE] = make_unique<UIObject>();
	UIs[HP_BASE]->SetScale(0.4f, 0.07f, 1.0f);
	UIs[HP_BASE]->SetLocate(0.0f, -0.9f, 0.5f);
	UIs[HP_BASE]->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::STATUS_HEALTH_BAR));

	constexpr float HEALTH_TEXTURE_VISIBLE_RATIO_X = 484.0f / 1086.0f;
	constexpr float HEALTH_BAR_FULL_WIDTH = 0.4f;
	constexpr float HEALTH_DOT_VISIBLE_WIDTH = HEALTH_BAR_FULL_WIDTH * 0.97f;
	constexpr float HEALTH_MAIN_FULL_SCALE_X = HEALTH_DOT_VISIBLE_WIDTH / HEALTH_TEXTURE_VISIBLE_RATIO_X;

	constexpr float HEALTH_TEXTURE_VISIBLE_CENTER_X = ((294.0f + 778.0f) * 0.5f) / 1086.0f;
	constexpr float HEALTH_MAIN_CENTER_OFFSET_X =
		-HEALTH_MAIN_FULL_SCALE_X * (HEALTH_TEXTURE_VISIBLE_CENTER_X - 0.5f);

	UIs[HP_MAIN] = make_unique<UIObject>();
	UIs[HP_MAIN]->SetScale(HEALTH_MAIN_FULL_SCALE_X, 0.025f, 1.0f);
	UIs[HP_MAIN]->SetLocate(HEALTH_MAIN_CENTER_OFFSET_X, -0.9f, 0.5f);
	UIs[HP_MAIN]->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::STATUS_HEALTH_DOT));

	UIs[MAG_BASE] = make_unique<UIObject>();
	XMFLOAT3 r = CalcPixelByRatio(1.0f);
	float wr = 0.15f;
	UIs[MAG_BASE]->SetScale(r.x * wr, r.y * wr, 1.0f);
	UIs[MAG_BASE]->SetLocate(0.25f, -0.9f, 0.5f);
	UIs[MAG_BASE]->SetUIMesh(Rifle);

	XMFLOAT3 br = CalcPixelByRatio(30, 58);
	float bwr = 0.05f;

	Bullets.clear();
	Bullets.reserve(35);

	for (int i = 0; i < 35; ++i)
	{
		UIObject* t = new UIObject();
		t->SetScale(br.x * bwr, br.y * bwr, 1.0f);
		t->SetLocate(0.3f + i * br.x * bwr, -0.9f, 0.5f);
		t->SetUIMesh(bullet);
		Bullets.push_back(unique_ptr<UIObject>(t));
	}

	float prb = 0.05f;

	ProgressBar.clear();
	ProgressBar.reserve(10);

	for (int i = 0; i < 10; ++i)
	{
		UIObject* t = new UIObject();
		t->SetScale(br.x * prb, br.y * prb, 1.0f);
		t->SetLocate(i * br.x * prb - 0.1f, -0.2f, 0.5f);
		t->SetUIMesh(ResourceManager::Instance().GetUIMesh(UIName::STATUS_HEALTH_DOT));
		ProgressBar.push_back(unique_ptr<UIObject>(t));
	}

	Update(0.0f);
}

void PlayerStatus::ResetForNewRound(short fullHp)
{
	FullHp = (fullHp > 0) ? fullHp : 100;
	hp = player ? player->GetHP() : FullHp;
	curammo = player ? player->GetCurrentAmmo() : 0;

	Escape = false;
	EscapeTime = 0.0f;
	isOpen = true;

	Update(0.0f);

	OutputDebugString(L"[RoundReset] PlayerStatus 초기화 완료\n");
}

bool PlayerStatus::ProcessClick(POINT mouse)
{
	for (auto& p : UIs)
	{
		if (p.second->GetBox().Intersects(mouse))
		{
			p.second->HandleClick();
		}
	}
}
void PlayerStatus::SubmitToShader(UIObjectShader* shader)
{
	for (auto& p : UIs)
	{
		shader->addObjects(p.second.get());
	}
	for (int i = 0; i < curammo; ++i)
	{
		shader->addObjects(Bullets[i].get());
	}
	int idx = (int)EscapeTime;
	idx = min(idx, 10);
	//OutputDebugStringW(to_wstring(idx).c_str());
	for (int i = 0; i < idx; ++i)
	{
		shader->addObjects(ProgressBar[i].get());
	}
}

void PlayerStatus::Update(float fTimeElapsed)
{
	if (!player) return;

	hp = player->GetHP();

	if (FullHp <= 0)
	{
		FullHp = 100;
	}

	float hpRatio = static_cast<float>(hp) / static_cast<float>(FullHp);
	hpRatio = std::clamp(hpRatio, 0.0f, 1.0f);

	constexpr float HEALTH_BAR_FULL_WIDTH = 0.4f;
	constexpr float HEALTH_BAR_CENTER_X = 0.0f;
	constexpr float HEALTH_BAR_CENTER_Y = -0.9f;

	constexpr float HEALTH_DOT_VISIBLE_WIDTH = HEALTH_BAR_FULL_WIDTH * 0.83f;
	constexpr float HEALTH_DOT_LEFT_X = HEALTH_BAR_CENTER_X - HEALTH_DOT_VISIBLE_WIDTH * 0.5f;

	constexpr float TEXTURE_WIDTH = 1086.0f;
	constexpr float VISIBLE_LEFT_PIXEL = 294.0f;
	constexpr float VISIBLE_RIGHT_PIXEL = 778.0f;

	constexpr float VISIBLE_LEFT_U = VISIBLE_LEFT_PIXEL / TEXTURE_WIDTH;
	constexpr float VISIBLE_RIGHT_U = VISIBLE_RIGHT_PIXEL / TEXTURE_WIDTH;
	constexpr float VISIBLE_WIDTH_RATIO = VISIBLE_RIGHT_U - VISIBLE_LEFT_U;
	constexpr float VISIBLE_CENTER_U = (VISIBLE_LEFT_U + VISIBLE_RIGHT_U) * 0.5f;

	const float visibleWidth = HEALTH_DOT_VISIBLE_WIDTH * hpRatio;

	const float objectScaleX = (hpRatio > 0.0f) ? visibleWidth / VISIBLE_WIDTH_RATIO : 0.0f;

	const float desiredVisibleCenterX = HEALTH_DOT_LEFT_X + visibleWidth * 0.5f;

	const float visibleCenterOffsetInObject = objectScaleX * (VISIBLE_CENTER_U - 0.5f);
	const float objectCenterX = desiredVisibleCenterX - visibleCenterOffsetInObject;

	auto hpMain = UIs.find(HP_MAIN);

	if (hpMain != UIs.end() && hpMain->second)
	{
		constexpr float HEALTH_DOT_SCALE_Y = 0.025f;

		hpMain->second->SetScale(objectScaleX, HEALTH_DOT_SCALE_Y, 1.0f);
		hpMain->second->SetLocate(objectCenterX, HEALTH_BAR_CENTER_Y, 0.5f);
	}

	curammo = player->GetCurrentAmmo();
	curammo = std::clamp(curammo, 0, static_cast<int>(Bullets.size()));

	// 탈출 진행도
	if (Escape)
	{
		EscapeTime += fTimeElapsed;
	}
}

void HUDManager::SubmitToShader(UIObjectShader* shader)
{
	for (const auto& o : objs)
	{
		if (o->isopen)
			shader->addObjects(o.get());
	}
	for (const auto& o : panels)
	{
		o->SubmitToShader(shader);
	}
}

void HUDManager::Release()
{
	texts.clear();
	objs.clear();
	panels.clear();
}

void HUDManager::Update(float fTimeElapsed)
{
	for (auto& o : panels)
	{
		o->Update(fTimeElapsed);
	}
}


void HUDManager::CloseUI(int idx)
{
	if (idx >= objs.size())return;
	objs[idx]->isopen = false;
}

bool HUDManager::ProcessClick(POINT mouse)
{
	for (auto& o : objs)
	{
		if (o->GetBox().Intersects(mouse))
		{
			o->HandleClick();
			return true;
		}
	}
	for (auto& o : panels)
	{
		if (o->isOpen)
		{
			o->ProcessClick(mouse);

		}
	}
	return false;
}

UIText* HUDManager::AddText(unique_ptr<UIText> text)
{
	if (!text)
		return nullptr;

	UIText* pText = text.get();

	texts.push_back(std::move(text));

	return pText;
}

void HUDManager::RemoveText(UIText* text)
{
	if (!text)
		return;

	auto removeBegin = std::remove_if(
		texts.begin(),
		texts.end(),
		[text](const unique_ptr<UIText>& ownedText)
		{
			return ownedText.get() == text;
		}
	);

	texts.erase(removeBegin, texts.end());
}

void HUDManager::ClearTexts()
{
	texts.clear();
}

void HUDManager::SubmitText(TextRenderer* renderer)
{
	if (!renderer)
		return;

	for (const auto& text : texts)
	{
		if (!text)
			continue;

		renderer->SubmitText(text.get());
	}
}