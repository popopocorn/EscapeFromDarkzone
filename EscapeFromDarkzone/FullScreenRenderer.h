#pragma once
#include"stdafx.h"

class UIMesh;
class CShader;
class ShaderManager;
class RenderTarget;

enum FShaderType {
	FSTANDARD = 0,
	FLIGHT,

	FTypeEnd
};


class FullScreenRenderer
{
public:
	FullScreenRenderer();
	~FullScreenRenderer();
	void init(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist, ShaderManager* s);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, RenderTarget* target, FShaderType type);


private:
	vector<CShader*> shaders;
	unique_ptr<UIMesh>	mesh;



};

