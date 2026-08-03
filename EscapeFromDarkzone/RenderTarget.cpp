#include "RenderTarget.h"

RenderTarget::RenderTarget()
{ 

}

RenderTarget::~RenderTarget()
{
	Release();
}

bool RenderTarget::CreateRenderTarget(ID3D12Device * pd3dDevice, UINT nWidth, UINT nHeight, DXGI_FORMAT dxgiFormat, D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvHandle, const float pfClearColor[4], D3D12_RESOURCE_STATES d3dInitState)
{
    Release();
    m_dxgiFormat = dxgiFormat;
    m_nWidth = nWidth;
    m_nHeight = nHeight;
    if (pfClearColor)
    {
        for (int i = 0; i < 4; ++i)
        {
            m_pfClearColor[i] = pfClearColor[i];
        }
    }
	D3D12_HEAP_PROPERTIES d3dHeapProperties{};
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc{};
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = nWidth;
	d3dResourceDesc.Height = nHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = dxgiFormat;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE d3dClearValue{};
	d3dClearValue.Format = dxgiFormat;
	d3dClearValue.Color[0] = m_pfClearColor[0];
	d3dClearValue.Color[1] = m_pfClearColor[1];
	d3dClearValue.Color[2] = m_pfClearColor[2];
	d3dClearValue.Color[3] = m_pfClearColor[3];

	HRESULT hResult = pd3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		d3dInitState,
		&d3dClearValue,
		IID_PPV_ARGS(&m_pd3dResource)
	);

	if (FAILED(hResult) || !m_pd3dResource)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[RenderTarget] CreateCommittedResource failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hResult));
		OutputDebugStringW(debugText);
		return false;
	}

	m_d3dCurrentState = d3dInitState;

	pd3dDevice->CreateRenderTargetView(m_pd3dResource, nullptr, d3dRtvHandle);
	m_d3dRtvHandle = d3dRtvHandle;

	return true;

}

bool RenderTarget::CreateSRV(ID3D12Device* pd3dDevice, D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUSrvHandle, D3D12_GPU_DESCRIPTOR_HANDLE d3dGPUSrvHandle)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC d3dSrvDesc{};
	d3dSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	d3dSrvDesc.Format = m_dxgiFormat;
	d3dSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	d3dSrvDesc.Texture2D.MostDetailedMip = 0;
	d3dSrvDesc.Texture2D.MipLevels = 1;
	d3dSrvDesc.Texture2D.PlaneSlice = 0;
	d3dSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	pd3dDevice->CreateShaderResourceView(m_pd3dResource, &d3dSrvDesc, d3dCPUSrvHandle);

	m_d3dCpuSrvHandle = d3dCPUSrvHandle;
	m_d3dGpuSrvHandle = d3dGPUSrvHandle;
	m_bHasSRV = true;

	return true;
}

void RenderTarget::Release()
{
	if (m_pd3dResource)
	{
		m_pd3dResource->Release();
		m_pd3dResource = nullptr;
	}

	m_d3dRtvHandle = D3D12_CPU_DESCRIPTOR_HANDLE{};
	m_d3dCpuSrvHandle = D3D12_CPU_DESCRIPTOR_HANDLE{};
	m_d3dGpuSrvHandle = D3D12_GPU_DESCRIPTOR_HANDLE{};
	m_bHasSRV = false;

	m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
	m_dxgiFormat = DXGI_FORMAT_UNKNOWN;
	m_nWidth = 0;
	m_nHeight = 0;
}

void RenderTarget::TransitionTo(ID3D12GraphicsCommandList * pd3dCommandList, D3D12_RESOURCE_STATES d3dTargetState)
{
	if (!pd3dCommandList || !m_pd3dResource)
		return;

	if (m_d3dCurrentState == d3dTargetState)
		return;

	D3D12_RESOURCE_BARRIER d3dResourceBarrier{};
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = m_pd3dResource;
	d3dResourceBarrier.Transition.StateBefore = m_d3dCurrentState;
	d3dResourceBarrier.Transition.StateAfter = d3dTargetState;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	m_d3dCurrentState = d3dTargetState;
}

void RenderTarget::Clear(ID3D12GraphicsCommandList * pd3dCommandList)
{
	if (!pd3dCommandList || !m_pd3dResource)
		return;

	if (m_d3dCurrentState != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		OutputDebugStringW(L"[RenderTarget] Clear called while not in RENDER_TARGET state.\n");
		return;
	}

	pd3dCommandList->ClearRenderTargetView(m_d3dRtvHandle, m_pfClearColor.data(), 0, nullptr);

}

// RenderTarget.cpp
RenderTarget::RenderTarget(RenderTarget&& other) 
	: m_pd3dResource(other.m_pd3dResource)
	, m_d3dRtvHandle(other.m_d3dRtvHandle)
	, m_d3dCpuSrvHandle(other.m_d3dCpuSrvHandle)
	, m_d3dGpuSrvHandle(other.m_d3dGpuSrvHandle)
	, m_bHasSRV(other.m_bHasSRV)
	, m_d3dCurrentState(other.m_d3dCurrentState)
	, m_dxgiFormat(other.m_dxgiFormat)
	, m_nWidth(other.m_nWidth)
	, m_nHeight(other.m_nHeight)
	, m_pfClearColor(other.m_pfClearColor)
{
	other.m_pd3dResource = nullptr; 
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) 
{
	if (this == &other) return *this;

	Release();

	m_pd3dResource = other.m_pd3dResource;
	m_d3dRtvHandle = other.m_d3dRtvHandle;
	m_d3dCpuSrvHandle = other.m_d3dCpuSrvHandle;
	m_d3dGpuSrvHandle = other.m_d3dGpuSrvHandle;
	m_bHasSRV = other.m_bHasSRV;
	m_d3dCurrentState = other.m_d3dCurrentState;
	m_dxgiFormat = other.m_dxgiFormat;
	m_nWidth = other.m_nWidth;
	m_nHeight = other.m_nHeight;
	m_pfClearColor = other.m_pfClearColor;

	other.m_pd3dResource = nullptr;
	return *this;
}