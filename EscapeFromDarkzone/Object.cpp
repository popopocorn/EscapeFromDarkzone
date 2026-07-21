//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include"ResourceManager.h"
#include "Object.h"
#include "Shader.h"
#include "Scene.h"
#include "Collision.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CTexture::CTexture(int nTextures, UINT nTextureType, int nSamplers, int nRootParameters)
{
	m_nTextureType = nTextureType;

	m_nTextures = nTextures;
	if (m_nTextures > 0)
	{
		m_ppd3dTextureUploadBuffers = new ID3D12Resource * [m_nTextures];
		m_ppd3dTextures = new ID3D12Resource * [m_nTextures];
		for (int i = 0; i < m_nTextures; i++) m_ppd3dTextureUploadBuffers[i] = m_ppd3dTextures[i] = NULL;

		m_pd3dSrvGpuDescriptorHandles = new D3D12_GPU_DESCRIPTOR_HANDLE[m_nTextures];

		m_pnResourceTypes = new UINT[m_nTextures];
		m_pdxgiBufferFormats = new DXGI_FORMAT[m_nTextures];
		m_pnBufferElements = new int[m_nTextures];
	}
	m_nRootParameters = nRootParameters;
	if (nRootParameters > 0) m_pnRootParameterIndices = new UINT[nRootParameters];

	m_nSamplers = nSamplers;
	if (m_nSamplers > 0) m_pd3dSamplerGpuDescriptorHandles = new D3D12_GPU_DESCRIPTOR_HANDLE[m_nSamplers];
}

CTexture::~CTexture()
{
	if (m_ppd3dTextures)
	{
		for (int i = 0; i < m_nTextures; i++) if (m_ppd3dTextures[i]) m_ppd3dTextures[i]->Release();
		delete[] m_ppd3dTextures;
	}
	if (m_pnResourceTypes) delete[] m_pnResourceTypes;
	if (m_pdxgiBufferFormats) delete[] m_pdxgiBufferFormats;
	if (m_pnBufferElements) delete[] m_pnBufferElements;

	if (m_pnRootParameterIndices) delete[] m_pnRootParameterIndices;
	if (m_pd3dSrvGpuDescriptorHandles) delete[] m_pd3dSrvGpuDescriptorHandles;

	if (m_pd3dSamplerGpuDescriptorHandles) delete[] m_pd3dSamplerGpuDescriptorHandles;
}

void CTexture::SetRootParameterIndex(int nIndex, UINT nRootParameterIndex)
{
	m_pnRootParameterIndices[nIndex] = nRootParameterIndex;
}

void CTexture::SetGpuDescriptorHandle(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSrvGpuDescriptorHandle)
{
	m_pd3dSrvGpuDescriptorHandles[nIndex] = d3dSrvGpuDescriptorHandle;
}

void CTexture::SetSampler(int nIndex, D3D12_GPU_DESCRIPTOR_HANDLE d3dSamplerGpuDescriptorHandle)
{
	m_pd3dSamplerGpuDescriptorHandles[nIndex] = d3dSamplerGpuDescriptorHandle;
}

void CTexture::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_nRootParameters == m_nTextures)
	{
		for (int i = 0; i < m_nRootParameters; i++)
		{
			pd3dCommandList->SetGraphicsRootDescriptorTable(m_pnRootParameterIndices[i], m_pd3dSrvGpuDescriptorHandles[i]);
		}
	}
	else
	{
		pd3dCommandList->SetGraphicsRootDescriptorTable(m_pnRootParameterIndices[0], m_pd3dSrvGpuDescriptorHandles[0]);
	}
}

void CTexture::UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, int nParameterIndex, int nTextureIndex)
{
	pd3dCommandList->SetGraphicsRootDescriptorTable(m_pnRootParameterIndices[nParameterIndex], m_pd3dSrvGpuDescriptorHandles[nTextureIndex]);
}

void CTexture::ReleaseShaderVariables()
{
}

void CTexture::ReleaseUploadBuffers()
{
	if (m_ppd3dTextureUploadBuffers)
	{
		for (int i = 0; i < m_nTextures; i++) if (m_ppd3dTextureUploadBuffers[i]) m_ppd3dTextureUploadBuffers[i]->Release();
		delete[] m_ppd3dTextureUploadBuffers;
		m_ppd3dTextureUploadBuffers = NULL;
	}
}

void CTexture::LoadTextureFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, wchar_t* pszFileName, UINT nIndex)
{
	m_ppd3dTextures[nIndex] = ::CreateTextureResourceFromDDSFile(pd3dDevice, pd3dCommandList, pszFileName, &m_ppd3dTextureUploadBuffers[nIndex], D3D12_RESOURCE_STATE_GENERIC_READ);
}

void CTexture::LoadTextureFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName, UINT nResourceType, UINT nIndex)
{
	m_pnResourceTypes[nIndex] = nResourceType;
	m_ppd3dTextures[nIndex] = ::CreateTextureResourceFromDDSFile(pd3dDevice, pd3dCommandList, pszFileName, &m_ppd3dTextureUploadBuffers[nIndex], D3D12_RESOURCE_STATE_GENERIC_READ/*D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE*/);
	m_ppd3dTextures[nIndex]->SetName(pszFileName);
}

void CTexture::LoadBuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nElements, UINT nStride, DXGI_FORMAT ndxgiFormat, UINT nIndex)
{
	m_pnResourceTypes[nIndex] = RESOURCE_BUFFER;
	m_pdxgiBufferFormats[nIndex] = ndxgiFormat;
	m_pnBufferElements[nIndex] = nElements;
	m_ppd3dTextures[nIndex] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pData, nElements * nStride, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_GENERIC_READ, &m_ppd3dTextureUploadBuffers[nIndex]);
}

ID3D12Resource* CTexture::CreateTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, UINT nIndex, UINT nResourceType, UINT nWidth, UINT nHeight, UINT nElements, UINT nMipLevels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS d3dResourceFlags, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_CLEAR_VALUE* pd3dClearValue)
{
	m_pnResourceTypes[nIndex] = nResourceType;
	m_ppd3dTextures[nIndex] = ::CreateTexture2DResource(pd3dDevice, pd3dCommandList, nWidth, nHeight, nElements, nMipLevels, dxgiFormat, d3dResourceFlags, d3dResourceStates, pd3dClearValue);
	return(m_ppd3dTextures[nIndex]);
}

D3D12_SHADER_RESOURCE_VIEW_DESC CTexture::GetShaderResourceViewDesc(int nIndex)
{
	ID3D12Resource* pShaderResource = GetResource(nIndex);
	D3D12_RESOURCE_DESC d3dResourceDesc = pShaderResource->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC d3dShaderResourceViewDesc;
	d3dShaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	int nTextureType = GetTextureType(nIndex);
	switch (nTextureType)
	{
	case RESOURCE_TEXTURE2D: //(d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)(d3dResourceDesc.DepthOrArraySize == 1)
	case RESOURCE_TEXTURE2D_ARRAY: //[]
		d3dShaderResourceViewDesc.Format = d3dResourceDesc.Format;
		d3dShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		d3dShaderResourceViewDesc.Texture2D.MipLevels = -1;
		d3dShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		d3dShaderResourceViewDesc.Texture2D.PlaneSlice = 0;
		d3dShaderResourceViewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		break;
	case RESOURCE_TEXTURE2DARRAY: //(d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)(d3dResourceDesc.DepthOrArraySize != 1)
		d3dShaderResourceViewDesc.Format = d3dResourceDesc.Format;
		d3dShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		d3dShaderResourceViewDesc.Texture2DArray.MipLevels = -1;
		d3dShaderResourceViewDesc.Texture2DArray.MostDetailedMip = 0;
		d3dShaderResourceViewDesc.Texture2DArray.PlaneSlice = 0;
		d3dShaderResourceViewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
		d3dShaderResourceViewDesc.Texture2DArray.FirstArraySlice = 0;
		d3dShaderResourceViewDesc.Texture2DArray.ArraySize = d3dResourceDesc.DepthOrArraySize;
		break;
	case RESOURCE_TEXTURE_CUBE: //(d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)(d3dResourceDesc.DepthOrArraySize == 6)
		d3dShaderResourceViewDesc.Format = d3dResourceDesc.Format;
		d3dShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		d3dShaderResourceViewDesc.TextureCube.MipLevels = 1;
		d3dShaderResourceViewDesc.TextureCube.MostDetailedMip = 0;
		d3dShaderResourceViewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		break;
	case RESOURCE_BUFFER: //(d3dResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		d3dShaderResourceViewDesc.Format = m_pdxgiBufferFormats[nIndex];
		d3dShaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		d3dShaderResourceViewDesc.Buffer.FirstElement = 0;
		d3dShaderResourceViewDesc.Buffer.NumElements = m_pnBufferElements[nIndex];
		d3dShaderResourceViewDesc.Buffer.StructureByteStride = 0;
		d3dShaderResourceViewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		break;
	}
	return(d3dShaderResourceViewDesc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CMaterial::CMaterial(int nTextures)
{
	m_nTextures = nTextures;

	m_ppTextures = new CTexture*[m_nTextures];
	m_ppstrTextureNames = new _TCHAR[m_nTextures][260];
	for (int i = 0; i < m_nTextures; i++) m_ppTextures[i] = NULL;
	for (int i = 0; i < m_nTextures; i++) m_ppstrTextureNames[i][0] = '\0';

	m_pShader = nullptr;
}

CMaterial::~CMaterial()
{
	//if (m_pShader) m_pShader->Release();

	if (m_nTextures > 0)
	{
		//for (int i = 0; i < m_nTextures; i++) if (m_ppTextures[i]) m_ppTextures[i]->Release();
		delete[] m_ppTextures;

		if (m_ppstrTextureNames) delete[] m_ppstrTextureNames;
	}
}

void CMaterial::SetShader(CShader *pShader)
{
	//if (m_pShader) m_pShader->Release();
	m_pShader = pShader;
	//if (m_pShader) m_pShader->AddRef();
}

void CMaterial::SetTexture(CTexture *pTexture, UINT nTexture) 
{ 
	m_ppTextures[nTexture] = pTexture; 
}

void CMaterial::ReleaseUploadBuffers()
{
	for (int i = 0; i < m_nTextures; i++)
	{
		if (m_ppTextures[i]) m_ppTextures[i]->ReleaseUploadBuffers();
	}
}

CShader *CMaterial::m_pSkinnedAnimationShader = NULL;
CShader *CMaterial::m_pStandardShader = NULL;

void CMaterial::PrepareShaders(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature)
{
	m_pStandardShader = new CStandardShader();
	m_pStandardShader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	m_pStandardShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	m_pSkinnedAnimationShader = new CSkinnedAnimationStandardShader();
	m_pSkinnedAnimationShader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	m_pSkinnedAnimationShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CMaterial::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 4, &m_xmf4AmbientColor, 16);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 4, &m_xmf4AlbedoColor, 20);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 4, &m_xmf4SpecularColor, 24);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 4, &m_xmf4EmissiveColor, 28);

	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 1, &m_nType, 32);

	for (int i = 0; i < m_nTextures; i++)
	{
		if (m_ppTextures[i]) m_ppTextures[i]->UpdateShaderVariables(pd3dCommandList);
		//		if (m_ppTextures[i]) m_ppTextures[i]->UpdateShaderVariable(pd3dCommandList, 0, 0);
	}
}

void CMaterial::LoadTextureFromFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, UINT nType, UINT nRootParameter, _TCHAR* pwstrTextureName, CTexture** ppTexture, CGameObject* pParent, FILE* pInFile, CShader* pShader)
{
	char pstrTextureName[260] = { '\0' };

	BYTE nStrLength = 0;
	UINT nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);

	if (nStrLength >= _countof(pstrTextureName))
	{
		nReads = (UINT)::fread(pstrTextureName, sizeof(char), _countof(pstrTextureName) - 1, pInFile);
		pstrTextureName[_countof(pstrTextureName) - 1] = '\0';

		int nRemain = (int)nStrLength - ((int)_countof(pstrTextureName) - 1);
		if (nRemain > 0) fseek(pInFile, nRemain, SEEK_CUR);
	}
	else
	{
		nReads = (UINT)::fread(pstrTextureName, sizeof(char), nStrLength, pInFile);
		pstrTextureName[nStrLength] = '\0';
	}

	bool bDuplicated = false;
	if (strcmp(pstrTextureName, "null"))
	{
		SetMaterialType(nType);

		char pstrFilePath[260] = { '\0' };
		strcpy_s(pstrFilePath, _countof(pstrFilePath), "Model/Textures/");

		bDuplicated = (pstrTextureName[0] == '@');

		strcat_s(
			pstrFilePath,
			_countof(pstrFilePath),
			(bDuplicated) ? (pstrTextureName + 1) : pstrTextureName
		);

		strcat_s(
			pstrFilePath,
			_countof(pstrFilePath),
			".dds"
		);

		size_t nConverted = 0;
		mbstowcs_s(&nConverted, pwstrTextureName, 260, pstrFilePath, _TRUNCATE);

		
		/*TCHAR pstrDebugTexture[512] = { 0 };
		_stprintf_s(pstrDebugTexture, 512, _T("[Texture Load] %s\n"), pwstrTextureName);
		OutputDebugString(pstrDebugTexture);*/
		
		
		//#define _WITH_DISPLAY_TEXTURE_NAME

#ifdef _WITH_DISPLAY_TEXTURE_NAME
		static int nTextures = 0, nRepeatedTextures = 0;
		TCHAR pstrDebug[256] = { 0 };
		_stprintf_s(pstrDebug, 256, _T("Texture Name: %d %c %s\n"), (pstrTextureName[0] == '@') ? nRepeatedTextures++ : nTextures++, (pstrTextureName[0] == '@') ? '@' : ' ', pwstrTextureName);
		OutputDebugString(pstrDebug);
#endif
		if (!bDuplicated)
		{
			*ppTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
			(*ppTexture)->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, pwstrTextureName, RESOURCE_TEXTURE2D, 0);

			if ((*ppTexture)->GetResource(0))
			{
				ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, *ppTexture, 0, nRootParameter);
			}
			else
			{
				delete* ppTexture;
				*ppTexture = NULL;
			}
		}
		else
		{
			if (pParent)
			{
				while (pParent)
				{
					if (!pParent->m_pParent) break;
					pParent = pParent->m_pParent;
				}
				CGameObject* pRootGameObject = pParent;
				*ppTexture = pRootGameObject->FindReplicatedTexture(pwstrTextureName);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CAnimationSet::CAnimationSet(float fLength, int nFramesPerSecond, int nKeyFrames, int nAnimatedBones, char *pstrName)
{
	m_fLength = fLength;
	m_nFramesPerSecond = nFramesPerSecond;
	m_nKeyFrames = nKeyFrames;

	strcpy_s(m_pstrAnimationSetName, _countof(m_pstrAnimationSetName), pstrName);

#ifdef _WITH_ANIMATION_SRT
	m_nKeyFrameTranslations = nKeyFrames;
	m_pfKeyFrameTranslationTimes = new float[m_nKeyFrameTranslations];
	m_ppxmf3KeyFrameTranslations = new XMFLOAT3 * [m_nKeyFrameTranslations];
	for (int i = 0; i < m_nKeyFrameTranslations; i++) m_ppxmf3KeyFrameTranslations[i] = new XMFLOAT4X4[nAnimatedBones];

	m_nKeyFrameScales = nKeyFrames;
	m_pfKeyFrameScaleTimes = new float[m_nKeyFrameScales];
	m_ppxmf3KeyFrameScales = new XMFLOAT3 * [m_nKeyFrameScales];
	for (int i = 0; i < m_nKeyFrameScales; i++) m_ppxmf3KeyFrameScales[i] = new XMFLOAT4X4[nAnimatedBones];

	m_nKeyFrameRotations = nKeyFrames;
	m_pfKeyFrameRotationTimes = new float[m_nKeyFrameRotations];
	m_ppxmf4KeyFrameRotations = new XMFLOAT3 * [m_nKeyFrameRotations];
	for (int i = 0; i < m_nKeyFrameRotations; i++) m_ppxmf4KeyFrameRotations[i] = new XMFLOAT4X4[nAnimatedBones];
#else
	m_pfKeyFrameTimes = new float[nKeyFrames];
	m_ppxmf4x4KeyFrameTransforms = new XMFLOAT4X4*[nKeyFrames];
	for (int i = 0; i < nKeyFrames; i++) m_ppxmf4x4KeyFrameTransforms[i] = new XMFLOAT4X4[nAnimatedBones];
#endif
}

CAnimationSet::~CAnimationSet()
{
#ifdef _WITH_ANIMATION_SRT
	if (m_pfKeyFrameTranslationTimes) delete[] m_pfKeyFrameTranslationTimes;
	for (int j = 0; j < m_nKeyFrameTranslations; j++) if (m_ppxmf3KeyFrameTranslations[j]) delete[] m_ppxmf3KeyFrameTranslations[j];
	if (m_ppxmf3KeyFrameTranslations) delete[] m_ppxmf3KeyFrameTranslations;

	if (m_pfKeyFrameScaleTimes) delete[] m_pfKeyFrameScaleTimes;
	for (int j = 0; j < m_nKeyFrameScales; j++) if (m_ppxmf3KeyFrameScales[j]) delete[] m_ppxmf3KeyFrameScales[j];
	if (m_ppxmf3KeyFrameScales) delete[] m_ppxmf3KeyFrameScales;

	if (m_pfKeyFrameRotationTimes) delete[] m_pfKeyFrameRotationTimes;
	for (int j = 0; j < m_nKeyFrameRotations; j++) if (m_ppxmf4KeyFrameRotations[j]) delete[] m_ppxmf4KeyFrameRotations[j];
	if (m_ppxmf4KeyFrameRotations) delete[] m_ppxmf4KeyFrameRotations;
#else
	if (m_pfKeyFrameTimes) delete[] m_pfKeyFrameTimes;
	for (int j = 0; j < m_nKeyFrames; j++) if (m_ppxmf4x4KeyFrameTransforms[j]) delete[] m_ppxmf4x4KeyFrameTransforms[j];
	if (m_ppxmf4x4KeyFrameTransforms) delete[] m_ppxmf4x4KeyFrameTransforms;
#endif
}

XMFLOAT4X4 CAnimationSet::GetSRT(int nBone, float fPosition)
{
	XMFLOAT4X4 xmf4x4Transform = Matrix4x4::Identity();
#ifdef _WITH_ANIMATION_SRT
	XMVECTOR S, R, T;
	for (int i = 0; i < (m_nKeyFrameTranslations - 1); i++)
	{
		if ((m_pfKeyFrameTranslationTimes[i] <= fPosition) && (fPosition <= m_pfKeyFrameTranslationTimes[i+1]))
		{
			float t = (fPosition - m_pfKeyFrameTranslationTimes[i]) / (m_pfKeyFrameTranslationTimes[i+1] - m_pfKeyFrameTranslationTimes[i]);
			T = XMVectorLerp(XMLoadFloat3(&m_ppxmf3KeyFrameTranslations[i][nBone]), XMLoadFloat3(&m_ppxmf3KeyFrameTranslations[i+1][nBone]), t);
			break;
		}
	}
	for (UINT i = 0; i < (m_nKeyFrameScales - 1); i++)
	{
		if ((m_pfKeyFrameScaleTimes[i] <= fPosition) && (fPosition <= m_pfKeyFrameScaleTimes[i+1]))
		{
			float t = (fPosition - m_pfKeyFrameScaleTimes[i]) / (m_pfKeyFrameScaleTimes[i+1] - m_pfKeyFrameScaleTimes[i]);
			S = XMVectorLerp(XMLoadFloat3(&m_ppxmf3KeyFrameScales[i][nBone]), XMLoadFloat3(&m_ppxmf3KeyFrameScales[i+1][nBone]), t);
			break;
		}
	}
	for (UINT i = 0; i < (m_nKeyFrameRotations - 1); i++)
	{
		if ((m_pfKeyFrameRotationTimes[i] <= fPosition) && (fPosition <= m_pfKeyFrameRotationTimes[i+1]))
		{
			float t = (m_fPosition - m_pfKeyFrameRotationTimes[i]) / (m_pfKeyFrameRotationTimes[i+1] - m_pfKeyFrameRotationTimes[i]);
			R = XMQuaternionSlerp(XMQuaternionConjugate(XMLoadFloat4(&m_ppxmf4KeyFrameRotations[i][nBone])), XMQuaternionConjugate(XMLoadFloat4(&m_ppxmf4KeyFrameRotations[i+1][nBone])), t);
			break;
		}
	}

	XMStoreFloat4x4(&xmf4x4Transform, XMMatrixAffineTransformation(S, XMVectorZero(), R, T));
#else   
	for (int i = 0; i < (m_nKeyFrames - 1); i++) 
	{
		if ((m_pfKeyFrameTimes[i] <= fPosition) && (fPosition < m_pfKeyFrameTimes[i+1]))
		{
			float t = (fPosition - m_pfKeyFrameTimes[i]) / (m_pfKeyFrameTimes[i+1] - m_pfKeyFrameTimes[i]);
			xmf4x4Transform = Matrix4x4::Interpolate(m_ppxmf4x4KeyFrameTransforms[i][nBone], m_ppxmf4x4KeyFrameTransforms[i+1][nBone], t);
			break;
		}
	}
	if (fPosition >= m_pfKeyFrameTimes[m_nKeyFrames-1]) xmf4x4Transform = m_ppxmf4x4KeyFrameTransforms[m_nKeyFrames-1][nBone];

#endif
	return(xmf4x4Transform);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CAnimationSets::CAnimationSets(int nAnimationSets)
{
	m_vAnimationSets.reserve(nAnimationSets);
}

CAnimationSets::~CAnimationSets()
{
	if (m_bOwnAnimationSets)
	{
		for (CAnimationSet* pSet : m_vAnimationSets)
		{
			if (pSet) delete pSet;
		}
	}

	m_vAnimationSets.clear();

	if (m_ppBoneFrameCaches)
	{
		delete[] m_ppBoneFrameCaches;
		m_ppBoneFrameCaches = nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CAnimationTrack::~CAnimationTrack()
{ 
	if (m_pCallbackKeys) delete[] m_pCallbackKeys;
	if (m_pAnimationCallbackHandler) delete m_pAnimationCallbackHandler;
}

void CAnimationTrack::SetCallbackKeys(int nCallbackKeys)
{
	m_nCallbackKeys = nCallbackKeys;
	m_pCallbackKeys = new CALLBACKKEY[nCallbackKeys];
}

void CAnimationTrack::SetCallbackKey(int nKeyIndex, float fKeyTime, void* pData)
{
	m_pCallbackKeys[nKeyIndex].m_fTime = fKeyTime;
	m_pCallbackKeys[nKeyIndex].m_pCallbackData = pData;
}

void CAnimationTrack::SetAnimationCallbackHandler(CAnimationCallbackHandler * pCallbackHandler)
{
	m_pAnimationCallbackHandler = pCallbackHandler;
}

void CAnimationTrack::HandleCallback()
{
	if (m_pAnimationCallbackHandler)
	{
		for (int i = 0; i < m_nCallbackKeys; i++)
		{
			if (::IsEqual(m_pCallbackKeys[i].m_fTime, m_fPosition, ANIMATION_CALLBACK_EPSILON))
			{
				if (m_pCallbackKeys[i].m_pCallbackData) m_pAnimationCallbackHandler->HandleCallback(m_pCallbackKeys[i].m_pCallbackData, m_fPosition);
				break;
			}
		}
	}
}

float CAnimationTrack::UpdatePosition(float fTrackPosition, float fElapsedTime, float fAnimationLength)
{
	float fTrackElapsedTime = fElapsedTime * m_fSpeed;

	switch (m_nType)
	{
	case ANIMATION_TYPE_LOOP:
	{
		m_fPosition = fTrackPosition + fTrackElapsedTime;

		if (m_fPosition >= fAnimationLength)
		{
			if (fAnimationLength > 0.0f)
				m_fPosition = fmod(m_fPosition, fAnimationLength);
			else
				m_fPosition = 0.0f;
		}
		break;
	}
	case ANIMATION_TYPE_ONCE:
		m_fPosition = fTrackPosition + fTrackElapsedTime;
		if (m_fPosition > fAnimationLength) m_fPosition = fAnimationLength;
		break;
	case ANIMATION_TYPE_PINGPONG:
		break;
	}

	return(m_fPosition);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CAnimationController::CAnimationController(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, int nAnimationTracks, CLoadedModelInfo* pModel)
{
	m_nAnimationTracks = nAnimationTracks;
	m_pAnimationTracks = new CAnimationTrack[nAnimationTracks];

	m_pAnimationSets = nullptr;
	m_pModelRootObject = nullptr;
	m_nSkinnedMeshes = 0;
	m_ppSkinnedMeshes = nullptr;
	m_ppd3dcbSkinningBoneTransforms = nullptr;
	m_ppcbxmf4x4MappedSkinningBoneTransforms = nullptr;
	m_pppSkinningBoneFrameCaches = nullptr;

	m_bIsBlending = false;
	m_fBlendTime = 0.0f;
	m_fBlendDuration = 0.2f;

	if (!pModel)
	{
		return;
	}

	m_pAnimationSets = pModel->m_pAnimationSets;
	m_pModelRootObject = pModel->m_pModelRootObject;
	m_nSkinnedMeshes = pModel->m_nSkinnedMeshes;

	if (!m_pModelRootObject)
	{
		m_nSkinnedMeshes = 0;
		return;
	}

	if (m_nSkinnedMeshes > 0 && !pModel->m_ppSkinnedMeshes)
	{
		pModel->m_ppSkinnedMeshes = new CSkinnedMesh * [m_nSkinnedMeshes];

		for (int i = 0; i < m_nSkinnedMeshes; ++i)
		{
			pModel->m_ppSkinnedMeshes[i] = nullptr;
		}

		int nFoundSkinnedMeshes = 0;
		m_pModelRootObject->FindAndSetSkinnedMesh(pModel->m_ppSkinnedMeshes, &nFoundSkinnedMeshes);

		if (nFoundSkinnedMeshes != m_nSkinnedMeshes)
		{
			m_nSkinnedMeshes = nFoundSkinnedMeshes;
			pModel->m_nSkinnedMeshes = nFoundSkinnedMeshes;
		}
	}

	if (m_nSkinnedMeshes <= 0 || !pModel->m_ppSkinnedMeshes)
	{
		m_nSkinnedMeshes = 0;
		return;
	}

	m_ppSkinnedMeshes = new CSkinnedMesh * [m_nSkinnedMeshes];

	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		m_ppSkinnedMeshes[i] = pModel->m_ppSkinnedMeshes[i];
	}

	m_ppd3dcbSkinningBoneTransforms = new ID3D12Resource * [m_nSkinnedMeshes];
	m_ppcbxmf4x4MappedSkinningBoneTransforms = new XMFLOAT4X4 * [m_nSkinnedMeshes];

	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		m_ppd3dcbSkinningBoneTransforms[i] = nullptr;
		m_ppcbxmf4x4MappedSkinningBoneTransforms[i] = nullptr;
	}

	UINT ncbElementBytes = (((sizeof(XMFLOAT4X4) * SKINNED_ANIMATION_BONES) + 255) & ~255);

	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		if (!m_ppSkinnedMeshes[i])
			continue;

		m_ppd3dcbSkinningBoneTransforms[i] = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);

		if (m_ppd3dcbSkinningBoneTransforms[i])
		{
			m_ppd3dcbSkinningBoneTransforms[i]->Map(0, NULL, (void**)&m_ppcbxmf4x4MappedSkinningBoneTransforms[i]);
		}
	}

	m_pppSkinningBoneFrameCaches = new CGameObject * *[m_nSkinnedMeshes];

	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		m_pppSkinningBoneFrameCaches[i] = nullptr;

		if (!m_ppSkinnedMeshes[i])
			continue;

		m_pppSkinningBoneFrameCaches[i] = new CGameObject * [m_ppSkinnedMeshes[i]->m_nSkinningBones];

		for (int j = 0; j < m_ppSkinnedMeshes[i]->m_nSkinningBones; j++)
		{
			m_pppSkinningBoneFrameCaches[i][j] = nullptr;

			if (m_pModelRootObject)
			{
				m_pppSkinningBoneFrameCaches[i][j] = m_pModelRootObject->FindFrame(m_ppSkinnedMeshes[i]->m_ppstrSkinningBoneNames[j]);
			}
		}
	}

	m_bIsBlending = false;
	m_fBlendTime = 0.0f;
	m_fBlendDuration = 0.2f;
}

CAnimationController::~CAnimationController()
{
	if (m_pAnimationTracks) delete[] m_pAnimationTracks;

	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		m_ppd3dcbSkinningBoneTransforms[i]->Unmap(0, NULL);
		m_ppd3dcbSkinningBoneTransforms[i]->Release();
	}
	if (m_ppd3dcbSkinningBoneTransforms) delete[] m_ppd3dcbSkinningBoneTransforms;
	if (m_ppcbxmf4x4MappedSkinningBoneTransforms) delete[] m_ppcbxmf4x4MappedSkinningBoneTransforms;
	
	if (m_pbUpperBodyMask)
	{
		delete[] m_pbUpperBodyMask;
		m_pbUpperBodyMask = nullptr;
	}

	if (m_ppSkinnedMeshes) delete[] m_ppSkinnedMeshes;
	if (m_pppSkinningBoneFrameCaches)
	{
		for (int i = 0; i < m_nSkinnedMeshes; i++)
		{
			if (m_pppSkinningBoneFrameCaches[i]) delete[] m_pppSkinningBoneFrameCaches[i];
		}
		delete[] m_pppSkinningBoneFrameCaches;
	}
}

void CAnimationController::SetCallbackKeys(int nAnimationTrack, int nCallbackKeys)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetCallbackKeys(nCallbackKeys);
}

void CAnimationController::SetCallbackKey(int nAnimationTrack, int nKeyIndex, float fKeyTime, void* pData)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetCallbackKey(nKeyIndex, fKeyTime, pData);
}

void CAnimationController::SetAnimationCallbackHandler(int nAnimationTrack, CAnimationCallbackHandler *pCallbackHandler)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetAnimationCallbackHandler(pCallbackHandler);
}

void CAnimationController::ReleaseUploadBuffers()
{
	
}

//void CAnimationController::SetTrackAnimationSet(int nAnimationTrack, int nAnimationSet)
//{
//	if (m_pAnimationTracks)
//	{
//		// 벡터 범위 체크 후 설정
//		if (m_pAnimationSets && (nAnimationSet < m_pAnimationSets->m_vAnimationSets.size()))
//		{
//			m_pAnimationTracks[nAnimationTrack].SetAnimationSet(nAnimationSet);
//		}
//	}
//}

void CAnimationController::SetTrackEnable(int nAnimationTrack, bool bEnable)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetEnable(bEnable);
}

void CAnimationController::SetTrackPosition(int nAnimationTrack, float fPosition)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetPosition(fPosition);
}

void CAnimationController::SetTrackSpeed(int nAnimationTrack, float fSpeed)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetSpeed(fSpeed);
}

void CAnimationController::SetTrackWeight(int nAnimationTrack, float fWeight)
{
	if (m_pAnimationTracks) m_pAnimationTracks[nAnimationTrack].SetWeight(fWeight);
}

void CAnimationController::SetTrackType(int nAnimationTrack, int nType)
{
	if (!m_pAnimationTracks) return;
	if (nAnimationTrack < 0 || nAnimationTrack >= m_nAnimationTracks) return;

	m_pAnimationTracks[nAnimationTrack].m_nType = nType;
}

float CAnimationController::GetTrackPosition(int nAnimationTrack) const
{
	if (!m_pAnimationTracks) return 0.0f;
	if (nAnimationTrack < 0 || nAnimationTrack >= m_nAnimationTracks) return 0.0f;
	return m_pAnimationTracks[nAnimationTrack].m_fPosition;
}

void CAnimationController::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	for (int i = 0; i < m_nSkinnedMeshes; i++)
	{
		
		for (int j = 0; j < m_ppSkinnedMeshes[i]->m_nSkinningBones; j++)
		{
			if (m_pppSkinningBoneFrameCaches[i][j])
			{
				XMStoreFloat4x4(&m_ppcbxmf4x4MappedSkinningBoneTransforms[i][j],
					XMMatrixTranspose(XMLoadFloat4x4(&m_pppSkinningBoneFrameCaches[i][j]->m_xmf4x4World)));
			}
		}

		
		m_ppSkinnedMeshes[i]->m_pd3dcbSkinningBoneTransforms = m_ppd3dcbSkinningBoneTransforms[i];
		m_ppSkinnedMeshes[i]->m_pcbxmf4x4MappedSkinningBoneTransforms = m_ppcbxmf4x4MappedSkinningBoneTransforms[i];
	}
}

inline XMMATRIX CalcBlendedMatrix(const SBoneBlendCache& start, const XMFLOAT4X4& targetMat, float t)
{
	XMVECTOR vTargetScale, vTargetRot, vTargetTrans;
	XMMatrixDecompose(&vTargetScale, &vTargetRot, &vTargetTrans, XMLoadFloat4x4(&targetMat));

	XMVECTOR vCurScale = XMVectorLerp(start.vScale, vTargetScale, t);
	XMVECTOR vCurRot = XMQuaternionSlerp(start.vRot, vTargetRot, t);
	XMVECTOR vCurTrans = XMVectorLerp(start.vTrans, vTargetTrans, t);

	return XMMatrixAffineTransformation(vCurScale, XMVectorZero(), vCurRot, vCurTrans);
}
void CAnimationController::AdvanceTime(float fTimeElapsed, CGameObject* pRootGameObject)
{
	m_fTime += fTimeElapsed;

	if (m_pAnimationTracks)
	{
		if (m_bIsBlending)
		{
			m_fBlendTime += fTimeElapsed;
			if (m_fBlendTime >= m_fBlendDuration)
			{
				m_bIsBlending = false;
			}
		}

		for (int k = 0; k < m_nAnimationTracks; k++)
		{
			if (m_pAnimationTracks[k].m_bEnable)
			{
				int nSetIndex = m_pAnimationTracks[k].m_nAnimationSet;
				if (nSetIndex >= m_pAnimationSets->m_vAnimationSets.size()) continue;

				CAnimationSet* pAnimationSet = m_pAnimationSets->m_vAnimationSets[nSetIndex];

				m_pAnimationTracks[k].m_fPosition = m_pAnimationTracks[k].UpdatePosition(m_pAnimationTracks[k].m_fPosition, fTimeElapsed, pAnimationSet->m_fLength);

				m_pAnimationTracks[k].HandleCallback();
			}
		}

		float fBlendRatio = (m_fBlendDuration > 0.0f) ? (m_fBlendTime / m_fBlendDuration) : 1.0f;
		if (fBlendRatio > 1.0f) fBlendRatio = 1.0f;

		for (int j = 0; j < m_pAnimationSets->m_nBoneFrames; j++)
		{
			CGameObject* pBoneFrame = m_pAnimationSets->m_ppBoneFrameCaches[j];
			if (!pBoneFrame) continue;

			int nSelectedTrack = IsUpperBodyBone(j) ? m_nUpperBodyTrack : m_nLowerBodyTrack;

			if (nSelectedTrack < 0 || nSelectedTrack >= m_nAnimationTracks || !m_pAnimationTracks[nSelectedTrack].m_bEnable)
			{
				XMStoreFloat4x4(&pBoneFrame->m_xmf4x4ToParent, XMMatrixIdentity());
				continue;
			}

			int nSetIndex = m_pAnimationTracks[nSelectedTrack].m_nAnimationSet;
			if (nSetIndex >= m_pAnimationSets->m_vAnimationSets.size())
			{
				XMStoreFloat4x4(&pBoneFrame->m_xmf4x4ToParent, XMMatrixIdentity());
				continue;
			}

			CAnimationSet* pAnimationSet = m_pAnimationSets->m_vAnimationSets[nSetIndex];
			XMFLOAT4X4 xmf4x4TrackTransform = pAnimationSet->GetSRT(j, m_pAnimationTracks[nSelectedTrack].m_fPosition);

			if (m_bIsBlending && !m_vBlendCaches.empty() && j < (int)m_vBlendCaches.size())
			{
				XMMATRIX mBlended = CalcBlendedMatrix(m_vBlendCaches[j], xmf4x4TrackTransform, fBlendRatio);
				XMStoreFloat4x4(&pBoneFrame->m_xmf4x4ToParent, mBlended);
			}
			else
			{
				XMStoreFloat4x4(&pBoneFrame->m_xmf4x4ToParent, XMLoadFloat4x4(&xmf4x4TrackTransform));
			}
		}

		pRootGameObject->UpdateTransform(NULL);
		OnRootMotion(pRootGameObject);
		OnAnimationIK(pRootGameObject);
		pRootGameObject->UpdateTransform(NULL);
	}
}
bool CAnimationController::IsUpperBodyBone(int nBoneIndex) const
{
	if (!m_pbUpperBodyMask) return false;
	if (nBoneIndex < 0 || nBoneIndex >= m_pAnimationSets->m_nBoneFrames) return false;
	return m_pbUpperBodyMask[nBoneIndex];
}

void CAnimationController::BuildUpperBodyMask(CGameObject* pRootGameObject, const char* pstrUpperBodyRootFrameName)
{
	if (!pRootGameObject) return;
	if (!pstrUpperBodyRootFrameName) return;
	if (!m_pAnimationSets) return;
	if (m_pAnimationSets->m_nBoneFrames <= 0) return;

	if (m_pbUpperBodyMask)
	{
		delete[] m_pbUpperBodyMask;
		m_pbUpperBodyMask = nullptr;
	}

	m_pbUpperBodyMask = new bool[m_pAnimationSets->m_nBoneFrames];
	for (int i = 0; i < m_pAnimationSets->m_nBoneFrames; ++i)
	{
		m_pbUpperBodyMask[i] = false;
	}

	CGameObject* pUpperRoot = pRootGameObject->FindFrame(pstrUpperBodyRootFrameName);
	if (!pUpperRoot)
	{
		OutputDebugStringA("[UpperBodyMask] upper root frame not found.\n");
		return;
	}

	for (int i = 0; i < m_pAnimationSets->m_nBoneFrames; ++i)
	{
		CGameObject* pBoneFrame = m_pAnimationSets->m_ppBoneFrameCaches[i];
		if (!pBoneFrame) continue;

		CGameObject* pCurrent = pBoneFrame;
		while (pCurrent)
		{
			if (pCurrent == pUpperRoot)
			{
				m_pbUpperBodyMask[i] = true;
				break;
			}
			pCurrent = pCurrent->m_pParent;
		}
	}
}

void CAnimationController::SetSplitBodyTrackIndices(int nLowerBodyTrack, int nUpperBodyTrack)
{
	m_nLowerBodyTrack = nLowerBodyTrack;
	m_nUpperBodyTrack = nUpperBodyTrack;
}

//void CAnimationController::SetTrackAnimationSetIfChanged(int nAnimationTrack, int nAnimationSet)
//{
//	if (!m_pAnimationTracks) return;
//	if (!m_pAnimationSets) return;
//	if (nAnimationTrack < 0 || nAnimationTrack >= m_nAnimationTracks) return;
//	if (nAnimationSet < 0 || nAnimationSet >= (int)m_pAnimationSets->m_vAnimationSets.size()) return;
//
//	if (m_pAnimationTracks[nAnimationTrack].m_nAnimationSet == nAnimationSet)
//		return;
//
//	if (m_pAnimationSets && m_pAnimationSets->m_nBoneFrames > 0)
//	{
//		m_vBlendCaches.resize(m_pAnimationSets->m_nBoneFrames);
//		for (int i = 0; i < m_pAnimationSets->m_nBoneFrames; i++)
//		{
//			XMMATRIX mStart = XMLoadFloat4x4(&m_pAnimationSets->m_ppBoneFrameCaches[i]->m_xmf4x4ToParent);
//			XMMatrixDecompose(&m_vBlendCaches[i].vScale, &m_vBlendCaches[i].vRot, &m_vBlendCaches[i].vTrans, mStart);
//		}
//	}
//
//	m_bIsBlending = true;
//	m_fBlendTime = 0.0f;
//	m_fBlendDuration = 0.25f;
//
//	m_pAnimationTracks[nAnimationTrack].SetAnimationSet(nAnimationSet);
//	m_pAnimationTracks[nAnimationTrack].SetPosition(0.0f);
//}
void CAnimationController::SetTrackAnimationSetIfChanged(int nAnimationTrack, int nAnimationSet)
{
	if (!m_pAnimationTracks) return;
	if (!m_pAnimationSets) return;
	if (nAnimationTrack < 0 || nAnimationTrack >= m_nAnimationTracks) return;
	if (nAnimationSet < 0 || nAnimationSet >= (int)m_pAnimationSets->m_vAnimationSets.size()) return;

	if (m_pAnimationTracks[nAnimationTrack].m_nAnimationSet == nAnimationSet)
		return;

	if (m_pAnimationSets && m_pAnimationSets->m_nBoneFrames > 0)
	{
		m_vBlendCaches.resize(m_pAnimationSets->m_nBoneFrames);

		for (int i = 0; i < m_pAnimationSets->m_nBoneFrames; i++)
		{
			CGameObject* pBoneFrame = m_pAnimationSets->m_ppBoneFrameCaches[i];

			if (!pBoneFrame)
			{
				m_vBlendCaches[i].vScale = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
				m_vBlendCaches[i].vRot = XMQuaternionIdentity();
				m_vBlendCaches[i].vTrans = XMVectorZero();
				continue;
			}

			XMMATRIX mStart = XMLoadFloat4x4(&pBoneFrame->m_xmf4x4ToParent);
			XMMatrixDecompose(&m_vBlendCaches[i].vScale, &m_vBlendCaches[i].vRot, &m_vBlendCaches[i].vTrans, mStart);
		}
	}

	m_bIsBlending = true;
	m_fBlendTime = 0.0f;
	m_fBlendDuration = 0.25f;

	m_pAnimationTracks[nAnimationTrack].SetAnimationSet(nAnimationSet);
	m_pAnimationTracks[nAnimationTrack].SetPosition(0.0f);
}


//*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

CLoadedModelInfo::~CLoadedModelInfo()
{
	if (m_ppSkinnedMeshes) delete[] m_ppSkinnedMeshes;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CGameObject::CGameObject()
{
	m_xmf4x4ToParent = Matrix4x4::Identity();
	m_xmf4x4World = Matrix4x4::Identity();
}

CGameObject::CGameObject(int nMaterials) : CGameObject()
{
	m_nMaterials = nMaterials;
	if (m_nMaterials > 0)
	{
		m_ppMaterials = new CMaterial*[m_nMaterials];
		for(int i = 0; i < m_nMaterials; i++) m_ppMaterials[i] = NULL;
	}
}

CGameObject::~CGameObject()
{
	ReleaseUploadBuffers();

	if (m_nMaterials > 0)
	{
		for (int i = 0; i < m_nMaterials; i++)
		{
			//if (m_ppMaterials[i]) m_ppMaterials[i]->Release();
		}
	}
	if (m_ppMaterials) delete[] m_ppMaterials;

	if (m_pSkinnedAnimationController) delete m_pSkinnedAnimationController;

}

void CGameObject::init()
{
}


void CGameObject::SetChild(CGameObject *pChild, bool bReferenceUpdate)
{
	if (pChild)
	{
		pChild->m_pParent = this;
	}
	if (m_pChild)
	{
		if (pChild) pChild->m_pSibling = m_pChild->m_pSibling;
		m_pChild->m_pSibling = pChild;
	}
	else
	{
		m_pChild = pChild;
	}
}

void CGameObject::ReplaceChild(CGameObject* pChild)
{
	if (not pChild)return;
	m_pChild = pChild;
}

void CGameObject::SetMesh(CMesh *pMesh)
{
	if (m_pMesh) m_pMesh->Release();
	m_pMesh = pMesh;
	if (m_pMesh) m_pMesh->AddRef();
}

void CGameObject::SetShader(CShader *pShader)
{
	m_nMaterials = 1;
	m_ppMaterials = new CMaterial*[m_nMaterials];
	m_ppMaterials[0] = new CMaterial(0);
	m_ppMaterials[0]->SetShader(pShader);
}

void CGameObject::SetShader(int nMaterial, CShader *pShader)
{
	if (m_ppMaterials[nMaterial]) m_ppMaterials[nMaterial]->SetShader(pShader);
}

void CGameObject::SetMaterial(int nMaterial, CMaterial *pMaterial)
{
	m_ppMaterials[nMaterial] = pMaterial;
}

void CGameObject::FindAndSetSkinnedMesh(CSkinnedMesh **ppSkinnedMeshes, int *pnSkinnedMesh)
{
	if (m_pMesh && (m_pMesh->GetType() & VERTEXT_BONE_INDEX_WEIGHT)) ppSkinnedMeshes[(*pnSkinnedMesh)++] = (CSkinnedMesh *)m_pMesh;

	if (m_pSibling) m_pSibling->FindAndSetSkinnedMesh(ppSkinnedMeshes, pnSkinnedMesh);
	if (m_pChild) m_pChild->FindAndSetSkinnedMesh(ppSkinnedMeshes, pnSkinnedMesh);
}

CGameObject *CGameObject::FindFrame(const char *pstrFrameName)
{
	CGameObject *pFrameObject = NULL;
	if (!strcmp(m_pstrFrameName, pstrFrameName)) return(this);
//	if (!strncmp(m_pstrFrameName, pstrFrameName, max(strlen(m_pstrFrameName), strlen(pstrFrameName)))) return(this);

	if (m_pSibling) if (pFrameObject = m_pSibling->FindFrame(pstrFrameName)) return(pFrameObject);
	if (m_pChild) if (pFrameObject = m_pChild->FindFrame(pstrFrameName)) return(pFrameObject);

	return(NULL);
}
void CGameObject::DebugPrintMixamoFrameNames(int nDepth)
{
	if (m_pstrFrameName)
	{
		if (strstr(m_pstrFrameName, "mixamorig:"))
		{
			char buffer[512] = { 0 };
			int offset = 0;

			for (int i = 0; i < nDepth; ++i)
			{
				offset += sprintf_s(buffer + offset, sizeof(buffer) - offset, "  ");
			}

			sprintf_s(buffer + offset, sizeof(buffer) - offset, "%s\n", m_pstrFrameName);
			OutputDebugStringA(buffer);
		}
	}

	if (m_pChild)   m_pChild->DebugPrintMixamoFrameNames(nDepth + 1);
	if (m_pSibling) m_pSibling->DebugPrintMixamoFrameNames(nDepth);
}
void CGameObject::UpdateTransform(XMFLOAT4X4 *pxmf4x4Parent)
{
	m_xmf4x4World = (pxmf4x4Parent) ? Matrix4x4::Multiply(m_xmf4x4ToParent, *pxmf4x4Parent) : m_xmf4x4ToParent;
	if(HasOOBB)
		OOBBModel.Transform(OOBBWorld, XMLoadFloat4x4(&m_xmf4x4World));
	if (m_pSibling) m_pSibling->UpdateTransform(pxmf4x4Parent);
	if (m_pChild) m_pChild->UpdateTransform(&m_xmf4x4World);
}

//void CGameObject::SetTrackAnimationSet(int nAnimationTrack, int nAnimationSet)
//{
//	if (m_pSkinnedAnimationController) m_pSkinnedAnimationController->SetTrackAnimationSet(nAnimationTrack, nAnimationSet);
//}

void CGameObject::SetTrackAnimationPosition(int nAnimationTrack, float fPosition)
{
	if (m_pSkinnedAnimationController) m_pSkinnedAnimationController->SetTrackPosition(nAnimationTrack, fPosition);
}

void CGameObject::Animate(float fTimeElapsed)
{
	if (m_pSibling) m_pSibling->Animate(fTimeElapsed);
	if (m_pChild) m_pChild->Animate(fTimeElapsed);
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera)
{
	if (m_bRenderEnabled && m_pMesh)
	{
		UpdateShaderVariable(pd3dCommandList, &m_xmf4x4World);

		if (m_nMaterials > 0)
		{
			for (int i = 0; i < m_nMaterials; i++)
			{
				if (m_ppMaterials[i])
				{
					if (not batch && m_ppMaterials[i]->m_pShader) m_ppMaterials[i]->m_pShader->Render(pd3dCommandList, pCamera, false, nPipelineState);
					m_ppMaterials[i]->UpdateShaderVariables(pd3dCommandList);
				}
				m_pMesh->Render(pd3dCommandList, i);
			}
		}
	}

	if (m_pSibling) m_pSibling->Render(pd3dCommandList, batch, nPipelineState, pCamera);
	if (m_bRenderEnabled && m_pChild) m_pChild->Render(pd3dCommandList, batch, nPipelineState, pCamera);
}

void CGameObject::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariable(ID3D12GraphicsCommandList *pd3dCommandList, XMFLOAT4X4 *pxmf4x4World)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(pxmf4x4World)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
}

void CGameObject::UpdateShaderVariable(ID3D12GraphicsCommandList *pd3dCommandList, CMaterial *pMaterial)
{
}

void CGameObject::ReleaseShaderVariables()
{
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_pMesh) m_pMesh->ReleaseUploadBuffers();

	for (int i = 0; i < m_nMaterials; i++)
	{
		if (m_ppMaterials[i]) m_ppMaterials[i]->ReleaseUploadBuffers();
	}

	if (m_pSibling) m_pSibling->ReleaseUploadBuffers();
	if (m_pChild) m_pChild->ReleaseUploadBuffers();
}

void CGameObject::SetPosition(float x, float y, float z)
{
	m_xmf4x4ToParent._41 = x;
	m_xmf4x4ToParent._42 = y;
	m_xmf4x4ToParent._43 = z;

	UpdateTransform(NULL);
}

void CGameObject::SetPosition(XMFLOAT3 xmf3Position)
{
	SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}

void CGameObject::Move(XMFLOAT3 xmf3Offset)
{
	m_xmf4x4ToParent._41 += xmf3Offset.x;
	m_xmf4x4ToParent._42 += xmf3Offset.y;
	m_xmf4x4ToParent._43 += xmf3Offset.z;

	UpdateTransform(NULL);
}

void CGameObject::SetScale(float x, float y, float z, bool replace)
{
	if(not replace){
		XMMATRIX mtxScale = XMMatrixScaling(x, y, z);
		m_xmf4x4ToParent = Matrix4x4::Multiply(mtxScale, m_xmf4x4ToParent);
	}

	UpdateTransform(NULL);
}


XMFLOAT3 CGameObject::GetPosition()
{
	return(XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43));
}

XMFLOAT3 CGameObject::GetToParentPosition()
{
	return(XMFLOAT3(m_xmf4x4ToParent._41, m_xmf4x4ToParent._42, m_xmf4x4ToParent._43));
}

XMFLOAT3 CGameObject::GetLook()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33)));
}

XMFLOAT3 CGameObject::GetUp()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23)));
}

XMFLOAT3 CGameObject::GetRight()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13)));
}

void CGameObject::MoveStrafe(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Right = GetRight();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Right, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Up = GetUp();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Up, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Look = GetLook();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Look, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	XMMATRIX mtxRotate = XMMatrixRotationRollPitchYaw(XMConvertToRadians(fPitch), XMConvertToRadians(fYaw), XMConvertToRadians(fRoll));
	m_xmf4x4ToParent = Matrix4x4::Multiply(mtxRotate, m_xmf4x4ToParent);

	UpdateTransform(NULL);
}

void CGameObject::Rotate(XMFLOAT3 *pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis), XMConvertToRadians(fAngle));
	m_xmf4x4ToParent = Matrix4x4::Multiply(mtxRotate, m_xmf4x4ToParent);

	UpdateTransform(NULL);
}

void CGameObject::Rotate(XMFLOAT4 *pxmf4Quaternion)
{
	XMMATRIX mtxRotate = XMMatrixRotationQuaternion(XMLoadFloat4(pxmf4Quaternion));
	m_xmf4x4ToParent = Matrix4x4::Multiply(mtxRotate, m_xmf4x4ToParent);

	UpdateTransform(NULL);
}

void CGameObject::SetRotate(float fPitch, float fYaw, float fRoll)
{
	XMFLOAT3 pos = XMFLOAT3(m_xmf4x4ToParent._41, m_xmf4x4ToParent._42, m_xmf4x4ToParent._43);
	XMMATRIX mtxRotate = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(fPitch),
		XMConvertToRadians(fYaw),
		XMConvertToRadians(fRoll)
	);
	XMStoreFloat4x4(&m_xmf4x4ToParent, mtxRotate);
	m_xmf4x4ToParent._41 = pos.x;
	m_xmf4x4ToParent._42 = pos.y;
	m_xmf4x4ToParent._43 = pos.z;

	UpdateTransform(NULL);
}

//#define _WITH_DEBUG_FRAME_HIERARCHY

CTexture *CGameObject::FindReplicatedTexture(_TCHAR *pstrTextureName)
{
	for (int i = 0; i < m_nMaterials; i++)
	{
		if (m_ppMaterials[i])
		{
			for (int j = 0; j < m_ppMaterials[i]->m_nTextures; j++)
			{
				if (m_ppMaterials[i]->m_ppTextures[j])
				{
					if (!_tcsncmp(m_ppMaterials[i]->m_ppstrTextureNames[j], pstrTextureName, max(_tcslen(m_ppMaterials[i]->m_ppstrTextureNames[j]), _tcslen(pstrTextureName)))) return(m_ppMaterials[i]->m_ppTextures[j]);
				}
			}
		}
	}
	CTexture *pTexture = NULL;
	if (m_pSibling) if (pTexture = m_pSibling->FindReplicatedTexture(pstrTextureName)) return(pTexture);
	if (m_pChild) if (pTexture = m_pChild->FindReplicatedTexture(pstrTextureName)) return(pTexture);

	return(NULL);
}

void CGameObject::SetOOBB(std::vector<BoundingOrientedBox*>* container)
{
	std::vector<BoundingOrientedBox*>* dest;
	if (container)
		dest = container;
	else
		dest = &OOBBs;
	if(m_pMesh){
		OOBBModel.Center = m_pMesh->GetAABBCenter();
		OOBBModel.Extents = m_pMesh->GetAABBExtents();
		OOBBModel.Orientation = XMFLOAT4(0, 0, 0, 1);
		OOBBWorld = OOBBModel;
		HasOOBB = true;
		dest->push_back(&OOBBWorld);
	}
	if (m_pSibling) m_pSibling->SetOOBB(dest);
	if (m_pChild)m_pChild->SetOOBB(dest);

}

void CGameObject::SetOOBB(BoundingOrientedBox obb)
{
	OOBBModel = obb;
	OOBBWorld = obb;
	HasOOBB = true;

	OOBBs.clear();
	OOBBs.push_back(&OOBBWorld);

	UpdateTransform(NULL);
}

void CGameObject::ClearOOBB(bool bRecursive)
{
	OOBBs.clear();
	HasOOBB = false;

	OOBBModel = BoundingOrientedBox();
	OOBBWorld = BoundingOrientedBox();

	if (bRecursive)
	{
		if (m_pChild) m_pChild->ClearOOBB(true);
		if (m_pSibling) m_pSibling->ClearOOBB(true);
	}
}

int ReadIntegerFromFile(FILE *pInFile)
{
	int nValue = 0;
	UINT nReads = (UINT)::fread(&nValue, sizeof(int), 1, pInFile); 
	return(nValue);
}

float ReadFloatFromFile(FILE *pInFile)
{
	float fValue = 0;
	UINT nReads = (UINT)::fread(&fValue, sizeof(float), 1, pInFile); 
	return(fValue);
}


BYTE ReadStringFromFile(FILE* pInFile, char* pstrToken, int nBufferSize)
{
	BYTE nStrLength = 0;
	UINT nReads = 0;

	if (!pInFile || !pstrToken || nBufferSize <= 0)
		return 0;

	pstrToken[0] = '\0';

	nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);
	if (nReads != 1)
		return 0;

	if (nStrLength >= nBufferSize)
	{
		nReads = (UINT)::fread(pstrToken, sizeof(char), nBufferSize - 1, pInFile);
		pstrToken[nBufferSize - 1] = '\0';

		int nRemain = (int)nStrLength - (nBufferSize - 1);
		if (nRemain > 0) fseek(pInFile, nRemain, SEEK_CUR);
	}
	else
	{
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
		pstrToken[nStrLength] = '\0';
	}

	return nStrLength;
}

BYTE ReadStringFromFile(FILE* pInFile, char* pstrToken)
{
	return ReadStringFromFile(pInFile, pstrToken, 260);
}

void CGameObject::LoadMaterialsFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, CGameObject *pParent, FILE *pInFile, CShader *pShader)
{
	char pstrToken[260] = { '\0' };
	int nMaterial = 0;
	UINT nReads = 0;

	m_nMaterials = ReadIntegerFromFile(pInFile);

	m_ppMaterials = new CMaterial*[m_nMaterials];
	for (int i = 0; i < m_nMaterials; i++) m_ppMaterials[i] = NULL;

	CMaterial *pMaterial = NULL;

	for ( ; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));

		if (!strcmp(pstrToken, "<Material>:"))
		{
			nMaterial = ReadIntegerFromFile(pInFile);

			pMaterial = new CMaterial(7); //0:Albedo, 1:Specular, 2:Metallic, 3:Normal, 4:Emission, 5:DetailAlbedo, 6:DetailNormal

			UINT nMeshType = GetMeshType();

			if (nMeshType & VERTEXT_NORMAL_TANGENT_TEXTURE)
			{
				if (nMeshType & VERTEXT_BONE_INDEX_WEIGHT)
				{
					if (pShader)
						pMaterial->SetShader(pShader);
					else
						pMaterial->SetSkinnedAnimationShader();
				}
				else
				{
					pMaterial->SetStandardShader();
				}
			}
			SetMaterial(nMaterial, pMaterial);
		}
		else if (!strcmp(pstrToken, "<AlbedoColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4AlbedoColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<EmissiveColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4EmissiveColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<SpecularColor>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_xmf4SpecularColor), sizeof(float), 4, pInFile);
		}
		else if (!strcmp(pstrToken, "<Glossiness>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fGlossiness), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Smoothness>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fSmoothness), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Metallic>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fSpecularHighlight), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<SpecularHighlight>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fMetallic), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<GlossyReflection>:"))
		{
			nReads = (UINT)::fread(&(pMaterial->m_fGlossyReflection), sizeof(float), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<AlbedoMap>:"))
		{
			pMaterial->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_ALBEDO_MAP, 3, pMaterial->m_ppstrTextureNames[0], &(pMaterial->m_ppTextures[0]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<SpecularMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_SPECULAR_MAP, 4, pMaterial->m_ppstrTextureNames[1], &(pMaterial->m_ppTextures[1]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<NormalMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_NORMAL_MAP, 5, pMaterial->m_ppstrTextureNames[2], &(pMaterial->m_ppTextures[2]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<MetallicMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_METALLIC_MAP, 6, pMaterial->m_ppstrTextureNames[3], &(pMaterial->m_ppTextures[3]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<EmissionMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_EMISSION_MAP, 7, pMaterial->m_ppstrTextureNames[4], &(pMaterial->m_ppTextures[4]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<DetailAlbedoMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_ALBEDO_MAP, 8, pMaterial->m_ppstrTextureNames[5], &(pMaterial->m_ppTextures[5]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<DetailNormalMap>:"))
		{
			m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_NORMAL_MAP, 9, pMaterial->m_ppstrTextureNames[6], &(pMaterial->m_ppTextures[6]), pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "</Materials>"))
		{
			break;
		}
	}
}

CGameObject *CGameObject::LoadFrameHierarchyFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, CGameObject *pParent, FILE *pInFile, CShader *pShader, int *pnSkinnedMeshes, const char* pstrFileName)
{
	char pstrToken[260] = { '\0' };
	UINT nReads = 0;

	int nFrame = 0, nTextures = 0;

	CGameObject *pGameObject = new CGameObject();

	for ( ; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
		if (!strcmp(pstrToken, "<Frame>:"))
		{
			nFrame = ::ReadIntegerFromFile(pInFile);
			nTextures = ::ReadIntegerFromFile(pInFile);

			::ReadStringFromFile(pInFile, pGameObject->m_pstrFrameName);
		}
		else if (!strcmp(pstrToken, "<Transform>:"))
		{
			XMFLOAT3 xmf3Position, xmf3Rotation, xmf3Scale;
			XMFLOAT4 xmf4Rotation;
			nReads = (UINT)::fread(&xmf3Position, sizeof(float), 3, pInFile);
			nReads = (UINT)::fread(&xmf3Rotation, sizeof(float), 3, pInFile); //Euler Angle
			nReads = (UINT)::fread(&xmf3Scale, sizeof(float), 3, pInFile);
			nReads = (UINT)::fread(&xmf4Rotation, sizeof(float), 4, pInFile); //Quaternion
		}
		else if (!strcmp(pstrToken, "<TransformMatrix>:"))
		{
			nReads = (UINT)::fread(&pGameObject->m_xmf4x4ToParent, sizeof(float), 16, pInFile);
		}
		else if (!strcmp(pstrToken, "<Mesh>:"))
		{
			CStandardMesh *pMesh = new CStandardMesh(pd3dDevice, pd3dCommandList);
			pMesh->LoadMeshFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);
			pGameObject->SetMesh(pMesh);
		}
		else if (!strcmp(pstrToken, "<SkinningInfo>:"))
		{
			if (pnSkinnedMeshes) (*pnSkinnedMeshes)++;

			CSkinnedMesh *pSkinnedMesh = new CSkinnedMesh(pd3dDevice, pd3dCommandList);
			pSkinnedMesh->LoadSkinInfoFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);
			pSkinnedMesh->CreateShaderVariables(pd3dDevice, pd3dCommandList);

			::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));//<Mesh>:
			if (!strcmp(pstrToken, "<Mesh>:")) pSkinnedMesh->LoadMeshFromFile(pd3dDevice, pd3dCommandList, pInFile, pstrFileName);

			pGameObject->SetMesh(pSkinnedMesh);
		}
		else if (!strcmp(pstrToken, "<Materials>:"))
		{
			pGameObject->LoadMaterialsFromFile(pd3dDevice, pd3dCommandList, pParent, pInFile, pShader);
		}
		else if (!strcmp(pstrToken, "<Children>:"))
		{
			int nChilds = ::ReadIntegerFromFile(pInFile);
			if (nChilds > 0)
			{
				for (int i = 0; i < nChilds; i++)
				{
					CGameObject *pChild = CGameObject::LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pGameObject, pInFile, pShader, pnSkinnedMeshes, pstrFileName);
					if (pChild) pGameObject->SetChild(pChild);
#ifdef _WITH_DEBUG_FRAME_HIERARCHY
					TCHAR pstrDebug[256] = { 0 };
					_stprintf_s(pstrDebug, 256, _T("(Frame: %p) (Parent: %p)\n"), pChild, pGameObject);
					OutputDebugString(pstrDebug);
#endif
				}
			}
		}
		else if (!strcmp(pstrToken, "</Frame>"))
		{
			break;
		}
	}
	return(pGameObject);
}

CGameObject* CGameObject::LoadGeometryModelByName(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CGameObject* pParent, const char* name, CShader* pShader, int* pnSkinnedMeshes)
{
	CGameObject* model = NULL;
	FILE* pInFile;
	::fopen_s(&pInFile, name, "rb");
	if (pInFile)
	{
		::rewind(pInFile);
		model = CGameObject::LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pParent, pInFile, pShader, 0, name);
		::fclose(pInFile);
	}
	else
	{
		OutputDebugString(L"Error: file not found.\n");
	}
	return model;
}

void CGameObject::PrintFrameInfo(CGameObject *pGameObject, CGameObject *pParent)
{
	TCHAR pstrDebug[256] = { 0 };

	_stprintf_s(pstrDebug, 256, _T("(Frame: %p) (Parent: %p)\n"), pGameObject, pParent);
	OutputDebugString(pstrDebug);

	if (pGameObject->m_pSibling) CGameObject::PrintFrameInfo(pGameObject->m_pSibling, pParent);
	if (pGameObject->m_pChild) CGameObject::PrintFrameInfo(pGameObject->m_pChild, pGameObject);
}

void CGameObject::LoadAnimationFromFile(FILE* pInFile, CLoadedModelInfo* pLoadedModel)
{
	char pstrToken[260] = { '\0' };
	UINT nReads = 0;

	int nAnimationSets = 0;

	for (; ; )
	{
		::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
		if (!strcmp(pstrToken, "<AnimationSets>:"))
		{
			nAnimationSets = ::ReadIntegerFromFile(pInFile);
			pLoadedModel->m_pAnimationSets = new CAnimationSets(nAnimationSets);
		}
		else if (!strcmp(pstrToken, "<FrameNames>:"))
		{
			pLoadedModel->m_pAnimationSets->m_nBoneFrames = ::ReadIntegerFromFile(pInFile);
			pLoadedModel->m_pAnimationSets->m_ppBoneFrameCaches = new CGameObject * [pLoadedModel->m_pAnimationSets->m_nBoneFrames];

			for (int j = 0; j < pLoadedModel->m_pAnimationSets->m_nBoneFrames; j++)
			{
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));

				// [수정] 본 프레임을 찾아서 저장하되, NULL인지 체크
				CGameObject* pFoundFrame = pLoadedModel->m_pModelRootObject->FindFrame(pstrToken);
				pLoadedModel->m_pAnimationSets->m_ppBoneFrameCaches[j] = pFoundFrame;

				// 프레임을 못 찾았으면(NULL이면) 아래 디버그 코드 실행 시 뻗으므로 건너뜀
				if (pFoundFrame == NULL)
				{
					continue;
				}

#ifdef _WITH_DEBUG_SKINNING_BONE
				TCHAR pstrDebug[256] = { 0 };
				TCHAR pwstrAnimationBoneName[260] = { 0 };
				TCHAR pwstrBoneCacheName[260] = { 0 };
				size_t nConverted = 0;
				mbstowcs_s(&nConverted, pwstrAnimationBoneName, _countof(pwstrAnimationBoneName), pstrToken, _TRUNCATE);
				mbstowcs_s(&nConverted, pwstrBoneCacheName, _countof(pwstrBoneCacheName), pLoadedModel->m_pAnimationSets->m_ppBoneFrameCaches[j]->m_pstrFrameName, _TRUNCATE);
				_stprintf_s(pstrDebug, 256, _T("AnimationBoneFrame:: Cache(%s) AnimationBone(%s)\n"), pwstrBoneCacheName, pwstrAnimationBoneName);
				OutputDebugString(pstrDebug);
#endif
			}
		}
		else if (!strcmp(pstrToken, "<AnimationSet>:"))
		{
			int nAnimationSet = ::ReadIntegerFromFile(pInFile);

			::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken)); //Animation Set Name

			float fLength = ::ReadFloatFromFile(pInFile);
			int nFramesPerSecond = ::ReadIntegerFromFile(pInFile);
			int nKeyFrames = ::ReadIntegerFromFile(pInFile);

			CAnimationSet* pNewAnimationSet = new CAnimationSet(fLength, nFramesPerSecond, nKeyFrames, pLoadedModel->m_pAnimationSets->m_nBoneFrames, pstrToken);
			pLoadedModel->m_pAnimationSets->m_vAnimationSets.push_back(pNewAnimationSet);

			for (int i = 0; i < nKeyFrames; i++)
			{
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken));
				if (!strcmp(pstrToken, "<Transforms>:"))
				{
					int nKey = ::ReadIntegerFromFile(pInFile);
					float fKeyTime = ::ReadFloatFromFile(pInFile);

#ifdef _WITH_ANIMATION_SRT
					// 주의: 원본 코드의 변수명(pAnimationSet) 오류 가능성을 pNewAnimationSet으로 맞춰둠
					pNewAnimationSet->m_pfKeyFrameScaleTimes[i] = fKeyTime;
					pNewAnimationSet->m_pfKeyFrameRotationTimes[i] = fKeyTime;
					pNewAnimationSet->m_pfKeyFrameTranslationTimes[i] = fKeyTime;
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf3KeyFrameScales[i], sizeof(XMFLOAT3), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf4KeyFrameRotations[i], sizeof(XMFLOAT4), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf3KeyFrameTranslations[i], sizeof(XMFLOAT3), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
#else
					pNewAnimationSet->m_pfKeyFrameTimes[i] = fKeyTime;
					nReads = (UINT)::fread(pNewAnimationSet->m_ppxmf4x4KeyFrameTransforms[i], sizeof(XMFLOAT4X4), pLoadedModel->m_pAnimationSets->m_nBoneFrames, pInFile);
#endif
				}
			}
		}
		else if (!strcmp(pstrToken, "</AnimationSets>"))
		{
			break;
		}
	}
}

CLoadedModelInfo *CGameObject::LoadGeometryAndAnimationFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, const char *pstrFileName, CShader *pShader)
{
	FILE *pInFile = NULL;
	::fopen_s(&pInFile, pstrFileName, "rb");
	::rewind(pInFile);

	CLoadedModelInfo *pLoadedModel = new CLoadedModelInfo();

	char pstrToken[260] = { '\0' };

	for ( ; ; )
	{
		if (::ReadStringFromFile(pInFile, pstrToken))
		{
			if (!strcmp(pstrToken, "<Hierarchy>:"))
			{
				pLoadedModel->m_pModelRootObject = CGameObject::LoadFrameHierarchyFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, NULL, pInFile, pShader, &pLoadedModel->m_nSkinnedMeshes, pstrFileName);
				::ReadStringFromFile(pInFile, pstrToken, _countof(pstrToken)); //"</Hierarchy>"
			}
			else if (!strcmp(pstrToken, "<Animation>:"))
			{
				CGameObject::LoadAnimationFromFile(pInFile, pLoadedModel);
				int nSkinnedMesh = 0;
				pLoadedModel->m_ppSkinnedMeshes = new CSkinnedMesh * [pLoadedModel->m_nSkinnedMeshes];
				pLoadedModel->m_pModelRootObject->FindAndSetSkinnedMesh(pLoadedModel->m_ppSkinnedMeshes, &nSkinnedMesh);
			}
			else if (!strcmp(pstrToken, "</Animation>:"))
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

#ifdef _WITH_DEBUG_FRAME_HIERARCHY
	TCHAR pstrDebug[256] = { 0 };
	_stprintf_s(pstrDebug, 256, _T("Frame Hierarchy\n"));
	OutputDebugString(pstrDebug);

	CGameObject::PrintFrameInfo(pGameObject, NULL);
#endif

	return(pLoadedModel);
}

//모델 리소스 관리 함수
static CGameObject* CloneModelHierarchyShared(CGameObject* pSrc)
{
	if (!pSrc) return nullptr;

	CGameObject* pDst = new CGameObject();
	pDst->SetModel(pSrc);

	for (CGameObject* pChildSrc = pSrc->m_pChild; pChildSrc; pChildSrc = pChildSrc->m_pSibling)
	{
		CGameObject* pChildDst = CloneModelHierarchyShared(pChildSrc);
		if (pChildDst)
		{
			pDst->SetChild(pChildDst, true);
		}
	}

	return pDst;
}

void CGameObject::ClearModelResources()
{
	if (m_pMesh)
	{
		m_pMesh->Release();
		m_pMesh = nullptr;
	}

	if (m_ppMaterials)
	{
		for (int i = 0; i < m_nMaterials; ++i)
		{
			if (m_ppMaterials[i])
			{
				//m_ppMaterials[i]->Release();
				m_ppMaterials[i] = nullptr;
			}
		}
		delete[] m_ppMaterials;
		m_ppMaterials = nullptr;
	}

	m_nMaterials = 0;
	HasOOBB = false;
	OOBBs.clear();
}

void CGameObject::SetModel(CGameObject* pModelPrototype)
{
	if (!pModelPrototype) return;

	ClearModelResources();

	m_xmf4x4ToParent = pModelPrototype->m_xmf4x4ToParent;
	m_xmf4x4World = pModelPrototype->m_xmf4x4World;

	strcpy_s(m_pstrFrameName, _countof(m_pstrFrameName), pModelPrototype->m_pstrFrameName);

	if (pModelPrototype->m_pMesh)
	{
		SetMesh(pModelPrototype->m_pMesh);
	}

	if (pModelPrototype->m_nMaterials > 0)
	{
		m_nMaterials = pModelPrototype->m_nMaterials;
		m_ppMaterials = new CMaterial * [m_nMaterials];

		for (int i = 0; i < m_nMaterials; ++i)
		{
			m_ppMaterials[i] = nullptr;
			if (pModelPrototype->m_ppMaterials[i])
			{
				SetMaterial(i, pModelPrototype->m_ppMaterials[i]);
			}
		}
	}

	HasOOBB = pModelPrototype->HasOOBB;
	OOBBModel = pModelPrototype->OOBBModel;
	OOBBWorld = pModelPrototype->OOBBWorld;

	OOBBs.clear();
	if (HasOOBB)
	{
		OOBBs.push_back(&OOBBWorld);
	}
}

CGameObject* CGameObject::CreateModelInstance(CGameObject* pModelPrototype)
{
	return CloneModelHierarchyShared(pModelPrototype);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
CSkyBox::CSkyBox(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature) : CGameObject(1)
{
	CSkyBoxMesh *pSkyBoxMesh = new CSkyBoxMesh(pd3dDevice, pd3dCommandList, 20.0f, 20.0f, 2.0f);
	SetMesh(pSkyBoxMesh);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CTexture* pSkyBoxTexture = new CTexture(1, RESOURCE_TEXTURE_CUBE, 0, 1);
	pSkyBoxTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"SkyBox/SkyBox_0.dds", RESOURCE_TEXTURE_CUBE, 0);

	CSkyBoxShader *pSkyBoxShader = new CSkyBoxShader();
	pSkyBoxShader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	pSkyBoxShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pSkyBoxTexture, 0, 10);

	CMaterial *pSkyBoxMaterial = new CMaterial(1);
	pSkyBoxMaterial->SetTexture(pSkyBoxTexture);
	pSkyBoxMaterial->SetShader(pSkyBoxShader);

	SetMaterial(0, pSkyBoxMaterial);
}

CSkyBox::~CSkyBox()
{
}

void CSkyBox::Render(ID3D12GraphicsCommandList *pd3dCommandList, bool batch, int nPipelineState, CCamera *pCamera)
{
	XMFLOAT3 xmf3CameraPos = pCamera->GetPosition();
	SetPosition(xmf3CameraPos.x, xmf3CameraPos.y, xmf3CameraPos.z);

	CGameObject::Render(pd3dCommandList, batch, nPipelineState, pCamera);
}

// 시야 오브젝트 배치
void ViewObject::Animate(float fTimeElapsed)
{
	if (!player) return;

	XMFLOAT3 viewPos = player->GetPosition();
	viewPos.y += 0.03f;

	m_xmf4x4ToParent = Matrix4x4::Identity();
	m_xmf4x4ToParent._41 = viewPos.x;
	m_xmf4x4ToParent._42 = viewPos.y;
	m_xmf4x4ToParent._43 = viewPos.z;

	if (m_pCircleObject)
	{
		m_pCircleObject->m_xmf4x4ToParent = Matrix4x4::Identity();
		m_pCircleObject->m_xmf4x4ToParent._42 = 0.0f;
	}

	//카메라 방향 회전
	if (m_pConeObject)
	{
		XMFLOAT3 look = player->GetLookVector();

		if (player->GetCamera())
			look = player->GetCamera()->GetLookVector();

		look.y = 0.0f;

		float lenSq = look.x * look.x + look.z * look.z;
		if (lenSq < 0.000001f)
		{
			look = XMFLOAT3(0.0f, 0.0f, 1.0f);
		}
		else
		{
			look = Vector3::Normalize(look);
		}

		float yaw = atan2f(look.x, look.z);

		XMMATRIX mRot = XMMatrixRotationY(yaw);
		XMFLOAT4X4 localRot;
		XMStoreFloat4x4(&localRot, mRot);

		localRot._42 = 0.001f; 
		m_pConeObject->m_xmf4x4ToParent = localRot;
	}
}

// block기준으로 시야 메시 계산
void ViewObject::UpdateClippedMeshes(const std::vector<CGameObject*>& blockers)
{
	if (!player) return;

	XMFLOAT3 origin = player->GetPosition();
	origin.y += 0.5f;

	XMFLOAT3 look = player->GetLookVector();
	if (player->GetCamera())
		look = player->GetCamera()->GetLookVector();

	look.y = 0.0f;

	float lenSq = look.x * look.x + look.z * look.z;
	if (lenSq < 0.000001f)
	{
		look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		look = Vector3::Normalize(look);
	}

	float yaw = atan2f(look.x, look.z);

	if (m_pCircleObject)
	{
		CViewCircleMesh* pCircleMesh = dynamic_cast<CViewCircleMesh*>(m_pCircleObject->GetMesh());
		if (pCircleMesh)
			pCircleMesh->UpdateClippedMesh(origin, blockers);
	}

	if (m_pConeObject)
	{
		CViewConeMesh* pConeMesh = dynamic_cast<CViewConeMesh*>(m_pConeObject->GetMesh());
		if (pConeMesh)
			pConeMesh->UpdateClippedMesh(origin, yaw, blockers);
	}
}