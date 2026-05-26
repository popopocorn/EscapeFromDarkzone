#pragma once

enum class ModelName
{
	PLAYER_01 = 0,
	PLAYER_02,
	PLAYER_03,

	ENEMY_01,
	ENEMY_02,
	ENEMY_03,

	RIFLE,
	PISTOL,
	SHOTGUN,

	LOOT_BOX,

	PLAYER = PLAYER_01,
	ENEMY = ENEMY_01
};

// 전방 선언
class CLoadedModelInfo;
class CGameObject;
class CTexture;
class ShadowMap;
class CShader;

class ResourceManager
{
private:
	ID3D12DescriptorHeap* m_pd3dCbvSrvDescriptorHeap = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE m_d3dCbvCPUDescriptorStartHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dCbvGPUDescriptorStartHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvCPUDescriptorStartHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dSrvGPUDescriptorStartHandle{};

	D3D12_CPU_DESCRIPTOR_HANDLE m_d3dCbvCPUDescriptorNextHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dCbvGPUDescriptorNextHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvCPUDescriptorNextHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dSrvGPUDescriptorNextHandle{};

	// 애니메이션 없는 모델 원본
	unordered_map<ModelName, unique_ptr<CGameObject>> m_ModelPrototypes;

	// 애니메이션 있는 모델 원본
	unordered_map<ModelName, unique_ptr<CLoadedModelInfo>> m_SkinnedModelPrototypes;

	//UI 원본
	//unordered_map<UIName, UI*> m_UIPrototypes;

private:
	ResourceManager() = default;
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	// 정적 모델 원본 등록
	bool LoadAndRegisterModelPrototype(
		ModelName key,
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		const char* modelPath,
		CShader* pShader
	);

	// 스킨드 애니메이션 모델 원본 등록
	bool LoadAndRegisterSkinnedModelPrototype(
		ModelName key,
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		const char* modelPath,
		CShader* pShader
	);

	void ReleaseSkinnedModelPrototypes();

public:
	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}

	void CreateCbvSrvDescriptorHeaps(
		ID3D12Device* pd3dDevice,
		int nConstantBufferViews,
		int nShaderResourceViews
	);

	D3D12_GPU_DESCRIPTOR_HANDLE CreateConstantBufferViews(
		ID3D12Device* pd3dDevice,
		int nConstantBufferViews,
		ID3D12Resource* pd3dConstantBuffers,
		UINT nStride
	);

	void CreateShaderResourceViews(
		ID3D12Device* pd3dDevice,
		CTexture* pTexture,
		UINT nDescriptorHeapIndex,
		UINT nRootParameterStartIndex
	);

	void CreateshadowResourceViews(
		ID3D12Device* pd3dDevice,
		ShadowMap* shadowmap,
		UINT nDescriptorHeapIndex,
		UINT nRootParameterStartIndex
	);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorStartHandle() { return m_d3dCbvCPUDescriptorStartHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorStartHandle() { return m_d3dCbvGPUDescriptorStartHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorStartHandle() { return m_d3dSrvCPUDescriptorStartHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorStartHandle() { return m_d3dSrvGPUDescriptorStartHandle; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorNextHandle() { return m_d3dCbvCPUDescriptorNextHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorNextHandle() { return m_d3dCbvGPUDescriptorNextHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorNextHandle() { return m_d3dSrvCPUDescriptorNextHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorNextHandle() { return m_d3dSrvGPUDescriptorNextHandle; }

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_pd3dCbvSrvDescriptorHeap; }

	void ReleaseResources();

	// 플레이어 / 무기 / 일반 모델 원본 등록
	void BuildModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pPlayerShader
	);

	// 적 모델 원본 등록
	void BuildEnemyModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pEnemyShader
	);

	//루팅 모델 원본 등록
	void BuildLootModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pLootShader
	);

	// 정적 모델 원본 가져오기
	CGameObject* GetModelPrototype(ModelName key) const;

	// 스킨드 모델 인스턴스 생성
	CLoadedModelInfo* CreateSkinnedModelInstance(ModelName key);

	
};