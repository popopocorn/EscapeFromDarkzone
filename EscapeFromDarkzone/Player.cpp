//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "object.h"
#include "InputManager.h"
#include "Collision.h"
#include "Item.h"
#include "ResourceManager.h"
#include"SoundManager.h"
#include "EnemyObject.h"
#include "EffectManager.h"
#include "Projectile.h"
#include"Scene.h"
#include "Network.h"	// 03.27 추가
#include "NetSession.h"

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
	if (fLength > m_fMaxVelocityY)
		m_xmf3Velocity.y *= (fMaxVelocityY / fLength);

	XMFLOAT3 xmf3Velocity = Vector3::ScalarProduct(m_xmf3Velocity, fTimeElapsed, false);
	Move(xmf3Velocity, false);

	// 04.10 추가: 서버 위치 보간
	if (NetworkManager::Instance().IsConnected())
	{
		float alpha = 10.0f * fTimeElapsed;		// 보간 속도 조정 (기본: 5.0f, 05.08: 10.f로 변경)
		m_xmf3Position.x += (m_xmf3ServerPosition.x - m_xmf3Position.x) * alpha;
		m_xmf3Position.z += (m_xmf3ServerPosition.z - m_xmf3Position.z) * alpha;
	}

	//UpdateWeaponCombat(fTimeElapsed);
	UpdateWeaponPose(fTimeElapsed);

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

	m_xmf3Velocity = Vector3::Add(
		m_xmf3Velocity,
		Vector3::ScalarProduct(m_xmf3Velocity, -fDeceleration, true)
	);

	if (m_xmf3Velocity.x != 0.0f || m_xmf3Velocity.z != 0.0f)
	{
		float fSpeedXZSq = m_xmf3Velocity.x * m_xmf3Velocity.x + m_xmf3Velocity.z * m_xmf3Velocity.z;
		if (fSpeedXZSq < 0.0001f * 0.0001f)
		{
			m_xmf3Velocity.x = 0.0f;
			m_xmf3Velocity.z = 0.0f;
		}
	}

	// 03.27 추가: 플레이어가 움직이는 경우 서버에 이동 패킷 전송
	if (NetworkManager::Instance().IsConnected())
	{
		char inputs = 0;
		auto& input = InputManager::Instance();
		if (input.KeyDown(INPUT_KEY::W) || input.KeyHold(INPUT_KEY::W)) inputs |= MOVE_W;
		if (input.KeyDown(INPUT_KEY::S) || input.KeyHold(INPUT_KEY::S)) inputs |= MOVE_S;
		if (input.KeyDown(INPUT_KEY::A) || input.KeyHold(INPUT_KEY::A)) inputs |= MOVE_A;
		if (input.KeyDown(INPUT_KEY::D) || input.KeyHold(INPUT_KEY::D)) inputs |= MOVE_D;

		bool bHasInput = (inputs != 0);

		if (bHasInput) {
			NetSession::Instance().Move(inputs, m_fYaw,
				static_cast<unsigned int>(GetTickCount()));
			//NetworkManager::Instance().SendMove(
			//	inputs, m_fYaw,
			//	static_cast<unsigned int>(GetTickCount())
			//);
		}
		else if (m_bWasMoving) {
			NetSession::Instance().Move(0, m_fYaw,
				static_cast<unsigned int>(GetTickCount()));
			//NetworkManager::Instance().SendMove(
			//	0, m_fYaw,
			//	static_cast<unsigned int>(GetTickCount())
			//);
		}

		m_bWasMoving = bHasInput;
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

bool CPlayer::IsShootState() const
{
	return (dynamic_cast<const PlayerShoot*>(state.get()) != nullptr);
}

void CPlayer::NotifyWeaponFired()
{
	if (m_bReloading) return;
	if (IsGrenadeState()) return;

	m_bShotAnimRequest = true;
}

bool CPlayer::ConsumeShotAnimRequest()
{
	if (!m_bShotAnimRequest) return false;
	m_bShotAnimRequest = false;
	return true;
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

struct PlayerWeaponVisualConfig
{
	ModelName modelName;
	ItemType itemType;

	XMFLOAT3 idlePos;
	XMFLOAT3 idleRot;

	XMFLOAT3 runPos;
	XMFLOAT3 runRot;

	XMFLOAT3 grenadePos;
	XMFLOAT3 grenadeRot;

	XMFLOAT3 shootPos;
	XMFLOAT3 shootRot;

	XMFLOAT3 scale;
};

static PlayerWeaponType GetPlayerWeaponTypeFromItemType(ItemType itemType)
{
	switch (itemType)
	{
	case ItemType::PISTOL:
		return PlayerWeaponType::Pistol;

	case ItemType::SMG:
		return PlayerWeaponType::SMG;

	case ItemType::SHOTGUN:
		return PlayerWeaponType::Shotgun;

	case ItemType::RIFLE:
	default:
		return PlayerWeaponType::Rifle;
	}
}
static PlayerWeaponVisualConfig GetPlayerWeaponVisualConfig(PlayerWeaponType weaponType)
{
	// Rifle 테스트값
	//x값 수정 시 위- 아래+, y값 수정 시 앞+ 뒤-, z값 수정 시 좌+ 우-
	const XMFLOAT3 rifleIdlePos = XMFLOAT3(-0.15f, 0.2f, 0.1f);
	const XMFLOAT3 rifleRunPos = XMFLOAT3(0.0f, 0.10f, -0.0f);
	const XMFLOAT3 rifleShootPos = XMFLOAT3(-0.10f, 0.0f, -0.0f);

	const XMFLOAT3 rifleIdleRot = XMFLOAT3(90.0f, 0.0f, 0.0f);
	const XMFLOAT3 rifleRunRot = XMFLOAT3(8.0f, 0.0f, 0.0f);
	const XMFLOAT3 rifleShootRot = XMFLOAT3(0.0f, 0.0f, 0.0f);

	const XMFLOAT3 rifleGrenadePos = XMFLOAT3(0.01f, 0.05f, 0.01f);
	const XMFLOAT3 rifleGrenadeRot = XMFLOAT3(90.0f, -90.0f, 90.0f);

	const XMFLOAT3 rifleScale = XMFLOAT3(1.1f, 1.1f, 1.1f);

	// SMG 기준값
	//x값 수정 시 위- 아래+, y값 수정 시 앞+ 뒤-, z값 수정 시 좌+ 우-
	const XMFLOAT3 smgIdlePos = XMFLOAT3(-0.14f, 0.10f, 0.16f);
	const XMFLOAT3 smgRunPos = XMFLOAT3(0.18f, 0.10f, -0.08f);
	const XMFLOAT3 smgShootPos = XMFLOAT3(0.20f, -0.10f, -0.20f);

	const XMFLOAT3 smgIdleRot = XMFLOAT3(90.0f, 0.0f, 0.0f);
	const XMFLOAT3 smgRunRot = XMFLOAT3(8.0f, 0.0f, 0.0f);
	const XMFLOAT3 smgShootRot = XMFLOAT3(0.0f, 0.0f, 0.0f);

	const XMFLOAT3 smgGrenadePos = XMFLOAT3(0.01f, 0.05f, 0.01f);
	const XMFLOAT3 smgGrenadeRot = XMFLOAT3(90.0f, -90.0f, 90.0f);

	// Shotgun 테스트값
	// x값 수정 시 좌+ 우-, y값 수정 시 앞+ 뒤-, z값 수정 시 위- 아래+
	const XMFLOAT3 shotgunIdlePos = XMFLOAT3(-0.15f, 0.3f, 0.05f);
	const XMFLOAT3 shotgunRunPos = XMFLOAT3(-0.0f, 0.0f, 0.0f);
	const XMFLOAT3 shotgunShootPos = XMFLOAT3(-0.0f, 0.0f, 0.0f);

	const XMFLOAT3 shotgunIdleRot = XMFLOAT3(180.0f, 0.0f, 90.0f);
	const XMFLOAT3 shotgunRunRot = XMFLOAT3(0.0f, 0.0f, -0.50f);
	const XMFLOAT3 shotgunShootRot = XMFLOAT3(0.40f, 0.0f, -0.0f);

	const XMFLOAT3 shotgunGrenadePos = XMFLOAT3(0.0f, 0.10f, 0.0f);
	const XMFLOAT3 shotgunGrenadeRot = XMFLOAT3(180.0f, -90.0f, 180.0f);

	const XMFLOAT3 shotgunScale = XMFLOAT3(1.1f, 1.1f, 1.1f);

	// 권총 테스트값
	// x값 수정 시 위- 아래+, y값 수정 시 앞+ 뒤-, z값 수정 시 좌+ 우-
	const XMFLOAT3 pistolIdlePos = XMFLOAT3(-0.0f, 0.3f, 0.05f);
	const XMFLOAT3 pistolRunPos = XMFLOAT3(-0.0f, 0.0f, 0.0f);
	const XMFLOAT3 pistolShootPos = XMFLOAT3(-0.0f, 0.0f, 0.0f);

	const XMFLOAT3 pistolIdleRot = XMFLOAT3(90.0f, 0.0f, 0.0f);
	const XMFLOAT3 pistolRunRot = XMFLOAT3(8.0f, 0.0f, 0.0f);
	const XMFLOAT3 pistolShootRot = XMFLOAT3(0.0f, 0.0f, 0.0f);

	const XMFLOAT3 pistolGrenadePos = XMFLOAT3(-0.0f, 0.f, 0.0f);
	const XMFLOAT3 pistolGrenadeRot = XMFLOAT3(180.0f, 0.0f, 110.0f);


	PlayerWeaponVisualConfig config{};
	switch (weaponType)
	{
	case PlayerWeaponType::SMG:
		config.modelName = ModelName::SMG;
		config.itemType = ItemType::SMG;

		config.idlePos = smgIdlePos;
		config.idleRot = smgIdleRot;
		config.runPos = smgRunPos;
		config.runRot = smgRunRot;
		config.grenadePos = smgGrenadePos;
		config.grenadeRot = smgGrenadeRot;
		config.shootPos = smgShootPos;
		config.shootRot = smgShootRot;

		config.scale = XMFLOAT3(
			rifleScale.x * 0.90f,
			rifleScale.y * 0.90f,
			rifleScale.z * 0.90f
		);
		break;

	case PlayerWeaponType::Shotgun:
		config.modelName = ModelName::SHOTGUN;
		config.itemType = ItemType::SHOTGUN;

		config.idlePos = shotgunIdlePos;
		config.idleRot = shotgunIdleRot;

		config.runPos = shotgunRunPos;
		config.runRot = shotgunRunRot;

		config.grenadePos = shotgunGrenadePos;
		config.grenadeRot = shotgunGrenadeRot;

		config.shootPos = shotgunShootPos;
		config.shootRot = shotgunShootRot;

		config.scale = shotgunScale;

		break;

	case PlayerWeaponType::Pistol:
		config.modelName = ModelName::PISTOL;
		config.itemType = ItemType::PISTOL;

		config.idlePos = pistolIdlePos;
		config.idleRot = pistolIdleRot;
		config.runPos = pistolRunPos;
		config.runRot = pistolRunRot;
		config.grenadePos = pistolGrenadePos;
		config.grenadeRot = pistolGrenadeRot;
		config.shootPos = pistolShootPos;
		config.shootRot = pistolShootRot;
		config.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		break;

	case PlayerWeaponType::Rifle:
	default:
		config.modelName = ModelName::RIFLE;
		config.itemType = ItemType::RIFLE;

		config.idlePos = rifleIdlePos;
		config.idleRot = rifleIdleRot;
		config.runPos = rifleRunPos;
		config.runRot = rifleRunRot;
		config.grenadePos = rifleGrenadePos;
		config.grenadeRot = rifleGrenadeRot;
		config.shootPos = rifleShootPos;
		config.shootRot = rifleShootRot;
		config.scale = rifleScale;
		break;
	}

	return config;
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

	m_pWeapon->m_pParent = m_pWeaponSocket;
	m_pWeapon->m_pSibling = m_pWeaponSocket->m_pChild;
	m_pWeaponSocket->m_pChild = m_pWeapon;

	//XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, XMMatrixIdentity());
	static std::unordered_map<CGameObject*, XMFLOAT4X4> s_originalTransformMap;

	// 1. 이 무기가 최초로 장착되는 거라면, 오프셋이 묻지 않은 '순정 FBX 행렬'을 백업합니다.
	if (s_originalTransformMap.find(m_pWeapon) == s_originalTransformMap.end())
	{
		s_originalTransformMap[m_pWeapon] = m_pWeapon->m_xmf4x4ToParent;
	}

	// 2. 무기를 장착할 때마다, 누적된 찌꺼기를 날리고 '순정 FBX 행렬'로 깨끗하게 복구합니다.
	m_pWeapon->m_xmf4x4ToParent = s_originalTransformMap[m_pWeapon];
	pWeapon->SetPosition(
		m_xmf3WeaponIdlePos.x,
		m_xmf3WeaponIdlePos.y,
		m_xmf3WeaponIdlePos.z
	);

	pWeapon->SetScale(
		m_xmf3WeaponScale.x,
		m_xmf3WeaponScale.y,
		m_xmf3WeaponScale.z
	);

	pWeapon->Rotate(
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

	m_pWeaponMuzzleSocket = m_pWeapon->FindFrame("Socket_Muzzle");
	if (m_pWeaponMuzzleSocket)
	{
		OutputDebugString(L"[Weapon] 성공: 현재 무기에서 Socket_Muzzle 프레임을 찾았습니다.\n");
	}
	else
	{
		OutputDebugString(L"[Weapon] 경고: 현재 무기에서 Socket_Muzzle 프레임을 찾지 못했습니다.\n");
	}
	m_pWeapon->UpdateTransform(&m_pWeaponSocket->m_xmf4x4World);

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

	switch (ePose)
	{
	case WEAPON_POSE::IDLE:
		return;

	case WEAPON_POSE::RUN:
		ApplyRunWeaponPose();
		return;

	case WEAPON_POSE::SHOOT:
		ApplyShootWeaponPose();
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

void CPlayer::ApplyShootWeaponPose()
{
	XMMATRIX mBase = XMLoadFloat4x4(&m_pWeapon->m_xmf4x4ToParent);
	XMMATRIX mShootRot = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_xmf3WeaponShootRot.x),
		XMConvertToRadians(m_xmf3WeaponShootRot.y),
		XMConvertToRadians(m_xmf3WeaponShootRot.z)
	);

	XMMATRIX mFinal = XMMatrixMultiply(mShootRot, mBase);
	XMStoreFloat4x4(&m_pWeapon->m_xmf4x4ToParent, mFinal);

	m_pWeapon->m_xmf4x4ToParent._41 += m_xmf3WeaponShootPos.x;
	m_pWeapon->m_xmf4x4ToParent._42 += m_xmf3WeaponShootPos.y;
	m_pWeapon->m_xmf4x4ToParent._43 += m_xmf3WeaponShootPos.z;
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
	else if (IsShootState())
	{
		ApplyWeaponPose(WEAPON_POSE::SHOOT);
	}
	else if (IsReloading())
	{
		// reload 중에는 run pose를 덮지 않게 idle 기준 유지
		ApplyWeaponPose(WEAPON_POSE::IDLE);
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

static void DeleteGameObjectTree(CGameObject* pObject)
{
	if (!pObject) return;

	CGameObject* pChild = pObject->m_pChild;

	while (pChild)
	{
		CGameObject* pNext = pChild->m_pSibling;

		pChild->m_pParent = nullptr;
		pChild->m_pSibling = nullptr;

		DeleteGameObjectTree(pChild);

		pChild = pNext;
	}

	pObject->m_pChild = nullptr;
	pObject->m_pSibling = nullptr;
	pObject->m_pParent = nullptr;

	delete pObject;
}

void CPlayer::DetachCurrentWeapon()
{
	if (!m_pWeapon)
	{
		m_pWeaponSocket = nullptr;
		m_pLeftHandGrip = nullptr;
		m_pWeaponMuzzleSocket = nullptr;
		m_bWeaponBaseLocalSaved = false;
		m_bWeaponGrenadeStartCaptured = false;
		m_eWeaponPose = WEAPON_POSE::IDLE;
		return;
	}

	if (m_pWeaponSocket)
	{
		CGameObject* pPrev = nullptr;
		CGameObject* pCur = m_pWeaponSocket->m_pChild;

		while (pCur)
		{
			if (pCur == m_pWeapon)
			{
				if (pPrev)
				{
					pPrev->m_pSibling = pCur->m_pSibling;
				}
				else
				{
					m_pWeaponSocket->m_pChild = pCur->m_pSibling;
				}

				pCur->m_pSibling = nullptr;
				pCur->m_pParent = nullptr;
				break;
			}

			pPrev = pCur;
			pCur = pCur->m_pSibling;
		}
	}

	m_pWeapon = nullptr;
	m_pWeaponSocket = nullptr;
	m_pLeftHandGrip = nullptr;
	m_pWeaponMuzzleSocket = nullptr;

	m_bWeaponBaseLocalSaved = false;
	m_bWeaponGrenadeStartCaptured = false;
	m_eWeaponPose = WEAPON_POSE::IDLE;
}

void CPlayer::ApplyWeaponVisualConfig(PlayerWeaponType weaponType)
{
	PlayerWeaponVisualConfig config = GetPlayerWeaponVisualConfig(weaponType);

	m_xmf3WeaponIdlePos = config.idlePos;
	m_xmf3WeaponIdleRot = config.idleRot;

	m_xmf3WeaponRunPos = config.runPos;
	m_xmf3WeaponRunRot = config.runRot;

	m_xmf3WeaponGrenadePos = config.grenadePos;
	m_xmf3WeaponGrenadeRot = config.grenadeRot;

	m_xmf3WeaponShootPos = config.shootPos;
	m_xmf3WeaponShootRot = config.shootRot;

	m_xmf3WeaponScale = config.scale;
}

//디버그용 무조건 장착 함수
//bool CPlayer::EquipDebugWeapon(PlayerWeaponType weaponType)
//{
//	PlayerWeaponVisualConfig config = GetPlayerWeaponVisualConfig(weaponType);
//
//	CGameObject* pWeaponPrototype =
//		ResourceManager::Instance().GetModelPrototype(config.modelName);
//
//	if (!pWeaponPrototype)
//	{
//		OutputDebugString(L"[Weapon] weapon prototype not found.\n");
//		return false;
//	}
//
//	if (!pWeaponPrototype)
//	{
//		OutputDebugString(L"[Weapon] weapon instance create failed.\n");
//		return false;
//	}
//
//	auto pWeaponItem = new WeaponItem(
//		ItemGrade::GRADE_1,
//		config.itemType
//	);
//
//	pWeaponItem->SetModelPrototype(pWeaponPrototype);
//
//	DetachCurrentWeapon();
//
//	m_eCurrentWeaponType = weaponType;
//	m_pEquippedWeaponItem = pWeaponItem;
//
//	m_bReloading = false;
//	m_fReloadElapsed = 0.0f;
//	m_fFireCooldown = 0.0f;
//	m_bShotAnimRequest = false;
//
//	ApplyWeaponVisualConfig(weaponType);
//
//	EquipWeapon(pWeaponPrototype, "mixamorig:RightHand");
//
//	if (m_pWeapon != pWeaponPrototype)
//	{
//		DeleteGameObjectTree(pWeaponPrototype);
//		m_pEquippedWeaponItem = NULL;
//		OutputDebugString(L"[Weapon] debug weapon equip failed.\n");
//		return false;
//	}
//
//	InitializeWeaponAmmo();
//
//	ApplyWeaponPose(WEAPON_POSE::IDLE);
//
//	if (m_pSkinnedAnimationController)
//	{
//		int idleAnim = GetIdleAnimationByWeapon();
//
//		m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
//		m_pSkinnedAnimationController->SetTrackType(1, ANIMATION_TYPE_LOOP);
//
//		m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, idleAnim);
//		m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(1, idleAnim);
//
//		m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
//		m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);
//
//		m_pSkinnedAnimationController->SetTrackEnable(0, true);
//		m_pSkinnedAnimationController->SetTrackEnable(1, true);
//
//		m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
//		m_pSkinnedAnimationController->SetTrackWeight(1, 1.0f);
//	}
//
//	ChangeState(std::make_unique<PlayerIdle>());
//
//	OutputDebugString(L"[Weapon] debug weapon equipped.\n");
//	return true;
//}


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

bool CPlayer::EquipWeaponItem(PlayerWeaponType type, const char* pstrSocketName)
{

	if (m_eCurrentWeaponType == type && m_pWeapon)return false;
	auto it = PlayerOwnWeapons.find(type);
	if (it == PlayerOwnWeapons.end())return false;

	DetachCurrentWeapon();

	m_eCurrentWeaponType = type;

	m_pEquippedWeaponItem = it->second.get();

	ApplyWeaponVisualConfig(type);
	
	CGameObject* pWeaponInstance = m_pEquippedWeaponItem->GetModelPrototype();

	EquipWeapon(pWeaponInstance, pstrSocketName);

	if (m_pWeapon != pWeaponInstance)
	{
		m_pEquippedWeaponItem = NULL;
		return false;
	}
	auto* pCtrl = GetAnimationController();
	if (pCtrl)
	{
		int upperAnim = GetIdleAnimationByWeapon();
		int lowerAnim = GetIdleAnimationByWeapon();

		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(1, upperAnim);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	InitializeWeaponAmmo();
	ApplyWeaponPose(WEAPON_POSE::IDLE);

	if (NetworkManager::Instance().IsConnected())
	{
		NetSession::Instance().ChangeWeapon(
			GetEquippedWeaponTypeForWire(),
			GetEquippedWeaponGradeForWire());
		// OtherPlayer 무기 동기화를 위해 현재 무기 정보를 패킷으로 전송
	}

	return true;
}

float CPlayer::GetWeaponShotInterval() const
{
	if (!m_pEquippedWeaponItem)
		return 0.1f;

	const WeaponSpec& spec = m_pEquippedWeaponItem->GetSpec();

	if (spec.rpm <= 0.0f)
		return 0.1f;

	return (60.0f / spec.rpm);
}

float CPlayer::GetWeaponDamage() const
{
	if (!m_pEquippedWeaponItem)
		return 10.0f;

	return m_pEquippedWeaponItem->GetSpec().damage;
}

void CPlayer::InitializeWeaponAmmo()
{
	if (!m_pEquippedWeaponItem)
	{
		m_bReloading = false;
		m_fReloadElapsed = 0.0f;
		m_fReloadDuration = 0.0f;
		m_fFireCooldown = 0.0f;
		return;
	}

	const WeaponSpec& spec = m_pEquippedWeaponItem->GetSpec();

	m_bReloading = false;
	m_fReloadElapsed = 0.0f;
	m_fReloadDuration = spec.reloadTime;
	m_fFireCooldown = 0.0f;

	wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"[Weapon] Ammo Init : %d / %d\n", m_pEquippedWeaponItem->CurAmmo, m_pEquippedWeaponItem->maxAmmo);
	OutputDebugStringW(debugBuf);
}

void CPlayer::UpdateWeaponCombat(float fTimeElapsed, const std::vector<CShader*>& ppShaders, EffectManager* pEffectManager)
{
	if (m_fFireCooldown > 0.0f)
	{
		m_fFireCooldown -= fTimeElapsed;
		if (m_fFireCooldown < 0.0f)
			m_fFireCooldown = 0.0f;
	}
	if (m_bFireHeld)
	{
		
		if (m_fFireCooldown <= 0.0f)
		{
			FireOneShot(ppShaders, pEffectManager);

			if (!IsCurrentWeaponAutomatic() || m_pEquippedWeaponItem->CurAmmo <= 0)
			{
				m_bFireHeld = false;
			}
		}
	}
	if (!m_bReloading)
		return;

	m_fReloadElapsed += fTimeElapsed;

	if (m_fReloadElapsed >= m_fReloadDuration)
	{
		m_bReloading = false;
		m_fReloadElapsed = 0.0f;
		m_pEquippedWeaponItem->CurAmmo = m_pEquippedWeaponItem->maxAmmo;

		wchar_t debugBuf[256];
		swprintf_s(debugBuf, L"[Weapon] Reload Complete : %d / %d\n", m_pEquippedWeaponItem->CurAmmo, m_pEquippedWeaponItem->maxAmmo);
		OutputDebugStringW(debugBuf);
	}
}
#include"SoundManager.h"
void CPlayer::FireOneShot(const std::vector<CShader*>& ppShaders, EffectManager* pEffectManager)
{
	if (!TryFireWeapon()) 
	{
		if(not m_bReloading)
			SoundManager::Instance()->Play(SoundName::DRY_RIFLE, m_pWeaponMuzzleSocket->GetPosition());
		return;
	}
	NotifyWeaponFired();
	
	// 1. 총구 위치 계산
	XMFLOAT3 muzzlePos, muzzleLook, muzzleRight, muzzleUp;
	muzzleLook = GetLookVector();
	muzzleRight = GetRightVector();
	muzzleUp = GetUpVector();
	if (Vector3::Length(muzzleLook) < 0.0001f) muzzleLook = XMFLOAT3(0.0f, 0.0f, 1.0f);
	if (Vector3::Length(muzzleRight) < 0.0001f) muzzleRight = XMFLOAT3(1.0f, 0.0f, 0.0f);
	if (Vector3::Length(muzzleUp) < 0.0001f) muzzleUp = XMFLOAT3(0.0f, 1.0f, 0.0f);

	muzzleLook = Vector3::Normalize(muzzleLook);
	muzzleRight = Vector3::Normalize(muzzleRight);
	muzzleUp = Vector3::Normalize(muzzleUp);

	if (m_pWeaponMuzzleSocket) {
		UpdateTransform(NULL);
		muzzlePos = m_pWeaponMuzzleSocket->GetPosition();
		muzzlePos.x += muzzleLook.x * 0.05f;
		muzzlePos.y += muzzleLook.y * 0.05f;
		muzzlePos.z += muzzleLook.z * 0.05f;
	}
	else {
		muzzlePos = GetPosition();
		muzzlePos.y += 1.2f; // 총구가 없을 때 기본 오프셋
	}


	// PvP 송신 (단발 무기도 첫 발에서 전송)
	if (NetworkManager::Instance().IsConnected())
	{
		short wType = GetEquippedWeaponTypeForWire();
		short wGrade = GetEquippedWeaponGradeForWire();
		NetSession::Instance().FireHitPlayer(muzzlePos, muzzleLook, wType, wGrade);
		NetSession::Instance().FireHit(muzzlePos, muzzleLook, wType, wGrade);
		//NetworkManager::Instance().SendHitNpc(muzzlePos, muzzleLook, 0);
		//NetSession::Instance().FireHit(muzzlePos, muzzleLook, 0);
	}

	PlayerWeaponType weaponType = GetCurrentPlayerWeaponType();
	switch (weaponType)
	{
	case PlayerWeaponType::Rifle:
		SoundManager::Instance()->Play(SoundName::FIRE_RIFLE, muzzlePos);
		break;
	case PlayerWeaponType::SMG:
		SoundManager::Instance()->Play(SoundName::FIRE_SMG, muzzlePos);
		break;
	case PlayerWeaponType::Shotgun:
		SoundManager::Instance()->Play(SoundName::FIRE_SHOTGUN, muzzlePos);
		break;
	case PlayerWeaponType::Pistol:
		SoundManager::Instance()->Play(SoundName::FIRE_PISTOL, muzzlePos);
		break;
	}
	
	// 2. 이펙트 재생
	
	EFFECT_TYPE sparkType = (weaponType == PlayerWeaponType::Shotgun) ? EFFECT_SPARK_SHOTGUN :
		(weaponType == PlayerWeaponType::Pistol) ? EFFECT_SPARK_PISTOL : EFFECT_SPARK_RIFLE_SMG;

	XMFLOAT3 sparkPos = GetSparkPositionByWeapon(weaponType,muzzlePos, muzzleLook, muzzleUp );

	if (pEffectManager) {
		pEffectManager->RequestPlayEffect(sparkType, sparkPos, muzzleRight, muzzleUp);
		pEffectManager->UpdateLaser(0, muzzlePos, muzzleRight, muzzleUp, muzzleLook, 15.0f);
	}

	float maxRange = 100.f;
	float hitDistance = maxRange;
	XMVECTOR rayOrigin = XMLoadFloat3(&muzzlePos);
	XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&muzzleLook));

	// 적 충돌 검사
	if (ppShaders.size() > SHADERIDX::ENEMY && ppShaders[SHADERIDX::ENEMY] && !ppShaders[SHADERIDX::ENEMY]->GetObj()->empty())
	{
		bool isIntersects = false;
		auto* objs = ppShaders[SHADERIDX::ENEMY]->GetObj();
		for (auto& obj : *objs) {
			if (!obj) continue;
			if (not obj->isColl) continue;
			const auto& oobbs = obj->GetOOBB();
			for (BoundingOrientedBox* pOOBB : oobbs) {
				if (!pOOBB) continue;
				float fDist = 0.0f;
				if (pOOBB->Intersects(rayOrigin, rayDir, fDist)) {
					CEnemyObject* pEnemy = dynamic_cast<CEnemyObject*>(obj);
					if (pEnemy) pEnemy->HandleHP(GetWeaponDamage());
					isIntersects = true;
					if (fDist < hitDistance) hitDistance = fDist;
					break;
				}
			}
		}

		if (!isIntersects && ppShaders.size() > SHADERIDX::MAP && ppShaders[SHADERIDX::MAP]) {
			auto* maps = ppShaders[SHADERIDX::MAP]->GetObj();
			for (auto& obj : *maps) {
				if (!obj) continue;
				if (not obj->isColl) continue;
				const auto& oobbs = obj->GetOOBB();
				for (BoundingOrientedBox* pOOBB : oobbs) {
					if (!pOOBB) continue;
					float fDist = 0.0f;
					if (pOOBB->Intersects(rayOrigin, rayDir, fDist)) {
						if (fDist < hitDistance) hitDistance = fDist;
						break;
					}
				}
			}
		}
	}

	XMFLOAT3 endPos;
	endPos.x = muzzlePos.x + muzzleLook.x * hitDistance;
	endPos.y = muzzlePos.y + muzzleLook.y * hitDistance;
	endPos.z = muzzlePos.z + muzzleLook.z * hitDistance;

	ProjectileManager::Instance()->SpawnProjectile(ProjectileType::RIFLE_BULLET, muzzlePos, endPos);

}

bool CPlayer::CanFireWeapon() const
{
	if (!m_pWeapon) return false;
	if (!m_pEquippedWeaponItem) return false;
	if (m_bReloading) return false;
	if (m_pEquippedWeaponItem->CurAmmo <= 0) return false;
	return true;
}

int CPlayer::GetCurrentAmmo() const
{
	return m_pEquippedWeaponItem->CurAmmo;
}
int CPlayer::GetMaxAmmo() const
{
	return m_pEquippedWeaponItem->maxAmmo;
}
bool CPlayer::TryFireWeapon()
{
	if (!CanFireWeapon())
		return false;

	if (m_fFireCooldown > 0.0f)
		return false;
	--m_pEquippedWeaponItem->CurAmmo;
	m_fFireCooldown = GetWeaponShotInterval();

	wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"[Weapon] Ammo : %d / %d\n", m_pEquippedWeaponItem->CurAmmo, m_pEquippedWeaponItem->maxAmmo);
	OutputDebugStringW(debugBuf);

	return true;
}

void CPlayer::StartReload()
{
	if (!m_pWeapon) return;
	if (!m_pEquippedWeaponItem) return;
	if (m_bReloading) return;
	if (m_pEquippedWeaponItem->CurAmmo >= m_pEquippedWeaponItem->maxAmmo) return;
	SoundManager::Instance()->Play(SoundName::RELOAD_PLAYER_RIFLE, m_pWeaponMuzzleSocket->GetPosition());
	const WeaponSpec& spec = m_pEquippedWeaponItem->GetSpec();

	m_bReloading = true;
	m_fReloadElapsed = 0.0f;
	m_fReloadDuration = (spec.reloadTime > 0.0f) ? spec.reloadTime : 1.0f;
	m_fFireCooldown = 0.0f;
	m_bShotAnimRequest = false;

	wchar_t debugBuf[256];
	swprintf_s(debugBuf, L"[Weapon] Reload Start (%d / %d)\n", m_pEquippedWeaponItem->CurAmmo, m_pEquippedWeaponItem->maxAmmo);
	OutputDebugStringW(debugBuf);
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

PlayerWeaponType CPlayer::GetCurrentPlayerWeaponType() const
{
	if (!m_pEquippedWeaponItem)
		return m_eCurrentWeaponType;

	return GetPlayerWeaponTypeFromItemType(m_pEquippedWeaponItem->GetType());
}

short CPlayer::GetEquippedWeaponTypeForWire() const
{
	if (!m_pEquippedWeaponItem)
		return static_cast<short>(ItemType::RIFLE);
	return static_cast<short>(m_pEquippedWeaponItem->GetType());
}

short CPlayer::GetEquippedWeaponGradeForWire() const
{
	if (!m_pEquippedWeaponItem)
		return static_cast<short>(ItemGrade::GRADE_1);
	return static_cast<short>(m_pEquippedWeaponItem->GetGrade());
}

bool CPlayer::IsCurrentWeaponAutomatic() const
{
	switch (GetCurrentPlayerWeaponType())
	{
	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Rifle:
		return true;

	case PlayerWeaponType::Pistol:
	case PlayerWeaponType::Shotgun:
	default:
		return false;
	}
}

int CPlayer::GetIdleAnimationByWeapon() const
{
	switch (GetCurrentPlayerWeaponType())
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_IDLE;
	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_IDLE;
	}
}

int CPlayer::GetGrenadeAnimationByWeapon() const
{
	switch (GetCurrentPlayerWeaponType())
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_GRENADE;
	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_GRENADE;
	}
}

int CPlayer::GetShootAnimationByWeapon() const
{
	switch (GetCurrentPlayerWeaponType())
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_SHOOT;
	case PlayerWeaponType::Shotgun:
		return PLAYER_SHOTGUN_SHOOT;
	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_SHOOT;
	}
}

int CPlayer::GetReloadAnimationByWeapon() const
{
	switch (GetCurrentPlayerWeaponType())
	{
	case PlayerWeaponType::Pistol:
		return PLAYER_PISTOL_RELOAD;
	case PlayerWeaponType::SMG:
	case PlayerWeaponType::Shotgun:
	case PlayerWeaponType::Rifle:
	default:
		return PLAYER_RIFLE_SMG_RELOAD;
	}
}

int CPlayer::GetDieAnimationByWeapon() const
{
	return PLAYER_DIE;
}

int CPlayer::GetRunAnimationFromInput(const XMFLOAT2& dir) const
{
	PlayerWeaponType weaponType = GetCurrentPlayerWeaponType();

	int runF = PLAYER_RIFLE_SMG_RUN_F;
	int runL = PLAYER_RIFLE_SMG_RUN_L;
	int runB = PLAYER_RIFLE_SMG_RUN_B;
	int runR = PLAYER_RIFLE_SMG_RUN_R;

	if (weaponType == PlayerWeaponType::Pistol)
	{
		runF = PLAYER_PISTOL_RUN_F;
		runL = PLAYER_PISTOL_RUN_L;
		runB = PLAYER_PISTOL_RUN_B;
		runR = PLAYER_PISTOL_RUN_R;
	}

	if (fabs(dir.x) < 0.01f && fabs(dir.y) < 0.01f)
		return runF;

	float angle = atan2f(dir.y, dir.x);

	if (angle > -XM_PIDIV4 && angle <= XM_PIDIV4)
		return runF;
	else if (angle > XM_PIDIV4 && angle <= 3 * XM_PIDIV4)
		return runR;
	else if (angle <= -XM_PIDIV4 && angle > -3 * XM_PIDIV4)
		return runL;
	else
		return runB;
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

void CPlayer::InitializeInventory(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* pUIShader)
{
	if (m_pInventory)
		return;

	m_pInventory = std::make_unique<Inventory>(
		pd3dDevice,
		pd3dCommandList,
		pd3dGraphicsRootSignature,
		pUIShader
	);

	m_pInventory->SetPosition(-0.25f, 0.0f);
	m_pInventory->isOpen = false;
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

CTerrainPlayer::CTerrainPlayer(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12RootSignature* pd3dGraphicsRootSignature,
	CShader* shader,
	CLoadedModelInfo* pPlayerModel,
	CGameObject* pDefaultWeaponPrototype)
{
	m_pCamera = ChangeCamera(THIRD_PERSON_CAMERA, 0.0f);

	if (!pPlayerModel)
	{
		OutputDebugString(L"Error: Player model instance is null.\n");
		return;
	}

	if (!pPlayerModel->m_pAnimationSets)
	{
		pPlayerModel->m_pAnimationSets = new CAnimationSets(0);
	}

	SetChild(pPlayerModel->m_pModelRootObject, true);

	BoundingOrientedBox playerBox;
	playerBox.Center = XMFLOAT3(0.0f, 1.0f, 0.0f);
	playerBox.Extents = XMFLOAT3(0.35f, 1.0f, 0.35f);
	playerBox.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	SetOOBB(playerBox);
	///*
	PlayerOwnWeapons[PlayerWeaponType::Rifle] = make_unique<WeaponItem>(ItemGrade::GRADE_1, ItemType::RIFLE);
	PlayerOwnWeapons[PlayerWeaponType::Rifle]->SetModelPrototype(
		ResourceManager::Instance().GetModelPrototype(ModelName::RIFLE)
	);
	PlayerOwnWeapons[PlayerWeaponType::Shotgun] = make_unique<WeaponItem>(ItemGrade::GRADE_1, ItemType::SHOTGUN);
	PlayerOwnWeapons[PlayerWeaponType::Shotgun]->SetModelPrototype(
		ResourceManager::Instance().GetModelPrototype(ModelName::SHOTGUN)
	);
	PlayerOwnWeapons[PlayerWeaponType::SMG] = make_unique<WeaponItem>(ItemGrade::GRADE_1, ItemType::SMG);
	PlayerOwnWeapons[PlayerWeaponType::SMG]->SetModelPrototype(
		ResourceManager::Instance().GetModelPrototype(ModelName::SMG)
	);
	//*/

	PlayerOwnWeapons[PlayerWeaponType::Pistol] = make_unique<WeaponItem>(ItemGrade::GRADE_1, ItemType::PISTOL);
	PlayerOwnWeapons[PlayerWeaponType::Pistol]->SetModelPrototype(
		ResourceManager::Instance().GetModelPrototype(ModelName::PISTOL)
	);


	EquipWeaponItem(PlayerWeaponType::Pistol, "mixamorig:RightHand");
	

	InitializeLeftHandIK();

	m_pSkinnedAnimationController = new CPlayerAnimationController(
		pd3dDevice,
		pd3dCommandList,
		2,
		pPlayerModel,
		this
	);

	m_pSkinnedAnimationController->BuildUpperBodyMask(this, "mixamorig:Spine");
	m_pSkinnedAnimationController->SetSplitBodyTrackIndices(0, 1);

	m_pSkinnedAnimationController->SetTrackType(0, ANIMATION_TYPE_LOOP);
	m_pSkinnedAnimationController->SetTrackType(1, ANIMATION_TYPE_LOOP);

	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(0, PLAYER_RIFLE_SMG_IDLE);
	m_pSkinnedAnimationController->SetTrackAnimationSetIfChanged(1, PLAYER_RIFLE_SMG_IDLE);

	m_pSkinnedAnimationController->SetTrackPosition(0, 0.0f);
	m_pSkinnedAnimationController->SetTrackPosition(1, 0.0f);

	m_pSkinnedAnimationController->SetTrackEnable(0, true);
	m_pSkinnedAnimationController->SetTrackEnable(1, true);

	m_pSkinnedAnimationController->SetTrackWeight(0, 1.0f);
	m_pSkinnedAnimationController->SetTrackWeight(1, 1.0f);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

}

CTerrainPlayer::~CTerrainPlayer()
{
	//if (m_pChild)m_pChild->Release();
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
		m_pCamera->SetOffset(XMFLOAT3(0.0f, cameraDistance, -5.0f));
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
				case INPUT_KEY::KEY_1:
					if (EquipWeaponItem(PlayerWeaponType::Rifle, "mixamorig:RightHand"))
						SoundManager::Instance()->Play(SoundName::EQUIP_WEAPON, GetPosition());
					break;

				case INPUT_KEY::KEY_2:
					if (EquipWeaponItem(PlayerWeaponType::SMG, "mixamorig:RightHand"))
					SoundManager::Instance()->Play(SoundName::EQUIP_WEAPON, GetPosition());
					break;

				case INPUT_KEY::KEY_3:
					if (EquipWeaponItem(PlayerWeaponType::Shotgun, "mixamorig:RightHand"))
					SoundManager::Instance()->Play(SoundName::EQUIP_WEAPON, GetPosition());
					break;

				case INPUT_KEY::KEY_4:
					if (EquipWeaponItem(PlayerWeaponType::Pistol, "mixamorig:RightHand"))
					SoundManager::Instance()->Play(SoundName::EQUIP_WEAPON, GetPosition());
					break;

				case INPUT_KEY::SPACE:
					if (!bGrenadeState)
						ChangeState(std::make_unique<PlayerGrenade>());
					break;

				case INPUT_KEY::R:
					if (!bGrenadeState)
					{
						StartReload();
						if (IsReloading())
							ChangeState(std::make_unique<PlayerReload>());
					}
					break;

				case INPUT_KEY::W:
				case INPUT_KEY::A:
				case INPUT_KEY::S:
				case INPUT_KEY::D:
					if (!bGrenadeState && !IsReloading())
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
					if (!bGrenadeState && !IsReloading())
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

	if (state)
		state->Update(this, fTimeElapsed);

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
	Player->ApplyWeaponPose(WEAPON_POSE::IDLE);
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackType(1, ANIMATION_TYPE_LOOP);

		pCtrl->SetTrackAnimationSetIfChanged(0, Player->GetIdleAnimationByWeapon());
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetIdleAnimationByWeapon());

		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackEnable(1, true);

		pCtrl->SetTrackWeight(0, 1.0f);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}

void PlayerIdle::Update(CPlayer* Player, float fTimeElapsed)
{
	if (Player->IsReloading())
	{
		Player->ChangeState(std::make_unique<PlayerReload>());
		return;
	}

	if (Player->ConsumeShotAnimRequest())
	{
		Player->ChangeState(std::make_unique<PlayerShoot>());
		return;
	}

	XMFLOAT2 dir = Player->GetMoveInput2D();

	if (Player->IsMoveInputActive(dir))
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

		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetIdleAnimationByWeapon());
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}


void PlayerRun::Update(CPlayer* Player, float fTimeElapsed)
{
	static float timeacu = 0;
	timeacu += fTimeElapsed;
	if (timeacu > 0.5)
	{
		SoundManager::Instance()->Play(SoundName::FOOSTEP, Player->GetPosition());
		timeacu -= 0.5;
	}
	if (Player->IsReloading())
	{
		Player->ChangeState(std::make_unique<PlayerReload>());
		return;
	}

	if (Player->ConsumeShotAnimRequest())
	{
		Player->ChangeState(std::make_unique<PlayerShoot>());
		return;
	}

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

		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetIdleAnimationByWeapon());
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
	m_nLastLowerAnim = Player->GetIdleAnimationByWeapon();

	Player->BeginGrenadeWeaponPose();
	Player->ApplyWeaponPose(WEAPON_POSE::GRENADE);

	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	m_bKeepRun = bMove;

	int nextLowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();
	m_nLastLowerAnim = nextLowerAnim;

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, nextLowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetGrenadeAnimationByWeapon());
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
	if (bMove)
	{
		static float timeacu = 0;
		timeacu += fTimeElapsed;
		if (timeacu > 0.5)
		{
			SoundManager::Instance()->Play(SoundName::FOOSTEP, Player->GetPosition());
			timeacu -= 0.5;
		}
	}
	int nextLowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();
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
		if (m_nLastLowerAnim != Player->GetIdleAnimationByWeapon())
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
bool PlayerShoot::Enter(CPlayer* Player)
{
	m_fElapsed = 0.0f;

	Player->SetWeaponBlending(false);
	Player->SetWeaponBlendTime(0.0f);
	Player->ApplyWeaponPose(WEAPON_POSE::SHOOT);

	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	int lowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetShootAnimationByWeapon());
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}

void PlayerShoot::Update(CPlayer* Player, float fTimeElapsed)
{
	if (Player->IsReloading())
	{
		Player->ChangeState(std::make_unique<PlayerReload>());
		return;
	}

	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	int lowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();
	if (bMove)
	{
		static float timeacu = 0;
		timeacu += fTimeElapsed;
		if (timeacu > 0.5)
		{
			SoundManager::Instance()->Play(SoundName::FOOSTEP, Player->GetPosition());
			timeacu -= 0.5;
		}
	}
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);
	}

	Player->SetMoveDir(Player->GetMoveDirectionFromInput(dir));

	m_fElapsed += fTimeElapsed;

	if (Player->ConsumeShotAnimRequest())
	{
		m_fElapsed = 0.0f;

		if (pCtrl)
		{
			pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
			pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetShootAnimationByWeapon());
			pCtrl->SetTrackPosition(1, 0.0f);
			pCtrl->SetTrackEnable(1, true);
			pCtrl->SetTrackWeight(1, 1.0f);
		}
	}

	if (m_fElapsed >= m_fAnimDuration)
	{
		if (bMove) Player->ChangeState(std::make_unique<PlayerRun>());
		else       Player->ChangeState(std::make_unique<PlayerIdle>());
	}
}

void PlayerShoot::Exit(CPlayer* Player)
{
	XMFLOAT2 dir = Player->GetMoveInput2D();
	if (Player->IsMoveInputActive(dir))
	{
		Player->ApplyWeaponPose(WEAPON_POSE::RUN);
	}
	else
	{
		Player->ApplyWeaponPose(WEAPON_POSE::IDLE);
	}
}

//-------------------------------------------------------------------------
bool PlayerReload::Enter(CPlayer* Player)
{
	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	int lowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();

	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);

		pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);
		pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetReloadAnimationByWeapon());
		pCtrl->SetTrackPosition(1, 0.0f);
		pCtrl->SetTrackEnable(1, true);
		pCtrl->SetTrackWeight(1, 1.0f);
	}
	return true;
}

void PlayerReload::Update(CPlayer* Player, float fTimeElapsed)
{
	XMFLOAT2 dir = Player->GetMoveInput2D();
	bool bMove = Player->IsMoveInputActive(dir);
	int lowerAnim = bMove ? Player->GetRunAnimationFromInput(dir) : Player->GetIdleAnimationByWeapon();
	if (bMove)
	{
		static float timeacu = 0;
		timeacu += fTimeElapsed;
		if (timeacu > 0.5)
		{
			SoundManager::Instance()->Play(SoundName::FOOSTEP, Player->GetPosition());
			timeacu -= 0.5;
		}
	}
	auto* pCtrl = Player->GetAnimationController();
	if (pCtrl)
	{
		pCtrl->SetTrackType(0, ANIMATION_TYPE_LOOP);
		pCtrl->SetTrackAnimationSetIfChanged(0, lowerAnim);
		pCtrl->SetTrackEnable(0, true);
		pCtrl->SetTrackWeight(0, 1.0f);
	}

	Player->SetMoveDir(Player->GetMoveDirectionFromInput(dir));

	if (!Player->IsReloading())
	{
		if (bMove) Player->ChangeState(std::make_unique<PlayerRun>());
		else       Player->ChangeState(std::make_unique<PlayerIdle>());
	}
}

void PlayerReload::Exit(CPlayer* Player)
{
}

//-------------------------------------------------------------------------
bool PlayerDie::Enter(CPlayer* Player)
{
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));
	Player->ApplyWeaponPose(WEAPON_POSE::IDLE);

	auto* pCtrl = Player->GetAnimationController();
	if (!pCtrl) return false;

	pCtrl->SetTrackType(0, ANIMATION_TYPE_ONCE);
	pCtrl->SetTrackType(1, ANIMATION_TYPE_ONCE);

	pCtrl->SetTrackAnimationSetIfChanged(0, Player->GetDieAnimationByWeapon());
	pCtrl->SetTrackAnimationSetIfChanged(1, Player->GetDieAnimationByWeapon());

	pCtrl->SetTrackPosition(0, 0.0f);
	pCtrl->SetTrackPosition(1, 0.0f);

	pCtrl->SetTrackEnable(0, true);
	pCtrl->SetTrackEnable(1, true);

	pCtrl->SetTrackWeight(0, 1.0f);
	pCtrl->SetTrackWeight(1, 1.0f);

	return true;
}

void PlayerDie::Update(CPlayer* Player, float fTimeElapsed)
{
	Player->SetMoveDir(XMFLOAT3(0, 0, 0));
}

void PlayerDie::Exit(CPlayer* Player)
{
}