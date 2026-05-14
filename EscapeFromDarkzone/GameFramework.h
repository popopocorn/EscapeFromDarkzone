#pragma once

//#define FRAME_BUFFER_WIDTH		640
//#define FRAME_BUFFER_HEIGHT		480

#include "Timer.h"
#include "Player.h"
#include "Scene.h"
#include "ShadowMap.h"

#include "Network.h"	// 03.27 추가

#include "OtherPlayer.h"	// 03.30 추가
#include <array>			// 05.08 추가

struct OtherPlayerSlot {
	short id = -1;
	OtherPlayer* pPlayer = nullptr;
};

struct NpcSlot {
	short id;
	CEnemyObject* pNpc;
	NpcSlot() : id(-1), pNpc(nullptr) {}
};

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

	CEnemyObject* FindNpc(short id);		// 05.10 추가
	bool AddNpc(short id, CEnemyObject* p);	// 05.10 추가
	void RemoveNpc(short id);				// 05.10 추가

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

	MainScene						*m_pScene = NULL;	//소유용
	CPlayer						*m_pPlayer = NULL;	//소유용
	CCamera						*m_pCamera = NULL;	// 참조용
	std::unique_ptr<CCamera>	observer;
	std::unique_ptr<ShadowMap>	shadowmap;
	
	bool						observing = false;

	POINT						m_ptOldCursorPos;

	_TCHAR						m_pszFrameRate[70];


	// 05.05 추가: unordered_map에서 array로, 최대 32
	short m_myId = -1;
	std::array<OtherPlayerSlot, 32> m_otherPlayers;	

	OtherPlayer* FindOtherPlayer(short id);
	bool AddOtherPlayer(short id, OtherPlayer* p);
	void RemoveOtherPlayer(short id);
	
	// 05.10 추가: 서버로부터 받아올 NPC 정보 관리
	
	std::array<NpcSlot, 32> m_npcs;	// MAX_NPC(128) 로 했을 때 간헐적으로 메모리오염 발생 -> 64로 줄였음.
};

