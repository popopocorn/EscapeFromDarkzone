#include "stdafx.h"
#include "FontResource.h"

#include "WICTextureLoader12.h"
#include "d3dx12.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

namespace
{
	bool ReadFileToString(const wchar_t* pFilePath, std::string& outText)
	{
		if (!pFilePath)
			return false;

		std::ifstream file(pFilePath, std::ios::binary);

		if (!file.is_open())
			return false;

		file.seekg(0, std::ios::end);
		std::streamoff fileSize = file.tellg();

		if (fileSize <= 0)
			return false;

		file.seekg(0, std::ios::beg);

		outText.resize(static_cast<size_t>(fileSize));
		file.read(outText.data(), fileSize);

		return file.good() || file.eof();
	}

	void SkipWhitespace(const std::string& text, size_t& position, size_t endPosition)
	{
		while (position < endPosition)
		{
			unsigned char ch = static_cast<unsigned char>(text[position]);

			if (!std::isspace(ch))
				break;

			++position;
		}
	}

	bool FindNumberValue(
		const std::string& text,
		size_t beginPosition,
		size_t endPosition,
		const char* pKey,
		double& outValue)
	{
		if (!pKey || beginPosition >= endPosition)
			return false;

		std::string token = "\"";
		token += pKey;
		token += "\":";

		size_t keyPosition = text.find(token, beginPosition);

		if (keyPosition == std::string::npos || keyPosition >= endPosition)
			return false;

		size_t valuePosition = keyPosition + token.length();
		SkipWhitespace(text, valuePosition, endPosition);

		if (valuePosition >= endPosition)
			return false;

		const char* pNumberStart = text.c_str() + valuePosition;
		char* pNumberEnd = nullptr;

		double parsedValue = std::strtod(pNumberStart, &pNumberEnd);

		if (pNumberEnd == pNumberStart)
			return false;

		size_t parsedEndPosition = static_cast<size_t>(pNumberEnd - text.c_str());

		if (parsedEndPosition > endPosition)
			return false;

		outValue = parsedValue;
		return true;
	}

	bool FindIntegerValue(
		const std::string& text,
		size_t beginPosition,
		size_t endPosition,
		const char* pKey,
		int& outValue)
	{
		double value = 0.0;

		if (!FindNumberValue(text, beginPosition, endPosition, pKey, value))
			return false;

		if (value < static_cast<double>(std::numeric_limits<int>::min()))
			return false;

		if (value > static_cast<double>(std::numeric_limits<int>::max()))
			return false;

		outValue = static_cast<int>(value);
		return true;
	}

	bool FindUnsignedIntegerValue(
		const std::string& text,
		size_t beginPosition,
		size_t endPosition,
		const char* pKey,
		uint32_t& outValue)
	{
		double value = 0.0;

		if (!FindNumberValue(text, beginPosition, endPosition, pKey, value))
			return false;

		if (value < 0.0)
			return false;

		if (value > static_cast<double>(std::numeric_limits<uint32_t>::max()))
			return false;

		outValue = static_cast<uint32_t>(value);
		return true;
	}

	bool FindFloatValue(
		const std::string& text,
		size_t beginPosition,
		size_t endPosition,
		const char* pKey,
		float& outValue)
	{
		double value = 0.0;

		if (!FindNumberValue(text, beginPosition, endPosition, pKey, value))
			return false;

		if (value < -static_cast<double>(std::numeric_limits<float>::max()))
			return false;

		if (value > static_cast<double>(std::numeric_limits<float>::max()))
			return false;

		outValue = static_cast<float>(value);
		return true;
	}

	size_t FindMatchingDelimiter(
		const std::string& text,
		size_t openPosition,
		char openDelimiter,
		char closeDelimiter)
	{
		if (openPosition >= text.size())
			return std::string::npos;

		if (text[openPosition] != openDelimiter)
			return std::string::npos;

		int depth = 0;
		bool insideString = false;
		bool escaped = false;

		for (size_t i = openPosition; i < text.size(); ++i)
		{
			char ch = text[i];

			if (insideString)
			{
				if (escaped)
				{
					escaped = false;
					continue;
				}

				if (ch == '\\')
				{
					escaped = true;
					continue;
				}

				if (ch == '"')
				{
					insideString = false;
				}

				continue;
			}

			if (ch == '"')
			{
				insideString = true;
				continue;
			}

			if (ch == openDelimiter)
			{
				++depth;
				continue;
			}

			if (ch == closeDelimiter)
			{
				--depth;

				if (depth == 0)
					return i;
			}
		}

		return std::string::npos;
	}

	void OutputFontLog(const wchar_t* pMessage)
	{
		if (!pMessage)
			return;

		OutputDebugStringW(pMessage);
	}
}

bool FontResource::Load(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	const wchar_t* pAtlasFilePath,
	const wchar_t* pMetadataFilePath)
{
	Unload();

	if (!pd3dDevice || !pd3dCommandList)
	{
		OutputFontLog(L"[FontResource] Load failed. Device or command list is null.\n");
		return false;
	}

	if (!pAtlasFilePath || !pMetadataFilePath)
	{
		OutputFontLog(L"[FontResource] Load failed. File path is null.\n");
		return false;
	}

	if (!LoadMetadata(pMetadataFilePath))
	{
		OutputFontLog(L"[FontResource] Metadata load failed.\n");
		Unload();
		return false;
	}

	if (!LoadAtlasTexture(pd3dDevice, pd3dCommandList, pAtlasFilePath))
	{
		OutputFontLog(L"[FontResource] Atlas texture load failed.\n");
		Unload();
		return false;
	}

	if (m_nAtlasWidth <= 0 || m_nAtlasHeight <= 0)
	{
		OutputFontLog(L"[FontResource] Invalid atlas size in metadata.\n");
		Unload();
		return false;
	}

	D3D12_RESOURCE_DESC textureDesc = m_pd3dAtlasTexture->GetDesc();

	if (static_cast<int>(textureDesc.Width) != m_nAtlasWidth ||
		static_cast<int>(textureDesc.Height) != m_nAtlasHeight)
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[FontResource] Atlas size mismatch. Metadata=(%d, %d), Texture=(%llu, %u)\n",
			m_nAtlasWidth,
			m_nAtlasHeight,
			textureDesc.Width,
			textureDesc.Height
		);

		OutputDebugStringW(debugText);
		Unload();
		return false;
	}

	const FontGlyph* pFallbackGlyph = GetFallbackGlyph();

	if (!pFallbackGlyph)
	{
		OutputFontLog(L"[FontResource] Warning: fallback glyph was not found.\n");
	}

	wchar_t debugText[256];

	swprintf_s(
		debugText,
		L"[FontResource] Load complete. Glyphs=%zu, Atlas=%dx%d, FontSize=%.1f\n",
		m_Glyphs.size(),
		m_nAtlasWidth,
		m_nAtlasHeight,
		m_fFontSize
	);

	OutputDebugStringW(debugText);

	return true;
}

void FontResource::Unload()
{
	m_pd3dAtlasUploadBuffer.Reset();
	m_pd3dAtlasTexture.Reset();

	m_d3dSrvGpuDescriptorHandle.ptr = 0;

	m_bTextureLoaded = false;
	m_bShaderResourceViewCreated = false;

	ResetMetadata();
}

void FontResource::ReleaseUploadBuffer()
{
	m_pd3dAtlasUploadBuffer.Reset();
}

bool FontResource::CreateShaderResourceView(
	ID3D12Device* pd3dDevice,
	D3D12_CPU_DESCRIPTOR_HANDLE d3dCpuDescriptorHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE d3dGpuDescriptorHandle)
{
	if (!pd3dDevice)
	{
		OutputFontLog(L"[FontResource] SRV creation failed. Device is null.\n");
		return false;
	}

	if (!m_pd3dAtlasTexture)
	{
		OutputFontLog(L"[FontResource] SRV creation failed. Atlas texture is null.\n");
		return false;
	}

	if (d3dCpuDescriptorHandle.ptr == 0 || d3dGpuDescriptorHandle.ptr == 0)
	{
		OutputFontLog(L"[FontResource] SRV creation failed. Descriptor handle is invalid.\n");
		return false;
	}

	D3D12_RESOURCE_DESC textureDesc = m_pd3dAtlasTexture->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	pd3dDevice->CreateShaderResourceView(
		m_pd3dAtlasTexture.Get(),
		&srvDesc,
		d3dCpuDescriptorHandle
	);

	m_d3dSrvGpuDescriptorHandle = d3dGpuDescriptorHandle;
	m_bShaderResourceViewCreated = true;

	OutputFontLog(L"[FontResource] Atlas SRV created.\n");

	return true;
}

void FontResource::UpdateShaderVariable(
	ID3D12GraphicsCommandList* pd3dCommandList,
	UINT nRootParameterIndex) const
{
	if (!pd3dCommandList)
		return;

	if (!m_bShaderResourceViewCreated)
		return;

	if (m_d3dSrvGpuDescriptorHandle.ptr == 0)
		return;

	pd3dCommandList->SetGraphicsRootDescriptorTable(
		nRootParameterIndex,
		m_d3dSrvGpuDescriptorHandle
	);
}

const FontGlyph* FontResource::FindGlyph(uint32_t codepoint) const
{
	auto it = m_Glyphs.find(codepoint);

	if (it == m_Glyphs.end())
		return nullptr;

	return &it->second;
}

const FontGlyph* FontResource::GetFallbackGlyph() const
{
	const FontGlyph* pFallbackGlyph = FindGlyph(m_nFallbackCodepoint);

	if (pFallbackGlyph)
		return pFallbackGlyph;

	pFallbackGlyph = FindGlyph(static_cast<uint32_t>(L'?'));

	if (pFallbackGlyph)
		return pFallbackGlyph;

	return nullptr;
}

bool FontResource::IsLoaded() const
{
	return m_bMetadataLoaded && m_bTextureLoaded;
}

bool FontResource::HasShaderResourceView() const
{
	return m_bShaderResourceViewCreated;
}

bool FontResource::LoadMetadata(const wchar_t* pMetadataFilePath)
{
	std::string jsonText;

	if (!ReadFileToString(pMetadataFilePath, jsonText))
	{
		OutputFontLog(L"[FontResource] Failed to open metadata JSON file.\n");
		return false;
	}

	size_t glyphKeyPosition = jsonText.find("\"glyphs\":");

	if (glyphKeyPosition == std::string::npos)
	{
		OutputFontLog(L"[FontResource] Metadata does not contain glyphs array.\n");
		return false;
	}

	size_t glyphArrayOpenPosition = jsonText.find('[', glyphKeyPosition);

	if (glyphArrayOpenPosition == std::string::npos)
	{
		OutputFontLog(L"[FontResource] Invalid glyphs array.\n");
		return false;
	}

	size_t glyphArrayClosePosition = FindMatchingDelimiter(
		jsonText,
		glyphArrayOpenPosition,
		'[',
		']'
	);

	if (glyphArrayClosePosition == std::string::npos)
	{
		OutputFontLog(L"[FontResource] Glyphs array closing bracket was not found.\n");
		return false;
	}

	size_t metadataEndPosition = glyphKeyPosition;

	if (!FindIntegerValue(jsonText, 0, metadataEndPosition, "atlasWidth", m_nAtlasWidth))
	{
		OutputFontLog(L"[FontResource] atlasWidth was not found.\n");
		return false;
	}

	if (!FindIntegerValue(jsonText, 0, metadataEndPosition, "atlasHeight", m_nAtlasHeight))
	{
		OutputFontLog(L"[FontResource] atlasHeight was not found.\n");
		return false;
	}

	if (!FindFloatValue(jsonText, 0, metadataEndPosition, "fontSize", m_fFontSize))
	{
		OutputFontLog(L"[FontResource] fontSize was not found.\n");
		return false;
	}

	if (!FindFloatValue(jsonText, 0, metadataEndPosition, "lineHeight", m_fLineHeight))
	{
		OutputFontLog(L"[FontResource] lineHeight was not found.\n");
		return false;
	}

	if (!FindFloatValue(jsonText, 0, metadataEndPosition, "ascent", m_fAscent))
	{
		OutputFontLog(L"[FontResource] ascent was not found.\n");
		return false;
	}

	if (!FindFloatValue(jsonText, 0, metadataEndPosition, "descent", m_fDescent))
	{
		OutputFontLog(L"[FontResource] descent was not found.\n");
		return false;
	}

	int declaredGlyphCount = 0;

	if (!FindIntegerValue(jsonText, 0, metadataEndPosition, "glyphCount", declaredGlyphCount))
	{
		OutputFontLog(L"[FontResource] glyphCount was not found.\n");
		return false;
	}

	if (declaredGlyphCount <= 0)
	{
		OutputFontLog(L"[FontResource] glyphCount is invalid.\n");
		return false;
	}

	m_Glyphs.clear();
	m_Glyphs.reserve(static_cast<size_t>(declaredGlyphCount));

	size_t currentPosition = glyphArrayOpenPosition + 1;

	while (currentPosition < glyphArrayClosePosition)
	{
		SkipWhitespace(jsonText, currentPosition, glyphArrayClosePosition);

		if (currentPosition >= glyphArrayClosePosition)
			break;

		if (jsonText[currentPosition] == ',')
		{
			++currentPosition;
			continue;
		}

		if (jsonText[currentPosition] != '{')
		{
			++currentPosition;
			continue;
		}

		size_t glyphObjectClosePosition = FindMatchingDelimiter(
			jsonText,
			currentPosition,
			'{',
			'}'
		);

		if (glyphObjectClosePosition == std::string::npos ||
			glyphObjectClosePosition > glyphArrayClosePosition)
		{
			OutputFontLog(L"[FontResource] Invalid glyph object.\n");
			return false;
		}

		size_t glyphObjectEndPosition = glyphObjectClosePosition + 1;

		FontGlyph glyph;

		if (!FindUnsignedIntegerValue(
			jsonText,
			currentPosition,
			glyphObjectEndPosition,
			"codepoint",
			glyph.codepoint))
		{
			OutputFontLog(L"[FontResource] Glyph codepoint parse failed.\n");
			return false;
		}

		if (!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "x", glyph.x) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "y", glyph.y) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "width", glyph.width) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "height", glyph.height) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "offsetX", glyph.offsetX) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "offsetY", glyph.offsetY) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "advanceX", glyph.advanceX) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "u0", glyph.u0) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "v0", glyph.v0) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "u1", glyph.u1) ||
			!FindFloatValue(jsonText, currentPosition, glyphObjectEndPosition, "v1", glyph.v1))
		{
			OutputFontLog(L"[FontResource] Glyph metric parse failed.\n");
			return false;
		}

		if (glyph.width < 0.0f || glyph.height < 0.0f || glyph.advanceX < 0.0f)
		{
			OutputFontLog(L"[FontResource] Glyph contains an invalid size.\n");
			return false;
		}

		if (glyph.u0 < 0.0f || glyph.v0 < 0.0f ||
			glyph.u1 > 1.0f || glyph.v1 > 1.0f ||
			glyph.u1 < glyph.u0 || glyph.v1 < glyph.v0)
		{
			OutputFontLog(L"[FontResource] Glyph contains an invalid UV coordinate.\n");
			return false;
		}

		auto insertResult = m_Glyphs.emplace(glyph.codepoint, glyph);

		if (!insertResult.second)
		{
			wchar_t debugText[128];

			swprintf_s(
				debugText,
				L"[FontResource] Duplicate glyph codepoint: U+%04X\n",
				glyph.codepoint
			);

			OutputDebugStringW(debugText);
		}

		currentPosition = glyphObjectEndPosition;
	}

	if (m_Glyphs.empty())
	{
		OutputFontLog(L"[FontResource] No glyphs were loaded.\n");
		return false;
	}

	if (m_Glyphs.size() != static_cast<size_t>(declaredGlyphCount))
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[FontResource] Glyph count mismatch. Declared=%d, Loaded=%zu\n",
			declaredGlyphCount,
			m_Glyphs.size()
		);

		OutputDebugStringW(debugText);
	}

	m_bMetadataLoaded = true;

	return true;
}

bool FontResource::LoadAtlasTexture(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	const wchar_t* pAtlasFilePath)
{
	if (!pd3dDevice || !pd3dCommandList || !pAtlasFilePath)
		return false;

	ID3D12Resource* pAtlasTexture = nullptr;

	std::unique_ptr<uint8_t[]> decodedData;
	D3D12_SUBRESOURCE_DATA subresourceData{};

	HRESULT result = DirectX::LoadWICTextureFromFileEx(
		pd3dDevice,
		pAtlasFilePath,
		0,
		D3D12_RESOURCE_FLAG_NONE,
		DirectX::WIC_LOADER_IGNORE_SRGB,
		&pAtlasTexture,
		decodedData,
		subresourceData
	);

	if (FAILED(result) || !pAtlasTexture)
	{
		wchar_t debugText[512];

		swprintf_s(
			debugText,
			L"[FontResource] WIC texture load failed. HRESULT=0x%08X, File=%s\n",
			static_cast<unsigned int>(result),
			pAtlasFilePath
		);

		OutputDebugStringW(debugText);

		if (pAtlasTexture)
			pAtlasTexture->Release();

		return false;
	}

	m_pd3dAtlasTexture.Attach(pAtlasTexture);

	UINT64 uploadBufferSize = GetRequiredIntermediateSize(
		m_pd3dAtlasTexture.Get(),
		0,
		1
	);

	if (uploadBufferSize == 0)
	{
		OutputFontLog(L"[FontResource] Invalid upload buffer size.\n");
		m_pd3dAtlasTexture.Reset();
		return false;
	}

	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProperties.CreationNodeMask = 1;
	uploadHeapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC uploadBufferDesc{};
	uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadBufferDesc.Alignment = 0;
	uploadBufferDesc.Width = uploadBufferSize;
	uploadBufferDesc.Height = 1;
	uploadBufferDesc.DepthOrArraySize = 1;
	uploadBufferDesc.MipLevels = 1;
	uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadBufferDesc.SampleDesc.Count = 1;
	uploadBufferDesc.SampleDesc.Quality = 0;
	uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	result = pd3dDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_pd3dAtlasUploadBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result) || !m_pd3dAtlasUploadBuffer)
	{
		wchar_t debugText[256];

		swprintf_s(
			debugText,
			L"[FontResource] Upload buffer creation failed. HRESULT=0x%08X\n",
			static_cast<unsigned int>(result)
		);

		OutputDebugStringW(debugText);

		m_pd3dAtlasTexture.Reset();
		return false;
	}

	UINT64 copiedSize = UpdateSubresources(
		pd3dCommandList,
		m_pd3dAtlasTexture.Get(),
		m_pd3dAtlasUploadBuffer.Get(),
		0,
		0,
		1,
		&subresourceData
	);

	if (copiedSize == 0)
	{
		OutputFontLog(L"[FontResource] UpdateSubresources failed.\n");

		m_pd3dAtlasUploadBuffer.Reset();
		m_pd3dAtlasTexture.Reset();

		return false;
	}

	D3D12_RESOURCE_BARRIER resourceBarrier{};
	resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarrier.Transition.pResource = m_pd3dAtlasTexture.Get();
	resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	pd3dCommandList->ResourceBarrier(1, &resourceBarrier);

	m_pd3dAtlasTexture->SetName(L"Korean Font Atlas Texture");
	m_pd3dAtlasUploadBuffer->SetName(L"Korean Font Atlas Upload Buffer");

	m_bTextureLoaded = true;

	return true;
}

void FontResource::ResetMetadata()
{
	m_Glyphs.clear();

	m_nAtlasWidth = 0;
	m_nAtlasHeight = 0;

	m_fFontSize = 0.0f;
	m_fLineHeight = 0.0f;
	m_fAscent = 0.0f;
	m_fDescent = 0.0f;

	m_nFallbackCodepoint = 0xFFFD;

	m_bMetadataLoaded = false;
}