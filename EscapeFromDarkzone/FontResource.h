#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>
#include <unordered_map>

struct FontGlyph
{
	uint32_t codepoint = 0;

	float x = 0.0f;
	float y = 0.0f;

	float width = 0.0f;
	float height = 0.0f;

	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float advanceX = 0.0f;

	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 0.0f;
	float v1 = 0.0f;
};

class FontResource
{
public:
	FontResource() = default;
	~FontResource() = default;

	FontResource(const FontResource&) = delete;
	FontResource& operator=(const FontResource&) = delete;

	FontResource(FontResource&&) = delete;
	FontResource& operator=(FontResource&&) = delete;

public:
	bool Load(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		const wchar_t* pAtlasFilePath,
		const wchar_t* pMetadataFilePath
	);

	void Unload();
	void ReleaseUploadBuffer();

	bool CreateShaderResourceView(
		ID3D12Device* pd3dDevice,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dCpuDescriptorHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE d3dGpuDescriptorHandle
	);

	void UpdateShaderVariable(
		ID3D12GraphicsCommandList* pd3dCommandList,
		UINT nRootParameterIndex
	) const;

	const FontGlyph* FindGlyph(uint32_t codepoint) const;
	const FontGlyph* GetFallbackGlyph() const;

	bool IsLoaded() const;
	bool HasShaderResourceView() const;

	int GetAtlasWidth() const { return m_nAtlasWidth; }
	int GetAtlasHeight() const { return m_nAtlasHeight; }

	float GetFontSize() const { return m_fFontSize; }
	float GetLineHeight() const { return m_fLineHeight; }
	float GetAscent() const { return m_fAscent; }
	float GetDescent() const { return m_fDescent; }

	size_t GetGlyphCount() const { return m_Glyphs.size(); }

	ID3D12Resource* GetAtlasTexture() const { return m_pd3dAtlasTexture.Get(); }

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle() const
	{
		return m_d3dSrvGpuDescriptorHandle;
	}

private:
	bool LoadMetadata(const wchar_t* pMetadataFilePath);

	bool LoadAtlasTexture(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		const wchar_t* pAtlasFilePath
	);

	void ResetMetadata();

private:
	std::unordered_map<uint32_t, FontGlyph> m_Glyphs;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_pd3dAtlasTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pd3dAtlasUploadBuffer;

	D3D12_GPU_DESCRIPTOR_HANDLE m_d3dSrvGpuDescriptorHandle{};

	int m_nAtlasWidth = 0;
	int m_nAtlasHeight = 0;

	float m_fFontSize = 0.0f;
	float m_fLineHeight = 0.0f;
	float m_fAscent = 0.0f;
	float m_fDescent = 0.0f;

	uint32_t m_nFallbackCodepoint = 0xFFFD;

	bool m_bMetadataLoaded = false;
	bool m_bTextureLoaded = false;
	bool m_bShaderResourceViewCreated = false;
};