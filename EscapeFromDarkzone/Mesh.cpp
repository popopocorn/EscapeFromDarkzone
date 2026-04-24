//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Mesh.h"
#include "Object.h"
#include "EnemyObject.h"
#include "OtherPlayer.h"

static XMFLOAT3 RotateVectorY(const XMFLOAT3& v, float yaw)
{
	float c = cosf(yaw);
	float s = sinf(yaw);

	return XMFLOAT3(
		v.x * c + v.z * s,
		0.0f,
		-v.x * s + v.z * c
	);
}

// 시야ray랑 block 충돌 거리 중에 가까운 값 반환
static float RaycastVisionBlockers(const XMFLOAT3& worldOrigin, const XMFLOAT3& worldDir, float maxDistance, const std::vector<CGameObject*>& blockers)
{
	float nearest = maxDistance;

	XMVECTOR rayOrigin = XMLoadFloat3(&worldOrigin);
	XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&worldDir));

	for (CGameObject* pObj : blockers)
	{
		if (!pObj) continue;

		const auto& oobbs = pObj->GetOOBB();
		for (BoundingOrientedBox* pOOBB : oobbs)
		{
			if (!pOOBB) continue;

			if (pOOBB->Extents.y < 0.3f)
				continue;

			float hitDist = 0.0f;
			if (pOOBB->Intersects(rayOrigin, rayDir, hitDist))
			{
				float clipDist = hitDist;

				if (dynamic_cast<CEnemyObject*>(pObj) || dynamic_cast<OtherPlayer*>(pObj))
				{
					float bodyDepth = (pOOBB->Extents.x > pOOBB->Extents.z) ? pOOBB->Extents.x : pOOBB->Extents.z;
					bodyDepth = bodyDepth * 2.0f;
					clipDist = hitDist + bodyDepth + 0.05f;
				}
				else
				{
					clipDist = hitDist - 0.05f;
				}

				if (clipDist < 0.05f) clipDist = 0.05f;
				if (clipDist < nearest) nearest = clipDist;
			}
		}
	}

	if (nearest > maxDistance) nearest = maxDistance;
	if (nearest < 0.05f) nearest = 0.05f;

	return nearest;
}

CMesh::CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
}

CMesh::~CMesh()
{
	if (m_pd3dPositionBuffer) m_pd3dPositionBuffer->Release();

	if (m_nSubMeshes > 0)
	{
		for (int i = 0; i < m_nSubMeshes; i++)
		{
			if (m_ppd3dSubSetIndexBuffers[i]) m_ppd3dSubSetIndexBuffers[i]->Release();
			if (m_ppnSubSetIndices[i]) delete[] m_ppnSubSetIndices[i];
		}
		if (m_ppd3dSubSetIndexBuffers) delete[] m_ppd3dSubSetIndexBuffers;
		if (m_pd3dSubSetIndexBufferViews) delete[] m_pd3dSubSetIndexBufferViews;

		if (m_pnSubSetIndices) delete[] m_pnSubSetIndices;
		if (m_ppnSubSetIndices) delete[] m_ppnSubSetIndices;
	}

	if (m_pxmf3Positions) delete[] m_pxmf3Positions;
}

void CMesh::ReleaseUploadBuffers()
{
	if (m_pd3dPositionUploadBuffer) m_pd3dPositionUploadBuffer->Release();
	m_pd3dPositionUploadBuffer = NULL;

	if ((m_nSubMeshes > 0) && m_ppd3dSubSetIndexUploadBuffers)
	{
		for (int i = 0; i < m_nSubMeshes; i++)
		{
			if (m_ppd3dSubSetIndexUploadBuffers[i]) m_ppd3dSubSetIndexUploadBuffers[i]->Release();
		}
		if (m_ppd3dSubSetIndexUploadBuffers) delete[] m_ppd3dSubSetIndexUploadBuffers;
		m_ppd3dSubSetIndexUploadBuffers = NULL;
	}
}

void CMesh::OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext)
{
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dPositionBufferView);
}

void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nSubSet, int nInstances)
{
	UpdateShaderVariables(pd3dCommandList);

	OnPreRender(pd3dCommandList, NULL);

	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);

	if ((m_nSubMeshes > 0) && (nSubSet < m_nSubMeshes))
	{
		pd3dCommandList->IASetIndexBuffer(&(m_pd3dSubSetIndexBufferViews[nSubSet]));
		pd3dCommandList->DrawIndexedInstanced(m_pnSubSetIndices[nSubSet], nInstances, 0, 0, 0);
	}
	else
	{
		pd3dCommandList->DrawInstanced(m_nVertices, nInstances, m_nOffset, 0);
	}
}

void CMesh::OnPostRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkyBoxMesh::CSkyBoxMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, float fWidth, float fHeight, float fDepth) : CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 36;
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_pxmf3Positions = new XMFLOAT3[m_nVertices];

	float fx = fWidth * 0.5f, fy = fHeight * 0.5f, fz = fDepth * 0.5f;
	// Front Quad (quads point inward)
	m_pxmf3Positions[0] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[1] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[2] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[3] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[4] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[5] = XMFLOAT3(+fx, -fx, +fx);
	// Back Quad										
	m_pxmf3Positions[6] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[7] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[8] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[9] = XMFLOAT3(+fx, -fx, -fx);
	m_pxmf3Positions[10] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[11] = XMFLOAT3(-fx, -fx, -fx);
	// Left Quad										
	m_pxmf3Positions[12] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[13] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[14] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[15] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[16] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[17] = XMFLOAT3(-fx, -fx, +fx);
	// Right Quad										
	m_pxmf3Positions[18] = XMFLOAT3(+fx, +fx, +fx);
	m_pxmf3Positions[19] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[20] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[21] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[22] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[23] = XMFLOAT3(+fx, -fx, -fx);
	// Top Quad											
	m_pxmf3Positions[24] = XMFLOAT3(-fx, +fx, -fx);
	m_pxmf3Positions[25] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[26] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[27] = XMFLOAT3(-fx, +fx, +fx);
	m_pxmf3Positions[28] = XMFLOAT3(+fx, +fx, -fx);
	m_pxmf3Positions[29] = XMFLOAT3(+fx, +fx, +fx);
	// Bottom Quad										
	m_pxmf3Positions[30] = XMFLOAT3(-fx, -fx, +fx);
	m_pxmf3Positions[31] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[32] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[33] = XMFLOAT3(-fx, -fx, -fx);
	m_pxmf3Positions[34] = XMFLOAT3(+fx, -fx, +fx);
	m_pxmf3Positions[35] = XMFLOAT3(+fx, -fx, -fx);

	m_pd3dPositionBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3Positions, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;
}

CSkyBoxMesh::~CSkyBoxMesh()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
CStandardMesh::CStandardMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList) : CMesh(pd3dDevice, pd3dCommandList)
{
}

CStandardMesh::~CStandardMesh()
{
	if (m_pd3dTextureCoord0Buffer) m_pd3dTextureCoord0Buffer->Release();
	if (m_pd3dNormalBuffer) m_pd3dNormalBuffer->Release();
	if (m_pd3dTangentBuffer) m_pd3dTangentBuffer->Release();
	if (m_pd3dBiTangentBuffer) m_pd3dBiTangentBuffer->Release();

	if (m_pxmf4Colors) delete[] m_pxmf4Colors;
	if (m_pxmf3Normals) delete[] m_pxmf3Normals;
	if (m_pxmf3Tangents) delete[] m_pxmf3Tangents;
	if (m_pxmf3BiTangents) delete[] m_pxmf3BiTangents;
	if (m_pxmf2TextureCoords0) delete[] m_pxmf2TextureCoords0;
	if (m_pxmf2TextureCoords1) delete[] m_pxmf2TextureCoords1;
}

void CStandardMesh::ReleaseUploadBuffers()
{
	CMesh::ReleaseUploadBuffers();

	if (m_pd3dTextureCoord0UploadBuffer) m_pd3dTextureCoord0UploadBuffer->Release();
	m_pd3dTextureCoord0UploadBuffer = NULL;

	if (m_pd3dNormalUploadBuffer) m_pd3dNormalUploadBuffer->Release();
	m_pd3dNormalUploadBuffer = NULL;

	if (m_pd3dTangentUploadBuffer) m_pd3dTangentUploadBuffer->Release();
	m_pd3dTangentUploadBuffer = NULL;

	if (m_pd3dBiTangentUploadBuffer) m_pd3dBiTangentUploadBuffer->Release();
	m_pd3dBiTangentUploadBuffer = NULL;
}

void CStandardMesh::LoadMeshFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, FILE *pInFile)
{
	char pstrToken[64] = { '\0' };
	int nPositions = 0, nColors = 0, nNormals = 0, nTangents = 0, nBiTangents = 0, nTextureCoords = 0, nIndices = 0, nSubMeshes = 0, nSubIndices = 0;

	UINT nReads = (UINT)::fread(&m_nVertices, sizeof(int), 1, pInFile);

	::ReadStringFromFile(pInFile, m_pstrMeshName);

	for ( ; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken);
		if (!strcmp(pstrToken, "<Bounds>:"))
		{
			nReads = (UINT)::fread(&m_xmf3AABBCenter, sizeof(XMFLOAT3), 1, pInFile);
			nReads = (UINT)::fread(&m_xmf3AABBExtents, sizeof(XMFLOAT3), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Positions>:"))
		{
			nReads = (UINT)::fread(&nPositions, sizeof(int), 1, pInFile);
			if (nPositions > 0)
			{
				m_nType |= VERTEXT_POSITION;
				m_pxmf3Positions = new XMFLOAT3[nPositions];
				nReads = (UINT)::fread(m_pxmf3Positions, sizeof(XMFLOAT3), nPositions, pInFile);

				m_pd3dPositionBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3Positions, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

				m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
				m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<Colors>:"))
		{
			nReads = (UINT)::fread(&nColors, sizeof(int), 1, pInFile);
			if (nColors > 0)
			{
				m_nType |= VERTEXT_COLOR;
				m_pxmf4Colors = new XMFLOAT4[nColors];
				nReads = (UINT)::fread(m_pxmf4Colors, sizeof(XMFLOAT4), nColors, pInFile);
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords0>:"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
				m_nType |= VERTEXT_TEXTURE_COORD0;
				m_pxmf2TextureCoords0 = new XMFLOAT2[nTextureCoords];
				nReads = (UINT)::fread(m_pxmf2TextureCoords0, sizeof(XMFLOAT2), nTextureCoords, pInFile);

				m_pd3dTextureCoord0Buffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf2TextureCoords0, sizeof(XMFLOAT2) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTextureCoord0UploadBuffer);

				m_d3dTextureCoord0BufferView.BufferLocation = m_pd3dTextureCoord0Buffer->GetGPUVirtualAddress();
				m_d3dTextureCoord0BufferView.StrideInBytes = sizeof(XMFLOAT2);
				m_d3dTextureCoord0BufferView.SizeInBytes = sizeof(XMFLOAT2) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords1>:"))
		{
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, pInFile);
			if (nTextureCoords > 0)
			{
				m_nType |= VERTEXT_TEXTURE_COORD1;
				m_pxmf2TextureCoords1 = new XMFLOAT2[nTextureCoords];
				nReads = (UINT)::fread(m_pxmf2TextureCoords1, sizeof(XMFLOAT2), nTextureCoords, pInFile);

				m_pd3dTextureCoord1Buffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf2TextureCoords1, sizeof(XMFLOAT2) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTextureCoord1UploadBuffer);

				m_d3dTextureCoord1BufferView.BufferLocation = m_pd3dTextureCoord1Buffer->GetGPUVirtualAddress();
				m_d3dTextureCoord1BufferView.StrideInBytes = sizeof(XMFLOAT2);
				m_d3dTextureCoord1BufferView.SizeInBytes = sizeof(XMFLOAT2) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<Normals>:"))
		{
			nReads = (UINT)::fread(&nNormals, sizeof(int), 1, pInFile);
			if (nNormals > 0)
			{
				m_nType |= VERTEXT_NORMAL;
				m_pxmf3Normals = new XMFLOAT3[nNormals];
				nReads = (UINT)::fread(m_pxmf3Normals, sizeof(XMFLOAT3), nNormals, pInFile);

				m_pd3dNormalBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3Normals, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dNormalUploadBuffer);

				m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
				m_d3dNormalBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dNormalBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<Tangents>:"))
		{
			nReads = (UINT)::fread(&nTangents, sizeof(int), 1, pInFile);
			if (nTangents > 0)
			{
				m_nType |= VERTEXT_TANGENT;
				m_pxmf3Tangents = new XMFLOAT3[nTangents];
				nReads = (UINT)::fread(m_pxmf3Tangents, sizeof(XMFLOAT3), nTangents, pInFile);

				m_pd3dTangentBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3Tangents, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTangentUploadBuffer);

				m_d3dTangentBufferView.BufferLocation = m_pd3dTangentBuffer->GetGPUVirtualAddress();
				m_d3dTangentBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dTangentBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<BiTangents>:"))
		{
			nReads = (UINT)::fread(&nBiTangents, sizeof(int), 1, pInFile);
			if (nBiTangents > 0)
			{
				m_pxmf3BiTangents = new XMFLOAT3[nBiTangents];
				nReads = (UINT)::fread(m_pxmf3BiTangents, sizeof(XMFLOAT3), nBiTangents, pInFile);

				m_pd3dBiTangentBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3BiTangents, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBiTangentUploadBuffer);

				m_d3dBiTangentBufferView.BufferLocation = m_pd3dBiTangentBuffer->GetGPUVirtualAddress();
				m_d3dBiTangentBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dBiTangentBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;
			}
		}
		
		else if (!strcmp(pstrToken, "<SubMeshes>:"))
		{
			nReads = (UINT)::fread(&(m_nSubMeshes), sizeof(int), 1, pInFile);
			if (m_nSubMeshes > 0)
			{
				m_pnSubSetIndices = new int[m_nSubMeshes];
				m_ppnSubSetIndices = new UINT*[m_nSubMeshes];

				m_ppd3dSubSetIndexBuffers = new ID3D12Resource*[m_nSubMeshes];
				m_ppd3dSubSetIndexUploadBuffers = new ID3D12Resource*[m_nSubMeshes];
				m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[m_nSubMeshes];

				for (int i = 0; i < m_nSubMeshes; i++)
				{
					::ReadStringFromFile(pInFile, pstrToken);
					if (!strcmp(pstrToken, "<SubMesh>:"))
					{
						int nIndex = 0;
						nReads = (UINT)::fread(&nIndex, sizeof(int), 1, pInFile); //i
						nReads = (UINT)::fread(&(m_pnSubSetIndices[i]), sizeof(int), 1, pInFile);
						if (m_pnSubSetIndices[i] > 0)
						{
							m_ppnSubSetIndices[i] = new UINT[m_pnSubSetIndices[i]];
							nReads = (UINT)::fread(m_ppnSubSetIndices[i], sizeof(UINT), m_pnSubSetIndices[i], pInFile);

							m_ppd3dSubSetIndexBuffers[i] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_ppnSubSetIndices[i], sizeof(UINT) * m_pnSubSetIndices[i], D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_ppd3dSubSetIndexUploadBuffers[i]);

							m_pd3dSubSetIndexBufferViews[i].BufferLocation = m_ppd3dSubSetIndexBuffers[i]->GetGPUVirtualAddress();
							m_pd3dSubSetIndexBufferViews[i].Format = DXGI_FORMAT_R32_UINT;
							m_pd3dSubSetIndexBufferViews[i].SizeInBytes = sizeof(UINT) * m_pnSubSetIndices[i];
						}
					}
				}
			}
		}
		else if (!strcmp(pstrToken, "</Mesh>"))
		{
			break;
		}
	}
}

void CStandardMesh::OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext)
{
	D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[5] = { m_d3dPositionBufferView, m_d3dTextureCoord0BufferView, m_d3dNormalBufferView, m_d3dTangentBufferView, m_d3dBiTangentBufferView };
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 5, pVertexBufferViews);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkinnedMesh::CSkinnedMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList) : CStandardMesh(pd3dDevice, pd3dCommandList)
{
}

CSkinnedMesh::~CSkinnedMesh()
{
	if (m_pxmn4BoneIndices) delete[] m_pxmn4BoneIndices;
	if (m_pxmf4BoneWeights) delete[] m_pxmf4BoneWeights;

	if (m_ppSkinningBoneFrameCaches) delete[] m_ppSkinningBoneFrameCaches;
	if (m_ppstrSkinningBoneNames) delete[] m_ppstrSkinningBoneNames;

	if (m_pxmf4x4BindPoseBoneOffsets) delete[] m_pxmf4x4BindPoseBoneOffsets;
	if (m_pd3dcbBindPoseBoneOffsets) m_pd3dcbBindPoseBoneOffsets->Release();

	if (m_pd3dBoneIndexBuffer) m_pd3dBoneIndexBuffer->Release();
	if (m_pd3dBoneWeightBuffer) m_pd3dBoneWeightBuffer->Release();

	ReleaseShaderVariables();
}

void CSkinnedMesh::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CSkinnedMesh::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	if (m_pd3dcbBindPoseBoneOffsets)
	{
		D3D12_GPU_VIRTUAL_ADDRESS d3dcbBoneOffsetsGpuVirtualAddress = m_pd3dcbBindPoseBoneOffsets->GetGPUVirtualAddress();
		pd3dCommandList->SetGraphicsRootConstantBufferView(11, d3dcbBoneOffsetsGpuVirtualAddress); //Skinned Bone Offsets
	}

	if (m_pd3dcbSkinningBoneTransforms)
	{
		D3D12_GPU_VIRTUAL_ADDRESS d3dcbBoneTransformsGpuVirtualAddress = m_pd3dcbSkinningBoneTransforms->GetGPUVirtualAddress();
		pd3dCommandList->SetGraphicsRootConstantBufferView(12, d3dcbBoneTransformsGpuVirtualAddress); //Skinned Bone Transforms

		for (int j = 0; j < m_nSkinningBones; j++)
		{
			XMStoreFloat4x4(&m_pcbxmf4x4MappedSkinningBoneTransforms[j], XMMatrixTranspose(XMLoadFloat4x4(&m_ppSkinningBoneFrameCaches[j]->m_xmf4x4World)));
		}
	}
}

void CSkinnedMesh::ReleaseShaderVariables()
{
}

void CSkinnedMesh::ReleaseUploadBuffers()
{
	CStandardMesh::ReleaseUploadBuffers();

	if (m_pd3dBoneIndexUploadBuffer) m_pd3dBoneIndexUploadBuffer->Release();
	m_pd3dBoneIndexUploadBuffer = NULL;

	if (m_pd3dBoneWeightUploadBuffer) m_pd3dBoneWeightUploadBuffer->Release();
	m_pd3dBoneWeightUploadBuffer = NULL;
}

void CSkinnedMesh::PrepareSkinning(CGameObject *pModelRootObject)
{
	for (int j = 0; j < m_nSkinningBones; j++)
	{
		m_ppSkinningBoneFrameCaches[j] = pModelRootObject->FindFrame(m_ppstrSkinningBoneNames[j]);
	}
}

void CSkinnedMesh::LoadSkinInfoFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, FILE *pInFile)
{
	char pstrToken[64] = { '\0' };
	UINT nReads = 0;

	::ReadStringFromFile(pInFile, m_pstrMeshName); 

	for ( ; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken);
		if (!strcmp(pstrToken, "<BonesPerVertex>:"))
		{
			m_nBonesPerVertex = ::ReadIntegerFromFile(pInFile);
		}
		else if (!strcmp(pstrToken, "<Bounds>:"))
		{
			nReads = (UINT)::fread(&m_xmf3AABBCenter, sizeof(XMFLOAT3), 1, pInFile);
			nReads = (UINT)::fread(&m_xmf3AABBExtents, sizeof(XMFLOAT3), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<BoneNames>:"))
		{
			m_nSkinningBones = ::ReadIntegerFromFile(pInFile);
			if (m_nSkinningBones > 0) 
			{
				m_ppstrSkinningBoneNames = new char[m_nSkinningBones][64];
				m_ppSkinningBoneFrameCaches = new CGameObject*[m_nSkinningBones];
				for (int i = 0; i < m_nSkinningBones; i++)
				{
					::ReadStringFromFile(pInFile, m_ppstrSkinningBoneNames[i]);
					m_ppSkinningBoneFrameCaches[i] = NULL;
				}
			}
		}
		else if (!strcmp(pstrToken, "<BoneOffsets>:"))
		{
			m_nSkinningBones = ::ReadIntegerFromFile(pInFile);
			if (m_nSkinningBones > 0)
			{
				m_pxmf4x4BindPoseBoneOffsets = new XMFLOAT4X4[m_nSkinningBones];
				nReads = (UINT)::fread(m_pxmf4x4BindPoseBoneOffsets, sizeof(XMFLOAT4X4), m_nSkinningBones, pInFile);

				UINT ncbElementBytes = (((sizeof(XMFLOAT4X4) * SKINNED_ANIMATION_BONES) + 255) & ~255); //256의 배수
				m_pd3dcbBindPoseBoneOffsets = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
				m_pd3dcbBindPoseBoneOffsets->Map(0, NULL, (void **)&m_pcbxmf4x4MappedBindPoseBoneOffsets);

				for (int i = 0; i < m_nSkinningBones; i++)
				{
					XMStoreFloat4x4(&m_pcbxmf4x4MappedBindPoseBoneOffsets[i], XMMatrixTranspose(XMLoadFloat4x4(&m_pxmf4x4BindPoseBoneOffsets[i])));
				}
			}
		}
		else if (!strcmp(pstrToken, "<BoneIndices>:"))
		{
			m_nType |= VERTEXT_BONE_INDEX_WEIGHT;

			m_nVertices = ::ReadIntegerFromFile(pInFile);
			if (m_nVertices > 0)
			{
				m_pxmn4BoneIndices = new XMINT4[m_nVertices];

				nReads = (UINT)::fread(m_pxmn4BoneIndices, sizeof(XMINT4), m_nVertices, pInFile);
				m_pd3dBoneIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmn4BoneIndices, sizeof(XMINT4) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBoneIndexUploadBuffer);

				m_d3dBoneIndexBufferView.BufferLocation = m_pd3dBoneIndexBuffer->GetGPUVirtualAddress();
				m_d3dBoneIndexBufferView.StrideInBytes = sizeof(XMINT4);
				m_d3dBoneIndexBufferView.SizeInBytes = sizeof(XMINT4) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "<BoneWeights>:"))
		{
			m_nType |= VERTEXT_BONE_INDEX_WEIGHT;

			m_nVertices = ::ReadIntegerFromFile(pInFile);
			if (m_nVertices > 0)
			{
				m_pxmf4BoneWeights = new XMFLOAT4[m_nVertices];

				nReads = (UINT)::fread(m_pxmf4BoneWeights, sizeof(XMFLOAT4), m_nVertices, pInFile);
				m_pd3dBoneWeightBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf4BoneWeights, sizeof(XMFLOAT4) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBoneWeightUploadBuffer);

				m_d3dBoneWeightBufferView.BufferLocation = m_pd3dBoneWeightBuffer->GetGPUVirtualAddress();
				m_d3dBoneWeightBufferView.StrideInBytes = sizeof(XMFLOAT4);
				m_d3dBoneWeightBufferView.SizeInBytes = sizeof(XMFLOAT4) * m_nVertices;
			}
		}
		else if (!strcmp(pstrToken, "</SkinningInfo>"))
		{
			break;
		}
	}
}

void CSkinnedMesh::OnPreRender(ID3D12GraphicsCommandList *pd3dCommandList, void *pContext)
{
	D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[7] = { m_d3dPositionBufferView, m_d3dTextureCoord0BufferView, m_d3dNormalBufferView, m_d3dTangentBufferView, m_d3dBiTangentBufferView, m_d3dBoneIndexBufferView, m_d3dBoneWeightBufferView };
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 7, pVertexBufferViews);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
CLaserMesh::CLaserMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) : CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 8;
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_pxmf3Positions = new XMFLOAT3[m_nVertices];
	m_pxmf3Positions[0] = XMFLOAT3(-0.5f, +0.5f, 0.0f); m_pxmf3Positions[1] = XMFLOAT3(+0.5f, +0.5f, 0.0f);
	m_pxmf3Positions[2] = XMFLOAT3(+0.5f, +0.5f, 1.0f); m_pxmf3Positions[3] = XMFLOAT3(-0.5f, +0.5f, 1.0f);
	m_pxmf3Positions[4] = XMFLOAT3(-0.5f, -0.5f, 0.0f); m_pxmf3Positions[5] = XMFLOAT3(+0.5f, -0.5f, 0.0f);
	m_pxmf3Positions[6] = XMFLOAT3(+0.5f, -0.5f, 1.0f); m_pxmf3Positions[7] = XMFLOAT3(-0.5f, -0.5f, 1.0f);

	m_pd3dPositionBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_pxmf3Positions, sizeof(XMFLOAT3) * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);
	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * m_nVertices;

	m_nSubMeshes = 1;
	m_pnSubSetIndices = new int[1];
	m_pnSubSetIndices[0] = 36;

	m_ppnSubSetIndices = new UINT * [1];
	m_ppnSubSetIndices[0] = new UINT[36]{ 3,1,0, 2,1,3, 0,5,4, 1,5,0, 3,4,7, 0,4,3, 1,6,5, 2,6,1, 2,7,6, 3,7,2, 6,4,5, 7,4,6 };

	m_ppd3dSubSetIndexBuffers = new ID3D12Resource * [1];
	m_ppd3dSubSetIndexUploadBuffers = new ID3D12Resource * [1];
	m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[1];

	m_ppd3dSubSetIndexBuffers[0] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_ppnSubSetIndices[0], sizeof(UINT) * 36, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_ppd3dSubSetIndexUploadBuffers[0]);
	m_pd3dSubSetIndexBufferViews[0].BufferLocation = m_ppd3dSubSetIndexBuffers[0]->GetGPUVirtualAddress();
	m_pd3dSubSetIndexBufferViews[0].Format = DXGI_FORMAT_R32_UINT;
	m_pd3dSubSetIndexBufferViews[0].SizeInBytes = sizeof(UINT) * 36;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
CParticleMesh::CParticleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fWidth, float fHeight) : CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 4;

	UINT nStride = sizeof(XMFLOAT3) + sizeof(XMFLOAT2);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	struct VERTEX { XMFLOAT3 p; XMFLOAT2 uv; };
	VERTEX pVertices[4];
	float halfW = fWidth * 0.5f;

	pVertices[0] = { XMFLOAT3(-halfW, 0.0f, 0.0f),    XMFLOAT2(0.0f, 1.0f) };
	pVertices[1] = { XMFLOAT3(-halfW, fHeight, 0.0f), XMFLOAT2(0.0f, 0.0f) };
	pVertices[2] = { XMFLOAT3(halfW, 0.0f, 0.0f),    XMFLOAT2(1.0f, 1.0f) };
	pVertices[3] = { XMFLOAT3(halfW, fHeight, 0.0f), XMFLOAT2(1.0f, 0.0f) };

	ID3D12Resource* pd3dUploadBuffer = NULL;

	m_pd3dPositionBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pVertices, nStride * m_nVertices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &pd3dUploadBuffer);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = nStride;
	m_d3dPositionBufferView.SizeInBytes = nStride * m_nVertices;

	m_nSubMeshes = 1;
	m_pnSubSetIndices = new int[1] { 6 };
	m_ppnSubSetIndices = new UINT * [1];
	m_ppnSubSetIndices[0] = new UINT[6]{ 0, 1, 2, 2, 1, 3 };

	m_ppd3dSubSetIndexBuffers = new ID3D12Resource * [1];
	m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[1];

	ID3D12Resource* pd3dIndexUploadBuffer = NULL;
	m_ppd3dSubSetIndexBuffers[0] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, m_ppnSubSetIndices[0], sizeof(UINT) * 6, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &pd3dIndexUploadBuffer);

	m_pd3dSubSetIndexBufferViews[0].BufferLocation = m_ppd3dSubSetIndexBuffers[0]->GetGPUVirtualAddress();
	m_pd3dSubSetIndexBufferViews[0].Format = DXGI_FORMAT_R32_UINT;

	m_pd3dSubSetIndexBufferViews[0].SizeInBytes = sizeof(UINT) * 6;
}

CParticleMesh::~CParticleMesh()
{
}

void CParticleMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nInstances)
{
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dPositionBufferView);

	pd3dCommandList->IASetIndexBuffer(&m_pd3dSubSetIndexBufferViews[0]);

	pd3dCommandList->DrawIndexedInstanced(6, nInstances, 0, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
CViewCircleMesh::CViewCircleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fRadius, int nSlices)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	if (nSlices < 3) nSlices = 3;
	if (nSlices > 16) nSlices = 16;

	m_fRadius = fRadius;
	m_nSlices = nSlices;

	m_nType |= VERTEXT_POSITION;
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	m_nVertices = m_nSlices + 1;
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];

	m_pxmf3Positions[0] = XMFLOAT3(0.0f, 0.0f, 0.0f);

	const float fStep = XM_2PI / float(m_nSlices);
	for (int i = 0; i < m_nSlices; ++i)
	{
		float a = fStep * i;
		float x = sinf(a) * m_fRadius;
		float z = cosf(a) * m_fRadius;
		m_pxmf3Positions[i + 1] = XMFLOAT3(x, 0.0f, z);
	}

	m_nSubMeshes = 1;
	m_pnSubSetIndices = new int[1];
	m_pnSubSetIndices[0] = m_nSlices * 3;

	m_ppnSubSetIndices = new UINT * [1];
	m_ppnSubSetIndices[0] = new UINT[m_pnSubSetIndices[0]];

	for (int i = 0; i < m_nSlices; ++i)
	{
		int next = (i + 1) % m_nSlices;

		m_ppnSubSetIndices[0][i * 3 + 0] = 0;
		m_ppnSubSetIndices[0][i * 3 + 1] = i + 1;
		m_ppnSubSetIndices[0][i * 3 + 2] = next + 1;
	}

	UINT nVertexBufferSize = sizeof(XMFLOAT3) * m_nVertices;
	m_pd3dPositionBuffer = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		NULL,
		nVertexBufferSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		NULL
	);
	m_pd3dPositionBuffer->Map(0, NULL, (void**)&m_pMappedPositions);
	memcpy(m_pMappedPositions, m_pxmf3Positions, nVertexBufferSize);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = nVertexBufferSize;

	m_ppd3dSubSetIndexBuffers = new ID3D12Resource * [1];
	m_ppd3dSubSetIndexUploadBuffers = new ID3D12Resource * [1];
	m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[1];

	m_ppd3dSubSetIndexBuffers[0] = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		m_ppnSubSetIndices[0],
		sizeof(UINT) * m_pnSubSetIndices[0],
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_ppd3dSubSetIndexUploadBuffers[0]
	);

	m_pd3dSubSetIndexBufferViews[0].BufferLocation = m_ppd3dSubSetIndexBuffers[0]->GetGPUVirtualAddress();
	m_pd3dSubSetIndexBufferViews[0].Format = DXGI_FORMAT_R32_UINT;
	m_pd3dSubSetIndexBufferViews[0].SizeInBytes = sizeof(UINT) * m_pnSubSetIndices[0];

	m_xmf3AABBCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3AABBExtents = XMFLOAT3(m_fRadius, 0.05f, m_fRadius);
}

CViewConeMesh::CViewConeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fRadius, float fAngleDegrees, int nSlices)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	if (nSlices < 1) nSlices = 1;
	if (nSlices > 40) nSlices = 40;

	m_fRadius = fRadius;
	m_fAngleDegrees = fAngleDegrees;
	m_nSlices = nSlices;

	m_nType |= VERTEXT_POSITION;
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	const int nArcVertices = m_nSlices + 1;

	m_nVertices = nArcVertices + 1;
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];

	m_pxmf3Positions[0] = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float fHalfAngleRad = XMConvertToRadians(m_fAngleDegrees * 0.5f);
	float fStart = -fHalfAngleRad;
	float fStep = (fHalfAngleRad * 2.0f) / float(m_nSlices);

	for (int i = 0; i <= m_nSlices; ++i)
	{
		float a = fStart + fStep * i;
		float x = sinf(a) * m_fRadius;
		float z = cosf(a) * m_fRadius;
		m_pxmf3Positions[i + 1] = XMFLOAT3(x, 0.0f, z);
	}

	m_nSubMeshes = 1;
	m_pnSubSetIndices = new int[1];
	m_pnSubSetIndices[0] = m_nSlices * 3;

	m_ppnSubSetIndices = new UINT * [1];
	m_ppnSubSetIndices[0] = new UINT[m_pnSubSetIndices[0]];

	for (int i = 0; i < m_nSlices; ++i)
	{
		m_ppnSubSetIndices[0][i * 3 + 0] = 0;
		m_ppnSubSetIndices[0][i * 3 + 1] = i + 1;
		m_ppnSubSetIndices[0][i * 3 + 2] = i + 2;
	}

	UINT nVertexBufferSize = sizeof(XMFLOAT3) * m_nVertices;
	m_pd3dPositionBuffer = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		NULL,
		nVertexBufferSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		NULL
	);
	m_pd3dPositionBuffer->Map(0, NULL, (void**)&m_pMappedPositions);
	memcpy(m_pMappedPositions, m_pxmf3Positions, nVertexBufferSize);

	m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
	m_d3dPositionBufferView.SizeInBytes = nVertexBufferSize;

	m_ppd3dSubSetIndexBuffers = new ID3D12Resource * [1];
	m_ppd3dSubSetIndexUploadBuffers = new ID3D12Resource * [1];
	m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[1];

	m_ppd3dSubSetIndexBuffers[0] = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		m_ppnSubSetIndices[0],
		sizeof(UINT) * m_pnSubSetIndices[0],
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_ppd3dSubSetIndexUploadBuffers[0]
	);

	m_pd3dSubSetIndexBufferViews[0].BufferLocation = m_ppd3dSubSetIndexBuffers[0]->GetGPUVirtualAddress();
	m_pd3dSubSetIndexBufferViews[0].Format = DXGI_FORMAT_R32_UINT;
	m_pd3dSubSetIndexBufferViews[0].SizeInBytes = sizeof(UINT) * m_pnSubSetIndices[0];

	m_xmf3AABBCenter = XMFLOAT3(0.0f, 0.0f, m_fRadius * 0.5f);
	m_xmf3AABBExtents = XMFLOAT3(m_fRadius, 0.05f, m_fRadius * 0.5f);
}

//block 충돌 결과에 맞춰서 잘린 형태 갱신
void CViewCircleMesh::UpdateClippedMesh(const XMFLOAT3& worldOrigin, const std::vector<CGameObject*>& blockers)
{
	if (!m_pMappedPositions) return;

	m_pMappedPositions[0] = XMFLOAT3(0.0f, 0.0f, 0.0f);

	const float fStep = XM_2PI / float(m_nSlices);

	for (int i = 0; i < m_nSlices; ++i)
	{
		float a = fStep * i;

		XMFLOAT3 localDir(
			sinf(a),
			0.0f,
			cosf(a)
		);

		float hitDist = RaycastVisionBlockers(worldOrigin, localDir, m_fRadius, blockers);

		m_pMappedPositions[i + 1] = XMFLOAT3(
			localDir.x * hitDist,
			0.0f,
			localDir.z * hitDist
		);
	}
}

void CViewConeMesh::UpdateClippedMesh(const XMFLOAT3& worldOrigin, float fYaw, const std::vector<CGameObject*>& blockers)
{
	if (!m_pMappedPositions) return;

	m_pMappedPositions[0] = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float fHalfAngleRad = XMConvertToRadians(m_fAngleDegrees * 0.5f);
	float fStart = -fHalfAngleRad;
	float fStep = (fHalfAngleRad * 2.0f) / float(m_nSlices);

	for (int i = 0; i <= m_nSlices; ++i)
	{
		float a = fStart + fStep * i;

		XMFLOAT3 localDir(
			sinf(a),
			0.0f,
			cosf(a)
		);

		XMFLOAT3 worldDir = RotateVectorY(localDir, fYaw);

		float hitDist = RaycastVisionBlockers(worldOrigin, worldDir, m_fRadius, blockers);

		m_pMappedPositions[i + 1] = XMFLOAT3(
			localDir.x * hitDist,
			0.0f,
			localDir.z * hitDist
		);
	}
}