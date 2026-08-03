#pragma once
#include"stdafx.h"
class RenderTarget
{
public:
	RenderTarget();
	~RenderTarget();

	bool CreateRenderTarget(
		ID3D12Device* pd3dDevice,
		UINT nWidth,
		UINT nHeight,
		DXGI_FORMAT dxgiFormat,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvHandle,
		const float pfClearColor[4],
		D3D12_RESOURCE_STATES d3dInitState = D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	bool CreateSRV(
		ID3D12Device* pd3dDevice,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUSrvHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE d3dGPUSrvHandle
	);
	void Release();
	void TransitionTo(ID3D12GraphicsCommandList* pd3dCommandList, D3D12_RESOURCE_STATES d3dTargetState);

	void Clear(ID3D12GraphicsCommandList* pd3dCommandList);


public:
	ID3D12Resource* GetResource() const { return m_pd3dResource; }
	D3D12_CPU_DESCRIPTOR_HANDLE  GetRTV() const { return m_d3dRtvHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE  GetSRV() const { return m_d3dGpuSrvHandle; }
	D3D12_RESOURCE_STATES        GetCurrentState() const { return m_d3dCurrentState; }
	DXGI_FORMAT                  GetFormat() const { return m_dxgiFormat; }
	UINT                         GetWidth() const { return m_nWidth; }
	UINT                         GetHeight() const { return m_nHeight; }
	bool                         HasSRV() const { return m_bHasSRV; }

private:
	ID3D12Resource*					m_pd3dResource = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE		m_d3dRtvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE		m_d3dCpuSrvHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dGpuSrvHandle{};
	bool							m_bHasSRV = false;

	D3D12_RESOURCE_STATES			m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
	DXGI_FORMAT						m_dxgiFormat = DXGI_FORMAT_UNKNOWN;
	UINT							m_nWidth = 0;
	UINT							m_nHeight = 0;

	array<float,4>					m_pfClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
public:
	RenderTarget(RenderTarget&& other);
	RenderTarget& operator=(RenderTarget&& other);
private:
	RenderTarget(const RenderTarget&) = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;
};

