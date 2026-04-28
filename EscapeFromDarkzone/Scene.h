//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"
#include "Player.h"
#include "EnemyObject.h"
#include "Effect.h"


#define MAX_LIGHTS						16 

#define POINT_LIGHT						1
#define SPOT_LIGHT						2
#define DIRECTIONAL_LIGHT				3

#define MAX_BOMB_EFFECTS 20				//폭탄 효과 최대 개수
struct LIGHT
{
	XMFLOAT4							m_xmf4Ambient;
	XMFLOAT4							m_xmf4Diffuse;
	XMFLOAT4							m_xmf4Specular;
	XMFLOAT3							m_xmf3Position;
	float 								m_fFalloff;
	XMFLOAT3							m_xmf3Direction;
	float 								m_fTheta; //cos(m_fTheta)
	XMFLOAT3							m_xmf3Attenuation;
	float								m_fPhi; //cos(m_fPhi)
	bool								m_bEnable;
	int									m_nType;
	float								m_fRange;
	float								padding;
};										
										
struct LIGHTS							
{										
	LIGHT								m_pLights[MAX_LIGHTS];
	XMFLOAT4							m_xmf4GlobalAmbient;
	int									m_nLights;
};

class ShadowMap;

enum SHADERIDX : size_t {
	MAP = 0,
	VIEW = 1,
	LASER = 2,
	ENEMY = 3,
	LOOT = 4,
};

struct ColResult;
class CollisionManager;
class Inventory;

class CScene
{
public:
    CScene();
    ~CScene();

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	void BuildDefaultLightsAndMaterials();
	void BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	void ReleaseObjects();

	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature *GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature); }

	bool ProcessInput(UCHAR *pKeysBuffer);
    void AnimateObjects(float fTimeElapsed);
    void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nPipelineState, CCamera *pCamera=NULL);
    void ThroughRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera=NULL);

	void ReleaseUploadBuffers();

	CPlayer								*m_pPlayer = NULL;//참조용 객체 관리 X, raw포인터가 맞음

protected:
	ID3D12RootSignature					*m_pd3dGraphicsRootSignature = NULL;
	CCamera* m_pCamera = nullptr;	
	
	static ID3D12DescriptorHeap			*m_pd3dCbvSrvDescriptorHeap;

	static D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dCbvCPUDescriptorStartHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dCbvGPUDescriptorStartHandle;
	static D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dSrvCPUDescriptorStartHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dSrvGPUDescriptorStartHandle;

	static D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dCbvCPUDescriptorNextHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dCbvGPUDescriptorNextHandle;
	static D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dSrvCPUDescriptorNextHandle;
	static D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dSrvGPUDescriptorNextHandle;

	LightCameraManager ShadowCameraManager;

public:
	static void CreateCbvSrvDescriptorHeaps(ID3D12Device *pd3dDevice, int nConstantBufferViews, int nShaderResourceViews);

	static D3D12_GPU_DESCRIPTOR_HANDLE CreateConstantBufferViews(ID3D12Device *pd3dDevice, int nConstantBufferViews, ID3D12Resource *pd3dConstantBuffers, UINT nStride);
	static void CreateShaderResourceViews(ID3D12Device* pd3dDevice, CTexture* pTexture, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex);
	void CreateshadowResourceViews(ID3D12Device* pd3dDevice, ShadowMap* shadowmap, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorStartHandle() { return(m_d3dCbvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorStartHandle() { return(m_d3dCbvGPUDescriptorStartHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorStartHandle() { return(m_d3dSrvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorStartHandle() { return(m_d3dSrvGPUDescriptorStartHandle); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorNextHandle() { return(m_d3dCbvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorNextHandle() { return(m_d3dCbvGPUDescriptorNextHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorNextHandle() { return(m_d3dSrvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorNextHandle() { return(m_d3dSrvGPUDescriptorNextHandle); }

	float								m_fElapsedTime = 0.0f;

	XMFLOAT3							m_xmf3RotatePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	vector<std::unique_ptr<CShader>>				m_ppShaders;

	unique_ptr<CSkyBox>								m_pSkyBox;

	vector<LIGHT>									m_pLights;
	int									m_nLights = 0;

	XMFLOAT4							m_xmf4GlobalAmbient;

	ID3D12Resource						*m_pd3dcbLights = NULL;
	LIGHTS								*m_pcbMappedLights = NULL;
	
	unique_ptr<CBoundingBoxShader> m_pDebugShader;

	vector<unique_ptr<CEffect>> m_vEffectPools[EFFECT_MAX];

	ID3D12Resource* m_pd3dInstBufferEffect[EFFECT_MAX];
	EFFECT_INFO* m_pMappedInstBufferEffect[EFFECT_MAX];
	D3D12_VERTEX_BUFFER_VIEW m_d3dInstBufferViewEffect[EFFECT_MAX];

	//레이저 관련 멤버 변수
	bool m_bLaserActive = false;
	CGameObject* m_pLaserMuzzle = nullptr;
	float m_fLaserLength = 15.0f;

	CMaterial* m_pEffectMaterials[EFFECT_MAX];
	unique_ptr<CParticleMesh> m_pEffectMesh;

	class CEffectShader* m_pEffectShader = NULL;

	//스파크 효과 관련 멤버 변수
	bool m_bSparkFireActive = false;
	float m_fSparkSpawnTimer = 0.0f;
	float m_fSparkSpawnInterval = 0.03f;

	CGameObject* m_pWeaponMuzzle = nullptr;

	CGameObject* m_pWeaponObject = nullptr;

	unique_ptr<UIObjectShader> UIShader;

	vector<CGameObject*> m_vVisionMapChunks;	//blocker용 벡터
private:
	CGameObject* m_pLaserObject = NULL;
	unique_ptr<CollisionManager> colManager;
	unique_ptr<Inventory> inventory;
	unique_ptr<Inventory> corpseInventory = nullptr;
	CLootContainerObject* m_pOpenedLoot = nullptr;
	float m_fLootInteractDistance = 3.0f;

public:

	void PlayEffect(EFFECT_TYPE type, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 up);

	void SetPlayer(CPlayer* p);

	CCamera* GetLightCamera(int idx);

	LightCameraManager GetLightCameraManager() { return ShadowCameraManager; }
	void SetCamera(CCamera* pCamera) { m_pCamera = pCamera; }
	void DeleteDeadObject(UINT64 Fence);
	void DeleteTrash(UINT64 Fence);

	//인벤토리용 함수
	bool IsAnyInventoryOpen() const;
	void CloseCorpseInventory();												// 시체 인벤토리 닫고 표시 데이터 초기화
	void OpenLootContainer(CLootContainerObject* pLoot);						// 특정 루팅 오브젝트의 인벤토리를 UI에 열기
	CLootContainerObject* FindNearestLootContainer(float fMaxDistance) const;	// 상호작용 거리 내 가장 가까운 루팅 오브젝트 찾기
	void SpawnLootContainerFromEnemy(CEnemyObject* pEnemy);						// 죽은 적 위치에 루팅 오브젝트 생성

	CGameObject* GetWeaponObject() { return m_pWeaponObject; }
	void SetWeaponObject(CGameObject* pWeaponObject) { m_pWeaponObject = pWeaponObject; }
};