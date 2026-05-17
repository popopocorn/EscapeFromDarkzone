#pragma once
#include "stdafx.h"
#include "Shader.h"

class CDebugObject
{
public:
	CDebugObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, bool bSolid = false);
	~CDebugObject();

	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

private:
	bool m_bSolid = false;

	ID3D12Resource* m_pd3dPositionBuffer = nullptr;
	ID3D12Resource* m_pd3dPositionUploadBuffer = nullptr;
	ID3D12Resource* m_pd3dIndexBuffer = nullptr;
	ID3D12Resource* m_pd3dIndexUploadBuffer = nullptr;

	D3D12_VERTEX_BUFFER_VIEW m_d3dPositionBufferView{};
	D3D12_INDEX_BUFFER_VIEW m_d3dIndexBufferView{};

	int m_nIndices = 0;
};