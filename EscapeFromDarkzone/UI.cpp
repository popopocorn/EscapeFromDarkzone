
#include"stdafx.h"
#include "UI.h"


void UIObject::setAABB()
{
	UpdateTransform(NULL);
	auto ext = m_pChild->GetMesh()->GetAABBExtents();
	auto center = m_pChild->GetMesh()->GetAABBCenter();


	XMMATRIX mWorld = XMLoadFloat4x4(&m_pChild->m_xmf4x4World);

	XMVECTOR corners[8];
	corners[0] = XMVectorSet(center.x - ext.x, center.y - ext.y, center.z - ext.z, 1.0f);
	corners[1] = XMVectorSet(center.x + ext.x, center.y - ext.y, center.z - ext.z, 1.0f);
	corners[2] = XMVectorSet(center.x - ext.x, center.y + ext.y, center.z - ext.z, 1.0f);
	corners[3] = XMVectorSet(center.x + ext.x, center.y + ext.y, center.z - ext.z, 1.0f);
	corners[4] = XMVectorSet(center.x - ext.x, center.y - ext.y, center.z + ext.z, 1.0f);
	corners[5] = XMVectorSet(center.x + ext.x, center.y - ext.y, center.z + ext.z, 1.0f);
	corners[6] = XMVectorSet(center.x - ext.x, center.y + ext.y, center.z + ext.z, 1.0f);
	corners[7] = XMVectorSet(center.x + ext.x, center.y + ext.y, center.z + ext.z, 1.0f);

	XMVECTOR vTrans = XMVector3TransformCoord(corners[0], mWorld);
	XMFLOAT3 pt;
	XMStoreFloat3(&pt, vTrans);

	float minX = pt.x, maxX = pt.x;
	float minY = pt.y, maxY = pt.y;

	for (int i = 1; i < 8; ++i)
	{
		vTrans = XMVector3TransformCoord(corners[i], mWorld);
		XMStoreFloat3(&pt, vTrans);

		if (pt.x < minX) minX = pt.x;
		if (pt.x > maxX) maxX = pt.x;
		if (pt.y < minY) minY = pt.y;
		if (pt.y > maxY) maxY = pt.y;
	}

	// 5. 충돌 박스에 최종 반영
	CollisionBox.minX = minX;
	CollisionBox.maxX = maxX;
	CollisionBox.minY = minY;
	CollisionBox.maxY = maxY;

}
