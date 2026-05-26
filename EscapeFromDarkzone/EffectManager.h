#pragma once
#include "stdafx.h"
#include "Effect.h"
#include "Shader.h"

class CCamera;
class CGameObject;
class CParticleMesh;
class CEffectShader;
class CLaserShader;
class CMaterial;


// 씬에서는 요청만 받고, 실제 이펙트 생성/업데이트/렌더링은 이 매니저가 담당
class EffectManager
{
public:
	EffectManager();
	~EffectManager();

	void Initialize(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature
	);

	void Release();

	void ReleaseUploadBuffers();

	void Update(float fTimeElapsed);

	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState);

	void RequestPlayEffect(EFFECT_TYPE type, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 up);

	void PlayEffectByID(const EffectSpawnDesc& desc);

	void UpdateLaser(
		int ownerId,
		const XMFLOAT3& origin,
		const XMFLOAT3& right,
		const XMFLOAT3& up,
		const XMFLOAT3& dir,
		float fLength
	);

	void HideLaser(int ownerId);

private:
	CGameObject* GetOrCreateLaserObject(int ownerId);

	void BuildEffectBasis(
		const XMFLOAT3& direction,
		XMFLOAT3& outRight,
		XMFLOAT3& outUp
	);

private:
	ID3D12Device* m_pd3dDevice = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCommandList = nullptr;
	ID3D12RootSignature* m_pd3dGraphicsRootSignature = nullptr;

	CEffectShader* m_pEffectShader = nullptr;
	CParticleMesh* m_pEffectMesh = nullptr;
	CMaterial* m_pEffectMaterials[EFFECT_MAX];

	std::vector<std::unique_ptr<CEffect>> m_vEffectPools[EFFECT_MAX];

	ID3D12Resource* m_pd3dInstBufferEffect[EFFECT_MAX];
	EFFECT_INFO* m_pMappedInstBufferEffect[EFFECT_MAX];
	D3D12_VERTEX_BUFFER_VIEW m_d3dInstBufferViewEffect[EFFECT_MAX];

	CLaserShader* m_pLaserShader = nullptr;
	std::unordered_map<int, CGameObject*> m_LaserObjects;

	static constexpr int INITIAL_EFFECT_POOL_SIZE = 20;
};