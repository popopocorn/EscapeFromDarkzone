
#include"stdafx.h"
#include "UI.h"
#include"Scene.h"

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
	m_pd3dPositionBuffer->SetName(L"fdsa");
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

	if (m_pd3dPositionUploadBuffer)
	{
		m_pd3dPositionUploadBuffer->Release();
		m_pd3dPositionUploadBuffer = nullptr;
	}

	if (UVUploadBuffer)
	{
		UVUploadBuffer->Release();
		UVUploadBuffer = nullptr;
	}
	texture->Release();
}

void UIMesh::ReleaseUploadBuffers()
{
}

void UIMesh::LoadTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName)
{
	texture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
	texture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, pszFileName, RESOURCE_TEXTURE2D, 0);
	MainScene::CreateShaderResourceViews(pd3dDevice, texture, 0, 3);
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
	XMMATRIX mtxScale = XMMatrixScaling(x, y, z);
	world = Matrix4x4::Multiply(mtxScale, world);

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
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&world)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
	object->Render(pd3dCommandList, 0, 1);
}

