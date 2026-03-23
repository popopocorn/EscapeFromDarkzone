//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "InputManager.h"

static XMVECTOR SafeNormalize3(XMVECTOR v)
{
	XMVECTOR lenSq = XMVector3LengthSq(v);
	if (XMVectorGetX(lenSq) < 0.000001f) return XMVectorZero();
	return XMVector3Normalize(v);
}

static float ClampFloat(float v, float vmin, float vmax)
{
	if (v < vmin) return vmin;
	if (v > vmax) return vmax;
	return v;
}

static XMVECTOR QuaternionFromTo(XMVECTOR vFrom, XMVECTOR vTo)
{
	vFrom = SafeNormalize3(vFrom);
	vTo = SafeNormalize3(vTo);

	float dot = XMVectorGetX(XMVector3Dot(vFrom, vTo));

	if (dot > 0.9999f)
	{
		return XMQuaternionIdentity();
	}

	if (dot < -0.9999f)
	{
		XMVECTOR axis = XMVector3Cross(vFrom, XMVectorSet(1, 0, 0, 0));
		if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.000001f)
		{
			axis = XMVector3Cross(vFrom, XMVectorSet(0, 1, 0, 0));
		}
		axis = SafeNormalize3(axis);
		return XMQuaternionRotationAxis(axis, XM_PI);
	}

	XMVECTOR axis = XMVector3Cross(vFrom, vTo);
	XMVECTOR q = XMVectorSet(
		XMVectorGetX(axis),
		XMVectorGetY(axis),
		XMVectorGetZ(axis),
		1.0f + dot
	);

	return XMQuaternionNormalize(q);
}
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

	// 네트워크 테스트
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	c_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &addr.sin_addr);
	WSAConnect(c_socket, reinterpret_cast<sockaddr*>(&addr),
		sizeof(SOCKADDR_IN), NULL, NULL, NULL, NULL);
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
	shift.y = 0.0f;

	Move(shift, bUpdateVelocity);
}

void CPlayer::Move(const XMFLOAT3& xmf3Shift, bool bUpdateVelocity)
{
	if (bUpdateVelocity)
	{
		m_xmf3Velocity = xmf3Shift;
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
	SavePrevPosition();

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

	UpdateWeaponPose();

	UpdateTransform(NULL);

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
void CPlayer::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera)
{
	//DWORD nCameraMode = (pCamera) ? pCamera->GetMode() : 0x00;
	//if (nCameraMode == THIRD_PERSON_CAMERA) 
		CGameObject::Render(pd3dCommandList, false, nPipelineState, pCamera);
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

void CPlayer::HandleCollision(XMFLOAT3 normal)
{
	if (normal.y > 0.5f)
	{
		if (m_xmf3Velocity.y < 0.0f) m_xmf3Velocity.y = 0.0f;
		return;
	}

	XMVECTOR vNormal = XMLoadFloat3(&normal);
	XMVECTOR vVelocity = XMLoadFloat3(&m_xmf3Velocity);
	XMVECTOR vCurrPos = XMLoadFloat3(&m_xmf3Position);

	XMVECTOR vPrevPos = XMLoadFloat3(&m_xmf3PrevPos);
	XMVECTOR vMoveDelta = vCurrPos - vPrevPos;
	XMVECTOR vDot = XMVector3Dot(vMoveDelta, vNormal);
	float fPenetrationDepth = XMVectorGetX(vDot);

	if (fPenetrationDepth < 0.0f)
	{
		XMVECTOR vCorrection = vNormal * (fabs(fPenetrationDepth) + 0.0001f);

		vCurrPos += vCorrection;
		XMStoreFloat3(&m_xmf3Position, vCurrPos);

		CGameObject::SetPosition(m_xmf3Position);

		UpdateTransform(NULL);
	}

	XMVECTOR vVelDot = XMVector3Dot(vVelocity, vNormal);
	float fVelDot = XMVectorGetX(vVelDot);

	if (fVelDot < 0.0f)
	{
		XMVECTOR vSlideVel = vVelocity - (vNormal * fVelDot);

		if (XMVectorGetX(XMVector3Length(vSlideVel)) < 0.01f)
		{
			vSlideVel = XMVectorZero();
		}
		XMStoreFloat3(&m_xmf3Velocity, vSlideVel);
	}
}
void CPlayer::UpdateDirection()
{
	if (MoveDir.x == 0.0f && MoveDir.y == 0.0f && MoveDir.z == 0.0f)
	{
		CollVector.clear(); // 이동 안 해도 충돌 정보는 비워줘야 함
		return;
	}
	XMVECTOR currentDirVec = XMLoadFloat3(&MoveDir);

	for (const XMFLOAT3& normal : CollVector)
	{
		XMVECTOR normalVec = XMLoadFloat3(&normal);
		
		XMVECTOR dotVec = XMVector3Dot(currentDirVec, normalVec);
		float dot = XMVectorGetX(dotVec);

		if (dot < 0.0f)
		{
			currentDirVec = currentDirVec - (normalVec * dot);
		}
	}

	// 4. (선택 사항) 미세한 떨림 방지
	// 연산 오차로 인해 0에 가까운 아주 작은 값이 남을 수 있음
	if (XMVectorGetX(XMVector3LengthSq(currentDirVec)) < 0.0001f)
	{
		currentDirVec = XMVectorZero();
	}

	// 5. 최종 보정된 방향 저장
	XMStoreFloat3(&MoveDir, currentDirVec);

	CollVector.clear();

	//wchar_t buffer[128];
	//swprintf_s(buffer, L"MoveDir: x=%.3f y=%.3f z=%.3f\n",
	//	MoveDir.x, MoveDir.y, MoveDir.z);
	//OutputDebugStringW(buffer);
}

void CPlayer::EquipWeapon(CGameObject* pWeapon, const char* pstrSocketName)
{
	if (!pWeapon) return;
	if (!pstrSocketName) return;

	CGameObject* pWeaponSocket = FindFrame(pstrSocketName);
	if (!pWeaponSocket)
	{
		OutputDebugString(L"경고: 소켓(뼈대) 프레임을 찾을 수 없습니다!\n");
		return;
	}

	m_pWeapon = pWeapon;

	// 오른손 소켓에 무기 부착
	pWeaponSocket->SetChild(m_pWeapon, true);

	// 기본 장착 위치/회전
	m_pWeapon->SetPosition(0.0f, 0.0f, 0.0f);
	m_pWeapon->SetScale(
		m_xmf3WeaponScale.x,
		m_xmf3WeaponScale.y,
		m_xmf3WeaponScale.z
	);
	m_pWeapon->Rotate(
		m_xmf3WeaponIdleRot.x,
		m_xmf3WeaponIdleRot.y,
		m_xmf3WeaponIdleRot.z
	);

	m_xmf4x4WeaponBaseLocal = m_pWeapon->m_xmf4x4ToParent;
	m_bWeaponBaseLocalSaved = true;

	// 무기 안에 넣어둔 LeftHandGrip 프레임 찾기
	m_pLeftHandGrip = m_pWeapon->FindFrame("LeftHandGrip");
	if (m_pLeftHandGrip)
	{
		OutputDebugString(L"[IK] 성공: 무기에서 LeftHandGrip 프레임을 찾았습니다.\n");
	}
	else
	{
		OutputDebugString(L"[IK] 경고: 무기에서 LeftHandGrip 프레임을 찾지 못했습니다.\n");
	}

	ApplyWeaponPose(WEAPON_POSE::IDLE);

	OutputDebugString(L"성공: 무기가 플레이어 오른손 소켓에 장착되었습니다.\n");
}

void CPlayer::ApplyWeaponPose(WEAPON_POSE ePose)
{
	if (!m_pWeapon) return;
	if (!m_bWeaponBaseLocalSaved) return;

	m_eWeaponPose = ePose;

	XMFLOAT3 posOffset;
	XMFLOAT3 rotOffset;

	if (ePose == WEAPON_POSE::IDLE)
	{
		posOffset = m_xmf3WeaponIdlePos;
		rotOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		posOffset = m_xmf3WeaponRunPos;
		rotOffset = m_xmf3WeaponRunRot;
	}

	XMMATRIX mBase = XMLoadFloat4x4(&m_xmf4x4WeaponBaseLocal);

	XMVECTOR vScaleBase, vRotBase, vTransBase;
	XMMatrixDecompose(&vScaleBase, &vRotBase, &vTransBase, mBase);

	XMVECTOR vPosOffset = XMVectorSet(
		posOffset.x,
		posOffset.y,
		posOffset.z,
		0.0f
	);

	XMVECTOR vFinalTrans = vTransBase + vPosOffset;

	XMVECTOR vRotOffsetQuat = XMQuaternionRotationRollPitchYaw(
		XMConvertToRadians(rotOffset.x),
		XMConvertToRadians(rotOffset.y),
		XMConvertToRadians(rotOffset.z)
	);

	XMVECTOR vFinalRot = XMQuaternionMultiply(vRotOffsetQuat, vRotBase);

	XMMATRIX mFinal = XMMatrixAffineTransformation(
		vScaleBase,
		XMVectorZero(),
		vFinalRot,
		vFinalTrans
	);

	XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, mFinal);

	m_pWeapon->UpdateTransform(NULL);
}

void CPlayer::UpdateWeaponPose()
{
	if (!m_pWeapon) return;

	float moveLenSq = MoveDir.x * MoveDir.x + MoveDir.y * MoveDir.y + MoveDir.z * MoveDir.z;

	if (moveLenSq > 0.0001f)
	{
		if (m_eWeaponPose != WEAPON_POSE::RUN)
		{
			ApplyWeaponPose(WEAPON_POSE::RUN);
		}
	}
	else
	{
		if (m_eWeaponPose != WEAPON_POSE::IDLE)
		{
			ApplyWeaponPose(WEAPON_POSE::IDLE);
		}
	}
}

CGameObject* CPlayer::FindFirstFrameByNames(const char* const* ppNames, int nCount)
{
	for (int i = 0; i < nCount; ++i)
	{
		CGameObject* pFound = FindFrame(ppNames[i]);
		if (pFound) return pFound;
	}
	return nullptr;
}

bool CPlayer::InitializeLeftHandIK()
{
	static const char* s_ppUpperArmNames[] =
	{
		"mixamorig:LeftArm"
	};

	static const char* s_ppForeArmNames[] =
	{
		"mixamorig:LeftForeArm"
	};

	static const char* s_ppHandNames[] =
	{
		"mixamorig:LeftHand"
	};

	m_pLeftUpperArm = FindFirstFrameByNames(s_ppUpperArmNames, _countof(s_ppUpperArmNames));
	m_pLeftForeArm = FindFirstFrameByNames(s_ppForeArmNames, _countof(s_ppForeArmNames));
	m_pLeftHand = FindFirstFrameByNames(s_ppHandNames, _countof(s_ppHandNames));

	if (!m_pLeftUpperArm || !m_pLeftForeArm || !m_pLeftHand)
	{
		OutputDebugString(L"[IK] 경고: 왼팔 본을 전부 찾지 못했습니다.\n");
		return false;
	}

	OutputDebugString(L"[IK] 성공: 왼팔 IK 본 캐싱 완료\n");
	return true;
}

//왼손 하박 안정화
XMVECTOR CPlayer::GetStableLeftElbowBendDir(FXMVECTOR vShoulder, FXMVECTOR vElbow, FXMVECTOR vTargetDir)
{
	XMVECTOR vPlayerLeft = -SafeNormalize3(XMLoadFloat3(&m_xmf3Right));
	XMVECTOR vPlayerUp = SafeNormalize3(XMLoadFloat3(&m_xmf3Up));

	XMVECTOR vLeftProjected = vPlayerLeft - vTargetDir * XMVectorGetX(XMVector3Dot(vPlayerLeft, vTargetDir));
	XMVECTOR vUpProjected = vPlayerUp - vTargetDir * XMVectorGetX(XMVector3Dot(vPlayerUp, vTargetDir));

	XMVECTOR vPreferred = vLeftProjected;
	if (XMVectorGetX(XMVector3LengthSq(vPreferred)) < 0.000001f)
	{
		vPreferred = vUpProjected;
	}
	if (XMVectorGetX(XMVector3LengthSq(vPreferred)) < 0.000001f)
	{
		vPreferred = XMVectorSet(-1, 0, 0, 0);
	}
	vPreferred = SafeNormalize3(vPreferred);

	XMVECTOR vCurrentElbowDir = vElbow - vShoulder;
	XMVECTOR vCurrentProjected = vCurrentElbowDir - vTargetDir * XMVectorGetX(XMVector3Dot(vCurrentElbowDir, vTargetDir));

	if (XMVectorGetX(XMVector3LengthSq(vCurrentProjected)) < 0.000001f)
	{
		vCurrentProjected = vPreferred;
	}
	vCurrentProjected = SafeNormalize3(vCurrentProjected);

	XMVECTOR vCandidate = SafeNormalize3(vCurrentProjected * 0.35f + vPreferred * 0.65f);

	if (m_bLeftElbowDirCached)
	{
		XMVECTOR vCached = SafeNormalize3(XMLoadFloat3(&m_xmf3CachedLeftElbowDir));
		if (XMVectorGetX(XMVector3Dot(vCandidate, vCached)) < 0.0f)
		{
			vCandidate = -vCandidate;
		}
	}

	if (XMVectorGetX(XMVector3Dot(vCandidate, vPreferred)) < 0.0f)
	{
		vCandidate = -vCandidate;
	}

	vCandidate = SafeNormalize3(vCandidate);

	XMStoreFloat3(&m_xmf3CachedLeftElbowDir, vCandidate);
	m_bLeftElbowDirCached = true;

	return vCandidate;
}

float CPlayer::GetLeftHandIKWeight() const
{
	if (!m_bUseLeftHandIK) return 0.0f;
	if (!m_pWeapon) return 0.0f;
	if (!m_pLeftHandGrip) return 0.0f;
	if (!m_pLeftUpperArm || !m_pLeftForeArm || !m_pLeftHand) return 0.0f;

	return m_fLeftHandIKWeight;
}

void CPlayer::RotateBoneTowardTarget(CGameObject* pBone, const XMFLOAT3& xmf3CurrentChildWorldPos, const XMFLOAT3& xmf3TargetChildWorldPos, float fWeight)
{
	if (!pBone) return;
	if (fWeight <= 0.0f) return;

	XMFLOAT3 xmf3BoneWorldPos = pBone->GetPosition();

	XMVECTOR vBonePos = XMLoadFloat3(&xmf3BoneWorldPos);
	XMVECTOR vCurrentChildPos = XMLoadFloat3(&xmf3CurrentChildWorldPos);
	XMVECTOR vTargetChildPos = XMLoadFloat3(&xmf3TargetChildWorldPos);

	XMVECTOR vCurrentDir = SafeNormalize3(vCurrentChildPos - vBonePos);
	XMVECTOR vTargetDir = SafeNormalize3(vTargetChildPos - vBonePos);

	if (XMVectorGetX(XMVector3LengthSq(vCurrentDir)) < 0.000001f) return;
	if (XMVectorGetX(XMVector3LengthSq(vTargetDir)) < 0.000001f) return;

	XMVECTOR vDeltaRot = QuaternionFromTo(vCurrentDir, vTargetDir);

	XMMATRIX mBoneWorld = XMLoadFloat4x4(&pBone->m_xmf4x4World);

	XMVECTOR vWorldScale, vWorldRot, vWorldTrans;
	XMMatrixDecompose(&vWorldScale, &vWorldRot, &vWorldTrans, mBoneWorld);

	XMVECTOR vTargetWorldRot = XMQuaternionMultiply(vDeltaRot, vWorldRot);
	vTargetWorldRot = XMQuaternionNormalize(vTargetWorldRot);

	// 즉시 적용 대신 스무딩
	float fSmoothing = 0.18f;
	float fApply = ClampFloat(fWeight * fSmoothing, 0.0f, 1.0f);

	XMVECTOR vNewWorldRot = XMQuaternionSlerp(vWorldRot, vTargetWorldRot, fApply);
	vNewWorldRot = XMQuaternionNormalize(vNewWorldRot);

	XMMATRIX mParentWorld = XMMatrixIdentity();
	if (pBone->m_pParent)
	{
		mParentWorld = XMLoadFloat4x4(&pBone->m_pParent->m_xmf4x4World);
	}

	XMMATRIX mDesiredWorld = XMMatrixAffineTransformation(
		vWorldScale,
		XMVectorZero(),
		vNewWorldRot,
		vWorldTrans
	);

	XMMATRIX mInvParent = XMMatrixInverse(nullptr, mParentWorld);
	XMMATRIX mNewLocal = XMMatrixMultiply(mDesiredWorld, mInvParent);

	XMVECTOR vLocalScale, vLocalRot, vLocalTrans;
	XMMatrixDecompose(&vLocalScale, &vLocalRot, &vLocalTrans, mNewLocal);

	XMMATRIX mOldLocal = XMLoadFloat4x4(&pBone->m_xmf4x4ToParent);
	XMVECTOR vOldLocalScale, vOldLocalRot, vOldLocalTrans;
	XMMatrixDecompose(&vOldLocalScale, &vOldLocalRot, &vOldLocalTrans, mOldLocal);

	XMMATRIX mFinalLocal = XMMatrixAffineTransformation(
		vOldLocalScale,
		XMVectorZero(),
		vLocalRot,
		vOldLocalTrans
	);

	XMStoreFloat4x4(&pBone->m_xmf4x4ToParent, mFinalLocal);
}

void CPlayer::MatchBoneWorldRotation(CGameObject* pBone, CGameObject* pTarget, float fWeight)
{
	if (!pBone || !pTarget) return;
	if (fWeight <= 0.0f) return;

	XMMATRIX mBoneWorld = XMLoadFloat4x4(&pBone->m_xmf4x4World);
	XMMATRIX mTargetWorld = XMLoadFloat4x4(&pTarget->m_xmf4x4World);

	XMVECTOR vBoneScale, vBoneRot, vBoneTrans;
	XMVECTOR vTargetScale, vTargetRot, vTargetTrans;

	XMMatrixDecompose(&vBoneScale, &vBoneRot, &vBoneTrans, mBoneWorld);
	XMMatrixDecompose(&vTargetScale, &vTargetRot, &vTargetTrans, mTargetWorld);

	XMVECTOR vNewWorldRot = XMQuaternionSlerp(vBoneRot, vTargetRot, fWeight);
	vNewWorldRot = XMQuaternionNormalize(vNewWorldRot);

	XMMATRIX mParentWorld = XMMatrixIdentity();
	if (pBone->m_pParent)
	{
		mParentWorld = XMLoadFloat4x4(&pBone->m_pParent->m_xmf4x4World);
	}

	XMMATRIX mDesiredWorld = XMMatrixAffineTransformation(
		vBoneScale,
		XMVectorZero(),
		vNewWorldRot,
		vBoneTrans
	);

	XMMATRIX mInvParent = XMMatrixInverse(nullptr, mParentWorld);
	XMMATRIX mNewLocal = XMMatrixMultiply(mDesiredWorld, mInvParent);

	XMVECTOR vLocalScale, vLocalRot, vLocalTrans;
	XMMatrixDecompose(&vLocalScale, &vLocalRot, &vLocalTrans, mNewLocal);

	XMMATRIX mOldLocal = XMLoadFloat4x4(&pBone->m_xmf4x4ToParent);
	XMVECTOR vOldLocalScale, vOldLocalRot, vOldLocalTrans;
	XMMatrixDecompose(&vOldLocalScale, &vOldLocalRot, &vOldLocalTrans, mOldLocal);

	XMMATRIX mFinalLocal = XMMatrixAffineTransformation(
		vOldLocalScale,
		XMVectorZero(),
		vLocalRot,
		vOldLocalTrans
	);

	XMStoreFloat4x4(&pBone->m_xmf4x4ToParent, mFinalLocal);
}

void CPlayer::SolveLeftHandIK()
{
	float fWeight = GetLeftHandIKWeight();
	if (fWeight <= 0.0f) return;

	XMFLOAT3 xmf3Shoulder = m_pLeftUpperArm->GetPosition();
	XMFLOAT3 xmf3Elbow = m_pLeftForeArm->GetPosition();
	XMFLOAT3 xmf3Hand = m_pLeftHand->GetPosition();
	XMFLOAT3 xmf3Target = m_pLeftHandGrip->GetPosition();

	XMVECTOR vShoulder = XMLoadFloat3(&xmf3Shoulder);
	XMVECTOR vElbow = XMLoadFloat3(&xmf3Elbow);
	XMVECTOR vHand = XMLoadFloat3(&xmf3Hand);
	XMVECTOR vTarget = XMLoadFloat3(&xmf3Target);

	float fUpperLen = XMVectorGetX(XMVector3Length(vElbow - vShoulder));
	float fLowerLen = XMVectorGetX(XMVector3Length(vHand - vElbow));

	if (fUpperLen < 0.0001f || fLowerLen < 0.0001f) return;

	XMVECTOR vToTarget = vTarget - vShoulder;
	float fDistToTarget = XMVectorGetX(XMVector3Length(vToTarget));
	if (fDistToTarget < 0.0001f) return;

	float fClampedDist = ClampFloat(fDistToTarget, 0.0001f, (fUpperLen + fLowerLen) - 0.001f);
	XMVECTOR vDir = SafeNormalize3(vToTarget);

	XMVECTOR vBendDir = GetStableLeftElbowBendDir(vShoulder, vElbow, vDir);

	float fCosShoulder = ClampFloat(
		((fUpperLen * fUpperLen) + (fClampedDist * fClampedDist) - (fLowerLen * fLowerLen)) / (2.0f * fUpperLen * fClampedDist),
		-1.0f,
		1.0f
	);

	float fShoulderAngle = acosf(fCosShoulder);
	float fProjLen = cosf(fShoulderAngle) * fUpperLen;
	float fBendLen = sinf(fShoulderAngle) * fUpperLen;

	XMVECTOR vElbowTarget = vShoulder + (vDir * fProjLen) + (vBendDir * fBendLen);

	XMFLOAT3 xmf3ElbowTarget;
	XMStoreFloat3(&xmf3ElbowTarget, vElbowTarget);

	RotateBoneTowardTarget(m_pLeftUpperArm, xmf3Elbow, xmf3ElbowTarget, fWeight);
	UpdateTransform(NULL);

	xmf3Elbow = m_pLeftForeArm->GetPosition();
	xmf3Hand = m_pLeftHand->GetPosition();

	RotateBoneTowardTarget(m_pLeftForeArm, xmf3Hand, xmf3Target, fWeight);
	UpdateTransform(NULL);

	xmf3Hand = m_pLeftHand->GetPosition();
	RotateBoneTowardTarget(m_pLeftUpperArm, xmf3Hand, xmf3Target, fWeight * 0.12f);
	UpdateTransform(NULL);

	xmf3Hand = m_pLeftHand->GetPosition();
	RotateBoneTowardTarget(m_pLeftForeArm, xmf3Hand, xmf3Target, fWeight * 0.18f);
	UpdateTransform(NULL);

	MatchBoneWorldRotation(m_pLeftHand, m_pLeftHandGrip, fWeight * 0.02f);
	UpdateTransform(NULL);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void CPlayerAnimationController::OnAnimationIK(CGameObject* pRootGameObject)
{
	if (!m_pOwner) return;
	m_pOwner->SolveLeftHandIK();
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

CTerrainPlayer::CTerrainPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, CShader* shader)
{
	m_pCamera = ChangeCamera(THIRD_PERSON_CAMERA, 0.0f);

	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, "Model/Ch15_nonPBR.bin", shader);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pPlayerModel->m_pModelRootObject, true);

	// 왼팔 IK용 본 찾기
	InitializeLeftHandIK();

	// 기본 컨트롤러 대신 플레이어 전용 컨트롤러 사용
	m_pSkinnedAnimationController = new CPlayerAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel, this);

	m_pSkinnedAnimationController->SetTrackAnimationSet(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, false);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (pPlayerModel) delete pPlayerModel;
	SetOOBB(NULL);
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
		SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetMaxVelocityXZ(300.0f);
		SetMaxVelocityY(400.0f);
		m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, nCurrentCameraMode);
		m_pCamera->SetTimeLag(0.0f);
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

}

void CTerrainPlayer::OnCameraUpdateCallback(float fTimeElapsed)
{
	
}

void CTerrainPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	CPlayer::Move(dwDirection, fDistance, bUpdateVelocity);
}

void CTerrainPlayer::Update(float fTimeElapsed)
{
	CAnimationController* pController = m_pSkinnedAnimationController;
	if (!pController && m_pChild)
	{
		pController = m_pChild->m_pSkinnedAnimationController;
	}
	
	// 네트워크 테스트용 버퍼 선언
	size_t buf_len = 0;

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

		// 네트워크 테스트용
		memcpy(send_buf + buf_len, &ev, sizeof(GameEvent));
		buf_len += sizeof(GameEvent);

		event_queue.pop();
	}

	// 네트워크 테스트용
	if (0 != buf_len) {
		// 여기서 버퍼 전송
		WSABUF wsabuf[1];
		wsabuf[0].buf = send_buf;
		wsabuf[0].len = static_cast<ULONG>(buf_len);
		WSAOVERLAPPED send_over;
		ZeroMemory(&send_over, sizeof(send_over));

		int ret = WSASend(c_socket, wsabuf, 1, NULL, 0, &send_over, NULL);
		if (SOCKET_ERROR == ret) {
			auto err_no = WSAGetLastError();
			//error_display("WSASEND : ", err_no);
		}
	}

	state.get()->Update(this);

	//충돌에 따른 방향 전환
	UpdateDirection();

	XMFLOAT3 direction = MoveDir;
	direction = Vector3::ScalarProduct(direction, 8.0f, false);
	CPlayer::Move(direction, true);

	/*wchar_t buffer[128];
	swprintf_s(buffer, L"MoveDir: x=%.3f y=%.3f z=%.3f\n",
		direction.x, direction.y, direction.z);
	OutputDebugStringW(buffer);*/

	CPlayer::Update(fTimeElapsed);
}

bool PlayerIdle::Enter(CPlayer* Player)
{
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->ChangeAnimation(ANIM_IDLE, 0.2f);
	}
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
	return true;
}

void PlayerRun::Update(CPlayer* Player)
{
	XMFLOAT2 dir = XMFLOAT2(0, 0);

	auto& input = InputManager::Instance();
	if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) dir.x += 1;
	if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) dir.x -= 1;
	if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) dir.y -= 1;
	if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) dir.y += 1;

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

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->ChangeAnimation(nextAnim, 0.2f);
	}

	XMFLOAT3 look = Player->GetLookVector();
	XMFLOAT3 right = Player->GetRightVector();

	XMFLOAT3 direction;
	direction.x = look.x * dir.x + right.x * dir.y;
	direction.z = look.z * dir.x + right.z * dir.y;
	direction = Vector3::Normalize(direction);
	Player->SetMoveDir(direction);
}

void PlayerRun::Exit(CPlayer* Player)
{
	
}
//-------------------------------------------------------------------------
bool PlayerDie::Enter(CPlayer* Player)
{
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->ChangeAnimation(4, 0.1f);
	}
	return true;
}

void PlayerDie::Update(CPlayer* Player)
{
}

void PlayerDie::Exit(CPlayer* Player)
{
}