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
	state = std::make_unique<PlayerIdle>();
}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

	if (m_pCamera) delete m_pCamera;
}

void CPlayer::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pCamera) m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CPlayer::ReleaseShaderVariables()
{
	if (m_pCamera) m_pCamera->ReleaseShaderVariables();
}

void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	if (!dwDirection) return;

	XMFLOAT3 dir = { 0, 0, 0 };

	if (dwDirection & DIR_FORWARD)  dir = Vector3::Add(dir, m_xmf3Look, 1.0f);
	if (dwDirection & DIR_BACKWARD) dir = Vector3::Add(dir, m_xmf3Look, -1.0f);
	if (dwDirection & DIR_RIGHT)    dir = Vector3::Add(dir, m_xmf3Right, 1.0f);
	if (dwDirection & DIR_LEFT)     dir = Vector3::Add(dir, m_xmf3Right, -1.0f);
	if (dwDirection & DIR_UP)       dir = Vector3::Add(dir, m_xmf3Up, 1.0f);
	if (dwDirection & DIR_DOWN)     dir = Vector3::Add(dir, m_xmf3Up, -1.0f);

	dir = Vector3::Normalize(dir);
	XMFLOAT3 shift = Vector3::ScalarProduct(dir, fDistance, false);

	Move(shift, bUpdateVelocity);
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

CCamera* CPlayer::OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode)
{
	CCamera* pNewCamera = NULL;
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

void CPlayer::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	DWORD nCameraMode = (pCamera) ? pCamera->GetMode() : 0x00;
	if (nCameraMode == THIRD_PERSON_CAMERA) CGameObject::Render(pd3dCommandList, pCamera);
}

void CPlayer::ChangeState(std::unique_ptr<PlayerState> new_state)
{
	if (!new_state)
		return;

	if (state && typeid(*state) == typeid(*new_state))
		return;

	if (state)
		state->Exit(this);

	state = std::move(new_state);

	state->Enter(this);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 

#define _WITH_DEBUG_CALLBACK_DATA

void CSoundCallbackHandler::HandleCallback(void* pCallbackData, float fTrackPosition)
{
	_TCHAR* pWavName = (_TCHAR*)pCallbackData;
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

CTerrainPlayer::CTerrainPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, void* pContext)
{
	m_pCamera = ChangeCamera(THIRD_PERSON_CAMERA, 0.0f);


	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", NULL);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pPlayerModel->m_pModelRootObject, true);

		//m_pSkinnedAnimationController->SetCallbackKeys(1, 2);

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

	m_pSkinnedAnimationController = new CAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel);

	m_pSkinnedAnimationController->SetTrackAnimationSet(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	//SetPlayerUpdatedContext(pContext);
	//SetCameraUpdatedContext(pContext);

	//CHeightMapTerrain *pTerrain = (CHeightMapTerrain *)pContext;
	//SetPosition(XMFLOAT3(310.0f, pTerrain->GetHeight(310.0f, 590.0f), 590.0f));
	//SetScale(XMFLOAT3(10.0f, 10.0f, 10.0f));

	if (pPlayerModel) delete pPlayerModel;
	dir = XMFLOAT2(0, 0);
}

CTerrainPlayer::~CTerrainPlayer()
{
}

CCamera* CTerrainPlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
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
		m_pCamera->SetOffset(XMFLOAT3(0.0f, 15, -5.0f));
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
	CHeightMapTerrain* pTerrain = (CHeightMapTerrain*)m_pPlayerUpdatedContext;
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
	CHeightMapTerrain* pTerrain = (CHeightMapTerrain*)m_pCameraUpdatedContext;
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
			CThirdPersonCamera* p3rdPersonCamera = (CThirdPersonCamera*)m_pCamera;
			p3rdPersonCamera->SetLookAt(GetPosition());
		}
	}
}

void CTerrainPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	CPlayer::Move(dwDirection, fDistance, bUpdateVelocity);
}

void CTerrainPlayer::Update(float fTimeElapsed)
{
	dir = XMFLOAT2(0, 0);
	CAnimationController* pController = m_pSkinnedAnimationController;
	if (!pController && m_pChild)
	{
		pController = m_pChild->m_pSkinnedAnimationController;
	}
	
	while (not event_queue.empty()) {
		const GameEvent& ev = event_queue.front();

		switch (ev.type)
		{
		case EventType::Input:
		{
			if (ev.keyEvent.state == KEY_STATE::DOWN) {
				switch (ev.keyEvent.key)
				{
				case INPUT_KEY::W:
				case INPUT_KEY::A:
				case INPUT_KEY::S:
				case INPUT_KEY::D:
					ChangeState(make_unique<PlayerRun>());
					break;
				default:
					break;
				}
			}
			if (ev.keyEvent.state == KEY_STATE::UP) {
				switch (ev.keyEvent.key)
				{
				case INPUT_KEY::W:
				case INPUT_KEY::A:
				case INPUT_KEY::S:
				case INPUT_KEY::D:
					if(! InputManager::Instance().KeyPress(INPUT_KEY::W) &&
					   ! InputManager::Instance().KeyPress(INPUT_KEY::A) &&
					   ! InputManager::Instance().KeyPress(INPUT_KEY::S) &&
					   ! InputManager::Instance().KeyPress(INPUT_KEY::D))
						ChangeState(make_unique<PlayerIdle>());
				default:
					break;
				}
			}
		}
		break;
		case EventType::Timeout:

			break;
		default:
			break;
		}
		event_queue.pop();
	}

	DWORD dwDirection = 0;

	auto& input = InputManager::Instance();

	if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) dwDirection |= DIR_FORWARD;
	if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) dwDirection |= DIR_BACKWARD;
	if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) dwDirection |= DIR_LEFT;
	if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) dwDirection |= DIR_RIGHT;
	if (dwDirection) Move(dwDirection, 8.0f, true);



	state.get()->Update(this);

	XMFLOAT3 direction = XMFLOAT3(0, 0, 0);
	direction.x = m_xmf3Look.x * dir.x + m_xmf3Right.x * dir.y;
	direction.z = m_xmf3Look.z * dir.x + m_xmf3Right.z * dir.y;
	direction = Vector3::Normalize(direction);
	SetMoveDir(direction);

	CPlayer::Update(fTimeElapsed);
}

bool PlayerIdle::Enter(CPlayer* Player)
{
	auto* pctrl = Player->GetAnimationController();
	if (!pctrl) return false;
	pctrl->SetTrackAnimationSet(0, 0);
	pctrl->SetTrackEnable(0, true);
	pctrl->SetTrackEnable(1, false);

	return true;
}

void PlayerIdle::Update(CPlayer* Player)
{
}

void PlayerIdle::Exit(CPlayer* Player)
{
}
//-------------------------------------------------------------------------
bool PlayerRun::Enter(CPlayer* Player)
{
	auto* pctrl = Player->GetAnimationController();
	if (!pctrl) return false;
	pctrl->SetTrackAnimationSet(1, 1);
	pctrl->SetTrackEnable(0, false);
	pctrl->SetTrackEnable(1, true);
	return true;
}

void PlayerRun::Update(CPlayer* Player)
{
	auto& input = InputManager::Instance();
	if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) Player->dir.x += 1;
	if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) Player->dir.x -= 1;
	if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) Player->dir.y -= 1;
	if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) Player->dir.y += 1;

	auto* pctrl = Player->GetAnimationController();
	if (!pctrl) return;

	const XMFLOAT2 dir = Player->GetDirection();
	if (fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f) return;

	float angle = atan2f(dir.y, dir.x);
	int nextAnim;

	if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4)
		nextAnim = ANIM_RUN_F;
	else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4)
		nextAnim = ANIM_RUN_R;
	else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4)
		nextAnim = ANIM_RUN_L;
	else
		nextAnim = ANIM_RUN_B;

	if (pctrl->m_pAnimationTracks[1].m_nAnimationSet != nextAnim)
	{
		pctrl->SetTrackAnimationSet(1, nextAnim);
	}



}

void PlayerRun::Exit(CPlayer* Player)
{
}
//-------------------------------------------------------------------------
bool PlayerDie::Enter(CPlayer* Player)
{
	auto* pctrl = Player->GetAnimationController();
	if (!pctrl) return false;
	pctrl->SetTrackAnimationSet(1, 4);
	pctrl->SetTrackEnable(0, false);
	pctrl->SetTrackEnable(1, true);
	return true;
}

void PlayerDie::Update(CPlayer* Player)
{
}

void PlayerDie::Exit(CPlayer* Player)
{
}