#include"stdafx.h"
#include"Shader.h"
#include "ShaderManager.h"

void ShaderManager::BuildShaders(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	Shaders[ShaderType::STANDARD] = make_unique<CStandardObjectsShader>();
	Shaders[ShaderType::STANDARD]->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	Shaders[ShaderType::STANDARD]->CreateShadowShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);

	Shaders[ShaderType::SKINNED] = make_unique<CSkinnedAnimationObjectsShader>();
	Shaders[ShaderType::SKINNED]->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	Shaders[ShaderType::SKINNED]->CreateShadowShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);

	ViewShader* temp = new ViewShader();
	temp->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	temp->CreateThroughShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	Shaders[ShaderType::VIEW] = unique_ptr<ViewShader>(temp);
	

	Shaders[ShaderType::UI] = make_unique<UIObjectShader>();
	Shaders[ShaderType::UI]->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	
	
	PlayerShader* pshader = new PlayerShader();
	pshader->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	pshader->CreateShadowShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	pshader->CreateThroughShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
	Shaders[ShaderType::PLAYER] = unique_ptr<PlayerShader>(pshader);


	Shaders[ShaderType::FULLSCREEN] = make_unique<FullscreenShader>();
	Shaders[ShaderType::FULLSCREEN]->CreateShader(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature);
}

CShader* ShaderManager::GetShader(ShaderType type)
{
	auto it = Shaders.find(type);
	if (it != Shaders.end())return it->second.get();
	return nullptr;
}
