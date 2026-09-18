#include "FullScreenRenderer.h"
#include"UI.h"
#include"Shader.h"
#include"RenderTarget.h"
#include"ShaderManager.h"

FullScreenRenderer::FullScreenRenderer()
{}

FullScreenRenderer::~FullScreenRenderer()
{}

void FullScreenRenderer::init(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist, ShaderManager* s)
{
	shaders.resize(FTypeEnd);
	shaders[FSTANDARD] = s->GetShader(ShaderType::FULLSCREEN);
	shaders[FLIGHT] = s->GetShader(ShaderType::LIGHT);

	mesh = make_unique<UIMesh>(device, commandlist, true);
}

void FullScreenRenderer::Render(ID3D12GraphicsCommandList * pd3dCommandList, RenderTarget* target, FShaderType type)
{
	shaders[type]->OnPrepareRender(pd3dCommandList, 0);
	if(target)
	{
		target->TransitionTo(pd3dCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		UINT rootParamIdx = (type == FLIGHT) ? 19 : 18;
		pd3dCommandList->SetGraphicsRootDescriptorTable(rootParamIdx, target->GetSRV());
	}
	if (mesh)
	{
		mesh->Render(pd3dCommandList);
	}
}
