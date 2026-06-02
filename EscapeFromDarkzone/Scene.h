//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"
#include "Player.h"
#include "EnemyObject.h"
#include "Effect.h"
#include "ResourceManager.h"
#include "Network.h"	// 05.14 추가: 네트워크 피격

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

enum SHADERIDX
{
	MAP = 0,
	VIEW = 1,
	ENEMY = 2,
	LOOT = 3
};

struct ColResult;
class CollisionManager;
class Inventory;
class AstarNavigation;
class EffectManager;
class InventoryManager;

class CGameFramework;
class ShaderManager;

class CScene
{
public:
	CScene() = default;
    CScene(CGameFramework* game);
    virtual ~CScene();

	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) {};
	virtual bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) { return false; }

	void DumpMapOOBBToCSV(const char* filename);

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void BuildDefaultLightsAndMaterials();
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {};
	void ReleaseObjects() {};

	void SetRoot(ID3D12RootSignature* root) { m_pd3dGraphicsRootSignature = root; }

	virtual bool ProcessInput(UCHAR *pKeysBuffer);
	virtual void AnimateObjects(float fTimeElapsed) {};
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera = NULL) {};
	virtual void ThroughRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL) {};

	virtual void ReleaseUploadBuffers() {
		if (m_pSkyBox) m_pSkyBox->ReleaseUploadBuffers();

		for (int i = 0; i < m_ppShaders.size(); i++)
		{
			if (m_ppShaders[i])
				m_ppShaders[i]->ReleaseUploadBuffers();
		}
	};

	CPlayer								*m_pPlayer = NULL;//참조용 객체 관리 X, raw포인터가 맞음

protected:
	ID3D12RootSignature					*m_pd3dGraphicsRootSignature = NULL;
	CCamera*							m_pCamera = nullptr;	
	
	LightCameraManager*					ShadowCameraManager;
	CGameFramework*						frame;
	unique_ptr<HUDManager>				uiManager;

public:

	float								m_fElapsedTime = 0.0f;

	ShaderManager*						shadermanager = nullptr;
	vector<CShader*>					m_ppShaders;

	unique_ptr<CSkyBox>								m_pSkyBox;

	vector<LIGHT>									m_pLights;
	int									m_nLights = 0;

	XMFLOAT4							m_xmf4GlobalAmbient;

	ID3D12Resource						*m_pd3dcbLights = NULL;
	LIGHTS								*m_pcbMappedLights = NULL;
	

	unique_ptr<UIObjectShader>			UIShader;
private:
	
public:
	virtual void SetPlayer(CPlayer* p) { m_pPlayer = p; }

	CCamera* GetLightCamera(int idx);

	LightCameraManager* GetLightCameraManager() { return ShadowCameraManager; }
	virtual void SetCamera(CCamera* pCamera) { m_pCamera = pCamera; }
	virtual void DeleteDeadObject(UINT64 Fence);
	virtual void DeleteTrash(UINT64 Fence);
	virtual EffectManager* GetEffectManager() { return NULL; }
	virtual InventoryManager* GetInventoryManager() { return NULL; }
};

class LobbyScene : public CScene {
public:
	LobbyScene(CGameFramework* game);
	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseObjects();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera = NULL);

private:

};


class MainScene : public CScene{
public:
	MainScene(CGameFramework* game);
	virtual ~MainScene();

	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseObjects();

	virtual bool ProcessInput(UCHAR* pKeysBuffer);
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera = NULL);
	virtual void ThroughRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL);

	virtual void ReleaseUploadBuffers();
protected:
	unique_ptr<AstarNavigation> AStarNav;
public:
	std::unique_ptr<CBoundingBoxShader> m_pDebugShader = nullptr;
	std::unique_ptr<CBoundingBoxShader> m_pLootBoxShader = nullptr;
	CBoundingBoxShader* GetLootBoxShader() { return m_pLootBoxShader.get(); }

	// 이펙트/레이저 렌더링은 EffectManager가 담당
	EffectManager* m_pEffectManager = nullptr;
	virtual EffectManager* GetEffectManager() { return m_pEffectManager; }

	// 입력/무기 상태는 Scene이 계속 들고 있음
	bool m_bLaserActive = false;
	CGameObject* m_pLaserMuzzle = nullptr;
	float m_fLaserLength = 15.0f;

	bool m_bSparkFireActive = false;
	float m_fSparkSpawnTimer = 0.0f;
	float m_fSparkSpawnInterval = 0.03f;

	CGameObject* m_pWeaponMuzzle = nullptr;

	vector<CGameObject*> m_vVisionMapChunks;	//blocker용 벡터
private:
	unique_ptr<CollisionManager> colManager;

	InventoryManager* m_pInventoryManager = nullptr;
	//InventoryManager* GetInventoryManager() { return m_pInventoryManager; }

	Inventory* inventory = nullptr;
	Inventory* corpseInventory = nullptr;
	Inventory* craftInventory = nullptr;

	float m_fLootInteractDistance = 3.0f;

	std::unique_ptr<CFogOverlayShader> m_pFogOverlayShader;
	void LinkToPlayer();
public:

	//이펙트	재생용 함수
	void PlayEffectFromServerLikeRequest(
		EffectID effectId,
		const XMFLOAT3& position,
		const XMFLOAT3& direction,
		int ownerId = 0,
		float value = 0.0f
	);

	void PlayTestEffectByKey(WPARAM keyCode);

	LightCameraManager* GetLightCameraManager() { return ShadowCameraManager; }
	void SetCamera(CCamera* pCamera) { m_pCamera = pCamera; }
	void DeleteDeadObject(UINT64 Fence) {};
	void DeleteTrash(UINT64 Fence) {};

	//인벤토리용 함수
	bool IsAnyInventoryOpen() const;
	void CloseCorpseInventory();												// 시체 인벤토리 닫고 표시 데이터 초기화
	void OpenLootContainer(CLootContainerObject* pLoot);						// 특정 루팅 오브젝트의 인벤토리를 UI에 열기

	virtual InventoryManager* GetInventoryManager() { return m_pInventoryManager; }		// private에서 public으로 옮김 (05.16)

};