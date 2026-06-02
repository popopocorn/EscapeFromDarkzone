//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------

#pragma once

class CGameObject;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
#define VERTEXT_POSITION				0x0001
#define VERTEXT_COLOR					0x0002
#define VERTEXT_NORMAL					0x0004
#define VERTEXT_TANGENT					0x0008
#define VERTEXT_TEXTURE_COORD0			0x0010
#define VERTEXT_TEXTURE_COORD1			0x0020

#define VERTEXT_BONE_INDEX_WEIGHT		0x1000

#define VERTEXT_TEXTURE					(VERTEXT_POSITION | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_DETAIL					(VERTEXT_POSITION | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)
#define VERTEXT_NORMAL_TEXTURE			(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_NORMAL_TANGENT_TEXTURE	(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TANGENT | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_NORMAL_DETAIL			(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)
#define VERTEXT_NORMAL_TANGENT__DETAIL	(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TANGENT | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CMesh
{
public:
	CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual ~CMesh();

private:
	int								m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }


	XMFLOAT3						GetAABBCenter() const { return m_xmf3AABBCenter; }
	XMFLOAT3						GetAABBExtents() const { return m_xmf3AABBExtents; }
public:
	char							m_pstrMeshName[64] = { 0 };

protected:
	UINT							m_nType = 0x00;

	XMFLOAT3						m_xmf3AABBCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3						m_xmf3AABBExtents = XMFLOAT3(0.0f, 0.0f, 0.0f);


	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;
	UINT							m_nOffset = 0;

protected:
	int								m_nVertices = 0;
	XMFLOAT3						m_nStrides = {};

	XMFLOAT3						*m_pxmf3Positions = NULL;

	ID3D12Resource					*m_pd3dPositionBuffer = NULL;
	ID3D12Resource					*m_pd3dPositionUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dPositionBufferView;

	int								m_nSubMeshes = 0;
	int								*m_pnSubSetIndices = NULL;
	UINT							**m_ppnSubSetIndices = NULL;

	ID3D12Resource					**m_ppd3dSubSetIndexBuffers = NULL;
	ID3D12Resource					**m_ppd3dSubSetIndexUploadBuffers = NULL;
	D3D12_INDEX_BUFFER_VIEW			*m_pd3dSubSetIndexBufferViews = NULL;

public:
	UINT GetType() { return(m_nType); }

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList) { }
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList) { }
	virtual void ReleaseShaderVariables() { }

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nSubSet = 0, int nInstances = 1);
	virtual void OnPostRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext);
	XMFLOAT3 GetAABBCenter() { return m_xmf3AABBCenter; }
	XMFLOAT3 GetAABBExtents() { return m_xmf3AABBExtents; }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CSkyBoxMesh : public CMesh
{
public:
	CSkyBoxMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, float fWidth = 20.0f, float fHeight = 20.0f, float fDepth = 20.0f);
	virtual ~CSkyBoxMesh();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CStandardMesh : public CMesh
{
public:
	CStandardMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual ~CStandardMesh();

protected:
	XMFLOAT4						*m_pxmf4Colors = NULL;
	XMFLOAT3						*m_pxmf3Normals = NULL;
	XMFLOAT3						*m_pxmf3Tangents = NULL;
	XMFLOAT3						*m_pxmf3BiTangents = NULL;

	XMFLOAT2						*m_pxmf2TextureCoords0 = NULL;
	XMFLOAT2						*m_pxmf2TextureCoords1 = NULL;

	ID3D12Resource					*m_pd3dTextureCoord0Buffer = NULL;
	ID3D12Resource					*m_pd3dTextureCoord0UploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTextureCoord0BufferView;

	ID3D12Resource					*m_pd3dTextureCoord1Buffer = NULL;
	ID3D12Resource					*m_pd3dTextureCoord1UploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTextureCoord1BufferView;

	ID3D12Resource					*m_pd3dNormalBuffer = NULL;
	ID3D12Resource					*m_pd3dNormalUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dNormalBufferView;

	ID3D12Resource					*m_pd3dTangentBuffer = NULL;
	ID3D12Resource					*m_pd3dTangentUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTangentBufferView;

	ID3D12Resource					*m_pd3dBiTangentBuffer = NULL;
	ID3D12Resource					*m_pd3dBiTangentUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBiTangentBufferView;

public:
	void LoadMeshFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, FILE *pInFile, const char* pstrFileName);

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
#define SKINNED_ANIMATION_BONES		256

class CSkinnedMesh : public CStandardMesh
{
public:
	CSkinnedMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual ~CSkinnedMesh();

public:	// Skinning Buffers
	ID3D12Resource					*m_pd3dBoneIndexBuffer = NULL;
	ID3D12Resource					*m_pd3dBoneIndexUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBoneIndexBufferView;

	ID3D12Resource					*m_pd3dBoneWeightBuffer = NULL;
	ID3D12Resource					*m_pd3dBoneWeightUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dBoneWeightBufferView;

public:	// Skinning Info
	int								m_nBonesPerVertex = 4;

	XMINT4*							m_pxmn4BoneIndices = NULL;
	XMFLOAT4*						m_pxmf4BoneWeights = NULL;

public:
	int								m_nSkinningBones = 0; 

	char							(*m_ppstrSkinningBoneNames)[64]; //[m_nSkinningBones]

	XMFLOAT4X4						*m_pxmf4x4BindPoseBoneOffsets = NULL; //[m_nSkinningBones], Transposed

	ID3D12Resource					*m_pd3dcbBindPoseBoneOffsets = NULL; //[m_nSkinningBones]
	XMFLOAT4X4						*m_pcbxmf4x4MappedBindPoseBoneOffsets = NULL; //[m_nSkinningBones]

	ID3D12Resource					*m_pd3dcbSkinningBoneTransforms = NULL; //[m_nSkinningBones], Pointer Only
	XMFLOAT4X4						*m_pcbxmf4x4MappedSkinningBoneTransforms = NULL; //[m_nSkinningBones]

public:
	
	void LoadSkinInfoFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, FILE *pInFile, const char* pstrFileName);

	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void ReleaseUploadBuffers();

	virtual void OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CLaserMesh : public CMesh
{
public:
	CLaserMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CLaserMesh() {}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CParticleMesh : public CMesh
{
public:
	CParticleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fWidth = 10.0f, float fHeight = 10.0f);
	virtual ~CParticleMesh();

	void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nInstances);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CViewCircleMesh : public CMesh
{
public:
	CViewCircleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fRadius = 1.0f, int nSlices = 72);
	virtual ~CViewCircleMesh() {}

	void UpdateClippedMesh(const XMFLOAT3& worldOrigin, const std::vector<CGameObject*>& blockers);

private:
	float m_fRadius = 1.0f;
	int m_nSlices = 72;
	XMFLOAT3* m_pMappedPositions = nullptr;
};

class CViewConeMesh : public CMesh
{
public:
	CViewConeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fRadius = 15.0f, float fAngleDegrees = 60.0f, int nSlices = 72);
	virtual ~CViewConeMesh() {}

	void UpdateClippedMesh(const XMFLOAT3& worldOrigin, float fYaw, const std::vector<CGameObject*>& blockers);

private:
	float m_fRadius = 15.0f;
	float m_fAngleDegrees = 60.0f;
	int m_nSlices = 72;
	XMFLOAT3* m_pMappedPositions = nullptr;
};
