#pragma once
#include"stdafx.h"

class UIMesh;
class CShader;
class RenderTarget;

class FullScreenRenderer
{
public:
	FullScreenRenderer();
	~FullScreenRenderer();
	void init(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist, CShader* s);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, RenderTarget& target);


private:
	CShader* shader;
	unique_ptr<UIMesh>	mesh;



};

