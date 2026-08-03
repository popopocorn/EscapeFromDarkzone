#include "FullScreenRenderer.h"
#include"UI.h"
#include"Shader.h"
#include"RenderTarget.h"

FullScreenRenderer::FullScreenRenderer()
{}

FullScreenRenderer::~FullScreenRenderer()
{}

void FullScreenRenderer::init(ID3D12Device* device, ID3D12GraphicsCommandList* commandlist, CShader* s)
{
	shader = s;

	mesh = make_unique<UIMesh>(device, commandlist, true);
}

void FullScreenRenderer::Render(ID3D12GraphicsCommandList * pd3dCommandList, RenderTarget& target)
{
	shader->OnPrepareRender(pd3dCommandList, 0);
	target.TransitionTo(pd3dCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	pd3dCommandList->SetGraphicsRootDescriptorTable(18, target.GetSRV());
	if (mesh)
	{
		mesh->Render(pd3dCommandList);
	}
}
