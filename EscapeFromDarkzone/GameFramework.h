#pragma once

//#define FRAME_BUFFER_WIDTH		640
//#define FRAME_BUFFER_HEIGHT		480

#include "Timer.h"
#include "Player.h"
#include "Scene.h"
#include "ShadowMap.h"

#include "Network.h"	// 03.27 추가

#include "OtherPlayer.h"	// 03.30 추가
#include <unordered_map>	// 03.30 추가

class CGameFramework
{
public:
	CGameFramework();
	~CGameFramework();


	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	void OnDestroy();

	void CreateSwapChain();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	void CreateRtvAndDsvDescriptorHeaps();

	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	void ChangeSwapChainState();

    void BuildObjects();
    void ReleaseObjects();

    void ProcessInput();
    void AnimateObjects(float fTimeElapsed);
    void FrameAdvance();

	void WaitForGpuComplete();
	void MoveToNextFrame();

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ProcessNetworkPackets();	// 03.27 추가

private:
	HINSTANCE					m_hInstance;
	HWND						m_hWnd; 

	int							m_nWndClientWidth;
	int							m_nWndClientHeight;
        
	IDXGIFactory4				*m_pdxgiFactory = NULL;
	IDXGISwapChain3				*m_pdxgiSwapChain = NULL;
	ID3D12Device				*m_pd3dDevice = NULL;

	bool						m_bMsaa4xEnable = false;
	UINT						m_nMsaa4xQualityLevels = 0;

	static const UINT			m_nSwapChainBuffers = 2;
	UINT						m_nSwapChainBufferIndex;

	ID3D12Resource				*m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ID3D12DescriptorHeap		*m_pd3dRtvDescriptorHeap = NULL;

	ID3D12Resource				*m_pd3dDepthStencilBuffer = NULL;
	ID3D12DescriptorHeap		*m_pd3dDsvDescriptorHeap = NULL;

	//ID3D12CommandAllocator		*m_pd3dCommandAllocator = NULL;
	ID3D12CommandAllocator*		m_pd3dCommandAllocators[m_nSwapChainBuffers];
	ID3D12CommandQueue			*m_pd3dCommandQueue = NULL;
	ID3D12GraphicsCommandList	*m_pd3dCommandList = NULL;

	ID3D12Fence					*m_pd3dFence = NULL;
	UINT64						m_nFenceValues[m_nSwapChainBuffers];
	HANDLE						m_hFenceEvent;
	bool						mouseMove = false;

#if defined(_DEBUG)
	ID3D12Debug					*m_pd3dDebugController;
#endif

	CGameTimer					m_GameTimer;

	CScene						*m_pScene = NULL;	//소유용
	CPlayer						*m_pPlayer = NULL;	//소유용
	CCamera						*m_pCamera = NULL;	// 참조용
	std::unique_ptr<CCamera>	observer;
	std::unique_ptr<ShadowMap>	shadowmap;
	
	bool						observing = false;

	POINT						m_ptOldCursorPos;

	_TCHAR						m_pszFrameRate[70];

	// 03.30 추가: 내 ID 저장용 (OtherPlayer와 구분 용도)
	short m_myId = -1;
	std::unordered_map<short, OtherPlayer*> m_otherPlayers;
};

