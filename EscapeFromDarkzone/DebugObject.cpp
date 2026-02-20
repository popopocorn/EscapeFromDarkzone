#include "stdafx.h"
#include "DebugObject.h"

CDebugObject::CDebugObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT3 pxmf3Positions[8];
	float h = 0.5f;
	pxmf3Positions[0] = XMFLOAT3(-h, +h, -h);
	pxmf3Positions[1] = XMFLOAT3(+h, +h, -h);
	pxmf3Positions[2] = XMFLOAT3(+h, +h, +h);
	pxmf3Positions[3] = XMFLOAT3(-h, +h, +h);
	pxmf3Positions[4] = XMFLOAT3(-h, -h, -h);
	pxmf3Positions[5] = XMFLOAT3(+h, -h, -h);
	pxmf3Positions[6] = XMFLOAT3(+h, -h, +h);
	pxmf3Positions[7] = XMFLOAT3(-h, -h, +h);

	m_pd3dPositionBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pxmf3Positions, sizeof(XMFLOAT3) * 8, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * 8;

	m_nIndices = 24;
	UINT pIndices[] = {
		0,1, 1,2, 2,3, 3,0, 
		4,5, 5,6, 6,7, 7,4, 
		0,4, 1,5, 2,6, 3,7
	};

	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pIndices, sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

CDebugObject::~CDebugObject()
{
	if (m_pd3dPositionBuffer) m_pd3dPositionBuffer->Release();
	if (m_pd3dPositionUploadBuffer) m_pd3dPositionUploadBuffer->Release();
	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer->Release();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
}

void CDebugObject::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->IASetVertexBuffers(0, 1, &m_d3dPositionBufferView);
	pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);

	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
}