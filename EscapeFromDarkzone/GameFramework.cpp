//-----------------------------------------------------------------------------
// File: CGameFramework.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "InputManager.h"
#include "EffectManager.h"
#include "InventoryManager.h"
#include "ResourceManager.h"
#include"ShaderManager.h"
#include "GameFramework.h"


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

	m_ptOldCursorPos.x = 1300;
	m_ptOldCursorPos.y = 600;
}

CGameFramework::~CGameFramework()
{
}

bool CGameFramework::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	m_hInstance = hInstance;
	m_hWnd = hMainWnd;

	InputManager::Instance().init(hMainWnd);
	CreateDirect3DDevice();
	root = make_unique<RootSignature>(m_pd3dDevice);
	
	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain();
	CreateDepthStencilView();

	CoInitialize(NULL);


	shadowmap = std::make_unique<ShadowMap>();
	shadowmap->Create(m_pd3dDevice);

	shadermanager = make_unique<ShaderManager>();
	
	ResourceManager::Instance().CreateCbvSrvDescriptorHeaps(m_pd3dDevice, 0, 480);

	BuildObjects();

	ResourceManager::Instance().CreateshadowResourceViews(m_pd3dDevice, shadowmap.get(), 0, 0);
	
	observer = make_unique<CCamera>();
	observer->CreateShaderVariables(m_pd3dDevice, m_pd3dCommandList);
	observer->GenerateViewMatrix(XMFLOAT3(0.0f, 100.0f, 0.0f), XMFLOAT3(0, -1, 0), XMFLOAT3(0, 0, 1));
	observer->GenerateProjectionMatrix(m_pPlayer->GetCamera()->GetProjectionMatrix());
	observer->SetViewport(m_pPlayer->GetCamera()->GetViewport());
	observer->SetScissorRect(m_pPlayer->GetCamera()->GetScissorRect());

	// 03.27 추가: 네트워크 초기화 및 연결
	if (!NetworkManager::Instance().Init("Player"))
	{
		OutputDebugString(L"DEBUG: Server Connect Fail.\n");
	}

	return(true);
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
	d3dDescriptorHeapDesc.NumDescriptors = m_nSwapChainBuffers;
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
}

void CGameFramework::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{

	if (not m_pScene.empty()) m_pScene.back()->OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
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
	case WM_MOUSEMOVE:
		break;
	default:
		break;
	}
}

void CGameFramework::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (not m_pScene.empty()) m_pScene.back()->OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);
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
			if (mouseMove) {
				::ClipCursor(NULL);
				::ShowCursor(TRUE);
			}
			else {
				RECT rect;
				::GetWindowRect(m_hWnd, &rect);
				::ClipCursor(&rect);

				::GetCursorPos(&m_ptOldCursorPos);
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
		case VK_SPACE:
		{
			if (m_pPlayer && not m_pScene.empty())
			{
				XMFLOAT3 pos = m_pPlayer->GetPosition();
				XMFLOAT3 look = m_pPlayer->GetLookVector();

				XMFLOAT3 bombPos = Vector3::Add(pos, Vector3::ScalarProduct(look, 3.0f, false));
				//bombPos.y += 5.0f;

				XMFLOAT3 bombRight = m_pPlayer->GetRightVector();
				XMFLOAT3 bombFlatLook = m_pPlayer->GetLookVector();
				bombFlatLook.y = 0.0f;
				bombFlatLook = Vector3::Normalize(bombFlatLook);

				if (not m_pScene.empty() && m_pScene.back()->GetEffectManager())
				{
					m_pScene.back()->GetEffectManager()->RequestPlayEffect(EFFECT_BOMB, bombPos, bombRight, bombFlatLook);
				}
			}
			break;
		}
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
	//ResourceManager::Instance().ReleaseResources();

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
	shadermanager->BuildShaders(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());

	m_pScene.push_back(make_unique<LobbyScene>(this));
	m_pScene.back()->SetRoot(root->GetRoot());
	CMaterial::PrepareShaders(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());
	ResourceManager::Instance().BuildUIMesh(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());
	
	PlayerShader* pshader = new PlayerShader();
	pshader->CreateShader(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());
	pshader->CreateShadowShader(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());
	pshader->CreateShaderVariables(m_pd3dDevice, m_pd3dCommandList);
	pshader->CreateThroughShader(m_pd3dDevice, m_pd3dCommandList, root->GetRoot());

	ResourceManager::Instance().BuildModelPrototypes(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		pshader
	);

	CTerrainPlayer* pPlayer = new CTerrainPlayer(
		m_pd3dDevice,
		m_pd3dCommandList,
		root->GetRoot(),
		pshader,
		ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::PLAYER_01),
		ResourceManager::Instance().GetModelPrototype(ModelName::RIFLE)
	);

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

	if (not m_pScene.empty()) m_pScene.back()->SetCamera(m_pCamera);

	if (not m_pScene.empty()) m_pScene.back()->BuildObjects(m_pd3dDevice, m_pd3dCommandList);


	m_pd3dCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	WaitForGpuComplete();

	//if (not m_pScene.back()) m_pScene.back()->ReleaseUploadBuffers();//포인팅 구조 변경 이후 사용X
	//if (m_pPlayer) m_pPlayer->ReleaseUploadBuffers();

	m_GameTimer.Reset();
}

void CGameFramework::ReleaseObjects()
{
	

	if (not m_pScene.empty()) m_pScene.back()->ReleaseObjects();
}

void CGameFramework::ProcessInput()
{
	static UCHAR pKeysBuffer[256];
	bool bProcessedByScene = false;
	if (GetKeyboardState(pKeysBuffer) && not m_pScene.empty()) bProcessedByScene = m_pScene.back()->ProcessInput(pKeysBuffer);
	if (!bProcessedByScene)
	{
		float cxDelta = 0.0f, cyDelta = 0.0f;
		POINT ptCursorPos;
		//if (GetCapture() == m_hWnd)
		
		if (!mouseMove)
		{
			::GetCursorPos(&ptCursorPos);
			::SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
			cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
			cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;

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
	}
}

void CGameFramework::AnimateObjects(float fTimeElapsed)
{
	if (not m_pScene.empty()) m_pScene.back()->AnimateObjects(fTimeElapsed);
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
	
	if (nextScene)
	{
		WaitForGpuComplete();
		ChangeScene();
		m_pd3dCommandList->Close(); 
		ID3D12CommandList* ppCommandLists[] = { m_pd3dCommandList };
		m_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists); 
		WaitForGpuComplete(); 
		
		m_pd3dCommandAllocators[m_nSwapChainBufferIndex]->Reset();
		m_pd3dCommandList->Reset(m_pd3dCommandAllocators[m_nSwapChainBufferIndex], NULL);
	}
	ProcessInput();

	AnimateObjects(fTimeElapsed);

	
	// 03.27 추가, 03.30 위치 변경
	if (NetworkManager::Instance().IsConnected())
	{
		ProcessNetworkPackets();
	}

	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
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


	//shadow rendering pass
	shadowmap->TransitionToDSV(m_pd3dCommandList);
	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		shadowmap->BindAsDepthTarget(m_pd3dCommandList, i);
		m_pScene.back()->Render(m_pd3dCommandList, SHADOW, m_pScene.back()->GetLightCamera(i));
		if (m_pPlayer) m_pPlayer->Render(m_pd3dCommandList, SHADOW, m_pScene.back()->GetLightCamera(i));
	}
	shadowmap->TransitionToSRV(m_pd3dCommandList);
	m_pScene.back()->GetLightCameraManager()->UpdateShaderVariables(m_pd3dCommandList);
	shadowmap->SetTextureOnParameter(m_pd3dCommandList);
	
	
	//main rendering pass
	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, pfClearColor, 0, NULL);

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

	m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE, &d3dDsvCPUDescriptorHandle);

	if (not m_pScene.empty()) m_pScene.back()->Render(m_pd3dCommandList, MAIN, m_pCamera);

#ifdef _WITH_PLAYER_TOP
	m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);
#endif


	/*if (m_pPlayer)
	{
		m_pd3dCommandList->OMSetStencilRef(0x04);
		m_pPlayer->Render(m_pd3dCommandList, MAIN, m_pCamera);
	}*/
	m_pScene.back()->ThroughRender(m_pd3dCommandList, m_pCamera);


	//compute pipline
	


	//rendering end
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

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

#ifdef _DEBUG
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		// 1. ����̽��� ���ŵ� ��¥ ���� Ȯ��
		HRESULT removedReason = m_pd3dDevice->GetDeviceRemovedReason();
		OutputDebugStringA("GPU ũ����(Device Removed) �߻�!\n");

		// 2. DRED ������ ����
		ComPtr<ID3D12DeviceRemovedExtendedData1> pDred;
		if (SUCCEEDED(m_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pDred)))) // ����̽����� DRED �������̽� ��������
		{
			D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 DredAutoBreadcrumbsOutput;
			D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;

			// ��ɾ� ���� ���(Breadcrumbs) �� ������ ��Ʈ ������ ��������
			pDred->GetAutoBreadcrumbsOutput1(&DredAutoBreadcrumbsOutput);
			pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput);

			// 3. ����Ÿ� ������ ���߰� ��
			// ���⼭ �ߴ���(Breakpoint)�� �ɸ���, Visual Studio�� '�����(Watch)' â����
			// DredAutoBreadcrumbsOutput ������ ���� � ���(��ɾ�)���� ����Ǵ� �׾����� Ȯ���մϴ�.
			__debugbreak();
		}
	}
#endif
#endif
#endif
	UINT64 targetFence = 0;
	for (int i = 0; i < m_nSwapChainBuffers; i++)
	{
		if (m_nFenceValues[i] > targetFence) targetFence = m_nFenceValues[i];
	}
	targetFence++; 
	if (not m_pScene.empty()) m_pScene.back()->DeleteDeadObject(targetFence);

	
	MoveToNextFrame();

	UINT64 done = m_pd3dFence->GetCompletedValue();
	if (not m_pScene.empty()) m_pScene.back()->DeleteTrash(done);



	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	size_t nLength = _tcslen(m_pszFrameRate);
	XMFLOAT3 xmf3Position = m_pPlayer->GetPosition();
	_stprintf_s(m_pszFrameRate + nLength, 70 - nLength, _T("(%4f, %4f, %4f)"), xmf3Position.x, xmf3Position.y, xmf3Position.z);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

// 03.27 추가
void CGameFramework::ProcessNetworkPackets()
{
	if (!NetworkManager::Instance().IsConnected()) return;

	NetworkManager::Instance().Recv();

	while (true)
	{
		std::vector<char> packet = NetworkManager::Instance().PopPacket();
		if (packet.empty()) break;

		char type = packet[1]; // 패킷 타입 확인
		switch (type)
		{
		case SC_LOGIN_INFO:
		{
			SC_LOGIN_INFO_PACKET* p = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(packet.data());
			//wchar_t szLog[128];
			//swprintf_s(szLog, L"[Network] SC_LOGIN_INFO - id: %d, pos(%.2f, %.2f, %.2f)\n", p->id, p->x, p->y, p->z);
			//OutputDebugString(szLog);
			// 03.30 추가: 서버로부터 받아온 초기 위치로 이동
			m_pPlayer->SetPosition(XMFLOAT3(p->x, p->y, p->z));
			m_pPlayer->SetServerPosition(XMFLOAT3(p->x, p->y, p->z)); // 04.10 추가: 서버 위치 초기화
			m_myId = p->id; // 03.30 추가: 내 ID 저장
			break;
		}
		case SC_ADD_PLAYER:
		{
			SC_ADD_PLAYER_PACKET* p = reinterpret_cast<SC_ADD_PLAYER_PACKET*>(packet.data());
			//wchar_t szLog[128];
			//swprintf_s(szLog, L"[Network] SC_ADD_PLAYER - id: %d, pos(%.2f, %.2f, %.2f)\n", p->id, p->x, p->y, p->z);
			//OutputDebugStringW(szLog);

			// 03.30 추가: 잘못된 패킷 넘기기 (Broadcast 관련 오류?)
			if (p->id == m_myId) {
				break;
			}
			if (FindOtherPlayer(p->id)) {
				break;
			}

			OtherPlayer* pOther = OtherPlayer::Create(m_pd3dDevice, m_pd3dCommandList, root->GetRoot(), p->x, p->y, p->z);
			pOther->SetServerYaw(p->yaw);

			// 상태 변경
			switch (p->state) {
			case PLAYER_STATE_IDLE:
				pOther->ChangeState(std::make_unique<OtherPlayerIdle>());
				break;
			case PLAYER_STATE_RUN:
				pOther->ChangeState(std::make_unique<OtherPlayerRun>());
				break;
			}
			
			if (!AddOtherPlayer(p->id, pOther)) {	// 슬롯 부족 — 생성 취소
				OutputDebugString(L"[Network] OtherPlayer slot full.\n");
				pOther->Kill();
				break;
			}

			m_pScene.back()->m_ppShaders[SHADERIDX::ENEMY]->addObjects(std::unique_ptr<CGameObject>(pOther));

			break;
		}
		case SC_REMOVE_PLAYER:
		{
			SC_REMOVE_PLAYER_PACKET* p = reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(packet.data());
			//wchar_t szLog[128];
			//swprintf_s(szLog, L"[Network] SC_REMOVE_PLAYER - id: %d\n", p->id);
			//OutputDebugStringW(szLog);

			// 03.30 추가: OtherPlayer 제거
			if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
				pOther->Kill();
				RemoveOtherPlayer(p->id);
			}

			break;
		}
		case SC_MOVE_PLAYER:
		{
			SC_MOVE_PLAYER_PACKET* p = reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(packet.data());
			//wchar_t szLog[128];
			//swprintf_s(szLog, L"[Network] SC_MOVE_PLAYER - id: %d, pos(%.2f, %.2f, %.2f)\n", p->id, p->x, p->y, p->z);
			//OutputDebugStringW(szLog);

			// 03.30 추가: OtherPlayer 위치 업데이트 (플레이어 위치 보정 미구현)
			if (p->id == m_myId) {
				m_pPlayer->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));	// 04.10 추가: 보간용 서버 위치
				break;
			}

			if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
				pOther->UpdatePosition(p->x, p->y, p->z);
				pOther->SetServerYaw(p->yaw);
			}

			break;
		}
		case SC_PLAYER_STATE_CHANGE:
		{
			SC_PLAYER_STATE_CHANGE_PACKET* p =
				reinterpret_cast<SC_PLAYER_STATE_CHANGE_PACKET*>(packet.data());

			if (p->id == m_myId) break;		// 서버에서 한 번 거르긴 했는데, 혹시 모르니까

			if (OtherPlayer* pOther = FindOtherPlayer(p->id)) {
				switch (p->state) {
				case PLAYER_STATE_IDLE:
					pOther->ChangeState(std::make_unique<OtherPlayerIdle>());
					break;
				case PLAYER_STATE_RUN:
					pOther->ChangeState(std::make_unique<OtherPlayerRun>());
					break;
				}
			}
			break;
		}
		case SC_ADD_NPC: 
		{

			SC_ADD_NPC_PACKET* p = reinterpret_cast<SC_ADD_NPC_PACKET*>(packet.data());

			// 중복 방지
			if (FindNpc(p->npc_id)) {
				break;
			}

			// CEnemyObject 동적 스폰 (생성자 시그니처는 BuildObjects의 고정 스폰과 동일)
			CEnemyObject* pNpc = new CEnemyObject(
				m_pd3dDevice,
				m_pd3dCommandList,
				root->GetRoot(),
				NULL,
				ResourceManager::Instance().CreateSkinnedModelInstance(ModelName::ENEMY_01)
			);
			pNpc->SetPosition(p->x, p->y, p->z);
			pNpc->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));   // lerp 시작점 = 서버 위치
			pNpc->SetServerYaw(p->yaw);

			if (!AddNpc(p->npc_id, pNpc)) {
				OutputDebugString(L"[Network] NPC slot full.\n");
				pNpc->Kill();
				break;
			}

			m_pScene.back()->m_ppShaders[SHADERIDX::ENEMY]->addObjects(std::unique_ptr<CGameObject>(pNpc));

			break;
		}
		case SC_REMOVE_NPC:
		{
			SC_REMOVE_NPC_PACKET* p = reinterpret_cast<SC_REMOVE_NPC_PACKET*>(packet.data());

			if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
				pNpc->Kill();
				RemoveNpc(p->npc_id);
			}

			break;
		}
		case SC_MOVE_NPC:
		{
			SC_MOVE_NPC_PACKET* p = reinterpret_cast<SC_MOVE_NPC_PACKET*>(packet.data());

			if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
				pNpc->SetServerPosition(XMFLOAT3(p->x, p->y, p->z));
				pNpc->SetServerYaw(p->yaw);
			}

			break;
		}
		case SC_NPC_STATE_CHANGE: {
			SC_NPC_STATE_CHANGE_PACKET* p = reinterpret_cast<SC_NPC_STATE_CHANGE_PACKET*>(packet.data());

			if (CEnemyObject* pNpc = FindNpc(p->npc_id)) {
				switch (p->state) {
				case NPC_STATE_IDLE:
					pNpc->ChangeState(std::make_unique<EnemyIdle>());
					pNpc->SnapToServerPosition();
					break;
				case NPC_STATE_RUN:
					pNpc->ChangeState(std::make_unique<EnemyRun>());
					break;
				case NPC_STATE_DIE:
					// EnemyDie::Enter가 m_bDying / m_fDieElapsed 설정
					// Scene::AnimateObjects가 1.2초 후 자체 루팅 박스 생성.
					// 서버는 같은 1.2초 후 SC_REMOVE_NPC 송신 → NPC 객체 자체 제거.
					// (루팅 박스는 별도 컨테이너로 남아 있음)
					pNpc->ChangeState(std::make_unique<EnemyDie>());
					break;
				}
			}

			break;
		}
		case SC_INVENTORY_UPDATE: {
			SC_INVENTORY_UPDATE_PACKET* p =
				reinterpret_cast<SC_INVENTORY_UPDATE_PACKET*>(packet.data());

			if (m_pScene.empty()) break;
			InventoryManager* pInvMgr = m_pScene.back()->GetInventoryManager();
			if (!pInvMgr) break;

			bool ok = pInvMgr->ApplyPlayerInventorySlotUpdate(
				p->item_id, p->count, p->slotidx);

			// 서버로부터 받은 패킷을 디버그콘솔에 출력
			//wchar_t buf[256];
			//swprintf_s(buf, L"[INV_APPLY] slot:%d item:%d count:%d %s\n",
			//	p->slotidx, static_cast<int>(p->item_id), p->count);
			//OutputDebugStringW(buf);

			break;
		}
		case SC_ADD_LOOT_BOX: {
			SC_ADD_LOOT_BOX_PACKET* p =
				reinterpret_cast<SC_ADD_LOOT_BOX_PACKET*>(packet.data());

			InventoryManager* pInvMgr = m_pScene.back()->GetInventoryManager();

			pInvMgr->SpawnLootContainer(p->npc_id,
				XMFLOAT3(p->x, p->y, p->z),
				p->items, p->counts, INVENTORY_SIZE);

			//wchar_t buf[256];
			//swprintf_s(buf, L"[LOOT_BOX_ADD] id:%d pos:(%.2f,%.2f,%.2f)\n",
			//	p->npc_id, p->x, p->y, p->z);
			//OutputDebugStringW(buf);

			break;
		}
		case SC_LOOT_BOX_SLOT_UPDATE: {
			SC_LOOT_BOX_SLOT_UPDATE_PACKET* p =
				reinterpret_cast<SC_LOOT_BOX_SLOT_UPDATE_PACKET*>(packet.data());

			if (m_pScene.empty()) break;
			InventoryManager* pInvMgr = m_pScene.back()->GetInventoryManager();
			if (!pInvMgr) break;

			pInvMgr->ApplyLootBoxSlotUpdate(p->box_id, p->slotidx, p->item_id, p->count);

			//wchar_t buf[256];
			//swprintf_s(buf, L"[BOX_APPLY] box_id: %d slot:%d item:%d count:%d %s\n",
			//	p->box_id, p->slotidx, static_cast<int>(p->item_id), p->count);
			//OutputDebugStringW(buf);

			break;
		}
		case SC_DEACTIVATE_LOOT_BOX: {
			SC_DEACTIVATE_LOOT_BOX_PACKET* p =
				reinterpret_cast<SC_DEACTIVATE_LOOT_BOX_PACKET*>(packet.data());

			if (m_pScene.empty()) break;
			InventoryManager* pInvMgr = m_pScene.back()->GetInventoryManager();
			if (!pInvMgr) break;

			pInvMgr->DeactivateLootBox(p->npc_id);
			break;
		}
		default:
			break;
		}
	}
}

// 05.08 추가: unordered_map에서 array로, 함수 추가

OtherPlayer* CGameFramework::FindOtherPlayer(short id)
{
	for (auto& s : m_otherPlayers)
		if (s.id == id) return s.pPlayer;
	return nullptr;
}
bool CGameFramework::AddOtherPlayer(short id, OtherPlayer* p)
{
	for (auto& s : m_otherPlayers) {
		if (s.id == -1) { s.id = id; s.pPlayer = p; return true; }
	}
	return false;
}
void CGameFramework::RemoveOtherPlayer(short id)
{
	for (auto& s : m_otherPlayers) {
		if (s.id == id) {
			s.id = -1; s.pPlayer = nullptr;
			return;
		}
	}
}

CEnemyObject* CGameFramework::FindNpc(short id)
{
	for (auto& s : m_npcs)
		if (s.id == id) return s.pNpc;
	return nullptr;
}
bool CGameFramework::AddNpc(short id, CEnemyObject* p)
{
	for (auto& s : m_npcs) {
		if (s.id == -1) { s.id = id; s.pNpc = p; return true; }
	}
	return false;
}
void CGameFramework::RemoveNpc(short id)
{
	for (auto& s : m_npcs) {
		if (s.id == id) {
			s.id = -1;
			s.pNpc = nullptr;
			return;
		}
	}
}

void CGameFramework::ChangeScene()
{
	m_pScene.back()->ReleaseObjects();
	m_pScene.push_back(unique_ptr<CScene>(nextScene));
	m_pScene.back()->SetRoot(root->GetRoot());
	m_pScene.back()->SetPlayer(m_pPlayer);
	m_pScene.back()->BuildObjects(m_pd3dDevice, m_pd3dCommandList);
	m_pScene.back()->SetCamera(m_pCamera);

	nextScene = nullptr;
}
