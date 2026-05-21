#pragma once


class RootSignature
{
public:
	RootSignature(ID3D12Device* pd3dDevice);
	~RootSignature();
	ID3D12RootSignature* GetRoot() { return m_pd3dGraphicsRootSignature; }

private:
	ID3D12RootSignature* m_pd3dGraphicsRootSignature;
};