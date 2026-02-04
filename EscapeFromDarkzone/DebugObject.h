#pragma once
#include "stdafx.h"
#include "Shader.h"

class CDebugObject
{
public:
	CDebugObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	~CDebugObject();

	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

private:
	ID3D12Resource* m_pd3dPositionBuffer = nullptr;
	ID3D12Resource* m_pd3dPositionUploadBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW	m_d3dPositionBufferView;

	ID3D12Resource* m_pd3dIndexBuffer = nullptr;
	ID3D12Resource* m_pd3dIndexUploadBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW		m_d3dIndexBufferView;

	UINT						m_nIndices = 0;
};