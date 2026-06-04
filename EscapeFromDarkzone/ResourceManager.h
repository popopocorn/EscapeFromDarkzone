#pragma once
#include "UI.h"

enum class ModelName
{

	PLAYER_01 = 0,
	PLAYER_02,
	PLAYER_03,

	ENEMY_01_1,
	ENEMY_01_2,
	ENEMY_01_3,
	ENEMY_02,
	ENEMY_03_1,
	ENEMY_03_2,
	ENEMY_03_3,

	RIFLE,
	PISTOL,
	SHOTGUN,
	SMG,

	LOOT_BOX,

	PLAYER = PLAYER_01,
	ENEMY = ENEMY_01_1,

	MAP_FLOOR,

	MAP_BLOCK01,   // block1
	MAP_BLOCK03,
	MAP_BLOCK04,
	MAP_BLOCK05,
	MAP_BLOCK06,
	MAP_BLOCK10,
	MAP_BLOCK11,
	MAP_BLOCK12,
	MAP_BLOCK13,
	MAP_BLOCK14,
	MAP_BLOCK15,
	MAP_BLOCK16,
	MAP_BLOCK17,
	MAP_BLOCK18,
	MAP_BLOCK19,
	MAP_BLOCK20,
	MAP_BLOCK21,
	MAP_BLOCK22,
	MAP_BLOCK23,
	MAP_BLOCK24,
	MAP_BLOCK30,
	MAP_BLOCK31,
	MAP_BLOCK32,
	MAP_BLOCK33,
	MAP_BLOCK34,
	MAP_BLOCK35,
	MAP_BLOCK36,
	MAP_BLOCK37,
	MAP_BLOCK38,
	MAP_BLOCK40,

	MAP_BLOCK51,
	MAP_BLOCK52,
	MAP_BLOCK53,
	MAP_BLOCK54,

	MAP_GREEN1,
	MAP_GREEN2,

	MAP_BOUNDARY_1,
	MAP_BOUNDARY_2,
	MAP_BOUNDARY_3,
	MAP_BOUNDARY_4,

	MAP_FACTORY1,
	MAP_FACTORY_WALL_1,
	MAP_FACTORY_WALL_2,
	MAP_FACTORY_WALL_3,

	MAP_CONT_WALL_1,
	MAP_CONT_WALL_2,
	MAP_CONT_WALL_3,

	MAP_MAZE_C1,
	MAP_MAZE_C2,
	MAP_MAZE_C3,
	MAP_MAZE_C4,
	MAP_MAZE_C5,
	MAP_MAZE_C6,

	MAP_BLOCK_CB_1,
	MAP_BLOCK_CB_2,
	MAP_BLOCK_CB_3,
	MAP_BLOCK_CB_4,
	MAP_BLOCK_CB_5,
	MAP_BLOCK_CB_6,
	MAP_BLOCK_CB_7,
	MAP_BLOCK_CB_8,
	MAP_BLOCK_CB_9,

	MAP_CARS_C1,
	MAP_CARS_C2,
	MAP_CARS_C3,
	MAP_CARS_C4,
	MAP_CARS_C5,
	MAP_CARS_C6,

	MAP_ROAD,

	MODEL_TYPE_END
};

inline ModelName& operator++(ModelName& e)
{
	if (e == ModelName::MODEL_TYPE_END)
		return e; // 또는 assert

	e = static_cast<ModelName>(static_cast<int>(e) + 1);
	return e;
}

static vector<string>s_mapFiles = {
			"Model/floor.bin",
			"Model/block1.bin",
			"Model/block3.bin",
			"Model/block4.bin",
			"Model/block5.bin",
			"Model/block6.bin",
			"Model/block10.bin",
			"Model/block11.bin",
			"Model/block12.bin",
			"Model/block13.bin",
			"Model/block14.bin",
			"Model/block15.bin",
			"Model/block16.bin",
			"Model/block17.bin",
			"Model/block18.bin",
			"Model/block19.bin",
			"Model/block20.bin",
			"Model/block21.bin",
			"Model/block22.bin",
			"Model/block23.bin",
			"Model/block24.bin",
			"Model/block30.bin",
			"Model/block31.bin",
			"Model/block32.bin",
			"Model/block33.bin",
			"Model/block34.bin",
			"Model/block35.bin",
			"Model/block36.bin",
			"Model/block37.bin",
			"Model/block38.bin",
			"Model/block40.bin",

			"Model/block51.bin",
			"Model/block52.bin",
			"Model/block53.bin",
			"Model/block54.bin",

			"Model/green1.bin",
			"Model/green2.bin",

			"Model/boundary_1.bin",
			"Model/boundary_2.bin",
			"Model/boundary_3.bin",
			"Model/boundary_4.bin",

			"Model/factory1.bin",
			"Model/factory_wall_1.bin",
			"Model/factory_wall_2.bin",
			"Model/factory_wall_3.bin",

			"Model/cont_wall_1.bin",
			"Model/cont_wall_2.bin",
			"Model/cont_wall_3.bin",

			"Model/maze_c1.bin",
			"Model/maze_c2.bin",
			"Model/maze_c3.bin",
			"Model/maze_c4.bin",
			"Model/maze_c5.bin",
			"Model/maze_c6.bin",

			"Model/block_cb_1.bin",
			"Model/block_cb_2.bin",
			"Model/block_cb_3.bin",
			"Model/block_cb_4.bin",
			"Model/block_cb_5.bin",
			"Model/block_cb_6.bin",
			"Model/block_cb_7.bin",
			"Model/block_cb_8.bin",
			"Model/block_cb_9.bin",

			"Model/cars_c1.bin",
			"Model/cars_c2.bin",
			"Model/cars_c3.bin",
			"Model/cars_c4.bin",
			"Model/cars_c5.bin",
			"Model/cars_c6.bin",

			"Model/road.bin",
};
enum class UIName {
	LOBBY_BACKGROUND = 0,
	LOBBY_START_BUTTON,

};
// 전방 선언
class CLoadedModelInfo;
class CGameObject;
class CTexture;
class ShadowMap;
class CShader;
//class UIMesh;

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

	// UI 원본
	unordered_map<UIName, unique_ptr<UIMesh>> m_UIPrototypes;

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

	void ReleaseUploadBuffers();

	void ReleaseResources();

	// 플레이어 / 무기 / 일반 모델 원본 등록

	//player mesh object load
	void BuildPlayerModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* PlayerShader
	);

	//skinned mesh object load
	void BuildSkinnedModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* SkinnedShader
	);

	//static mesh object load
	void BuildModelPrototypes(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		CShader* StandardShader
	);


	//ui object load
	void BuildUIMesh(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature
	);

	// 정적 모델 원본 가져오기
	CGameObject* GetModelPrototype(ModelName key) const;

	// 스킨드 모델 인스턴스 생성
	CLoadedModelInfo* CreateSkinnedModelInstance(ModelName key);

	UIMesh* GetUIMesh(UIName name);
};