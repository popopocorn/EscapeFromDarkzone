//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "InputManager.h"
#include "Collision.h"
#include "Item.h"

/*
#include "Network.h"	// 03.27 추가
*/

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

	/*
	// 04.10 추가: 서버 위치 보간
	if (NetworkManager::Instance().IsConnected())
	{
		float alpha = 5.0f * fTimeElapsed;		// 추후 보간 속도 조정 (5.0f)
		m_xmf3Position.x += (m_xmf3ServerPosition.x - m_xmf3Position.x) * alpha;
		m_xmf3Position.z += (m_xmf3ServerPosition.z - m_xmf3Position.z) * alpha;
	}
	*/

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

	/*
	// 03.27 추가: 플레이어가 움직이는 경우 서버에 이동 패킷 전송
	if (NetworkManager::Instance().IsConnected() && (state && typeid(*state) == typeid(PlayerRun)))
	{
		char inputs = 0;
		auto& input = InputManager::Instance();
		if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) inputs |= MOVE_W;
		if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) inputs |= MOVE_S;
		if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) inputs |= MOVE_A;
		if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) inputs |= MOVE_D;

		if (inputs != 0)
		{
			NetworkManager::Instance().SendMove(
				inputs,
				m_fYaw,
				static_cast<unsigned int>(GetTickCount())
			);
		}
	}
	*/
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

	return pNewCamera;
}

void CPlayer::OnPrepareRender()
{
	m_xmf4x4ToParent._11 = m_xmf3Right.x; m_xmf4x4ToParent._12 = m_xmf3Right.y; m_xmf4x4ToParent._13 = m_xmf3Right.z;
	m_xmf4x4ToParent._21 = m_xmf3Up.x;    m_xmf4x4ToParent._22 = m_xmf3Up.y;    m_xmf4x4ToParent._23 = m_xmf3Up.z;
	m_xmf4x4ToParent._31 = m_xmf3Look.x;  m_xmf4x4ToParent._32 = m_xmf3Look.y;  m_xmf4x4ToParent._33 = m_xmf3Look.z;
	m_xmf4x4ToParent._41 = m_xmf3Position.x;
	m_xmf4x4ToParent._42 = m_xmf3Position.y;
	m_xmf4x4ToParent._43 = m_xmf3Position.z;

	m_xmf4x4ToParent = Matrix4x4::Multiply(
		XMMatrixScaling(m_xmf3Scale.x, m_xmf3Scale.y, m_xmf3Scale.z),
		m_xmf4x4ToParent
	);
}

void CPlayer::Render(ID3D12GraphicsCommandList* pd3dCommandList, int nPipelineState, CCamera* pCamera)
{
	CGameObject::Render(pd3dCommandList, false, nPipelineState, pCamera);
}

void CPlayer::ChangeState(std::unique_ptr<State<CPlayer>> new_state)
{
	if (!new_state) return;

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

	XMVECTOR vBackPos = XMLoadFloat3(&result.mtv);
	XMVECTOR vNormal = XMLoadFloat3(&result.normal);

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

	XMStoreFloat3(&MoveDir, currentDirVec);

	CollVector.clear();
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

	pWeaponSocket->SetChild(m_pWeapon, true);

	m_pWeapon->SetPosition(
		m_xmf3WeaponIdlePos.x,
		m_xmf3WeaponIdlePos.y,
		m_xmf3WeaponIdlePos.z
	);

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

bool CPlayer::EquipWeaponItem(const std::shared_ptr<WeaponItem>& pItem, const char* pstrSocketName)
{
	if (!pItem) return false;

	CGameObject* pWeaponInstance = pItem->CreateModelInstance();
	if (!pWeaponInstance) return false;

	m_pEquippedWeaponItem = pItem;
	EquipWeapon(pWeaponInstance, pstrSocketName);
	return true;
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

	switch (ePose)
	{
	case WEAPON_POSE::IDLE:
		return;

	case WEAPON_POSE::RUN:
		ApplyRunWeaponPose();
		return;

	case WEAPON_POSE::GRENADE:
		ApplyGrenadeWeaponPose();
		return;
	}
}

void CPlayer::ApplyRunWeaponPose()
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
}

void CPlayer::ApplyGrenadeWeaponPose()
{
	if (!m_pWeapon) return;
	if (!m_pWeaponSocket) return;
	if (!m_pLeftHand) return;
	if (!m_pLeftForeArm) return;
	if (!m_pLeftHandGrip) return;
	if (!m_bWeaponGrenadeStartCaptured) return;

	XMMATRIX mStartLocal = XMLoadFloat4x4(&m_xmf4x4WeaponGrenadeStartLocal);
	XMVECTOR vStartLocalScale, vStartLocalRot, vStartLocalTrans;
	XMMatrixDecompose(&vStartLocalScale, &vStartLocalRot, &vStartLocalTrans, mStartLocal);

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

	XMMATRIX mInvSocketWorld = XMMatrixInverse(nullptr, mSocketWorld);
	XMVECTOR vDeltaLocal = XMVector3TransformNormal(vDeltaWorld, mInvSocketWorld);

	XMFLOAT3 deltaLocal;
	XMStoreFloat3(&deltaLocal, vDeltaLocal);

	m_pWeapon->m_xmf4x4ToParent._41 += deltaLocal.x;
	m_pWeapon->m_xmf4x4ToParent._42 += deltaLocal.y;
	m_pWeapon->m_xmf4x4ToParent._43 += deltaLocal.z;

	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
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
		const float moveLenSq = MoveDir.x * MoveDir.x + MoveDir.y * MoveDir.y + MoveDir.z * MoveDir.z;
		const WEAPON_POSE targetPose = (moveLenSq > 0.0001f) ? WEAPON_POSE::RUN : WEAPON_POSE::IDLE;

		if (m_eWeaponPose != targetPose || m_bWeaponBlending)
		{
			ApplyWeaponPose(targetPose);
		}
	}

	if (!m_bWeaponBlending)
		return;

	m_fWeaponBlendTime += fTimeElapsed;
	float t = m_fWeaponBlendTime / m_fWeaponBlendDuration;

	if (t >= 1.0f)
	{
		m_bWeaponBlending = false;
		return;
	}

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
	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
}

void CPlayer::BeginGrenadeWeaponPose()
{
	if (!m_pWeapon) return;
	if (!m_pWeaponSocket) return;

	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);
	m_xmf4x4WeaponGrenadeStartLocal = m_pWeapon->m_xmf4x4ToParent;
	m_bWeaponGrenadeStartCaptured = true;
}

void CPlayer::EndGrenadeWeaponPose()
{
	if (!m_pWeapon) return;

	m_pWeapon->m_xmf4x4ToParent = m_xmf4x4WeaponBaseLocal;
	m_bWeaponGrenadeStartCaptured = false;
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

XMFLOAT2 CPlayer::GetMoveInput2D() const
{
	auto& input = InputManager::Instance();

	XMFLOAT2 dir = XMFLOAT2(0, 0);
	if (input.KeyPress(INPUT_KEY::W) || input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) dir.x += 1.0f;
	if (input.KeyPress(INPUT_KEY::S) || input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) dir.x -= 1.0f;
	if (input.KeyPress(INPUT_KEY::A) || input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) dir.y -= 1.0f;
	if (input.KeyPress(INPUT_KEY::D) || input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) dir.y += 1.0f;

	return dir;
}

int CPlayer::GetRunAnimationFromInput(const XMFLOAT2& dir) const
{
	if (fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f)
		return ANIM_RUN_F;

	float angle = atan2f(dir.y, dir.x);

	if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4)
		return ANIM_RUN_F;
	else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4)
		return ANIM_RUN_R;
	else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4)
		return ANIM_RUN_L;
	else
		return ANIM_RUN_B;
}

bool CPlayer::IsMoveInputActive(const XMFLOAT2& dir) const
{
	return !(fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f);
}

XMFLOAT3 CPlayer::GetMoveDirectionFromInput(const XMFLOAT2& dir) const
{
	if (!IsMoveInputActive(dir))
		return XMFLOAT3(0, 0, 0);

	XMFLOAT3 look = GetLookVector();
	XMFLOAT3 right = GetRightVector();

	XMFLOAT3 direction;
	direction.x = look.x * dir.x + right.x * dir.y;
	direction.y = 0.0f;
	direction.z = look.z * dir.x + right.z * dir.y;
	return Vector3::Normalize(direction);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
void CPlayerAnimationController::OnAnimationIK(CGameObject* pRootGameObject)
{
	if (!m_pOwner) return;
	if (m_pOwner->IsGrenadeState()) return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

	CLoadedModelInfo* pPlayerModel = CGameObject::LoadGeometryAndAnimationFromFile(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		"Model/Ch15_nonPBR.bin",
		shader
	);
	if (!pPlayerModel->m_pAnimationSets) pPlayerModel->m_pAnimationSets = new CAnimationSets(0);

	SetChild(pPlayerModel->m_pModelRootObject, true);

	SetOOBB(NULL);

	auto pDefaultWeaponItem = WeaponItem::CreateDefaultPlayerRifle(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		shader
	);
	//디버깅용
	/*if (pDefaultWeaponItem)
	{
		OutputDebugString(L"[Item] Default weapon item created\n");
	}
	else
	{
		OutputDebugString(L"[Item] Default weapon item creation failed\n");
	}
	if (pDefaultWeaponItem)
	{
		if (!EquipWeaponItem(pDefaultWeaponItem, "mixamorig:RightHand"))
		{
			OutputDebugString(L"Error: Default weapon equip failed.\n");
		}
	}
	else
	{
		OutputDebugString(L"Error: Default weapon item creation failed.\n");
	}*/

	InitializeLeftHandIK();

	m_pSkinnedAnimationController = new CPlayerAnimationController(pd3dDevice, pd3dCommandList, 2, pPlayerModel, this);

	m_pSkinnedAnimationController->BuildUpperBodyMask(this, "mixamorig:Spine");
	m_pSkinnedAnimationController->SetSplitBodyTrackIndices(0, 1);

	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, ANIM_IDLE);
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(1, ANIM_IDLE);

	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, true);

	m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
	m_pSkinnedAnimationController->SetTrackWeight(1, 1.0f);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	if (pPlayerModel) delete pPlayerModel;
}
CTerrainPlayer::~CTerrainPlayer()
{
}

CCamera* CTerrainPlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
{
	DWORD nCurrentCameraMode = (m_pCamera) ? m_pCamera->GetMode() : 0x00;
	if (nCurrentCameraMode == nNewCameraMode) return m_pCamera;

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

	return m_pCamera;
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


		event_queue.pop();
	}


	state.get()->Update(this, fTimeElapsed);

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
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));
	Player->ApplyWeaponPose(WEAPON_POSE::IDLE);

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

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
	Player->ApplyWeaponPose(WEAPON_POSE::RUN);

	XMFLOAT2 dir = Player->GetMoveInput2D();
	int nextAnim = Player->GetRunAnimationFromInput(dir);

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
	return true;
}

void PlayerRun::Update(CPlayer* Player, float fTimeElapsed)
{
	XMFLOAT2 dir = Player->GetMoveInput2D();

	if (!Player->IsMoveInputActive(dir))
	{
		Player->SetMoveDir(XMFLOAT3(0, 0, 0));
		Player->ChangeState(std::make_unique<PlayerIdle>());
		return;
	}

	int nextAnim = Player->GetRunAnimationFromInput(dir);

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

	Player->SetMoveDir(Player->GetMoveDirectionFromInput(dir));
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

	Player->BeginGrenadeWeaponPose();
	Player->ApplyWeaponPose(WEAPON_POSE::GRENADE);

	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	m_bKeepRun = bMove;

	int nextLowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : ANIM_IDLE;
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

	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	m_bKeepRun = bMove;

	int nextLowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : ANIM_IDLE;
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

	Player->SetMoveDir(Player->GetMoveDirectionFromInput(dir));

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
	return false;
}

void PlayerDie::Update(CPlayer* Player, float fTimeElapsed)
{
}

void PlayerDie::Exit(CPlayer* Player)
{
}