//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "InputManager.h"
#include "Collision.h"

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

static bool IsMoveHeld(InputManager& input, INPUT_KEY key)
{
	return input.KeyDown(key) || input.KeyHold(key);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

	UpdateTransform(NULL);

	UpdateWeaponPose(fTimeElapsed);

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

	if (m_xmf3Velocity.x != 0.0f || m_xmf3Velocity.z != 0.0f)
	{
		float fSpeedXZSq = m_xmf3Velocity.x * m_xmf3Velocity.x + m_xmf3Velocity.z * m_xmf3Velocity.z;
		if (fSpeedXZSq < 0.0001f * 0.0001f)
		{
			m_xmf3Velocity.x = 0.0f;
			m_xmf3Velocity.z = 0.0f;
		}
	}
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

void CPlayer::ChangeState(std::unique_ptr<State<CPlayer>> new_state)
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

bool CPlayer::IsGrenadeState() const
{
	return (dynamic_cast<const PlayerGrenade*>(state.get()) != nullptr);
}

void CPlayer::HandleCollision(const ColResult& result)
{
	if (!result.isCollide) return;

	
	CollVector.push_back(result.normal);

	
	XMFLOAT3 curPos = GetPosition();

	// MTV(backPos)만큼 밀어내되, 벽 방향(노멀의 반대)으로 0.001f 만큼 덜 밀어냄
	XMVECTOR vBackPos = XMLoadFloat3(&result.mtv);
	XMVECTOR vNormal = XMLoadFloat3(&result.normal);

	// 0.001f 만큼 벽에 파묻힌 상태를 유지시킴 (Skin Width)
	vBackPos -= vNormal * 0.001f;

	XMFLOAT3 finalBackPos;
	XMStoreFloat3(&finalBackPos, vBackPos);

	curPos.x += finalBackPos.x;
	curPos.z += finalBackPos.z;

	SetPosition(curPos);
}

void CPlayer::UpdateDirection()
{
	if (MoveDir.x == 0.0f && MoveDir.y == 0.0f && MoveDir.z == 0.0f)
	{
		CollVector.clear(); 
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
	m_pWeaponSocket = pWeaponSocket;

	// 오른손 소켓에 무기 부착
	pWeaponSocket->SetChild(m_pWeapon, true);

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
	m_eWeaponPose = WEAPON_POSE::IDLE;

	//LeftHandGrip 프레임 찾기
	m_pLeftHandGrip = m_pWeapon->FindFrame("LeftHandGrip");
	if (m_pLeftHandGrip)
	{
		OutputDebugString(L"[IK] 성공: 무기에서 LeftHandGrip 프레임을 찾았습니다.\n");
	}
	else
	{
		OutputDebugString(L"[IK] 경고: 무기에서 LeftHandGrip 프레임을 찾지 못했습니다.\n");
	}

	OutputDebugString(L"성공: 무기가 플레이어 오른손 소켓에 장착되었습니다.\n");
}

void CPlayer::ApplyWeaponPose(WEAPON_POSE ePose)
{
	if (!m_pWeapon) return;
	if (!m_bWeaponBaseLocalSaved) return;
	if (!m_pWeaponSocket) return;

	m_eWeaponPose = ePose;

	if (ePose != WEAPON_POSE::GRENADE)
	{
		m_pWeapon->m_xmf4x4ToParent = m_xmf4x4WeaponBaseLocal;
	}

	if (ePose == WEAPON_POSE::IDLE)
	{
		return;
	}
	else if (ePose == WEAPON_POSE::RUN)
	{
		XMMATRIX mBase = XMLoadFloat4x4(&m_pWeapon->m_xmf4x4ToParent);
		XMMATRIX mRunRot = XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(m_xmf3WeaponRunRot.x),
			XMConvertToRadians(m_xmf3WeaponRunRot.y),
			XMConvertToRadians(m_xmf3WeaponRunRot.z)
		);

		XMMATRIX mFinal = XMMatrixMultiply(mRunRot, mBase);
		XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, mFinal);

		m_pWeapon->m_xmf4x4ToParent._41 += m_xmf3WeaponRunPos.x;
		m_pWeapon->m_xmf4x4ToParent._42 += m_xmf3WeaponRunPos.y;
		m_pWeapon->m_xmf4x4ToParent._43 += m_xmf3WeaponRunPos.z;
		return;
	}
	else if (ePose == WEAPON_POSE::GRENADE)
	{
		if (!m_pLeftHand) return;
		if (!m_pLeftForeArm) return;
		if (!m_pLeftHandGrip) return;
		if (!m_bWeaponGrenadeStartCaptured) return;

		XMMATRIX mStartLocal = XMLoadFloat4x4(&m_xmf4x4WeaponGrenadeStartLocal);
		XMVECTOR vStartLocalScale, vStartLocalRot, vStartLocalTrans;
		XMMatrixDecompose(&vStartLocalScale, &vStartLocalRot, &vStartLocalTrans, mStartLocal);

		// 현재 오른손 소켓 월드 회전
		XMMATRIX mSocketWorld = XMLoadFloat4x4(&m_pWeaponSocket->m_xmf4x4World);
		XMVECTOR vSocketScale, vSocketRot, vSocketTrans;
		XMMatrixDecompose(&vSocketScale, &vSocketRot, &vSocketTrans, mSocketWorld);


		XMMATRIX mLeftHandWorld = XMLoadFloat4x4(&m_pLeftHand->m_xmf4x4World);
		XMMATRIX mLeftForeArmWorld = XMLoadFloat4x4(&m_pLeftForeArm->m_xmf4x4World);

		XMVECTOR vHandPos = mLeftHandWorld.r[3];
		XMVECTOR vForeArmPos = mLeftForeArmWorld.r[3];

		XMVECTOR vForward = XMVector3Normalize(XMVectorSubtract(vHandPos, vForeArmPos));

		XMVECTOR vWorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vWorldUp, vForward));

		XMVECTOR vUpForearm = XMVector3Normalize(XMVector3Cross(vForward, vRight));

		XMMATRIX mTargetWorldRotMatrix = XMMatrixIdentity();
		mTargetWorldRotMatrix.r[0] = vRight;
		mTargetWorldRotMatrix.r[1] = vUpForearm;
		mTargetWorldRotMatrix.r[2] = vForward;
		mTargetWorldRotMatrix.r[3] = XMVectorSet(0, 0, 0, 1);

		XMVECTOR vTargetWorldRot = XMQuaternionRotationMatrix(mTargetWorldRotMatrix);

		XMVECTOR vOffsetRot = XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(m_xmf3WeaponGrenadeRot.x),
			XMConvertToRadians(m_xmf3WeaponGrenadeRot.y),
			XMConvertToRadians(m_xmf3WeaponGrenadeRot.z)
		);
		vTargetWorldRot = XMQuaternionMultiply(vOffsetRot, vTargetWorldRot);
		vTargetWorldRot = XMQuaternionNormalize(vTargetWorldRot);


		XMVECTOR vLocalRotFinal = XMQuaternionMultiply(
			vTargetWorldRot,
			XMQuaternionInverse(vSocketRot)
		);
		vLocalRotFinal = XMQuaternionNormalize(vLocalRotFinal);

		XMMATRIX mLocal = XMMatrixAffineTransformation(
			vStartLocalScale,
			XMVectorZero(),
			vLocalRotFinal,
			vStartLocalTrans
		);

		XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, mLocal);

		// 현재 소켓 기준으로 무기/그립 월드 갱신
		m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);

		XMFLOAT3 curGripWorld = m_pLeftHandGrip->GetPosition();

		XMVECTOR vGripOffsetLocal = XMVectorSet(
			m_xmf3WeaponGrenadePos.x,
			m_xmf3WeaponGrenadePos.y,
			m_xmf3WeaponGrenadePos.z,
			1.0f
		);

		XMVECTOR vDesiredGripWorld = XMVector3TransformCoord(vGripOffsetLocal, mLeftHandWorld);

		XMVECTOR vCurGripWorld = XMVectorSet(
			curGripWorld.x,
			curGripWorld.y,
			curGripWorld.z,
			1.0f
		);

		XMVECTOR vDeltaWorld = vDesiredGripWorld - vCurGripWorld;

		// world delta -> socket local delta
		XMMATRIX mInvSocketWorld = XMMatrixInverse(nullptr, mSocketWorld);
		XMVECTOR vDeltaLocal = XMVector3TransformNormal(vDeltaWorld, mInvSocketWorld);

		XMFLOAT3 deltaLocal;
		XMStoreFloat3(&deltaLocal, vDeltaLocal);

		m_pWeapon->m_xmf4x4ToParent._41 += deltaLocal.x;
		m_pWeapon->m_xmf4x4ToParent._42 += deltaLocal.y;
		m_pWeapon->m_xmf4x4ToParent._43 += deltaLocal.z;

		m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
		return;
	}
}

void CPlayer::BeginGrenadeWeaponPose()
{
	if (!m_pWeapon) return;
	if (!m_pWeaponSocket) return;

	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
	m_xmf4x4WeaponGrenadeStartLocal = m_pWeapon->m_xmf4x4ToParent;
	m_xmf4x4WeaponGrenadeStartWorld = m_pWeapon->m_xmf4x4World;
	m_bWeaponGrenadeStartCaptured = true;
}

void CPlayer::EndGrenadeWeaponPose()
{
	m_bWeaponGrenadeStartCaptured = false;

	if (m_pWeapon)
	{
		m_xmf4x4WeaponBlendStartWorld = m_pWeapon->m_xmf4x4World;
		m_bWeaponBlending = true;
		m_fWeaponBlendTime = 0.0f;
	}
}

void CPlayer::UpdateWeaponPose(float fTimeElapsed)
{
	if (!m_pWeapon) return;

	if (IsGrenadeState())
	{
		ApplyWeaponPose(WEAPON_POSE::GRENADE);
	}
	else
	{
		float moveLenSq = MoveDir.x * MoveDir.x + MoveDir.y * MoveDir.y + MoveDir.z * MoveDir.z;

		WEAPON_POSE targetPose = (moveLenSq > 0.0001f) ? WEAPON_POSE::RUN : WEAPON_POSE::IDLE;

		if (m_eWeaponPose != targetPose || m_bWeaponBlending)
		{
			ApplyWeaponPose(targetPose);
		}
	}

	//무기 복귀 블렌딩
	if (m_bWeaponBlending)
	{
		m_fWeaponBlendTime += fTimeElapsed;
		float t = m_fWeaponBlendTime / m_fWeaponBlendDuration;

		if (t >= 1.0f)
		{
			m_bWeaponBlending = false;
		}
		else
		{
			float easeT = 1.0f - powf(1.0f - t, 3.0f);

			XMMATRIX mTargetLocal = XMLoadFloat4x4(&m_pWeapon->m_xmf4x4ToParent);
			XMMATRIX mSocketWorld = XMLoadFloat4x4(&m_pWeaponSocket->m_xmf4x4World);
			XMMATRIX mTargetWorld = XMMatrixMultiply(mTargetLocal, mSocketWorld);

			XMVECTOR vTargetScale, vTargetRot, vTargetTrans;
			XMMatrixDecompose(&vTargetScale, &vTargetRot, &vTargetTrans, mTargetWorld);

			XMMATRIX mStartWorld = XMLoadFloat4x4(&m_xmf4x4WeaponBlendStartWorld);
			XMVECTOR vStartScale, vStartRot, vStartTrans;
			XMMatrixDecompose(&vStartScale, &vStartRot, &vStartTrans, mStartWorld);

			XMVECTOR vCurScale = XMVectorLerp(vStartScale, vTargetScale, easeT);
			XMVECTOR vCurRot = XMQuaternionSlerp(vStartRot, vTargetRot, easeT);
			XMVECTOR vCurTrans = XMVectorLerp(vStartTrans, vTargetTrans, easeT);

			XMMATRIX mCurWorld = XMMatrixAffineTransformation(vCurScale, XMVectorZero(), vCurRot, vCurTrans);

			XMMATRIX mInvSocket = XMMatrixInverse(nullptr, mSocketWorld);
			XMMATRIX mCurLocal = XMMatrixMultiply(mCurWorld, mInvSocket);

			XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, mCurLocal);
		}

		m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
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

	// grenade 중에는 왼손 IK가 상체 throw 동작을 덮어쓰지 않게 막음
	if (m_pOwner->IsGrenadeState()) return;

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

	//OutputDebugStringA("========== MIXAMO FRAME NAMES BEGIN ==========\n");
	//DebugPrintMixamoFrameNames();
	//OutputDebugStringA("========== MIXAMO FRAME NAMES END ==========\n");

	// 왼팔 IK용 본 찾기
	InitializeLeftHandIK();

	m_pSkinnedAnimationController = new CPlayerAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel, this);
	
	m_pSkinnedAnimationController->BuildUpperBodyMask(this, "mixamorig:Spine");

	// Track 0 = lower body, Track 1 = upper body
	m_pSkinnedAnimationController->SetSplitBodyTrackIndices(0, 1);

	// 시작은 둘 다 idle
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(1, ANIM_IDLE);

	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, true);

	m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
	m_pSkinnedAnimationController->SetTrackWeight(1, 1.0f);

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
	// 네트워크 테스트용 버퍼 선언
	size_t buf_len = 0;

	while (!event_queue.empty())
	{
		const GameEvent& ev = event_queue.front();

		switch (ev.type)
		{
		case EventType::Input:
		{
			const bool bGrenadeState = IsGrenadeState();

			if (ev.keyEvent.state == KEY_STATE::DOWN)
			{
				switch (ev.keyEvent.key)
				{
				case INPUT_KEY::SPACE:
					if (!bGrenadeState)
						ChangeState(std::make_unique<PlayerGrenade>());
					break;

				case INPUT_KEY::W:
				case INPUT_KEY::A:
				case INPUT_KEY::S:
				case INPUT_KEY::D:
					if (!bGrenadeState)
						ChangeState(std::make_unique<PlayerRun>());
					break;

				default:
					break;
				}
			}
			else if (ev.keyEvent.state == KEY_STATE::UP)
			{
				switch (ev.keyEvent.key)
				{
				case INPUT_KEY::W:
				case INPUT_KEY::A:
				case INPUT_KEY::S:
				case INPUT_KEY::D:
					if (!bGrenadeState)
					{
						if (!InputManager::Instance().KeyPress(INPUT_KEY::W) &&
							!InputManager::Instance().KeyPress(INPUT_KEY::A) &&
							!InputManager::Instance().KeyPress(INPUT_KEY::S) &&
							!InputManager::Instance().KeyPress(INPUT_KEY::D))
						{
							ChangeState(std::make_unique<PlayerIdle>());
						}
					}
					break;

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
	if (buf_len != 0)
	{
		WSABUF wsabuf[1];
		wsabuf[0].buf = send_buf;
		wsabuf[0].len = static_cast<ULONG>(buf_len);

		WSAOVERLAPPED send_over;
		ZeroMemory(&send_over, sizeof(send_over));

		int ret = WSASend(c_socket, wsabuf, 1, NULL, 0, &send_over, NULL);
		if (SOCKET_ERROR == ret)
		{
			auto err_no = WSAGetLastError();
			// error_display("WSASEND : ", err_no);
		}
	}

	state.get()->Update(this, fTimeElapsed);

	// 충돌에 따른 방향 전환
	UpdateDirection();

	XMFLOAT3 direction = MoveDir;
	direction.y = 0.0f;
	direction = Vector3::ScalarProduct(direction, 8.0f, false);
	CPlayer::Move(direction, true);

	CPlayer::Update(fTimeElapsed);
}

//-------------------------------------------------------------------------
bool PlayerIdle::Enter(CPlayer* Player)
{
	Player->SetUseLeftHandIK(true);
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));
	Player->ApplyWeaponPose(WEAPON_POSE::IDLE);

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		// idle 상태에서는 상하체 모두 idle
		pCtrl->SetTrackAnimationSetIfChanged(0, ANIM_IDLE);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackAnimationSetIfChanged(1, ANIM_IDLE);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}

void PlayerIdle::Update(CPlayer* Player, float fTimeElapsed)
{
	auto& input = InputManager::Instance();

	bool bW = input.KeyPress(INPUT_KEY::W);
	bool bS = input.KeyPress(INPUT_KEY::S);
	bool bA = input.KeyPress(INPUT_KEY::A);
	bool bD = input.KeyPress(INPUT_KEY::D);

	if (bW || bS || bA || bD)
	{
		Player->ChangeState(std::make_unique<PlayerRun>());
		return;
	}

	Player->SetMoveDir(XMFLOAT3(0, 0, 0));
}

void PlayerIdle::Exit(CPlayer* Player)
{
}
//-------------------------------------------------------------------------
bool PlayerRun::Enter(CPlayer* Player)
{
	Player->SetUseLeftHandIK(true);
	Player->ApplyWeaponPose(WEAPON_POSE::RUN);

	auto& input = InputManager::Instance();

	XMFLOAT2 dir = XMFLOAT2(0, 0);
	if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) dir.x += 1;
	if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) dir.x -= 1;
	if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) dir.y -= 1;
	if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) dir.y += 1;

	int nextAnim = ANIM_RUN_F;

	if (!(fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f))
	{
		float angle = atan2f(dir.y, dir.x);

		if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4)
			nextAnim = ANIM_RUN_F;
		else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4)
			nextAnim = ANIM_RUN_R;
		else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4)
			nextAnim = ANIM_RUN_L;
		else
			nextAnim = ANIM_RUN_B;
	}

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		// 평소 run 상태에서는 상하체 둘 다 run
		pCtrl->SetTrackAnimationSetIfChanged(0, nextAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackAnimationSetIfChanged(1, nextAnim);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}

void PlayerRun::Update(CPlayer* Player, float fTimeElapsed)
{
	XMFLOAT2 dir = XMFLOAT2(0, 0);

	auto& input = InputManager::Instance();
	if (input.KeyPress(INPUT_KEY::W)) dir.x += 1;
	if (input.KeyPress(INPUT_KEY::S)) dir.x -= 1;
	if (input.KeyPress(INPUT_KEY::A)) dir.y -= 1;
	if (input.KeyPress(INPUT_KEY::D)) dir.y += 1;

	// 입력이 없으면 반드시 idle로 돌아가야 함
	if (fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f)
	{
		Player->SetMoveDir(XMFLOAT3(0, 0, 0));
		Player->ChangeState(std::make_unique<PlayerIdle>());
		return;
	}

	float angle = atan2f(dir.y, dir.x);
	int nextAnim = ANIM_RUN_F;

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
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		pCtrl->SetTrackAnimationSetIfChanged(0, nextAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackAnimationSetIfChanged(1, nextAnim);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	XMFLOAT3 look = Player->GetLookVector();
	XMFLOAT3 right = Player->GetRightVector();

	XMFLOAT3 direction;
	direction.x = look.x * dir.x + right.x * dir.y;
	direction.y = 0.0f;
	direction.z = look.z * dir.x + right.z * dir.y;
	direction = Vector3::Normalize(direction);

	Player->SetMoveDir(direction);
}

void PlayerRun::Exit(CPlayer* Player)
{
}
//-------------------------------------------------------------------------
bool PlayerGrenade::Enter(CPlayer* Player)
{
	OutputDebugStringA("[Grenade] Enter\n");

	m_fElapsed = 0.0f;
	m_bKeepRun = false;
	m_nLastLowerAnim = ANIM_IDLE;

	Player->SetUseLeftHandIK(false);
	Player->BeginGrenadeWeaponPose();
	Player->ApplyWeaponPose(WEAPON_POSE::GRENADE);

	auto& input = InputManager::Instance();

	bool bW = IsMoveHeld(input, INPUT_KEY::W);
	bool bS = IsMoveHeld(input, INPUT_KEY::S);
	bool bA = IsMoveHeld(input, INPUT_KEY::A);
	bool bD = IsMoveHeld(input, INPUT_KEY::D);

	XMFLOAT2 dir = XMFLOAT2(0, 0);
	if (bW) dir.x += 1;
	if (bS) dir.x -= 1;
	if (bA) dir.y -= 1;
	if (bD) dir.y += 1;

	bool bMove = !(fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f);
	m_bKeepRun = bMove;

	int nextLowerAnim = ANIM_IDLE;
	if (bMove)
	{
		float angle = atan2f(dir.y, dir.x);

		if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4) nextLowerAnim = ANIM_RUN_F;
		else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4) nextLowerAnim = ANIM_RUN_R;
		else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4) nextLowerAnim = ANIM_RUN_L;
		else nextLowerAnim = ANIM_RUN_B;
	}

	m_nLastLowerAnim = nextLowerAnim;

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, nextLowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, ANIM_GRENADE);
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	return true;
}

void PlayerGrenade::Update(CPlayer* Player, float fTimeElapsed)
{
	m_fElapsed += fTimeElapsed;

	auto& input = InputManager::Instance();

	bool bW = IsMoveHeld(input, INPUT_KEY::W);
	bool bS = IsMoveHeld(input, INPUT_KEY::S);
	bool bA = IsMoveHeld(input, INPUT_KEY::A);
	bool bD = IsMoveHeld(input, INPUT_KEY::D);

	XMFLOAT2 dir = XMFLOAT2(0, 0);
	if (bW) dir.x += 1;
	if (bS) dir.x -= 1;
	if (bA) dir.y -= 1;
	if (bD) dir.y += 1;

	bool bMove = !(fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f);
	m_bKeepRun = bMove;

	int nextLowerAnim = ANIM_IDLE;
	if (bMove)
	{
		float angle = atan2f(dir.y, dir.x);

		if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4) nextLowerAnim = ANIM_RUN_F;
		else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4) nextLowerAnim = ANIM_RUN_R;
		else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4) nextLowerAnim = ANIM_RUN_L;
		else nextLowerAnim = ANIM_RUN_B;
	}

	m_nLastLowerAnim = nextLowerAnim;

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, nextLowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}

	XMFLOAT3 look = Player->GetLookVector();
	XMFLOAT3 right = Player->GetRightVector();

	XMFLOAT3 direction;
	if (bMove)
	{
		direction.x = look.x * dir.x + right.x * dir.y;
		direction.y = 0.0f;
		direction.z = look.z * dir.x + right.z * dir.y;
		direction = Vector3::Normalize(direction);
	}
	else
	{
		direction = XMFLOAT3(0, 0, 0);
	}

	Player->SetMoveDir(direction);

	if (m_fElapsed >= 2.80f)
	{
		if (m_nLastLowerAnim != ANIM_IDLE)
			Player->ChangeState(std::make_unique<PlayerRun>());
		else
			Player->ChangeState(std::make_unique<PlayerIdle>());
		return;
	}
}

void PlayerGrenade::Exit(CPlayer* Player)
{
	Player->SetUseLeftHandIK(true);
	Player->EndGrenadeWeaponPose();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(1, m_nLastLowerAnim);

		float fLowerPosition = pCtrl->GetTrackPosition(0);
		pCtrl->SetTrackPosition(1, fLowerPosition);
	}
}
//-------------------------------------------------------------------------
bool PlayerDie::Enter(CPlayer* Player)
{
}

void PlayerDie::Update(CPlayer* Player, float fTimeElapsed)
{
}

void PlayerDie::Exit(CPlayer* Player)
{
}
