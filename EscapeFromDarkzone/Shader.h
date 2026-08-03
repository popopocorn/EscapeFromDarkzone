//-----------------------------------------------------------------------------
// File: Shader.h
//-----------------------------------------------------------------------------

#pragma once

#include "Object.h"
#include "Camera.h"
#include "DebugObject.h"
#include "UI.h"
class CGameObject;
class CCamera;
class CDebugObject;

struct Trash {
	CGameObject* obj;
	UINT64 FenceValue;
};

class CShader
{
public:
	CShader();
	virtual ~CShader();

private:
	int									m_nReferences = 0;
	bool								do_shadow = false;
protected:
	std::vector<CGameObject*>			m_ppObjects;
public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState();
	virtual D3D12_BLEND_DESC CreateBlendState();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
	virtual D3D12_SHADER_BYTECODE CreatePixelShader();

	D3D12_SHADER_BYTECODE CompileShaderFromFile(const WCHAR *pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob **ppd3dShaderBlob);
	D3D12_SHADER_BYTECODE ReadCompiledShaderFromFile(const WCHAR *pszFileName, ID3DBlob **ppd3dShaderBlob=NULL);

	virtual void CreateShader(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature);
	virtual void CreateShadowShader(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature);

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList) { }
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList) { }
	virtual void ReleaseShaderVariables() { }

	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList *pd3dCommandList, XMFLOAT4X4 *pxmf4x4World) { }

	virtual void OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, int nPipelineState);
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera, bool batch, int nPipelineState);

	virtual void ReleaseUploadBuffers() { }

	virtual void BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CLoadedModelInfo *pModel, void *pContext = NULL) { }
	virtual void AnimateObjects(float fTimeElapsed) { }
	virtual void ReleaseObjects() { }

	bool DoShadow() { return do_shadow; }
	virtual void DeleteObject(UINT64 fence ) {};
	virtual void ProcessingGarbageQueue(UINT64 completed) {};
	virtual void addObjects(CGameObject* obj) { m_ppObjects.push_back(obj); }
	std::vector<CGameObject*>* GetObj() { return &m_ppObjects; }

protected:
	ID3DBlob							*m_pd3dVertexShaderBlob = NULL;
	ID3DBlob							*m_pd3dPixelShaderBlob = NULL;


	D3D12_GRAPHICS_PIPELINE_STATE_DESC	m_d3dPipelineStateDesc;

	float								m_fElapsedTime = 0.0f;
	std::queue<Trash>					GarbageQueue;
public: 
	std::vector<ID3D12PipelineState*> m_pd3dPipelineState;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Bounding Box Shader선언
struct VS_CB_DEBUG_INFO
{
	XMFLOAT4X4 m_xmf4x4World;
};
struct DebugInstance
{
	CGameObject* m_pTargetObject = nullptr;
	XMFLOAT4X4   m_xmf4x4Local;			// 미리 계산된 로컬 변환 행렬
};
class CBoundingBoxShader : public CShader
{
public:
	CBoundingBoxShader(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		bool bSolid = false
	);
	virtual ~CBoundingBoxShader();

protected:
	bool m_bSolid = false;
	std::unique_ptr<CDebugObject> m_pDebugObject;
	std::vector<DebugInstance> m_DebugInstances;

	void CleanupDeadInstances();
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	virtual D3D12_SHADER_BYTECODE CreateVertexShader() override;
	virtual D3D12_SHADER_BYTECODE CreatePixelShader() override;
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;

	void CreateShader(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	void AddObject(CGameObject* pGameObject);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSkyBoxShader : public CShader
{
public:
	CSkyBoxShader();
	virtual ~CSkyBoxShader();

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
	virtual D3D12_SHADER_BYTECODE CreatePixelShader();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CStandardShader : public CShader
{
public:
	CStandardShader();
	virtual ~CStandardShader();

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
	virtual D3D12_SHADER_BYTECODE CreatePixelShader();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//스텐실X쉐이더
class CStandardObjectsShader : public CStandardShader
{
public:
	CStandardObjectsShader();
	virtual ~CStandardObjectsShader();

	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CLoadedModelInfo* pModel, void* pContext = NULL);
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void ReleaseObjects();

	virtual void ReleaseUploadBuffers();

	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch, int nPipelineState);
	virtual void DeleteObject(UINT64 fence);
	virtual void ProcessingGarbageQueue(UINT64 completed);
	
};

class CLaserShader : public CStandardObjectsShader
{
public:
	CLaserShader() {}
	virtual ~CLaserShader() {}
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch, int nPipelineState);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader() override;
};
//map, object


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CSkinnedAnimationStandardShader : public CStandardShader
{
public:
	CSkinnedAnimationStandardShader();
	virtual ~CSkinnedAnimationStandardShader();

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//스텐실O
class CSkinnedAnimationObjectsShader : public CSkinnedAnimationStandardShader
{
public:
	CSkinnedAnimationObjectsShader();
	virtual ~CSkinnedAnimationObjectsShader();

	virtual void BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CLoadedModelInfo *pModel, void *pContext = NULL);
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void ReleaseObjects();
	virtual void ReleaseUploadBuffers();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera, bool batch, int nPipelineState);
	virtual void DeleteObject(UINT64 fence);
	virtual void ProcessingGarbageQueue(UINT64 completed);	
};

class ViewShader : public CStandardShader
{
public:
	ViewShader();

	//시야 메쉬용 셰이더
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	virtual D3D12_SHADER_BYTECODE CreateVertexShader() override;
	virtual D3D12_SHADER_BYTECODE CreatePixelShader() override;
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
	virtual D3D12_BLEND_DESC CreateBlendState() override;

	virtual void CreateThroughShader(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual D3D12_DEPTH_STENCIL_DESC CreateThroughDepthStencilState();
	
	virtual void ReleaseObjects();
	//시야 오브젝트 갱신
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch, int nPipelineState);
};

class PlayerShader : public CSkinnedAnimationStandardShader {

public:
	PlayerShader();
	virtual void CreateThroughShader(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual D3D12_DEPTH_STENCIL_DESC CreateThroughDepthStencilState();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();
};


class UIObjectShader : public CShader {
private:
	std::vector<UIObject*>		m_ppObjects;
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
	virtual D3D12_SHADER_BYTECODE CreatePixelShader();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();
	void addObjects(UIObject* obj) { m_ppObjects.push_back(obj); }
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch, int nPipelineState);

	
};

class CFogOverlayShader : public CShader
{
public:
	CFogOverlayShader() = default;
	virtual ~CFogOverlayShader() = default;

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	virtual D3D12_BLEND_DESC CreateBlendState() override;
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;

	virtual D3D12_SHADER_BYTECODE CreateVertexShader() override;
	virtual D3D12_SHADER_BYTECODE CreatePixelShader() override;

	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, bool batch, int nPipelineState) override;
};


class FullscreenShader : public CShader {

public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader();
	virtual D3D12_SHADER_BYTECODE CreatePixelShader();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();

};