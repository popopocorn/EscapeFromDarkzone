#include "stdafx.h"
#include "TextRenderer.h"
#include "FontResource.h"
#include "UIText.h"
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
	bool CompileTextShader(
		const wchar_t* pShaderFilePath,
		const char* pEntryPoint,
		const char* pShaderProfile,
		ComPtr<ID3DBlob>& outShaderBlob)
	{
		if (!pShaderFilePath || !pEntryPoint || !pShaderProfile)
			return false;

		UINT nCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
		nCompileFlags |= D3DCOMPILE_DEBUG;
		nCompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		nCompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		ComPtr<ID3DBlob> pErrorBlob;

		HRESULT hResult = D3DCompileFromFile(
			pShaderFilePath,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			pEntryPoint,
			pShaderProfile,
			nCompileFlags,
			0,
			outShaderBlob.ReleaseAndGetAddressOf(),
			pErrorBlob.ReleaseAndGetAddressOf()
		);

		if (pErrorBlob)
		{
			OutputDebugStringA(
				static_cast<const char*>(pErrorBlob->GetBufferPointer())
			);
		}

		if (FAILED(hResult))
		{
			wchar_t debugText[512];

			swprintf_s(
				debugText,
				L"[TextRenderer] Shader compile failed. Entry=%S, HRESULT=0x%08X, File=%s\n",
				pEntryPoint,
				static_cast<unsigned int>(hResult),
				pShaderFilePath
			);

			OutputDebugStringW(debugText);
			return false;
		}

		return true;
	}

	D3D12_RASTERIZER_DESC CreateTextRasterizerState()
	{
		D3D12_RASTERIZER_DESC rasterizerDesc{};

		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		return rasterizerDesc;
	}

	D3D12_BLEND_DESC CreateTextBlendState()
	{
		D3D12_BLEND_DESC blendDesc{};

		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;

		D3D12_RENDER_TARGET_BLEND_DESC& renderTarget = blendDesc.RenderTarget[0];

		renderTarget.BlendEnable = TRUE;
		renderTarget.LogicOpEnable = FALSE;

		renderTarget.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTarget.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		renderTarget.BlendOp = D3D12_BLEND_OP_ADD;

		renderTarget.SrcBlendAlpha = D3D12_BLEND_ONE;
		renderTarget.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		renderTarget.BlendOpAlpha = D3D12_BLEND_OP_ADD;

		renderTarget.LogicOp = D3D12_LOGIC_OP_NOOP;
		renderTarget.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		return blendDesc;
	}

	D3D12_DEPTH_STENCIL_DESC CreateTextDepthStencilState()
	{
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

		depthStencilDesc.DepthEnable = FALSE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		depthStencilDesc.StencilEnable = FALSE;
		depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		depthStencilDesc.BackFace = depthStencilDesc.FrontFace;

		return depthStencilDesc;
	}
}

TextRenderer::~TextRenderer()
{
	Shutdown();
}

bool TextRenderer::Initialize(
	ID3D12Device* pd3dDevice,
	ID3D12RootSignature* pd3dRootSignature,
	ID3D12DescriptorHeap* pd3dDescriptorHeap,
	FontResource* pFontResource,
	UINT nScreenWidth,
	UINT nScreenHeight,
	const wchar_t* pShaderFilePath,
	UINT nMaxGlyphCount)
{
	Shutdown();

	if (!pd3dDevice)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Device is null.\n");
		return false;
	}

	if (!pd3dRootSignature)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Root signature is null.\n");
		return false;
	}

	if (!pd3dDescriptorHeap)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Descriptor heap is null.\n");
		return false;
	}

	if (!pFontResource || !pFontResource->IsLoaded())
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Font resource is not loaded.\n");
		return false;
	}

	if (!pFontResource->HasShaderResourceView())
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Font SRV is not created.\n");
		return false;
	}

	if (nScreenWidth == 0 || nScreenHeight == 0)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Invalid screen size.\n");
		return false;
	}

	if (!pShaderFilePath)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Shader path is null.\n");
		return false;
	}

	if (nMaxGlyphCount == 0)
	{
		OutputDebugStringW(L"[TextRenderer] Initialize failed. Maximum glyph count is zero.\n");
		return false;
	}

	m_pd3dRootSignature = pd3dRootSignature;
	m_pd3dDescriptorHeap = pd3dDescriptorHeap;
	m_pFontResource = pFontResource;

	m_nScreenWidth = nScreenWidth;
	m_nScreenHeight = nScreenHeight;

	if (!CreatePipelineState(pd3dDevice, pd3dRootSignature, pShaderFilePath))
	{
		OutputDebugStringW(L"[TextRenderer] Pipeline state creation failed.\n");
		Shutdown();
		return false;
	}

	if (!CreateVertexBuffer(pd3dDevice, nMaxGlyphCount))
	{
		OutputDebugStringW(L"[TextRenderer] Vertex buffer creation failed.\n");
		Shutdown();
		return false;
	}

	m_SubmittedTexts.reserve(128);
	m_TextVertices.reserve(m_nVerticesPerFrame);

	m_bInitialized = true;

	wchar_t debugText[256];

	swprintf_s(
		debugText,
		L"[TextRenderer] Initialize complete. Screen=%ux%u, MaxGlyphs=%u\n",
		m_nScreenWidth,
		m_nScreenHeight,
		m_nMaxGlyphCount
	);

	OutputDebugStringW(debugText);

	return true;
}

void TextRenderer::Shutdown()
{
	if (m_pd3dVertexBuffer && m_pMappedVertexBuffer)
	{
		m_pd3dVertexBuffer->Unmap(0, nullptr);
	}

	m_pMappedVertexBuffer = nullptr;

	m_pd3dVertexBuffer.Reset();
	m_pd3dPipelineState.Reset();

	m_pd3dRootSignature = nullptr;
	m_pd3dDescriptorHeap = nullptr;
	m_pFontResource = nullptr;

	m_SubmittedTexts.clear();
	m_TextVertices.clear();

	m_nScreenWidth = 0;
	m_nScreenHeight = 0;

	m_nMaxGlyphCount = 0;
	m_nVerticesPerFrame = 0;
	m_nCurrentFrameBuffer = 0;

	m_bInitialized = false;
	m_bVertexOverflowLogged = false;
}

void TextRenderer::BeginFrame()
{
	m_SubmittedTexts.clear();
	m_TextVertices.clear();
	m_bVertexOverflowLogged = false;
}

void TextRenderer::SubmitText(UIText* pText)
{
	if (!m_bInitialized)
		return;

	if (!pText)
		return;

	if (!pText->IsVisible())
		return;

	if (pText->GetText().empty())
		return;

	m_SubmittedTexts.push_back(pText);
}

void TextRenderer::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (!m_bInitialized)
		return;

	if (!pd3dCommandList)
		return;

	if (!m_pd3dPipelineState ||
		!m_pd3dVertexBuffer ||
		!m_pMappedVertexBuffer ||
		!m_pd3dRootSignature ||
		!m_pd3dDescriptorHeap ||
		!m_pFontResource)
	{
		return;
	}

	BuildSubmittedTextVertices();

	if (m_TextVertices.empty())
	{
		m_SubmittedTexts.clear();
		return;
	}

	const UINT nVertexCount = static_cast<UINT>(m_TextVertices.size());

	const UINT64 nSingleFrameBufferSize =
		static_cast<UINT64>(m_nVerticesPerFrame) * sizeof(TextVertex);

	const UINT64 nFrameBufferOffset =
		nSingleFrameBufferSize * m_nCurrentFrameBuffer;

	TextVertex* pFrameVertexBuffer =
		reinterpret_cast<TextVertex*>(
			reinterpret_cast<unsigned char*>(m_pMappedVertexBuffer) +
			nFrameBufferOffset
			);

	std::memcpy(
		pFrameVertexBuffer,
		m_TextVertices.data(),
		static_cast<size_t>(nVertexCount) * sizeof(TextVertex)
	);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	vertexBufferView.BufferLocation =
		m_pd3dVertexBuffer->GetGPUVirtualAddress() +
		nFrameBufferOffset;

	vertexBufferView.SizeInBytes =
		nVertexCount * sizeof(TextVertex);

	vertexBufferView.StrideInBytes =
		sizeof(TextVertex);

	ID3D12DescriptorHeap* descriptorHeaps[] =
	{
		m_pd3dDescriptorHeap
	};

	pd3dCommandList->SetDescriptorHeaps(
		_countof(descriptorHeaps),
		descriptorHeaps
	);

	pd3dCommandList->SetGraphicsRootSignature(
		m_pd3dRootSignature
	);

	pd3dCommandList->SetPipelineState(
		m_pd3dPipelineState.Get()
	);

	m_pFontResource->UpdateShaderVariable(
		pd3dCommandList,
		FONT_TEXTURE_ROOT_PARAMETER_INDEX
	);

	pd3dCommandList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	pd3dCommandList->IASetVertexBuffers(
		0,
		1,
		&vertexBufferView
	);

	pd3dCommandList->DrawInstanced(
		nVertexCount,
		1,
		0,
		0
	);

	m_nCurrentFrameBuffer =
		(m_nCurrentFrameBuffer + 1) %
		FRAME_BUFFER_COUNT;

	m_SubmittedTexts.clear();
	m_TextVertices.clear();
}

void TextRenderer::SetScreenSize(
	UINT nScreenWidth,
	UINT nScreenHeight)
{
	if (nScreenWidth == 0 || nScreenHeight == 0)
		return;

	m_nScreenWidth = nScreenWidth;
	m_nScreenHeight = nScreenHeight;
}

bool TextRenderer::CreatePipelineState(
	ID3D12Device* pd3dDevice,
	ID3D12RootSignature* pd3dRootSignature,
	const wchar_t* pShaderFilePath)
{
	ComPtr<ID3DBlob> pVertexShaderBlob;
	ComPtr<ID3DBlob> pPixelShaderBlob;

	if (!CompileTextShader(
		pShaderFilePath,
		"VSText",
		"vs_5_1",
		pVertexShaderBlob))
	{
		return false;
	}

	if (!CompileTextShader(
		pShaderFilePath,
		"PSText",
		"ps_5_1",
		pPixelShaderBlob))
	{
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};

	pipelineStateDesc.pRootSignature = pd3dRootSignature;

	pipelineStateDesc.VS.pShaderBytecode =
		pVertexShaderBlob->GetBufferPointer();

	pipelineStateDesc.VS.BytecodeLength =
		pVertexShaderBlob->GetBufferSize();

	pipelineStateDesc.PS.pShaderBytecode =
		pPixelShaderBlob->GetBufferPointer();

	pipelineStateDesc.PS.BytecodeLength =
		pPixelShaderBlob->GetBufferSize();

	pipelineStateDesc.BlendState =
		CreateTextBlendState();

	pipelineStateDesc.SampleMask =
		std::numeric_limits<UINT>::max();

	pipelineStateDesc.RasterizerState =
		CreateTextRasterizerState();

	pipelineStateDesc.DepthStencilState =
		CreateTextDepthStencilState();

	pipelineStateDesc.InputLayout.pInputElementDescs =
		inputElementDescs;

	pipelineStateDesc.InputLayout.NumElements =
		_countof(inputElementDescs);

	pipelineStateDesc.IBStripCutValue =
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	pipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	pipelineStateDesc.NumRenderTargets = 1;

	pipelineStateDesc.RTVFormats[0] =
		DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateDesc.DSVFormat =
		DXGI_FORMAT_D24_UNORM_S8_UINT;

	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleDesc.Quality = 0;

	pipelineStateDesc.NodeMask = 0;
	pipelineStateDesc.CachedPSO.pCachedBlob = nullptr;
	pipelineStateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	pipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT hResult =
		pd3dDevice->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(
				m_pd3dPipelineState.ReleaseAndGetAddressOf()
			)
		);

	if (FAILED(hResult))
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[TextRenderer] CreateGraphicsPipelineState failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult)
		);

		OutputDebugStringW(debugText);
		return false;
	}

	m_pd3dPipelineState->SetName(
		L"Text Renderer Pipeline State"
	);

	return true;
}

bool TextRenderer::CreateVertexBuffer(
	ID3D12Device* pd3dDevice,
	UINT nMaxGlyphCount)
{
	if (!pd3dDevice || nMaxGlyphCount == 0)
		return false;

	if (nMaxGlyphCount >
		(std::numeric_limits<UINT>::max() / 6))
	{
		OutputDebugStringW(
			L"[TextRenderer] Maximum glyph count is too large.\n"
		);

		return false;
	}

	m_nMaxGlyphCount = nMaxGlyphCount;
	m_nVerticesPerFrame = nMaxGlyphCount * 6;

	const UINT64 nSingleFrameBufferSize =
		static_cast<UINT64>(m_nVerticesPerFrame) *
		sizeof(TextVertex);

	const UINT64 nTotalBufferSize =
		nSingleFrameBufferSize *
		FRAME_BUFFER_COUNT;

	D3D12_HEAP_PROPERTIES heapProperties{};

	heapProperties.Type =
		D3D12_HEAP_TYPE_UPLOAD;

	heapProperties.CPUPageProperty =
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN;

	heapProperties.MemoryPoolPreference =
		D3D12_MEMORY_POOL_UNKNOWN;

	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc{};

	resourceDesc.Dimension =
		D3D12_RESOURCE_DIMENSION_BUFFER;

	resourceDesc.Alignment = 0;
	resourceDesc.Width = nTotalBufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hResult =
		pd3dDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(
				m_pd3dVertexBuffer.ReleaseAndGetAddressOf()
			)
		);

	if (FAILED(hResult) || !m_pd3dVertexBuffer)
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[TextRenderer] Vertex buffer creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult)
		);

		OutputDebugStringW(debugText);
		return false;
	}

	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	hResult = m_pd3dVertexBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(&m_pMappedVertexBuffer)
	);

	if (FAILED(hResult) || !m_pMappedVertexBuffer)
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[TextRenderer] Vertex buffer map failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(hResult)
		);

		OutputDebugStringW(debugText);

		m_pd3dVertexBuffer.Reset();
		m_pMappedVertexBuffer = nullptr;

		return false;
	}

	m_pd3dVertexBuffer->SetName(
		L"Text Renderer Dynamic Vertex Buffer"
	);

	return true;
}

void TextRenderer::BuildSubmittedTextVertices()
{
	m_TextVertices.clear();

	for (UIText* pText : m_SubmittedTexts)
	{
		if (!pText)
			continue;

		if (!pText->IsVisible())
			continue;

		if (!AppendTextVertices(*pText))
			break;
	}
}

bool TextRenderer::AppendTextVertices(
	const UIText& text)
{
	if (!m_pFontResource)
		return false;

	if (text.GetText().empty())
		return true;

	std::vector<TextLine> lines;
	BuildTextLines(text, lines);

	if (lines.empty())
		return true;

	float fLineSpacing = text.GetLineSpacing();

	if (fLineSpacing <= 0.0f)
	{
		fLineSpacing =
			m_pFontResource->GetLineHeight();
	}

	const XMFLOAT2& textPosition =
		text.GetPosition();

	const XMFLOAT4& textColor =
		text.GetColor();

	for (size_t lineIndex = 0;
		lineIndex < lines.size();
		++lineIndex)
	{
		const TextLine& line = lines[lineIndex];

		float fCursorX = textPosition.x;

		if (text.GetAlign() == UITextAlign::CENTER)
		{
			fCursorX -= line.width * 0.5f;
		}

		const float fLineTop =
			textPosition.y +
			(static_cast<float>(lineIndex) * fLineSpacing);

		for (uint32_t codepoint : line.codepoints)
		{
			const FontGlyph* pGlyph =
				m_pFontResource->FindGlyph(codepoint);

			if (!pGlyph)
			{
				pGlyph =
					m_pFontResource->GetFallbackGlyph();
			}

			if (!pGlyph)
				continue;

			if (pGlyph->width > 0.0f &&
				pGlyph->height > 0.0f)
			{
				if (m_TextVertices.size() + 6 >
					m_nVerticesPerFrame)
				{
					if (!m_bVertexOverflowLogged)
					{
						OutputDebugStringW(
							L"[TextRenderer] Vertex capacity exceeded. Remaining text was skipped.\n"
						);

						m_bVertexOverflowLogged = true;
					}

					return false;
				}

				const float fLeft =
					fCursorX + pGlyph->offsetX;

				const float fTop =
					fLineTop + pGlyph->offsetY;

				const float fRight =
					fLeft + pGlyph->width;

				const float fBottom =
					fTop + pGlyph->height;

				AppendGlyphQuad(
					fLeft,
					fTop,
					fRight,
					fBottom,
					pGlyph->u0,
					pGlyph->v0,
					pGlyph->u1,
					pGlyph->v1,
					textColor
				);
			}

			fCursorX += pGlyph->advanceX;
		}
	}

	return true;
}

void TextRenderer::BuildTextLines(
	const UIText& text,
	std::vector<TextLine>& outLines) const
{
	outLines.clear();

	if (!m_pFontResource)
		return;

	const std::wstring& string =
		text.GetText();

	if (string.empty())
		return;

	const float fMaxWidth =
		text.GetMaxWidth();

	TextLine currentLine;

	for (size_t i = 0; i < string.length(); ++i)
	{
		const wchar_t character = string[i];

		if (character == L'\r')
			continue;

		if (character == L'\n')
		{
			outLines.push_back(std::move(currentLine));
			currentLine = TextLine();
			continue;
		}

		if (character == L'\t')
		{
			const FontGlyph* pSpaceGlyph =
				m_pFontResource->FindGlyph(
					static_cast<uint32_t>(L' ')
				);

			const float fSpaceAdvance =
				pSpaceGlyph ?
				pSpaceGlyph->advanceX :
				0.0f;

			for (int tabIndex = 0;
				tabIndex < 4;
				++tabIndex)
			{
				if (fMaxWidth > 0.0f &&
					!currentLine.codepoints.empty() &&
					currentLine.width + fSpaceAdvance >
					fMaxWidth)
				{
					outLines.push_back(
						std::move(currentLine)
					);

					currentLine = TextLine();
				}

				currentLine.codepoints.push_back(
					static_cast<uint32_t>(L' ')
				);

				currentLine.width += fSpaceAdvance;
			}

			continue;
		}

		const uint32_t codepoint =
			static_cast<uint32_t>(character);

		const FontGlyph* pGlyph =
			m_pFontResource->FindGlyph(codepoint);

		if (!pGlyph)
		{
			pGlyph =
				m_pFontResource->GetFallbackGlyph();
		}

		if (!pGlyph)
			continue;

		const float fAdvance =
			pGlyph->advanceX;

		if (fMaxWidth > 0.0f &&
			!currentLine.codepoints.empty() &&
			currentLine.width + fAdvance >
			fMaxWidth)
		{
			outLines.push_back(
				std::move(currentLine)
			);

			currentLine = TextLine();
		}

		currentLine.codepoints.push_back(codepoint);
		currentLine.width += fAdvance;
	}

	outLines.push_back(std::move(currentLine));
}

XMFLOAT2 TextRenderer::ConvertPixelToNdc(
	float x,
	float y) const
{
	if (m_nScreenWidth == 0 ||
		m_nScreenHeight == 0)
	{
		return XMFLOAT2(0.0f, 0.0f);
	}

	const float ndcX =
		(x / static_cast<float>(m_nScreenWidth)) *
		2.0f -
		1.0f;

	const float ndcY =
		1.0f -
		(y / static_cast<float>(m_nScreenHeight)) *
		2.0f;

	return XMFLOAT2(ndcX, ndcY);
}

void TextRenderer::AppendGlyphQuad(
	float left,
	float top,
	float right,
	float bottom,
	float u0,
	float v0,
	float u1,
	float v1,
	const XMFLOAT4& color)
{
	const XMFLOAT2 topLeft =
		ConvertPixelToNdc(left, top);

	const XMFLOAT2 topRight =
		ConvertPixelToNdc(right, top);

	const XMFLOAT2 bottomLeft =
		ConvertPixelToNdc(left, bottom);

	const XMFLOAT2 bottomRight =
		ConvertPixelToNdc(right, bottom);

	m_TextVertices.push_back(
		{
			topLeft,
			XMFLOAT2(u0, v0),
			color
		}
	);

	m_TextVertices.push_back(
		{
			topRight,
			XMFLOAT2(u1, v0),
			color
		}
	);

	m_TextVertices.push_back(
		{
			bottomLeft,
			XMFLOAT2(u0, v1),
			color
		}
	);

	m_TextVertices.push_back(
		{
			bottomLeft,
			XMFLOAT2(u0, v1),
			color
		}
	);

	m_TextVertices.push_back(
		{
			topRight,
			XMFLOAT2(u1, v0),
			color
		}
	);

	m_TextVertices.push_back(
		{
			bottomRight,
			XMFLOAT2(u1, v1),
			color
		}
	);
}