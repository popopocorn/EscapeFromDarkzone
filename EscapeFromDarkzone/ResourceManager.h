#pragma once



enum class ResourceName {

};

enum class ModelName
{
	PLAYER = 0,
	ENEMY,
	RIFLE,
	PISTOL,
	SHOTGUN
};

//전방선언
class CLoadedModelInfo;
class CGameObject;
class UIMesh;
class CTexture;
class ShadowMap;
class CShader;


class ResourceManager{
private:
	ID3D12DescriptorHeap* m_pd3dCbvSrvDescriptorHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dCbvCPUDescriptorStartHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dCbvGPUDescriptorStartHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dSrvCPUDescriptorStartHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dSrvGPUDescriptorStartHandle;

	D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dCbvCPUDescriptorNextHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dCbvGPUDescriptorNextHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_d3dSrvCPUDescriptorNextHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	m_d3dSrvGPUDescriptorNextHandle;


	unordered_map<ResourceName, unique_ptr<CLoadedModelInfo>> skinnedModelpool;
	unordered_map<ResourceName, unique_ptr<CGameObject>> Modelpool;
	unordered_map<ResourceName, unique_ptr<UIMesh>> UIpool;

	unordered_map<ModelName, CGameObject*> m_ModelPrototypes;
public:
	void CreateCbvSrvDescriptorHeaps(ID3D12Device* pd3dDevice, int nConstantBufferViews, int nShaderResourceViews);

	D3D12_GPU_DESCRIPTOR_HANDLE CreateConstantBufferViews(ID3D12Device* pd3dDevice, int nConstantBufferViews, ID3D12Resource* pd3dConstantBuffers, UINT nStride);
	void CreateShaderResourceViews(ID3D12Device* pd3dDevice, CTexture* pTexture, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex);
	void CreateshadowResourceViews(ID3D12Device* pd3dDevice, ShadowMap* shadowmap, UINT nDescriptorHeapIndex, UINT nRootParameterStartIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorStartHandle() { return(m_d3dCbvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorStartHandle() { return(m_d3dCbvGPUDescriptorStartHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorStartHandle() { return(m_d3dSrvCPUDescriptorStartHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorStartHandle() { return(m_d3dSrvGPUDescriptorStartHandle); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUCbvDescriptorNextHandle() { return(m_d3dCbvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUCbvDescriptorNextHandle() { return(m_d3dCbvGPUDescriptorNextHandle); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSrvDescriptorNextHandle() { return(m_d3dSrvCPUDescriptorNextHandle); }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSrvDescriptorNextHandle() { return(m_d3dSrvGPUDescriptorNextHandle); }
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_pd3dCbvSrvDescriptorHeap; }

	void ReleaseResources();

	static ResourceManager& Instance() 
	{
		static ResourceManager instance;
		return instance;
	}

	//object pooling
	CLoadedModelInfo* GetSkinnedModel(ResourceName name);
	CGameObject* GetModel(ResourceName name);
	UIMesh* GetUI(ResourceName name);

	// 모델 원본 prototype 관리
	bool LoadAndRegisterModelPrototype(ModelName key, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, const char* modelPath, CShader* pShader);
	
	void RegisterModelPrototype(ModelName key, CGameObject* pPrototype);
	CGameObject* GetModelPrototype(ModelName key) const;
	void ReleaseModelPrototypes();

	void BuildModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* pPlayerShader
	);
private:
	ResourceManager() {};
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
};

