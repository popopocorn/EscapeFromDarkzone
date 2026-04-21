#include"stdafx.h"
#include"UI.h"
#include "Item.h"
#include"Shader.h"

Inventory::Inventory(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* pShader)
{
	
	box.maxY = 0.5;
	box.minY = -0.5;
	box.minX = -0.5;
	box.maxX = 0.0;

	float totalW = box.maxX - box.minX;
	float totalH = box.maxY - box.minY;

	// 2. 개별 슬롯의 크기 결정 (리스트 형태이므로 가로는 전체 너비)
	float slotW = totalW;
	float slotH = totalH / static_cast<float>(MAX_SLOTS);
	float posX = box.minX + (totalW * 0.5f);
	float gap = 0.025;
	//CGameObject* uibase = CGameObject::LoadGeometryModelByName(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, NULL, "Model/pannel.bin", pShader, 0);
	UIMesh* m = new UIMesh(pd3dDevice, pd3dCommandList);
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		float posY = box.maxY - (slotH * 0.5f) - (i * slotH);
		if (not slots[i].ui)
		{
			slots[i].ui = new UIObject();
		}
		auto* ui = slots[i].ui;
		ui->SetFunc([this, i]() {
			this->SlotClicked(i);
			});
		ui->SetUIMesh(m);
		ui->SetScale(slotW, slotH - gap, 1.0f);
		ui->SetLocate(posX, posY, 0.5f);
		ui->setAABB();

	}
	
}

void Inventory::SubmitToShader(UIObjectShader* shader)
{
	if (not isOpen)return;
	for (int i = 0; i < MAX_SLOTS; ++i)
	{
		shader->addObjects(slots[i].ui);
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
		auto* ui = slots[i].ui;
		if (ui->GetBox().Intersects(mouse))
		{
			ui->HandleClick();
		}
	}


}
