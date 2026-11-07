#include "DepthBuffer.h"

DepthBuffer::DepthBuffer()
{}

DepthBuffer::~DepthBuffer()
{
    Release();
}

bool DepthBuffer::CreateDepthBuffer(ID3D12Device * pd3dDevice, UINT nWidth, UINT nHeight, DXGI_FORMAT dsvFormat, D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvHandle, float fClearDepth, UINT8 nClearStencil, D3D12_RESOURCE_STATES d3dInitState)
{
    Release();

    DXGI_FORMAT dxgiResourceFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dxgiSrvFormat = DXGI_FORMAT_UNKNOWN;
    if (!pd3dDevice || !GetDepthFormats(dsvFormat, dxgiResourceFormat, dxgiSrvFormat))
    {
        OutputDebugStringW(L"[DepthBuffer] Unsupported depth format or null device.\n");
        return false;
    }

    m_dxgiResourceFormat = dxgiResourceFormat;
    m_dxgiDsvFormat = dsvFormat;
    m_dxgiSrvFormat = dxgiSrvFormat;
    m_nWidth = nWidth;
    m_nHeight = nHeight;
    m_fClearDepth = fClearDepth;
    m_nClearStencil = nClearStencil;

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
    d3dResourceDesc.Format = m_dxgiResourceFormat;     // typeless : DSV와 SRV를 둘 다 만들기 위함
    d3dResourceDesc.SampleDesc.Count = 1;              // MSAA면 Texture2D SRV 불가
    d3dResourceDesc.SampleDesc.Quality = 0;
    d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DENY_SHADER_RESOURCE 넣지 말 것

    D3D12_CLEAR_VALUE d3dClearValue{};
    d3dClearValue.Format = m_dxgiDsvFormat;
    d3dClearValue.DepthStencil.Depth = m_fClearDepth;
    d3dClearValue.DepthStencil.Stencil = m_nClearStencil;

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
        swprintf_s(debugText, L"[DepthBuffer] CreateCommittedResource failed. HRESULT=0x%08X\n", static_cast<unsigned int>(hResult));
        OutputDebugStringW(debugText);
        m_pd3dResource = nullptr;
        return false;
    }

    m_d3dCurrentState = d3dInitState;

    // 리소스가 typeless라 desc를 반드시 명시해야 함 (nullptr이면 실패)
    D3D12_DEPTH_STENCIL_VIEW_DESC d3dDsvDesc{};
    d3dDsvDesc.Format = m_dxgiDsvFormat;
    d3dDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    d3dDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    d3dDsvDesc.Texture2D.MipSlice = 0;

    pd3dDevice->CreateDepthStencilView(m_pd3dResource, &d3dDsvDesc, d3dDsvHandle);
    m_d3dDsvHandle = d3dDsvHandle;

    return true;
}

bool DepthBuffer::CreateSRV(ID3D12Device* pd3dDevice, D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUSrvHandle, D3D12_GPU_DESCRIPTOR_HANDLE d3dGPUSrvHandle)
{
    if (!pd3dDevice || !m_pd3dResource)
    {
        OutputDebugStringW(L"[DepthBuffer] CreateSRV called before CreateDepthBuffer.\n");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC d3dSrvDesc{};
    d3dSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d3dSrvDesc.Format = m_dxgiSrvFormat;               // 깊이 plane만 읽음 (R24_UNORM_X8_TYPELESS)
    d3dSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    d3dSrvDesc.Texture2D.MostDetailedMip = 0;
    d3dSrvDesc.Texture2D.MipLevels = 1;
    d3dSrvDesc.Texture2D.PlaneSlice = 0;               // 0 = depth, 스텐실은 plane 1 (X24_TYPELESS_G8_UINT)
    d3dSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    pd3dDevice->CreateShaderResourceView(m_pd3dResource, &d3dSrvDesc, d3dCPUSrvHandle);

    m_d3dCpuSrvHandle = d3dCPUSrvHandle;
    m_d3dGpuSrvHandle = d3dGPUSrvHandle;
    m_bHasSRV = true;

    return true;
}

void DepthBuffer::TransitionTo(ID3D12GraphicsCommandList* pd3dCommandList, D3D12_RESOURCE_STATES d3dTargetState)
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
    d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // depth + stencil plane 함께

    pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

    m_d3dCurrentState = d3dTargetState;
}

void DepthBuffer::Clear(ID3D12GraphicsCommandList * pd3dCommandList, D3D12_CLEAR_FLAGS d3dClearFlags)
{
    if (!pd3dCommandList || !m_pd3dResource)
        return;

    if (m_d3dCurrentState != D3D12_RESOURCE_STATE_DEPTH_WRITE)
    {
        OutputDebugStringW(L"[DepthBuffer] Clear called while not in DEPTH_WRITE state.\n");
        return;
    }

    pd3dCommandList->ClearDepthStencilView(m_d3dDsvHandle, d3dClearFlags, m_fClearDepth, m_nClearStencil, 0, nullptr);
}

void DepthBuffer::Release()
{
    if (m_pd3dResource)
    {
        m_pd3dResource->Release();
        m_pd3dResource = nullptr;
    }

    m_d3dDsvHandle = D3D12_CPU_DESCRIPTOR_HANDLE{};
    m_d3dCpuSrvHandle = D3D12_CPU_DESCRIPTOR_HANDLE{};
    m_d3dGpuSrvHandle = D3D12_GPU_DESCRIPTOR_HANDLE{};
    m_bHasSRV = false;

    m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
    m_dxgiResourceFormat = DXGI_FORMAT_UNKNOWN;
    m_dxgiDsvFormat = DXGI_FORMAT_UNKNOWN;
    m_dxgiSrvFormat = DXGI_FORMAT_UNKNOWN;
    m_nWidth = 0;
    m_nHeight = 0;
}

bool DepthBuffer::GetDepthFormats(DXGI_FORMAT dsvFormat, DXGI_FORMAT & outResourceFormat, DXGI_FORMAT & outSrvFormat)
{
    switch (dsvFormat)
    {
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        outResourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
        outSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        return true;
    case DXGI_FORMAT_D32_FLOAT:
        outResourceFormat = DXGI_FORMAT_R32_TYPELESS;
        outSrvFormat = DXGI_FORMAT_R32_FLOAT;
        return true;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        outResourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
        outSrvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        return true;
    case DXGI_FORMAT_D16_UNORM:
        outResourceFormat = DXGI_FORMAT_R16_TYPELESS;
        outSrvFormat = DXGI_FORMAT_R16_UNORM;
        return true;
    default:
        outResourceFormat = DXGI_FORMAT_UNKNOWN;
        outSrvFormat = DXGI_FORMAT_UNKNOWN;
        return false;
    }
}

DepthBuffer::DepthBuffer(DepthBuffer&& other)
    : m_pd3dResource(other.m_pd3dResource)
    , m_d3dDsvHandle(other.m_d3dDsvHandle)
    , m_d3dCpuSrvHandle(other.m_d3dCpuSrvHandle)
    , m_d3dGpuSrvHandle(other.m_d3dGpuSrvHandle)
    , m_bHasSRV(other.m_bHasSRV)
    , m_d3dCurrentState(other.m_d3dCurrentState)
    , m_dxgiResourceFormat(other.m_dxgiResourceFormat)
    , m_dxgiDsvFormat(other.m_dxgiDsvFormat)
    , m_dxgiSrvFormat(other.m_dxgiSrvFormat)
    , m_nWidth(other.m_nWidth)
    , m_nHeight(other.m_nHeight)
    , m_fClearDepth(other.m_fClearDepth)
    , m_nClearStencil(other.m_nClearStencil)
{
    other.m_pd3dResource = nullptr;
    other.Release();
}

DepthBuffer& DepthBuffer::operator=(DepthBuffer && other)
{
    if (this == &other) return *this;

    Release();

    m_pd3dResource = other.m_pd3dResource;
    m_d3dDsvHandle = other.m_d3dDsvHandle;
    m_d3dCpuSrvHandle = other.m_d3dCpuSrvHandle;
    m_d3dGpuSrvHandle = other.m_d3dGpuSrvHandle;
    m_bHasSRV = other.m_bHasSRV;
    m_d3dCurrentState = other.m_d3dCurrentState;
    m_dxgiResourceFormat = other.m_dxgiResourceFormat;
    m_dxgiDsvFormat = other.m_dxgiDsvFormat;
    m_dxgiSrvFormat = other.m_dxgiSrvFormat;
    m_nWidth = other.m_nWidth;
    m_nHeight = other.m_nHeight;
    m_fClearDepth = other.m_fClearDepth;
    m_nClearStencil = other.m_nClearStencil;

    other.m_pd3dResource = nullptr;
    other.Release();
    return *this;
}
