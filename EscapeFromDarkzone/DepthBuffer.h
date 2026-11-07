#pragma once
#include "stdafx.h"
class DepthBuffer
{
public:
	DepthBuffer();
	~DepthBuffer();

	
	bool CreateDepthBuffer(
		ID3D12Device* pd3dDevice,
		UINT nWidth,
		UINT nHeight,
		DXGI_FORMAT dsvFormat,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvHandle,
		float fClearDepth = 1.0f,
		UINT8 nClearStencil = 0,
		D3D12_RESOURCE_STATES d3dInitState = D3D12_RESOURCE_STATE_DEPTH_WRITE
	);

	bool CreateSRV(
		ID3D12Device* pd3dDevice,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUSrvHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE d3dGPUSrvHandle
	);

	void TransitionTo(ID3D12GraphicsCommandList* pd3dCommandList, D3D12_RESOURCE_STATES d3dTargetState);

	void Clear(ID3D12GraphicsCommandList* pd3dCommandList, D3D12_CLEAR_FLAGS d3dClearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
	void Release();

public:
	ID3D12Resource*                     GetResource() const { return m_pd3dResource; }
	const D3D12_CPU_DESCRIPTOR_HANDLE&  GetDSV() const { return m_d3dDsvHandle; }	// OMSetRenderTargets(..., &GetDSV())
	D3D12_GPU_DESCRIPTOR_HANDLE         GetSRV() const { return m_d3dGpuSrvHandle; }
	D3D12_RESOURCE_STATES               GetCurrentState() const { return m_d3dCurrentState; }
	DXGI_FORMAT                         GetResourceFormat() const { return m_dxgiResourceFormat; }
	DXGI_FORMAT                         GetDSVFormat() const { return m_dxgiDsvFormat; }
	DXGI_FORMAT                         GetSRVFormat() const { return m_dxgiSrvFormat; }
	UINT                                GetWidth() const { return m_nWidth; }
	UINT                                GetHeight() const { return m_nHeight; }
	bool                                HasSRV() const { return m_bHasSRV; }

private:
	
	static bool GetDepthFormats(DXGI_FORMAT dsvFormat, DXGI_FORMAT& outResourceFormat, DXGI_FORMAT& outSrvFormat);

private:
	ID3D12Resource*					m_pd3dResource = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE		m_d3dDsvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE		m_d3dCpuSrvHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dGpuSrvHandle{};
	bool							m_bHasSRV = false;

	D3D12_RESOURCE_STATES			m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
	DXGI_FORMAT						m_dxgiResourceFormat = DXGI_FORMAT_UNKNOWN;	// R24G8_TYPELESS
	DXGI_FORMAT						m_dxgiDsvFormat = DXGI_FORMAT_UNKNOWN;		// D24_UNORM_S8_UINT
	DXGI_FORMAT						m_dxgiSrvFormat = DXGI_FORMAT_UNKNOWN;		// R24_UNORM_X8_TYPELESS
	UINT							m_nWidth = 0;
	UINT							m_nHeight = 0;

	float							m_fClearDepth = 1.0f;
	UINT8							m_nClearStencil = 0;

public:
	DepthBuffer(DepthBuffer&& other);
	DepthBuffer& operator=(DepthBuffer&& other);
private:
	DepthBuffer(const DepthBuffer&) = delete;
	DepthBuffer& operator=(const DepthBuffer&) = delete;
};
