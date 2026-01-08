//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "InputManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayer

CPlayer::CPlayer()
{
	m_pCamera = NULL;

	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fMaxVelocityXZ = 0.0f;
	m_fMaxVelocityY = 0.0f;
	m_fFriction = 0.0f;

	m_fPitch = 0.0f;
	m_fRoll = 0.0f;
	m_fYaw = 0.0f;

	m_pPlayerUpdatedContext = NULL;
	m_pCameraUpdatedContext = NULL;
}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

	if (m_pCamera) delete m_pCamera;
}

void CPlayer::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	if (m_pCamera) m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CPlayer::ReleaseShaderVariables()
{
	if (m_pCamera) m_pCamera->ReleaseShaderVariables();
}

void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	if (dwDirection)
	{
		XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
		if (dwDirection & DIR_FORWARD) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, fDistance);
		if (dwDirection & DIR_BACKWARD) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, -fDistance);
		if (dwDirection & DIR_RIGHT) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, fDistance);
		if (dwDirection & DIR_LEFT) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, -fDistance);
		if (dwDirection & DIR_UP) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, fDistance);
		if (dwDirection & DIR_DOWN) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, -fDistance);

		Move(xmf3Shift, bUpdateVelocity);
	}
}

void CPlayer::Move(const XMFLOAT3& xmf3Shift, bool bUpdateVelocity)
{
	if (bUpdateVelocity)
	{
		m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, xmf3Shift);
	}
	else
	{
		m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Shift);
		m_pCamera->Move(xmf3Shift);
	}
}

void CPlayer::Rotate(float x, float y, float z)
{
	DWORD nCurrentCameraMode = m_pCamera->GetMode();
	if ((nCurrentCameraMode == FIRST_PERSON_CAMERA) || (nCurrentCameraMode == THIRD_PERSON_CAMERA))
	{
		if (x != 0.0f)
		{
			m_fPitch += x;
			if (m_fPitch > +89.0f) { x -= (m_fPitch - 89.0f); m_fPitch = +89.0f; }
			if (m_fPitch < -89.0f) { x -= (m_fPitch + 89.0f); m_fPitch = -89.0f; }
		}
		if (y != 0.0f)
		{
			m_fYaw += y;
			if (m_fYaw > 360.0f) m_fYaw -= 360.0f;
			if (m_fYaw < 0.0f) m_fYaw += 360.0f;
		}
		if (z != 0.0f)
		{
			m_fRoll += z;
			if (m_fRoll > +20.0f) { z -= (m_fRoll - 20.0f); m_fRoll = +20.0f; }
			if (m_fRoll < -20.0f) { z -= (m_fRoll + 20.0f); m_fRoll = -20.0f; }
		}
		m_pCamera->Rotate(x, y, z);
		if (y != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Up), XMConvertToRadians(y));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
	}
	else if (nCurrentCameraMode == SPACESHIP_CAMERA)
	{
		m_pCamera->Rotate(x, y, z);
		if (x != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Right), XMConvertToRadians(x));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		}
		if (y != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Up), XMConvertToRadians(y));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
		if (z != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Look), XMConvertToRadians(z));
			m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
	}

	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);
}

void CPlayer::Update(float fTimeElapsed)
{
	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, m_xmf3Gravity);
	float fLength = sqrtf(m_xmf3Velocity.x * m_xmf3Velocity.x + m_xmf3Velocity.z * m_xmf3Velocity.z);
	float fMaxVelocityXZ = m_fMaxVelocityXZ;
	if (fLength > m_fMaxVelocityXZ)
	{
		m_xmf3Velocity.x *= (fMaxVelocityXZ / fLength);
		m_xmf3Velocity.z *= (fMaxVelocityXZ / fLength);
	}
	float fMaxVelocityY = m_fMaxVelocityY;
	fLength = sqrtf(m_xmf3Velocity.y * m_xmf3Velocity.y);
	if (fLength > m_fMaxVelocityY) m_xmf3Velocity.y *= (fMaxVelocityY / fLength);

	XMFLOAT3 xmf3Velocity = Vector3::ScalarProduct(m_xmf3Velocity, fTimeElapsed, false);
	Move(xmf3Velocity, false);

	if (m_pPlayerUpdatedContext) OnPlayerUpdateCallback(fTimeElapsed);

	DWORD nCurrentCameraMode = m_pCamera->GetMode();
	if (nCurrentCameraMode == THIRD_PERSON_CAMERA) m_pCamera->Update(m_xmf3Position, fTimeElapsed);
	if (m_pCameraUpdatedContext) OnCameraUpdateCallback(fTimeElapsed);
	if (nCurrentCameraMode == THIRD_PERSON_CAMERA) m_pCamera->SetLookAt(m_xmf3Position);
	m_pCamera->RegenerateViewMatrix();

	fLength = Vector3::Length(m_xmf3Velocity);
	float fDeceleration = (m_fFriction * fTimeElapsed);
	if (fDeceleration > fLength) fDeceleration = fLength;
	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, Vector3::ScalarProduct(m_xmf3Velocity, -fDeceleration, true));
}

CCamera *CPlayer::OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode)
{
	CCamera *pNewCamera = NULL;
	switch (nNewCameraMode)
	{
		case FIRST_PERSON_CAMERA:
			pNewCamera = new CFirstPersonCamera(m_pCamera);
			break;
		case THIRD_PERSON_CAMERA:
			pNewCamera = new CThirdPersonCamera(m_pCamera);
			break;
		case SPACESHIP_CAMERA:
			pNewCamera = new CSpaceShipCamera(m_pCamera);
			break;
	}
	if (nCurrentCameraMode == SPACESHIP_CAMERA)
	{
		m_xmf3Right = Vector3::Normalize(XMFLOAT3(m_xmf3Right.x, 0.0f, m_xmf3Right.z));
		m_xmf3Up = Vector3::Normalize(XMFLOAT3(0.0f, 1.0f, 0.0f));
		m_xmf3Look = Vector3::Normalize(XMFLOAT3(m_xmf3Look.x, 0.0f, m_xmf3Look.z));

		m_fPitch = 0.0f;
		m_fRoll = 0.0f;
		m_fYaw = Vector3::Angle(XMFLOAT3(0.0f, 0.0f, 1.0f), m_xmf3Look);
		if (m_xmf3Look.x < 0.0f) m_fYaw = -m_fYaw;
	}
	else if ((nNewCameraMode == SPACESHIP_CAMERA) && m_pCamera)
	{
		m_xmf3Right = m_pCamera->GetRightVector();
		m_xmf3Up = m_pCamera->GetUpVector();
		m_xmf3Look = m_pCamera->GetLookVector();
	}

	if (pNewCamera)
	{
		pNewCamera->SetMode(nNewCameraMode);
		pNewCamera->SetPlayer(this);
	}

	if (m_pCamera) delete m_pCamera;

	return(pNewCamera);
}

void CPlayer::OnPrepareRender()
{
	m_xmf4x4ToParent._11 = m_xmf3Right.x; m_xmf4x4ToParent._12 = m_xmf3Right.y; m_xmf4x4ToParent._13 = m_xmf3Right.z;
	m_xmf4x4ToParent._21 = m_xmf3Up.x; m_xmf4x4ToParent._22 = m_xmf3Up.y; m_xmf4x4ToParent._23 = m_xmf3Up.z;
	m_xmf4x4ToParent._31 = m_xmf3Look.x; m_xmf4x4ToParent._32 = m_xmf3Look.y; m_xmf4x4ToParent._33 = m_xmf3Look.z;
	m_xmf4x4ToParent._41 = m_xmf3Position.x; m_xmf4x4ToParent._42 = m_xmf3Position.y; m_xmf4x4ToParent._43 = m_xmf3Position.z;

	m_xmf4x4ToParent = Matrix4x4::Multiply(XMMatrixScaling(m_xmf3Scale.x, m_xmf3Scale.y, m_xmf3Scale.z), m_xmf4x4ToParent);
}

void CPlayer::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	DWORD nCameraMode = (pCamera) ? pCamera->GetMode() : 0x00;
	if (nCameraMode == THIRD_PERSON_CAMERA) CGameObject::Render(pd3dCommandList, pCamera);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// 
//CAirplanePlayer::CAirplanePlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext)
//{
//	m_pCamera = ChangeCamera(/*SPACESHIP_CAMERA*/THIRD_PERSON_CAMERA, 0.0f);
//
//	CLoadedModelInfo *pModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Mi24.bin", NULL);
//	SetChild(pModel->m_pModelRootObject, true);
//
//	OnPrepareAnimate();
//
//	CreateShaderVariables(pd3dDevice, pd3dCommandList);
//
//	if (pModel) delete pModel;
//}
//
//CAirplanePlayer::~CAirplanePlayer()
//{
//}
//
//void CAirplanePlayer::OnPrepareAnimate()
//{
//	m_pMainRotorFrame = FindFrame("Top_Rotor");
//	m_pTailRotorFrame = FindFrame("Tail_Rotor");
//}
//
//void CAirplanePlayer::Animate(float fTimeElapsed)
//{
//	if (m_pMainRotorFrame)
//	{
//		XMMATRIX xmmtxRotate = XMMatrixRotationY(XMConvertToRadians(360.0f * 2.0f) * fTimeElapsed);
//		m_pMainRotorFrame->m_xmf4x4ToParent = Matrix4x4::Multiply(xmmtxRotate, m_pMainRotorFrame->m_xmf4x4ToParent);
//	}
//	if (m_pTailRotorFrame)
//	{
//		XMMATRIX xmmtxRotate = XMMatrixRotationX(XMConvertToRadians(360.0f * 4.0f) * fTimeElapsed);
//		m_pTailRotorFrame->m_xmf4x4ToParent = Matrix4x4::Multiply(xmmtxRotate, m_pTailRotorFrame->m_xmf4x4ToParent);
//	}
//
//	CPlayer::Animate(fTimeElapsed);
//}
//
//void CAirplanePlayer::OnPrepareRender()
//{
//	CPlayer::OnPrepareRender();
//}
//
//CCamera *CAirplanePlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
//{
//	DWORD nCurrentCameraMode = (m_pCamera) ? m_pCamera->GetMode() : 0x00;
//	if (nCurrentCameraMode == nNewCameraMode) return(m_pCamera);
//	switch (nNewCameraMode)
//	{
//		case FIRST_PERSON_CAMERA:
//			SetFriction(2.0f);
//			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
//			SetMaxVelocityXZ(2.5f);
//			SetMaxVelocityY(40.0f);
//			m_pCamera = OnChangeCamera(FIRST_PERSON_CAMERA, nCurrentCameraMode);
//			m_pCamera->SetTimeLag(0.0f);
//			m_pCamera->SetOffset(XMFLOAT3(0.0f, 20.0f, 0.0f));
//			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
//			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
//			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
//			break;
//		case SPACESHIP_CAMERA:
//			SetFriction(100.5f);
//			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
//			SetMaxVelocityXZ(40.0f);
//			SetMaxVelocityY(40.0f);
//			m_pCamera = OnChangeCamera(SPACESHIP_CAMERA, nCurrentCameraMode);
//			m_pCamera->SetTimeLag(0.0f);
//			m_pCamera->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));
//			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
//			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
//			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
//			break;
//		case THIRD_PERSON_CAMERA:
//			SetFriction(20.5f);
//			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
//			SetMaxVelocityXZ(25.5f);
//			SetMaxVelocityY(20.0f);
//			m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, nCurrentCameraMode);
//			m_pCamera->SetTimeLag(0.25f);
//			m_pCamera->SetOffset(XMFLOAT3(0.0f, 15.0f, -30.0f));
//			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
//			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
//			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
//			break;
//		default:
//			break;
//	}
//	m_pCamera->SetPosition(Vector3::Add(m_xmf3Position, m_pCamera->GetOffset()));
//	Update(fTimeElapsed);
//
//	return(m_pCamera);
//}
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// 

#define _WITH_DEBUG_CALLBACK_DATA

void CSoundCallbackHandler::HandleCallback(void *pCallbackData, float fTrackPosition)
{
   _TCHAR *pWavName = (_TCHAR *)pCallbackData; 
#ifdef _WITH_DEBUG_CALLBACK_DATA
	TCHAR pstrDebug[256] = { 0 };
	_stprintf_s(pstrDebug, 256, _T("%s(%f)\n"), pWavName, fTrackPosition);
	OutputDebugString(pstrDebug);
#endif
#ifdef _WITH_SOUND_RESOURCE
   PlaySound(pWavName, ::ghAppInstance, SND_RESOURCE | SND_ASYNC);
#else
   PlaySound(pWavName, NULL, SND_FILENAME | SND_ASYNC);
#endif
}

CTerrainPlayer::CTerrainPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext)
{
	m_pCamera = ChangeCamera(THIRD_PERSON_CAMERA, 0.0f);


	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", NULL);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	if (pPlayerModel)
	{
		// idle 애니메이션 로드
		CLoadedModelInfo* pAnimIdle = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR@Rifle_Idle.bin", NULL);
		if (pAnimIdle && pAnimIdle->m_pAnimationSets)
		{
			// idle 애니메이션을 animationsets에 추가
			pPlayerModel->m_pAnimationSets->m_vAnimationSets.push_back(pAnimIdle->m_pAnimationSets->m_vAnimationSets[0]);
			delete pAnimIdle;
		}
		// walk 애니메이션 로드
		CLoadedModelInfo* pAnimRun = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR@Rifle_Run.bin", NULL);
		if (pAnimRun && pAnimRun->m_pAnimationSets)
		{
			pPlayerModel->m_pAnimationSets->m_vAnimationSets.push_back(pAnimRun->m_pAnimationSets->m_vAnimationSets[0]);
			delete pAnimRun;
		}
		// shffling 애니메이션 로드	
		CLoadedModelInfo* pAnimShuffling = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR@Shuffling.bin", NULL);
		if (pAnimShuffling && pAnimRun->m_pAnimationSets)
		{
			pPlayerModel->m_pAnimationSets->m_vAnimationSets.push_back(pAnimShuffling->m_pAnimationSets->m_vAnimationSets[0]);
			delete pAnimShuffling;
		}
		// (4) death 애니메이션 로드
		CLoadedModelInfo* pAnimDeath = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR@Death.bin", NULL);
		if (pAnimDeath && pAnimRun->m_pAnimationSets)
		{
			pPlayerModel->m_pAnimationSets->m_vAnimationSets.push_back(pAnimDeath->m_pAnimationSets->m_vAnimationSets[0]);
			delete pAnimDeath;
		}
	}
	SetChild(pPlayerModel->m_pModelRootObject, true);

//	사운드 일단 보류
	//m_pSkinnedAnimationController->SetCallbackKeys(1, 2); 사운드 설정

//#ifdef _WITH_SOUND_RESOURCE
//	m_pSkinnedAnimationController->SetCallbackKey(0, 0.1f, _T("Footstep01"));
//	m_pSkinnedAnimationController->SetCallbackKey(1, 0.5f, _T("Footstep02"));
//	m_pSkinnedAnimationController->SetCallbackKey(2, 0.9f, _T("Footstep03"));
//#else
//	m_pSkinnedAnimationController->SetCallbackKey(1, 0, 0.2f, (void*)_T("Sound/Footstep01.wav"));
//	m_pSkinnedAnimationController->SetCallbackKey(1, 1, 0.5f, (void*)_T("Sound/Footstep02.wav"));
////	m_pSkinnedAnimationController->SetCallbackKey(1, 2, 0.39f, _T("Sound/Footstep03.wav"));
//#endif
//	CAnimationCallbackHandler *pAnimationCallbackHandler = new CSoundCallbackHandler();
//	m_pSkinnedAnimationController->SetAnimationCallbackHandler(1, pAnimationCallbackHandler);

	// 2. 컨트롤러 생성(Idle, Action)
	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel);

	// 트랙 0: Idle - 항상 켜
	m_pSkinnedAnimationController->SetTrackAnimationSet(0, 1);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);

	// 트랙 1: Action - 처음엔 꺼
	m_pSkinnedAnimationController->SetTrackAnimationSet(1, 2);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	
	SetPlayerUpdatedContext(pContext);
	SetCameraUpdatedContext(pContext);

	CHeightMapTerrain *pTerrain = (CHeightMapTerrain *)pContext;
	SetPosition(XMFLOAT3(310.0f, pTerrain->GetHeight(310.0f, 590.0f), 590.0f));
	SetScale(XMFLOAT3(10.0f, 10.0f, 10.0f));

	if (pPlayerModel) delete pPlayerModel;

	m_pCurrentState = new CIdleState();
	m_pCurrentState->Enter(this);
}

CTerrainPlayer::~CTerrainPlayer()
{
	if (m_pCurrentState)
	{
		delete m_pCurrentState;
		m_pCurrentState = nullptr;
	}
}

CCamera *CTerrainPlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
{
	DWORD nCurrentCameraMode = (m_pCamera) ? m_pCamera->GetMode() : 0x00;
	if (nCurrentCameraMode == nNewCameraMode) return(m_pCamera);
	switch (nNewCameraMode)
	{
		case FIRST_PERSON_CAMERA:
			SetFriction(250.0f);
			SetGravity(XMFLOAT3(0.0f, -400.0f, 0.0f));
			SetMaxVelocityXZ(300.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(FIRST_PERSON_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.0f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 20.0f, 0.0f));
			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		case SPACESHIP_CAMERA:
			SetFriction(125.0f);
			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
			SetMaxVelocityXZ(300.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(SPACESHIP_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.0f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));
			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		case THIRD_PERSON_CAMERA:
			SetFriction(250.0f);
			SetGravity(XMFLOAT3(0.0f, -250.0f, 0.0f));
			SetMaxVelocityXZ(300.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.25f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 20.0f, -50.0f));
			m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		default:
			break;
	}
	m_pCamera->SetPosition(Vector3::Add(m_xmf3Position, m_pCamera->GetOffset()));
	Update(fTimeElapsed);

	return(m_pCamera);
}

void CTerrainPlayer::OnPlayerUpdateCallback(float fTimeElapsed)
{
	CHeightMapTerrain *pTerrain = (CHeightMapTerrain *)m_pPlayerUpdatedContext;
	XMFLOAT3 xmf3Scale = pTerrain->GetScale();
	XMFLOAT3 xmf3PlayerPosition = GetPosition();
	int z = (int)(xmf3PlayerPosition.z / xmf3Scale.z);
	bool bReverseQuad = ((z % 2) != 0);
	float fHeight = pTerrain->GetHeight(xmf3PlayerPosition.x, xmf3PlayerPosition.z, bReverseQuad) + 0.0f;
	if (xmf3PlayerPosition.y < fHeight)
	{
		XMFLOAT3 xmf3PlayerVelocity = GetVelocity();
		xmf3PlayerVelocity.y = 0.0f;
		SetVelocity(xmf3PlayerVelocity);
		xmf3PlayerPosition.y = fHeight;
		SetPosition(xmf3PlayerPosition);
	}
}

void CTerrainPlayer::OnCameraUpdateCallback(float fTimeElapsed)
{
	CHeightMapTerrain *pTerrain = (CHeightMapTerrain *)m_pCameraUpdatedContext;
	XMFLOAT3 xmf3Scale = pTerrain->GetScale();
	XMFLOAT3 xmf3CameraPosition = m_pCamera->GetPosition();
	int z = (int)(xmf3CameraPosition.z / xmf3Scale.z);
	bool bReverseQuad = ((z % 2) != 0);
	float fHeight = pTerrain->GetHeight(xmf3CameraPosition.x, xmf3CameraPosition.z, bReverseQuad) + 5.0f;
	if (xmf3CameraPosition.y <= fHeight)
	{
		xmf3CameraPosition.y = fHeight;
		m_pCamera->SetPosition(xmf3CameraPosition);
		if (m_pCamera->GetMode() == THIRD_PERSON_CAMERA)
		{
			CThirdPersonCamera *p3rdPersonCamera = (CThirdPersonCamera *)m_pCamera;
			p3rdPersonCamera->SetLookAt(GetPosition());
		}
	}
}

void CTerrainPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	if (dwDirection)
	{
		m_pSkinnedAnimationController->SetTrackEnable(0, false);
		m_pSkinnedAnimationController->SetTrackEnable(1, true);
	}

	CPlayer::Move(dwDirection, fDistance, bUpdateVelocity);
}

void CTerrainPlayer::Update(float fTimeElapsed)
{
	CPlayer::Update(fTimeElapsed);

	if (m_pCurrentState)
	{
		m_pCurrentState->Update(this, fTimeElapsed);
	}

	// 지형 높이 보정
	if (m_pPlayerUpdatedContext)
	{
		OnPlayerUpdateCallback(fTimeElapsed);
	}
}

//상태 변경 함수
void CTerrainPlayer::ChangeState(CPlayerState* pNewState)
{
	if (m_pCurrentState)
	{
		m_pCurrentState->Exit(this);
		delete m_pCurrentState; // 이전 상태 메모리 해제
	}

	m_pCurrentState = pNewState;
	m_pCurrentState->Enter(this); // 새 상태 시작
}

//애니메이션 재생 처리
void CTerrainPlayer::PlayAnimation(EPlayerAnim eAnim)
{
	CAnimationController* pController = m_pSkinnedAnimationController;
	if (!pController && m_pChild) 
		pController = m_pChild->m_pSkinnedAnimationController;
	if (!pController) 
		return;

	int nAnimIndex = (int)eAnim;

	if (eAnim == EPlayerAnim::IDLE)
	{
		pController->SetTrackEnable(1, false);
		pController->SetTrackEnable(0, true);
	}
	else
	{
		if (pController->m_pAnimationTracks[1].m_nAnimationSet != nAnimIndex)
		{
			pController->SetTrackAnimationSet(1, nAnimIndex);
		}

		pController->SetTrackEnable(0, false);
		pController->SetTrackEnable(1, true);
	}
}

// 상태 클래스 구현
void CIdleState::Enter(CTerrainPlayer* pPlayer)
{
	pPlayer->PlayAnimation(EPlayerAnim::IDLE);
}
void CIdleState::Update(CTerrainPlayer* pPlayer, float fTimeElapsed)
{
	if (InputManager::Instance().KeyPress((INPUT_KEY)0x57) ||
		InputManager::Instance().KeyPress((INPUT_KEY)0x53) ||
		InputManager::Instance().KeyPress((INPUT_KEY)0x41) ||
		InputManager::Instance().KeyPress((INPUT_KEY)0x44))
	{
		pPlayer->ChangeState(new CRunState());
		return;
	}

	if (InputManager::Instance().KeyPress((INPUT_KEY)0x33))
	{
		pPlayer->ChangeState(new CShuffleState());
		return;
	}

	if (InputManager::Instance().KeyPress((INPUT_KEY)0x34))
	{
		pPlayer->ChangeState(new CDeathState());
		return;
	}
}

void CRunState::Enter(CTerrainPlayer* pPlayer)
{
	pPlayer->PlayAnimation(EPlayerAnim::RUN);
}
void CRunState::Update(CTerrainPlayer* pPlayer, float fTimeElapsed)
{
	bool bIsMoving = false;
	float fMoveSpeed = 300.0f;

	if (InputManager::Instance().KeyPress((INPUT_KEY)0x57)) { pPlayer->Move(DIR_FORWARD, fMoveSpeed * fTimeElapsed, true); bIsMoving = true; }
	if (InputManager::Instance().KeyPress((INPUT_KEY)0x53)) { pPlayer->Move(DIR_BACKWARD, fMoveSpeed * fTimeElapsed, true); bIsMoving = true; }
	if (InputManager::Instance().KeyPress((INPUT_KEY)0x41)) { pPlayer->Move(DIR_LEFT, fMoveSpeed * fTimeElapsed, true); bIsMoving = true; }
	if (InputManager::Instance().KeyPress((INPUT_KEY)0x44)) { pPlayer->Move(DIR_RIGHT, fMoveSpeed * fTimeElapsed, true); bIsMoving = true; }

	if (!bIsMoving)
	{
		pPlayer->ChangeState(new CIdleState());
	}
}

void CShuffleState::Enter(CTerrainPlayer* pPlayer)
{
	pPlayer->PlayAnimation(EPlayerAnim::SHUFFLE);
}
void CShuffleState::Update(CTerrainPlayer* pPlayer, float fTimeElapsed)
{
	if (!InputManager::Instance().KeyPress((INPUT_KEY)0x33))
	{
		pPlayer->ChangeState(new CIdleState());
	}
}

void CDeathState::Enter(CTerrainPlayer* pPlayer)
{
	pPlayer->PlayAnimation(EPlayerAnim::DEATH);
}
void CDeathState::Update(CTerrainPlayer* pPlayer, float fTimeElapsed)
{
	if (!InputManager::Instance().KeyPress((INPUT_KEY)0x34))
	{
		pPlayer->ChangeState(new CIdleState());
	}
}