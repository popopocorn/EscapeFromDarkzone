//-----------------------------------------------------------------------------
// File: CGameFramework.cpp
//-----------------------------------------------------------------------------

#include "NetPacketDispatcher.h"
#include "NetEntityManager.h"
#include "NetSession.h"

#include "stdafx.h"
#include "InputManager.h"
#include "EffectManager.h"
#include "InventoryManager.h"
#include "ResourceManager.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "TextRenderer.h"
#include"Scene.h"
#include "RenderTarget.h"


#include "GameFramework.h"



int FRAME_BUFFER_WIDTH = 1392;
int FRAME_BUFFER_HEIGHT = 738;


static XMFLOAT3 SafeNormalizeOrDefault(XMFLOAT3 v, XMFLOAT3 fallback)
{
	if (Vector3::Length(v) < 0.0001f)
		return fallback;

	return Vector3::Normalize(v);
}
static bool GetAttachedEffectMuzzleInfo(
	CGameObject* pTarget,
	XMFLOAT3& outPos,
	XMFLOAT3& outDir)
{
	if (!pTarget)
		return false;

	pTarget->UpdateTransform(NULL);

	CGameObject* pMuzzle = nullptr;

	if (CEnemyObject* pNpc = dynamic_cast<CEnemyObject*>(pTarget))
	{
		pMuzzle = pNpc->GetWeaponMuzzleSocket();
	}
	else if (OtherPlayer* pOther = dynamic_cast<OtherPlayer*>(pTarget))
	{
		pMuzzle = pOther->GetWeaponMuzzleSocket();
	}

	if (!pMuzzle)
	{
		pMuzzle = pTarget->FindFrame("Socket_Muzzle");
	}

	if (!pMuzzle)
	{
		OutputDebugString(L"[Effect] Socket_Muzzle not found. attached effect skipped.\n");
		return false;
	}

	outPos = pMuzzle->GetPosition();

	outDir = pTarget->GetLook();
	outDir = SafeNormalizeOrDefault(outDir, XMFLOAT3(0.0f, 0.0f, 1.0f));

	outPos.x += outDir.x * 0.05f;
	outPos.y += outDir.y * 0.05f;
	outPos.z += outDir.z * 0.05f;

	return true;
}



CGameFramework::CGameFramework()
{
	m_pdxgiFactory = NULL;
	m_pdxgiSwapChain = NULL;
	m_pd3dDevice = NULL;

	for (int i = 0; i < m_nSwapChainBuffers; i++) m_ppd3dSwapChainBackBuffers[i] = NULL;
	m_nSwapChainBufferIndex = 0;

	for (int i = 0; i < m_nSwapChainBuffers; i++) m_pd3dCommandAllocators[i] = NULL;
	m_pd3dCommandQueue = NULL;
	m_pd3dCommandList = NULL;

	m_pd3dRtvDescriptorHeap = NULL;
	m_pd3dDsvDescriptorHeap = NULL;

	m_hFenceEvent = NULL;
	m_pd3dFence = NULL;
	for (int i = 0; i < m_nSwapChainBuffers; i++) m_nFenceValues[i] = 0;

	m_nWndClientWidth = FRAME_BUFFER_WIDTH;
	m_nWndClientHeight = FRAME_BUFFER_HEIGHT;

	m_pPlayer = NULL;

	_tcscpy_s(m_pszFrameRate, _T("LabProject ("));

	m_ptOldCursorPos.x = FRAME_BUFFER_WIDTH/2.0;
	m_ptOldCursorPos.y = FRAME_BUFFER_HEIGHT/2.0;
}

CGameFramework::~CGameFramework()
{
}

bool CGameFramework::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	m_hInstance = hInstance;
	m_hWnd = hMainWnd;

	InputManager::Instance().init(hMainWnd);
	SoundManager::Instance()->Init();

	CreateDirect3DDevice();

	root = make_unique<RootSignature>(m_pd3dDevice);

	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain();
	CreateDepthStencilView();

	CreateRenderBuffers();

	CoInitialize(NULL);

	shadowmap = std::make_unique<ShadowMap>();
	shadowmap->Create(m_pd3dDevice);

	shadermanager = make_unique<ShaderManager>();

	SoundManager::Instance()->BuildSound();

	ResourceManager::Instance().CreateCbvSrvDescriptorHeaps(
		m_pd3dDevice,
		0,
		512
	);
	CreateRenderBuffersSRV();

	BuildObjects();
	
	ResourceManager::Instance().CreateshadowResourceViews(
		m_pd3dDevice,
		shadowmap.get(),
		0,
		0
	);

	observer = make_unique<CCamera>();
	observer->CreateShaderVariables(m_pd3dDevice, m_pd3dCommandList);
	observer->GenerateViewMatrix(
		XMFLOAT3(0.0f, 100.0f, 0.0f),
		XMFLOAT3(0.0f, -1.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, 1.0f)
	);
	observer->GenerateProjectionMatrix(m_pPlayer->GetCamera()->GetProjectionMatrix());
	observer->SetViewport(m_pPlayer->GetCamera()->GetViewport());
	observer->SetScissorRect(m_pPlayer->GetCamera()->GetScissorRect());

	//// 03.27 추가: 네트워크 초기화 및 연결
	//if (!NetworkManager::Instance().Init("Player"))
	//{
	//	OutputDebugString(L"DEBUG: Server Connect Fail.\n");
	//}

	return true;
}

void CGameFramework::CreateSwapChain()
{
	RECT rcClient;
	::GetClientRect(m_hWnd, &rcClient);
	m_nWndClientWidth = rcClient.right - rcClient.left;
	m_nWndClientHeight = rcClient.bottom - rcClient.top;

#ifdef _WITH_CREATE_SWAPCHAIN_FOR_HWND
	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC1));
	dxgiSwapChainDesc.Width = m_nWndClientWidth;
	dxgiSwapChainDesc.Height = m_nWndClientHeight;
	dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.BufferCount = m_nSwapChainBuffers;
	dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC dxgiSwapChainFullScreenDesc;
	::ZeroMemory(&dxgiSwapChainFullScreenDesc, sizeof(DXGI_SWAP_CHAIN_FULLSCREEN_DESC));
	dxgiSwapChainFullScreenDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainFullScreenDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainFullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiSwapChainFullScreenDesc.Windowed = TRUE;

	HRESULT hResult = m_pdxgiFactory->CreateSwapChainForHwnd(m_pd3dCommandQueue, m_hWnd, &dxgiSwapChainDesc, &dxgiSwapChainFullScreenDesc, NULL, (IDXGISwapChain1**)&m_pdxgiSwapChain);
#else
	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(dxgiSwapChainDesc));
	dxgiSwapChainDesc.BufferCount = m_nSwapChainBuffers;
	dxgiSwapChainDesc.BufferDesc.Width = m_nWndClientWidth;
	dxgiSwapChainDesc.BufferDesc.Height = m_nWndClientHeight;
	dxgiSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.OutputWindow = m_hWnd;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	dxgiSwapChainDesc.Windowed = TRUE;
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hResult = m_pdxgiFactory->CreateSwapChain(m_pd3dCommandQueue, &dxgiSwapChainDesc, (IDXGISwapChain**)&m_pdxgiSwapChain);
#endif
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	hResult = m_pdxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

#ifndef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	CreateRenderTargetViews();
#endif
}

void CGameFramework::CreateDirect3DDevice()
{
	HRESULT hResult;

	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	ID3D12Debug* pd3dDebugController = NULL;
	hResult = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pd3dDebugController);
	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}

	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings))))
	{
		// Auto-Breadcrumbs�� Page Fault ���� ���� Ȱ��ȭ
		pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
	}
#endif

	hResult = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void**)&m_pdxgiFactory);

	IDXGIAdapter1* pd3dAdapter = NULL;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); i++)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);
		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
		if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), (void**)&m_pd3dDevice))) break;
	}

	if (!pd3dAdapter)
	{
		m_pdxgiFactory->EnumWarpAdapter(_uuidof(IDXGIFactory4), (void**)&pd3dAdapter);
		hResult = D3D12CreateDevice(pd3dAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), (void**)&m_pd3dDevice);
	}
	

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4;
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	hResult = m_pd3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &d3dMsaaQualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;
	m_bMsaa4xEnable = (m_nMsaa4xQualityLevels > 1) ? true : false;

	hResult = m_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_pd3dFence);
	for (UINT i = 0; i < m_nSwapChainBuffers; i++) m_nFenceValues[i] = 0;

	m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	::gnCbvSrvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	::gnRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	::gnDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	if (pd3dAdapter) pd3dAdapter->Release();
}

void CGameFramework::CreateCommandQueueAndList()
{
	/*HRESULT hResult;

	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hResult = m_pd3dDevice->CreateCommandQueue(&d3dCommandQueueDesc, _uuidof(ID3D12CommandQueue), (void**)&m_pd3dCommandQueue);

	hResult = m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_pd3dCommandAllocator);

	hResult = m_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pd3dCommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&m_pd3dCommandList);
	hResult = m_pd3dCommandList->Close();*/
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	HRESULT hResult = m_pd3dDevice->CreateCommandQueue(&d3dCommandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&m_pd3dCommandQueue);

	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		hResult = m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_pd3dCommandAllocators[i]);
	}

	hResult = m_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pd3dCommandAllocators[0], NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&m_pd3dCommandList);

	hResult = m_pd3dCommandList->Close();
}

void CGameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = RTV_SLOT_COUNT;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	HRESULT hResult = m_pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dRtvDescriptorHeap);
	::gnRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = m_pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dDsvDescriptorHeap);
	::gnDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void CGameFramework::CreateRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < m_nSwapChainBuffers; i++)
	{
		m_pdxgiSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_ppd3dSwapChainBackBuffers[i]);
		m_pd3dDevice->CreateRenderTargetView(m_ppd3dSwapChainBackBuffers[i], NULL, d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += ::gnRtvDescriptorIncrementSize;
	}
}

void CGameFramework::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = m_nWndClientWidth;
	d3dResourceDesc.Height = m_nWndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;

	m_pd3dDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &d3dClearValue, __uuidof(ID3D12Resource), (void**)&m_pd3dDepthStencilBuffer);

	D3D12_DEPTH_STENCIL_VIEW_DESC d3dDepthStencilViewDesc;
	::ZeroMemory(&d3dDepthStencilViewDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
	d3dDepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dDepthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	d3dDepthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dDevice->CreateDepthStencilView(m_pd3dDepthStencilBuffer, &d3dDepthStencilViewDesc, d3dDsvCPUDescriptorHandle);
}

void CGameFramework::CreateRenderBuffers()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dColorBufferRtvHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dColorBufferRtvHandle.ptr += (RtvSlot::RTV_COLOR_BUFFER * ::gnRtvDescriptorIncrementSize);

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };

	renderBuffers.emplace_back(RenderTarget());
	renderBuffers[0].CreateRenderTarget(
		m_pd3dDevice,
		m_nWndClientWidth,
		m_nWndClientHeight,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		d3dColorBufferRtvHandle,
		pfClearColor
	);
}

void CGameFramework::CreateRenderBuffersSRV()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dCpuSrvHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE d3dGpuSrvHandle;

	for (int i = 0; i < renderBuffers.size();++i) {
		if (!ResourceManager::Instance().AllocateNextSrvDescriptor(d3dCpuSrvHandle, d3dGpuSrvHandle))
		{
			OutputDebugStringW(L"[GameFramework] Color buffer SRV slot allocation failed.\n");
			return;
		}

		renderBuffers[i].CreateSRV(m_pd3dDevice, d3dCpuSrvHandle, d3dGpuSrvHandle);
	}
}

void CGameFramework::ChangeSwapChainState()
{

	WaitForGpuComplete();

	BOOL bFullScreenState = FALSE;
	m_pdxgiSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	m_pdxgiSwapChain->SetFullscreenState(!bFullScreenState, NULL);

	DXGI_MODE_DESC dxgiTargetParameters;
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = m_nWndClientWidth;
	dxgiTargetParameters.Height = m_nWndClientHeight;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	m_pdxgiSwapChain->ResizeTarget(&dxgiTargetParameters);

	for (int i = 0; i < m_nSwapChainBuffers; i++) if (m_ppd3dSwapChainBackBuffers[i]) m_ppd3dSwapChainBackBuffers[i]->Release();

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	m_pdxgiSwapChain->GetDesc(&dxgiSwapChainDesc);
	m_pdxgiSwapChain->ResizeBuffers(m_nSwapChainBuffers, m_nWndClientWidth, m_nWndClientHeight, dxgiSwapChainDesc.BufferDesc.Format, dxgiSwapChainDesc.Flags);

	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	CreateRenderTargetViews();

	renderBuffers.clear();
	CreateRenderBuffers();
	CreateRenderBuffersSRV();
}

void CGameFramework::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (!m_pScene.empty() && m_pScene.back())
	{
		m_pScene.back()->OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
	}

	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
		::SetCapture(hWnd);
		::GetCursorPos(&m_ptOldCursorPos);
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		::ReleaseCapture();
		break;

	default:
		break;
	}
}

void CGameFramework::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (!m_pScene.empty() && m_pScene.back())
	{
		if (m_pScene.back()->OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam))
			return;
	}

	switch (nMessageID)
	{
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_ESCAPE:
			::PostQuitMessage(0);
			break;

		case VK_F9:
			ChangeSwapChainState();
			break;

		case 'M':
			mouseMove = !mouseMove;

			::ClipCursor(NULL);
			::ShowCursor(TRUE);
			::GetCursorPos(&m_ptOldCursorPos);

			if (mouseMove)
			{
				OutputDebugString(L"[Mouse] Free Mouse Mode ON\n");
			}
			else
			{
				OutputDebugString(L"[Mouse] Free Mouse Mode OFF\n");
			}
			break;

		case 'O':
			observing = !observing;
			if (observing) {
				m_pCamera = observer.get();
			}
			else {
				m_pCamera = m_pPlayer->GetCamera();
			}
			break;
		//갓 모드
		case 'K':
		{
			if (NetworkManager::Instance().IsConnected()) {
				NetSession::Instance().ToggleGodmode();					// 디버그용 갓모드 요청
			}
		}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}

LRESULT CALLBACK CGameFramework::OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	
	switch (nMessageID)
	{
	case WM_ACTIVATE:
	{
		if (LOWORD(wParam) == WA_INACTIVE)
			m_GameTimer.Stop();
		else
			m_GameTimer.Start();
		break;
	}
	case WM_SIZE:
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);
		break;
	}
	return(0);
}

void CGameFramework::OnDestroy()
{
	if (NetworkManager::Instance().IsConnected()) {
		NetworkManager::Instance().Shutdown();
	}

	WaitForGpuComplete();
	ReleaseObjects();
	ResourceManager::Instance().ReleaseResources();

	::CloseHandle(m_hFenceEvent);

	if (m_pd3dDepthStencilBuffer) m_pd3dDepthStencilBuffer->Release();
	if (m_pd3dDsvDescriptorHeap) m_pd3dDsvDescriptorHeap->Release();

	if (m_pd3dCommandList) m_pd3dCommandList->Release();
	if (m_pd3dCommandQueue) m_pd3dCommandQueue->Release();
	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		if (m_pd3dCommandAllocators[i]) m_pd3dCommandAllocators[i]->Release();
	}
	

	for (int i = 0; i < m_nSwapChainBuffers; i++) if (m_ppd3dSwapChainBackBuffers[i]) m_ppd3dSwapChainBackBuffers[i]->Release();
	if (m_pd3dRtvDescriptorHeap) m_pd3dRtvDescriptorHeap->Release();

	if (m_pdxgiSwapChain) 
	{
		m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);
		m_pdxgiSwapChain->Release();
	}

	
	if (m_pd3dDevice) m_pd3dDevice->Release();
	if (m_pdxgiFactory) m_pdxgiFactory->Release();

	if (m_pd3dFence) m_pd3dFence->Release();

#if defined(_DEBUG)
	IDXGIDebug1* pdxgiDebug = NULL;
	DXGIGetDebugInterface1(0, __uuidof(IDXGIDebug1), (void**)&pdxgiDebug);
	HRESULT hResult = pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, 
		(DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	pdxgiDebug->Release();
#endif
}

//#define _WITH_TERRAIN_PLAYER

void CGameFramework::BuildObjects()
{
	m_pd3dCommandList->Reset(m_pd3dCommandAllocators[0], NULL);

	shadermanager->BuildShaders(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot()
	);

	fscreenrenderer.init(m_pd3dDevice, m_pd3dCommandList, shadermanager->GetShader(ShaderType::FULLSCREEN));
	
	m_pScene.push_back(make_unique<LobbyScene>(this));
	//m_pScene.push_back(make_unique<MainScene>(this));
	m_pScene.back()->SetRoot(root->GetRoot());

	CMaterial::PrepareShaders(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot()
	);

	ResourceManager::Instance().BuildUIMesh(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot()
	);

	ResourceManager::Instance().BuildPlayerModelPrototypes(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		shadermanager->GetShader(ShaderType::PLAYER)
	);

	ResourceManager::Instance().BuildSkinnedModelPrototypes(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		shadermanager->GetShader(ShaderType::SKINNED)
	);

	ResourceManager::Instance().BuildModelPrototypes(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		shadermanager->GetShader(ShaderType::STANDARD)
	);

	CTerrainPlayer* pPlayer = new CTerrainPlayer(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		shadermanager->GetShader(ShaderType::PLAYER),
		ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_01),
		ResourceManager::Instance().GetModelInstance(ModelName::RIFLE)
	);
	pPlayer->frame = this;
	pPlayer->SetPosition(XMFLOAT3(0, 0.1, 0));

	pPlayer->InitializeInventory(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		nullptr
	);

	m_pPlayer = pPlayer;
	m_pScene.back()->SetPlayer(m_pPlayer);

	m_pCamera = m_pPlayer->GetCamera();

	if (!m_pScene.empty() && m_pScene.back())
	{
		m_pScene.back()->SetCamera(m_pCamera);
	}

	if (!m_pScene.empty() && m_pScene.back())
	{
		m_pScene.back()->BuildObjects(
			m_pd3dDevice,
			m_pd3dCommandList
		);
	}

	bool bFontLoaded =
		ResourceManager::Instance().BuildFontResource(
			m_pd3dDevice,
			m_pd3dCommandList
		);

	if (!bFontLoaded)
	{
		OutputDebugStringW(L"[GameFramework] Korean font resource build failed.\n");
	}
	else
	{
		BuildTextSystem();
	}

	m_pd3dCommandList->Close();

	ID3D12CommandList* ppd3dCommandLists[] =
	{
		m_pd3dCommandList
	};

	m_pd3dCommandQueue->ExecuteCommandLists(
		1,
		ppd3dCommandLists
	);

	WaitForGpuComplete();

	ResourceManager::Instance().ReleaseUploadBuffers();

	if (!m_pScene.empty() && m_pScene.back())
	{
		m_pScene.back()->ReleaseUploadBuffers(); //포인팅 구조 변경 이후 사용X
	}

	//if (m_pPlayer) m_pPlayer->ReleaseUploadBuffers();

	// 06.07 추가
	m_pNetEntityMgr = std::make_unique<NetEntityManager>();
	m_pNetEntityMgr->Init(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		shadermanager.get(),
		m_pPlayer
	);
	m_pNetEntityMgr->SetActiveScene(m_pScene.back().get());

	m_GameTimer.Reset();
}

void CGameFramework::BuildTextSystem()
{
	FontResource* pFontResource = ResourceManager::Instance().GetFontResource();

	if (!pFontResource)
	{
		OutputDebugStringW(L"[GameFramework] Text system build failed. Font resource is null.\n");
		return;
	}

	if (!pFontResource->IsLoaded())
	{
		OutputDebugStringW(L"[GameFramework] Text system build failed. Font resource is not loaded.\n");
		return;
	}

	ID3D12DescriptorHeap* pDescriptorHeap = ResourceManager::Instance().GetDescriptorHeap();

	if (!pDescriptorHeap)
	{
		OutputDebugStringW(L"[GameFramework] Text system build failed. Descriptor heap is null.\n");
		return;
	}

	m_pTextRenderer = std::make_unique<TextRenderer>();

	bool bInitialized = m_pTextRenderer->Initialize(
		m_pd3dDevice,
		root->GetRoot(),
		pDescriptorHeap,
		pFontResource,
		static_cast<UINT>(m_nWndClientWidth),
		static_cast<UINT>(m_nWndClientHeight),
		L"Text.hlsli",
		4096
	);

	if (!bInitialized)
	{
		OutputDebugStringW(L"[GameFramework] TextRenderer initialization failed.\n");
		m_pTextRenderer.reset();
		return;
	}

	OutputDebugStringW(L"[GameFramework] Text system build complete.\n");
}

void CGameFramework::RenderTextSystem()
{
	if (!m_pTextRenderer)
		return;

	if (!m_pTextRenderer->IsInitialized())
		return;

	m_pTextRenderer->BeginFrame();

	if (!m_pScene.empty() && m_pScene.back())
	{
		HUDManager* pUIManager =
			m_pScene.back()->GetUIManager();

		if (pUIManager)
		{
			pUIManager->SubmitText(
				m_pTextRenderer.get()
			);
		}

		InventoryManager* pInventoryManager =
			m_pScene.back()->GetInventoryManager();

		if (pInventoryManager)
		{
			pInventoryManager->SubmitText(
				m_pTextRenderer.get()
			);
		}
	}

	m_pTextRenderer->Render(
		m_pd3dCommandList
	);
}

void CGameFramework::ReleaseObjects()
{
	
	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->ReleaseObjects();
}

void CGameFramework::ProcessInput()
{
	static UCHAR pKeysBuffer[256];
	static bool bGameplayCursorInitialized = false;

	bool bProcessedByScene = false;
	if (GetKeyboardState(pKeysBuffer) && !m_pScene.empty() && m_pScene.back()) bProcessedByScene = m_pScene.back()->ProcessInput(pKeysBuffer);

	/*if (!bProcessedByScene)
	{
		float cxDelta = 0.0f, cyDelta = 0.0f;
		bool bInventoryOpen = false;

		if (!m_pScene.empty() && m_pScene.back() && m_pScene.back()->GetInventoryManager())
		{
			bInventoryOpen = m_pScene.back()->GetInventoryManager()->IsAnyInventoryOpen();
		}

		bool bFreeMouseMode = mouseMove || bInventoryOpen;

		if (bFreeMouseMode)
		{
			::ClipCursor(NULL);
			::ShowCursor(TRUE);
			::GetCursorPos(&m_ptOldCursorPos);
			bGameplayCursorInitialized = false;
		}
		else
		{
			RECT rc;
			::GetClientRect(m_hWnd, &rc);

			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;

			if (width > 0 && height > 0)
			{
				int centerX = width / 2;
				int centerY = height / 2;

				POINT ptCursorPos;
				::GetCursorPos(&ptCursorPos);

				POINT ptClientCursor = ptCursorPos;
				::ScreenToClient(m_hWnd, &ptClientCursor);

				if (ptClientCursor.y < 0) ptClientCursor.y = 0;
				if (ptClientCursor.y > centerY) ptClientCursor.y = centerY;

				POINT ptFixedClientCursor = { centerX, ptClientCursor.y };
				POINT ptFixedScreenCursor = ptFixedClientCursor;
				::ClientToScreen(m_hWnd, &ptFixedScreenCursor);

				if (!bGameplayCursorInitialized)
				{
					::ClipCursor(NULL);
					::ShowCursor(TRUE);
					::SetCursorPos(ptFixedScreenCursor.x, ptFixedScreenCursor.y);
					m_ptOldCursorPos = ptFixedScreenCursor;
					bGameplayCursorInitialized = true;
				}
				else
				{
					cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
					cyDelta = (float)(ptFixedScreenCursor.y - m_ptOldCursorPos.y) / 3.0f;

					::ClipCursor(NULL);
					::ShowCursor(TRUE);
					::SetCursorPos(ptFixedScreenCursor.x, ptFixedScreenCursor.y);

					m_ptOldCursorPos = ptFixedScreenCursor;
				}
			}
		}

		DWORD dwDirection = 0;
		if (pKeysBuffer[VK_RIGHT] & 0xF0) dwDirection |= DIR_RIGHT;
		if (pKeysBuffer[VK_LEFT] & 0xF0)  dwDirection |= DIR_LEFT;
		if (pKeysBuffer[VK_UP] & 0xF0)    dwDirection |= DIR_FORWARD;
		if (pKeysBuffer[VK_DOWN] & 0xF0)  dwDirection |= DIR_BACKWARD;

		if (dwDirection && observing)
		{
			XMFLOAT3 move = XMFLOAT3(0, 0, 0);
			if (dwDirection & DIR_RIGHT)    move.x += 1.0f;
			if (dwDirection & DIR_LEFT)     move.x -= 1.0f;
			if (dwDirection & DIR_FORWARD)  move.z += 1.0f;
			if (dwDirection & DIR_BACKWARD) move.z -= 1.0f;
			observer->Move(move);
			observer->RegenerateViewMatrix();
		}

		if ((dwDirection != 0) || (cxDelta != 0.0f) || (cyDelta != 0.0f))
		{
			if (cxDelta || cyDelta)
			{
				if (pKeysBuffer[VK_RBUTTON] & 0xF0)
					m_pPlayer->Rotate(cyDelta, 0.0f, -cxDelta);
				else
					m_pPlayer->Rotate(cyDelta, cxDelta, 0.0f);
			}
		}
	}*/
}

void CGameFramework::AnimateObjects(float fTimeElapsed)
{
	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->AnimateObjects(fTimeElapsed);


	m_pPlayer->OnPrepareRender();

	if (m_pPlayer->m_pSkinnedAnimationController)
	{
		m_pPlayer->m_pSkinnedAnimationController->AdvanceTime(fTimeElapsed, m_pPlayer);
		m_pPlayer->m_pSkinnedAnimationController->UpdateShaderVariables(m_pd3dCommandList);
	}
	else
	{
		m_pPlayer->Animate(fTimeElapsed);
		m_pPlayer->UpdateTransform(NULL);
	}

	m_pPlayer->Update(m_GameTimer.GetTimeElapsed());
	m_pPlayer->UpdateTransform(NULL);


}

void CGameFramework::WaitForGpuComplete()
{
	UINT64 nFenceValue = 0;
	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		if (m_nFenceValues[i] > nFenceValue) nFenceValue = m_nFenceValues[i];
	}
	nFenceValue++;

	m_nFenceValues[m_nSwapChainBufferIndex] = nFenceValue;
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence, nFenceValue);

	if (m_pd3dFence->GetCompletedValue() < nFenceValue)
	{
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::MoveToNextFrame()
{
	UINT64 nFenceValue = 0;
	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		if (m_nFenceValues[i] > nFenceValue) nFenceValue = m_nFenceValues[i];
	}
	nFenceValue++;

	m_nFenceValues[m_nSwapChainBufferIndex] = nFenceValue;
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence, nFenceValue);

	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	UINT64 fenceValueToWaitFor = m_nFenceValues[m_nSwapChainBufferIndex];
	if (fenceValueToWaitFor != 0 && m_pd3dFence->GetCompletedValue() < fenceValueToWaitFor)
	{
		hResult = m_pd3dFence->SetEventOnCompletion(fenceValueToWaitFor, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

//#define _WITH_PLAYER_TOP

void CGameFramework::FrameAdvance()
{
	InputManager::Instance().update();
	m_GameTimer.Tick(0);
	float fTimeElapsed = m_GameTimer.GetTimeElapsed();

	HRESULT hResult = m_pd3dCommandAllocators[m_nSwapChainBufferIndex]->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocators[m_nSwapChainBufferIndex], NULL);

	//if (m_pScene && m_pPlayer)m_pScene->DoCollision(m_pPlayer, 0);
	
	ChangeScene();
	
	ProcessInput();

	

	
	AnimateObjects(fTimeElapsed);
	SoundManager::Instance()->UpdateListener(
		m_pPlayer->GetPosition(),
		m_pPlayer->GetLookVector(),
		m_pPlayer->GetUpVector()
	);
	SoundManager::Instance()->Update();
	
	// 03.27 추가, 03.30 위치 변경
	if (NetworkManager::Instance().IsConnected())
	{
		ProcessNetworkPackets();
	}


	ShadowRendering();

	PrepareMainRender();
	MainRendering();
	TransparentRendering();
	RenderTextSystem();

	//compute pipline
	PreparePostRender();



	//rendering end
	BlitToBackBuffer();
	


	hResult = m_pd3dCommandList->Close();

	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);


#ifdef _WITH_PRESENT_PARAMETERS
	DXGI_PRESENT_PARAMETERS dxgiPresentParameters;
	dxgiPresentParameters.DirtyRectsCount = 0;
	dxgiPresentParameters.pDirtyRects = NULL;
	dxgiPresentParameters.pScrollRect = NULL;
	dxgiPresentParameters.pScrollOffset = NULL;
	m_pdxgiSwapChain->Present1(1, 0, &dxgiPresentParameters);
#else
#ifdef _WITH_SYNCH_SWAPCHAIN
	m_pdxgiSwapChain->Present(1, 0);
#else
	HRESULT hr =  m_pdxgiSwapChain->Present(0, 0);

#endif
#endif
	UINT64 targetFence = 0;
	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		if (m_nFenceValues[i] > targetFence) targetFence = m_nFenceValues[i];
	}
	targetFence++; 
	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->DeleteDeadObject(targetFence);

	
	MoveToNextFrame();

	UINT64 done = m_pd3dFence->GetCompletedValue();
	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->DeleteTrash(done);



	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	size_t nLength = _tcslen(m_pszFrameRate);
	XMFLOAT3 xmf3Position = m_pPlayer->GetPosition();
	_stprintf_s(m_pszFrameRate + nLength, 70 - nLength, _T("(%4f, %4f, %4f)"), xmf3Position.x, xmf3Position.y, xmf3Position.z);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

void CGameFramework::PrepareMainRender()
{
	renderBuffers[BufferName::COLOR].TransitionTo(m_pd3dCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (RtvSlot::RTV_COLOR_BUFFER * ::gnRtvDescriptorIncrementSize);

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, pfClearColor, 0, NULL);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE, &d3dDsvCPUDescriptorHandle);

}

void CGameFramework::PreparePostRender()
{

}

void CGameFramework::BlitToBackBuffer()
{
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex];
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * ::gnRtvDescriptorIncrementSize);

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, pfClearColor, 0, NULL);
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE, &d3dDsvCPUDescriptorHandle);
	m_pd3dCommandList->SetGraphicsRootSignature(root->GetRoot());
	//color->backbuffer
	fscreenrenderer.Render(m_pd3dCommandList, renderBuffers[BufferName::COLOR]);

	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

}



void CGameFramework::ShadowRendering()
{
	//shadow rendering pass
	shadowmap->TransitionToDSV(m_pd3dCommandList);
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		shadowmap->BindAsDepthTarget(m_pd3dCommandList, i);
		m_pScene.back()->Render(m_pd3dCommandList, SHADOW, m_pScene.back()->GetLightCamera(i));
	}
	shadowmap->TransitionToSRV(m_pd3dCommandList);
	m_pScene.back()->GetLightCameraManager()->UpdateShaderVariables(m_pd3dCommandList);
	shadowmap->SetTextureOnParameter(m_pd3dCommandList);

}

void CGameFramework::MainRendering()
{
	//main rendering pass
	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->Render(m_pd3dCommandList, MAIN, m_pCamera);

#ifdef _WITH_PLAYER_TOP
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
#endif

	m_pScene.back()->ThroughRender(m_pd3dCommandList, m_pCamera);

}

void CGameFramework::TransparentRendering()
{

	if (!m_pScene.empty() && m_pScene.back()) m_pScene.back()->TransparentRender(m_pd3dCommandList, MAIN, m_pCamera);

}


// 03.27 추가
void CGameFramework::ProcessNetworkPackets()
{
	if (!NetworkManager::Instance().IsConnected()) return;

	if (!m_pPacketDispatcher)
		m_pPacketDispatcher = std::make_unique<NetPacketDispatcher>(this);

	NetworkManager::Instance().Recv();

	while (true)
	{
		std::vector<char> packet = NetworkManager::Instance().PopPacket();
		if (packet.empty()) break;

		m_pPacketDispatcher->Handle(packet);
	}
}

void CGameFramework::PushScene()
{
	m_pScene.back()->ReleaseObjects();
	m_pScene.push_back(unique_ptr<CScene>(nextScene));
	m_pScene.back()->SetRoot(root->GetRoot());
	m_pScene.back()->SetPlayer(m_pPlayer);
	m_pScene.back()->BuildObjects(m_pd3dDevice, m_pd3dCommandList);
	m_pScene.back()->SetCamera(m_pCamera);

	if (m_pNetEntityMgr) m_pNetEntityMgr->SetActiveScene(m_pScene.back().get());	// 06.07 추가

	nextScene = nullptr;
}

void CGameFramework::PopScene()
{
	m_pScene.back()->ReleaseObjects();
	m_pScene.pop_back();
	m_pScene.back()->SetRoot(root->GetRoot());
	m_pScene.back()->SetPlayer(m_pPlayer);
	m_pScene.back()->BuildObjects(m_pd3dDevice, m_pd3dCommandList);
	m_pScene.back()->SetCamera(m_pCamera);

	if (m_pNetEntityMgr) m_pNetEntityMgr->SetActiveScene(m_pScene.back().get());	// 06.07 추가
}

void CGameFramework::PopScene(SceneName name)
{
	
}

void CGameFramework::ChangeScene()
{
	if (nextScene)
	{
		WaitForGpuComplete();
		PushScene();
		m_pd3dCommandList->Close(); 
		ID3D12CommandList* ppCommandLists[] = { m_pd3dCommandList };
		m_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists); 
		WaitForGpuComplete(); 
		
		m_pd3dCommandAllocators[m_nSwapChainBufferIndex]->Reset();
		m_pd3dCommandList->Reset(m_pd3dCommandAllocators[m_nSwapChainBufferIndex], NULL);
	}
}
