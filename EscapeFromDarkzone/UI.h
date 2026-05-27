#pragma once
#include "Object.h"


class UIMesh {
private:
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	
	vector<XMFLOAT3>				m_pxmf3Positions;
	vector<XMFLOAT2>				UVs;

	ID3D12Resource*					m_pd3dPositionBuffer = NULL;
	ID3D12Resource*					m_pd3dPositionUploadBuffer = NULL;
	ID3D12Resource*					UVBuffer = NULL;
	ID3D12Resource*					UVUploadBuffer = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dPositionBufferView;
	D3D12_VERTEX_BUFFER_VIEW		UVBufferView;
	unique_ptr<CTexture>			texture;
public:	
	UIMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist);
	~UIMesh();
	virtual void ReleaseUploadBuffers();
	virtual void LoadTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName);
	virtual void OnPreRender(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, int nSubSet = 0, int nInstances = 1);

};

struct CheckBox {
	float minX;
	float minY;
	float maxX;
	float maxY;
	bool Intersects(POINT p)
	{
		float ndcX = (2.0f * static_cast<float>(p.x)) / FRAME_BUFFER_WIDTH - 1.0f;
		float ndcY = 1.0f - (2.0f * static_cast<float>(p.y)) / FRAME_BUFFER_HEIGHT; // DirectX 기준 Y축 상하 반전
		if (ndcX < minX) return false;
		if (ndcX > maxX) return false;
		if (ndcY < minY) return false;
		if (ndcY > maxY) return false;
		return true;

	}
};


class UIObject {
protected:
	UIMesh*					object;
	XMFLOAT4X4				world;
	std::function<void()>	Task = nullptr;
	CheckBox				CollisionBox;
public:
	UIObject();

	void SetUIMesh(UIMesh* mesh) { object = mesh; }
	void SetScale(float x, float y, float z);
	void SetLocate(float x, float y, float z);
	void HandleClick() { if (Task) Task(); }
	void SetFunc(std::function<void()> func) { Task = func; }
	void setAABB();
	CheckBox GetBox() { return CollisionBox; }


	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, bool batch, int nPipelineState, CCamera* pCamera = NULL);
};

class UIObjectShader;

class HUDManager {
private:
	vector<unique_ptr<UIObject>> objs;
public:
	bool ProcessClick(POINT mouse);
	void SubmitToShader(UIObjectShader* shader);
	void release();

	void BuildLobby();
};