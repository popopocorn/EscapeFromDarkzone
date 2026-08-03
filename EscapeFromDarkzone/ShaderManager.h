#pragma once

enum class ShaderType {
	STANDARD,
	SKINNED,
	VIEW,
	UI,
	PLAYER,
	FULLSCREEN,
};

class ShaderManager
{
private:
	unordered_map<ShaderType, unique_ptr<CShader>> Shaders;

public:
	void BuildShaders(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature);
	
	CShader* GetShader(ShaderType type);
};

