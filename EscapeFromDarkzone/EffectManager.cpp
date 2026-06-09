#include "stdafx.h"
#include "Scene.h"
#include "Object.h"
#include "Camera.h"
#include "EffectShader.h"
#include "ResourceManager.h"
#include "EffectManager.h"


EffectManager::EffectManager()
{
	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		m_pEffectMaterials[i] = nullptr;
		m_pd3dInstBufferEffect[i] = nullptr;
		m_pMappedInstBufferEffect[i] = nullptr;
		ZeroMemory(&m_d3dInstBufferViewEffect[i], sizeof(D3D12_VERTEX_BUFFER_VIEW));
	}
}

EffectManager::~EffectManager()
{
	Release();
}

//리소스 초기화
void EffectManager::Initialize(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	m_pd3dDevice = pd3dDevice;
	m_pd3dCommandList = pd3dCommandList;
	m_pd3dGraphicsRootSignature = pd3dGraphicsRootSignature;

	// 이펙트 쉐이더 생성
	m_pEffectShader = new CEffectShader();
	m_pEffectShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	m_pEffectShader->CreateGraphicsPipelineState(pd3dDevice, pd3dGraphicsRootSignature, 0);

	float effectWidth = 1.0f;
	float effectHeight = 1.0f * (180.0f / 182.0f);
	m_pEffectMesh = new CParticleMesh(pd3dDevice, pd3dCommandList, effectWidth, effectHeight);

	// 이펙트 텍스처 / 머티리얼
	// Bomb
	{
		CTexture* pBombTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
		pBombTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Explosion3.dds", RESOURCE_TEXTURE2D, 0);
		ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pBombTexture, 0, 3);

		m_pEffectMaterials[EFFECT_BOMB] = new CMaterial(1);
		m_pEffectMaterials[EFFECT_BOMB]->SetTexture(pBombTexture);
		m_pEffectMaterials[EFFECT_BOMB]->SetShader(m_pEffectShader);
	}

	// Spark - Rifle / SMG
	{
		CTexture* pSparkRifleSMGTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
		pSparkRifleSMGTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Spark_Rifle_SMG.dds", RESOURCE_TEXTURE2D, 0);
		ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pSparkRifleSMGTexture, 0, 3);

		m_pEffectMaterials[EFFECT_SPARK_RIFLE_SMG] = new CMaterial(1);
		m_pEffectMaterials[EFFECT_SPARK_RIFLE_SMG]->SetTexture(pSparkRifleSMGTexture);
		m_pEffectMaterials[EFFECT_SPARK_RIFLE_SMG]->SetShader(m_pEffectShader);
	}

	// Spark - Shotgun
	{
		CTexture* pSparkShotgunTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
		pSparkShotgunTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Spark_Shotgun.dds", RESOURCE_TEXTURE2D, 0);
		ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pSparkShotgunTexture, 0, 3);

		m_pEffectMaterials[EFFECT_SPARK_SHOTGUN] = new CMaterial(1);
		m_pEffectMaterials[EFFECT_SPARK_SHOTGUN]->SetTexture(pSparkShotgunTexture);
		m_pEffectMaterials[EFFECT_SPARK_SHOTGUN]->SetShader(m_pEffectShader);
	}

	// Spark - Pistol, Rifle/SMG 이펙트 재사용 + 크기만 작게
	{
		CTexture* pSparkPistolTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
		pSparkPistolTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Spark_Rifle_SMG.dds", RESOURCE_TEXTURE2D, 0);
		ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pSparkPistolTexture, 0, 3);

		m_pEffectMaterials[EFFECT_SPARK_PISTOL] = new CMaterial(1);
		m_pEffectMaterials[EFFECT_SPARK_PISTOL]->SetTexture(pSparkPistolTexture);
		m_pEffectMaterials[EFFECT_SPARK_PISTOL]->SetShader(m_pEffectShader);
	}
	//// Blood
	//{
	//	CTexture* pBloodTexture = new CTexture(1, RESOURCE_TEXTURE2D, 0, 1);
	//	pBloodTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, L"Model/Explosion1.dds", RESOURCE_TEXTURE2D, 0);
	//	ResourceManager::Instance().CreateShaderResourceViews(pd3dDevice, pBloodTexture, 0, 3);

	//	m_pEffectMaterials[EFFECT_BLOOD] = new CMaterial(1);
	//	m_pEffectMaterials[EFFECT_BLOOD]->SetTexture(pBloodTexture);
	//	m_pEffectMaterials[EFFECT_BLOOD]->SetShader(m_pEffectShader);
	//}

	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		UINT nBufferSize = sizeof(EFFECT_INFO) * 100;

		m_pd3dInstBufferEffect[i] = ::CreateBufferResource(
			pd3dDevice,
			pd3dCommandList,
			NULL,
			nBufferSize,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			NULL
		);

		if (m_pd3dInstBufferEffect[i])
		{
			HRESULT hr = m_pd3dInstBufferEffect[i]->Map(0, NULL, (void**)&m_pMappedInstBufferEffect[i]);
			if (SUCCEEDED(hr) && m_pMappedInstBufferEffect[i])
			{
				m_d3dInstBufferViewEffect[i].BufferLocation = m_pd3dInstBufferEffect[i]->GetGPUVirtualAddress();
				m_d3dInstBufferViewEffect[i].StrideInBytes = sizeof(EFFECT_INFO);
				m_d3dInstBufferViewEffect[i].SizeInBytes = nBufferSize;
			}
		}
	}

	// 기본 이펙트 풀 생성
	for (int i = 0; i < INITIAL_EFFECT_POOL_SIZE; ++i)
	{
		m_vEffectPools[EFFECT_BOMB].push_back(std::make_unique<CEffect>(EFFECT_BOMB, 1.0f));
		m_vEffectPools[EFFECT_SPARK_RIFLE_SMG].push_back(std::make_unique<CEffect>(EFFECT_SPARK_RIFLE_SMG, 0.055f));
		m_vEffectPools[EFFECT_SPARK_SHOTGUN].push_back(std::make_unique<CEffect>(EFFECT_SPARK_SHOTGUN, 0.045f));
		m_vEffectPools[EFFECT_SPARK_PISTOL].push_back(std::make_unique<CEffect>(EFFECT_SPARK_PISTOL, 0.045f));
		m_vEffectPools[EFFECT_BLOOD].push_back(std::make_unique<CEffect>(EFFECT_BLOOD, 0.8f));
	}

	// 레이저 쉐이더 생성
	m_pLaserShader = new CLaserShader();
	m_pLaserShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	m_pLaserShader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);

	{
		CGameObject* pLaserObject = new CGameObject();
		pLaserObject->SetMesh(new CLaserMesh(pd3dDevice, pd3dCommandList));
		pLaserObject->SetShader(m_pLaserShader);

		m_pLaserShader->addObjects(pLaserObject);

		XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
		pLaserObject->UpdateTransform(NULL);

		m_LaserObjects.emplace(0, pLaserObject);
	}
}

void EffectManager::Release()
{
	for (int type = 0; type < EFFECT_MAX; ++type)
	{
		m_vEffectPools[type].clear();

		if (m_pd3dInstBufferEffect[type])
		{
			m_pd3dInstBufferEffect[type]->Unmap(0, NULL);
			m_pd3dInstBufferEffect[type]->Release();
			m_pd3dInstBufferEffect[type] = nullptr;
		}
		m_pMappedInstBufferEffect[type] = nullptr;

		if (m_pEffectMaterials[type])
		{
			delete m_pEffectMaterials[type];
			m_pEffectMaterials[type] = nullptr;
		}
	}

	m_LaserObjects.clear();

	if (m_pLaserShader)
	{
		delete m_pLaserShader;
		m_pLaserShader = nullptr;
	}

	if (m_pEffectMesh)
	{
		delete m_pEffectMesh;
		m_pEffectMesh = nullptr;
	}

	if (m_pEffectShader)
	{
		delete m_pEffectShader;
		m_pEffectShader = nullptr;
	}

	m_pd3dDevice = nullptr;
	m_pd3dCommandList = nullptr;
	m_pd3dGraphicsRootSignature = nullptr;
}

void EffectManager::ReleaseUploadBuffers()
{
	if (m_pEffectMesh)
	{
		m_pEffectMesh->ReleaseUploadBuffers();
	}

	if (m_pEffectShader)
	{
		m_pEffectShader->ReleaseUploadBuffers();
	}

	if (m_pLaserShader)
	{
		m_pLaserShader->ReleaseUploadBuffers();
	}
}

void EffectManager::RequestPlayEffect(EFFECT_TYPE type, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 up)
{
	if (type < 0 || type >= EFFECT_MAX)
		return;

	for (auto& pEffect : m_vEffectPools[type])
	{
		if (pEffect && pEffect->IsDead())
		{
			pEffect->Play(pos, right, up);
			return;
		}
	}

	float lifeTime = 0.5f;

	switch (type)
	{
	case EFFECT_BOMB:
		lifeTime = 1.0f;
		break;

	case EFFECT_SPARK_RIFLE_SMG:
		lifeTime = 0.055f;
		break;

	case EFFECT_SPARK_SHOTGUN:
		lifeTime = 0.045f;
		break;

	case EFFECT_SPARK_PISTOL:
		lifeTime = 0.045f;
		break;

	case EFFECT_BLOOD:
		lifeTime = 0.8f;
		break;

	default:
		lifeTime = 0.5f;
		break;
	}

	auto pNewEffect = std::make_unique<CEffect>(type, lifeTime);
	pNewEffect->Play(pos, right, up);

	m_vEffectPools[type].push_back(std::move(pNewEffect));
}

void EffectManager::BuildEffectBasis(
	const XMFLOAT3& direction,
	XMFLOAT3& outRight,
	XMFLOAT3& outUp)
{
	XMFLOAT3 dir = direction;

	if (Vector3::Length(dir) < 0.0001f)
	{
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
	}

	dir = Vector3::Normalize(dir);

	XMVECTOR vDir = XMLoadFloat3(&dir);
	XMVECTOR vWorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR vRight = XMVector3Cross(vWorldUp, vDir);

	if (XMVectorGetX(XMVector3LengthSq(vRight)) < 0.0001f)
	{
		vWorldUp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		vRight = XMVector3Cross(vWorldUp, vDir);
	}

	vRight = XMVector3Normalize(vRight);

	XMVECTOR vUp = XMVector3Normalize(XMVector3Cross(vDir, vRight));

	XMStoreFloat3(&outRight, vRight);
	XMStoreFloat3(&outUp, vUp);
}

void EffectManager::PlayEffectByID(const EffectSpawnDesc& desc)
{
	if (desc.id == EffectID::NONE)
		return;

	XMFLOAT3 right;
	XMFLOAT3 up;

	BuildEffectBasis(desc.direction, right, up);

	switch (desc.id)
	{
	case EffectID::GRENADE_EXPLOSION:
	{
		RequestPlayEffect(
			EFFECT_BOMB,
			desc.position,
			right,
			up
		);
		break;
	}

	case EffectID::SPARK:
	{
		RequestPlayEffect(EFFECT_SPARK_RIFLE_SMG, desc.position, right, up);
		break;
	}

	case EffectID::BLOOD:
	{
		RequestPlayEffect(
			EFFECT_BLOOD,
			desc.position,
			right,
			up
		);
		break;
	}

	case EffectID::HIT:
	{
		RequestPlayEffect(EFFECT_SPARK_RIFLE_SMG, desc.position, right, up);
		break;
	}

	default:
		break;
	}
}

void EffectManager::Update(float fTimeElapsed)
{
	for (int type = 0; type < EFFECT_MAX; ++type)
	{
		for (auto& pEffect : m_vEffectPools[type])
		{
			if (pEffect && !pEffect->IsDead())
			{
				pEffect->Animate(fTimeElapsed);
			}
		}
	}
}

CGameObject* EffectManager::GetOrCreateLaserObject(int ownerId)
{
	auto it = m_LaserObjects.find(ownerId);
	if (it != m_LaserObjects.end())
	{
		return it->second;
	}

	return nullptr;
}

void EffectManager::UpdateLaser(
	int ownerId,
	const XMFLOAT3& origin,
	const XMFLOAT3& right,
	const XMFLOAT3& up,
	const XMFLOAT3& dir,
	float fLength)
{
	CGameObject* pLaserObject = GetOrCreateLaserObject(ownerId);
	if (!pLaserObject) return;

	XMVECTOR vOrigin = XMLoadFloat3(&origin);
	XMVECTOR vRight = XMVector3Normalize(XMLoadFloat3(&right));
	XMVECTOR vUp = XMVector3Normalize(XMLoadFloat3(&up));
	XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&dir));

	XMMATRIX matScale = XMMatrixScaling(0.05f, 0.05f, fLength);

	XMMATRIX matRotation = XMMatrixIdentity();
	matRotation.r[0] = XMVectorSetW(vRight, 0.0f);
	matRotation.r[1] = XMVectorSetW(vUp, 0.0f);
	matRotation.r[2] = XMVectorSetW(vDir, 0.0f);
	matRotation.r[3] = XMVectorSet(0, 0, 0, 1);

	XMMATRIX matTranslation = XMMatrixTranslationFromVector(vOrigin);
	XMMATRIX matWorld = matScale * matRotation * matTranslation;

	XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, matWorld);
	pLaserObject->UpdateTransform(NULL);
}

void EffectManager::HideLaser(int ownerId)
{
	auto it = m_LaserObjects.find(ownerId);
	if (it == m_LaserObjects.end()) return;

	CGameObject* pLaserObject = it->second;
	if (!pLaserObject) return;

	XMStoreFloat4x4(&pLaserObject->m_xmf4x4ToParent, XMMatrixScaling(0.0f, 0.0f, 0.0f));
	pLaserObject->UpdateTransform(NULL);
}

void EffectManager::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, int nPipelineState)
{
	if (nPipelineState == SHADOW) return;

	if (m_pLaserShader)
	{
		m_pLaserShader->Render(pd3dCommandList, pCamera, true, nPipelineState);
	}

	if (m_pEffectShader)
	{
		m_pEffectShader->Render(pd3dCommandList, pCamera, false, nPipelineState);
	}

	for (int type = 0; type < EFFECT_MAX; ++type)
	{
		if (!m_pMappedInstBufferEffect[type] || !m_pd3dInstBufferEffect[type])
			continue;

		int activeCount = 0;

		for (auto& pEffect : m_vEffectPools[type])
		{
			if (!pEffect || pEffect->IsDead()) continue;

			m_pMappedInstBufferEffect[type][activeCount].vPosition = pEffect->GetPosition();
			m_pMappedInstBufferEffect[type][activeCount].fProgress = pEffect->GetProgress();

			switch (type)
			{
			case EFFECT_BOMB:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(6.0f, 6.0f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.86f, 0.65f, 0.45f);
				break;

			case EFFECT_SPARK_RIFLE_SMG:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(1.45f, 1.45f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.58f, 0.20f, 1.0f);
				break;

			case EFFECT_SPARK_SHOTGUN:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(1.8f, 1.8f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.58f, 0.20f, 1.0f);
				break;

			case EFFECT_SPARK_PISTOL:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(0.75f, 0.75f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.58f, 0.20f, 1.0f);
				break;

			case EFFECT_BLOOD:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(2.0f, 2.0f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.15f, 0.10f, 1.0f);
				break;

			default:
				m_pMappedInstBufferEffect[type][activeCount].vSize = XMFLOAT2(1.0f, 1.0f);
				m_pMappedInstBufferEffect[type][activeCount].vColor = XMFLOAT4(1.0f, 0.58f, 0.20f, 1.0f);
				break;
			}

			m_pMappedInstBufferEffect[type][activeCount].vRight = pEffect->GetRight();
			m_pMappedInstBufferEffect[type][activeCount].vUp = pEffect->GetUp();

			activeCount++;

			if (activeCount >= 100)
				break;
		}

		if (activeCount > 0 && m_pEffectMesh && m_pEffectMaterials[type])
		{
			m_pEffectMaterials[type]->UpdateShaderVariables(pd3dCommandList);
			pd3dCommandList->IASetVertexBuffers(1, 1, &m_d3dInstBufferViewEffect[type]);
			m_pEffectMesh->Render(pd3dCommandList, activeCount);
		}
	}
}