#pragma once

#include "stdafx.h"
#include <cstdint>

class FontResource;
class UIText;

struct TextVertex
{
	DirectX::XMFLOAT2 position;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT4 color;
};

class TextRenderer
{
public:
	static constexpr UINT FONT_TEXTURE_ROOT_PARAMETER_INDEX = 3;
	static constexpr UINT FRAME_BUFFER_COUNT = 3;

public:
	TextRenderer() = default;
	~TextRenderer();

	TextRenderer(const TextRenderer&) = delete;
	TextRenderer& operator=(const TextRenderer&) = delete;

	TextRenderer(TextRenderer&&) = delete;
	TextRenderer& operator=(TextRenderer&&) = delete;

public:
	bool Initialize(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dRootSignature,
		ID3D12DescriptorHeap* pd3dDescriptorHeap,
		FontResource* pFontResource,
		UINT nScreenWidth,
		UINT nScreenHeight,
		const wchar_t* pShaderFilePath,
		UINT nMaxGlyphCount = 4096
	);

	void Shutdown();

	void BeginFrame();
	void SubmitText(UIText* pText);

	void Render(ID3D12GraphicsCommandList* pd3dCommandList);

	void SetScreenSize(UINT nScreenWidth, UINT nScreenHeight);

	UINT GetScreenWidth() const
	{
		return m_nScreenWidth;
	}

	UINT GetScreenHeight() const
	{
		return m_nScreenHeight;
	}

	UINT GetMaxGlyphCount() const
	{
		return m_nMaxGlyphCount;
	}

	size_t GetSubmittedTextCount() const
	{
		return m_SubmittedTexts.size();
	}

	bool IsInitialized() const
	{
		return m_bInitialized;
	}

private:
	struct TextLine
	{
		std::vector<uint32_t> codepoints;
		float width = 0.0f;
	};

private:
	bool CreatePipelineState(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dRootSignature,
		const wchar_t* pShaderFilePath
	);

	bool CreateVertexBuffer(
		ID3D12Device* pd3dDevice,
		UINT nMaxGlyphCount
	);

	void BuildSubmittedTextVertices();

	bool AppendTextVertices(const UIText& text);

	void BuildTextLines(
		const UIText& text,
		std::vector<TextLine>& outLines
	) const;

	DirectX::XMFLOAT2 ConvertPixelToNdc(float x, float y) const;

	void AppendGlyphQuad(
		float left,
		float top,
		float right,
		float bottom,
		float u0,
		float v0,
		float u1,
		float v1,
		const DirectX::XMFLOAT4& color
	);

private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pd3dPipelineState;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pd3dVertexBuffer;

	ID3D12RootSignature* m_pd3dRootSignature = nullptr;
	ID3D12DescriptorHeap* m_pd3dDescriptorHeap = nullptr;

	FontResource* m_pFontResource = nullptr;

	TextVertex* m_pMappedVertexBuffer = nullptr;

	std::vector<UIText*> m_SubmittedTexts;
	std::vector<TextVertex> m_TextVertices;

	UINT m_nScreenWidth = 0;
	UINT m_nScreenHeight = 0;

	UINT m_nMaxGlyphCount = 0;
	UINT m_nVerticesPerFrame = 0;
	UINT m_nCurrentFrameBuffer = 0;

	bool m_bInitialized = false;
	bool m_bVertexOverflowLogged = false;
};