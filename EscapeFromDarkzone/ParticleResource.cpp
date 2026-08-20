#include "stdafx.h"
#include "Object.h"
#include "ParticleResource.h"
#include "Particle.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

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

	size_t FindMatchingDelimiter(const std::string& text, size_t openPosition, char openDelimiter, char closeDelimiter)
	{
		if (openPosition >= text.size() || text[openPosition] != openDelimiter)
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
					insideString = false;

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

	bool FindValueStart(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, size_t& outValuePosition)
	{
		if (!pKey || beginPosition >= endPosition)
			return false;

		std::string token = "\"";
		token += pKey;
		token += "\"";

		size_t keyPosition = text.find(token, beginPosition);

		if (keyPosition == std::string::npos || keyPosition >= endPosition)
			return false;

		size_t colonPosition = text.find(':', keyPosition + token.length());

		if (colonPosition == std::string::npos || colonPosition >= endPosition)
			return false;

		outValuePosition = colonPosition + 1;
		SkipWhitespace(text, outValuePosition, endPosition);

		return outValuePosition < endPosition;
	}

	bool ParseJsonStringAt(const std::string& text, size_t valuePosition, size_t endPosition, std::string& outValue)
	{
		if (valuePosition >= endPosition || text[valuePosition] != '"')
			return false;

		outValue.clear();

		for (size_t i = valuePosition + 1; i < endPosition; ++i)
		{
			char ch = text[i];

			if (ch == '"')
				return true;

			if (ch == '\\')
			{
				if (i + 1 >= endPosition)
					return false;

				char escaped = text[++i];

				switch (escaped)
				{
				case '"': outValue.push_back('"'); break;
				case '\\': outValue.push_back('\\'); break;
				case '/': outValue.push_back('/'); break;
				case 'n': outValue.push_back('\n'); break;
				case 'r': outValue.push_back('\r'); break;
				case 't': outValue.push_back('\t'); break;
				default: return false;
				}

				continue;
			}

			outValue.push_back(ch);
		}

		return false;
	}

	bool FindStringValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, std::string& outValue)
	{
		size_t valuePosition = 0;

		if (!FindValueStart(text, beginPosition, endPosition, pKey, valuePosition))
			return false;

		return ParseJsonStringAt(text, valuePosition, endPosition, outValue);
	}

	bool FindNumberValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, double& outValue)
	{
		size_t valuePosition = 0;

		if (!FindValueStart(text, beginPosition, endPosition, pKey, valuePosition))
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

	bool FindFloatValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, float& outValue)
	{
		double value = 0.0;

		if (!FindNumberValue(text, beginPosition, endPosition, pKey, value))
			return false;

		if (value < -static_cast<double>(std::numeric_limits<float>::max()) ||
			value > static_cast<double>(std::numeric_limits<float>::max()))
		{
			return false;
		}

		outValue = static_cast<float>(value);
		return true;
	}

	bool FindUIntValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, UINT& outValue)
	{
		double value = 0.0;

		if (!FindNumberValue(text, beginPosition, endPosition, pKey, value))
			return false;

		if (value < 0.0 || value > static_cast<double>(std::numeric_limits<UINT>::max()))
			return false;

		if (std::floor(value) != value)
			return false;

		outValue = static_cast<UINT>(value);
		return true;
	}

	bool FindArrayRange(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, size_t& outOpenPosition, size_t& outClosePosition)
	{
		size_t valuePosition = 0;

		if (!FindValueStart(text, beginPosition, endPosition, pKey, valuePosition))
			return false;

		if (text[valuePosition] != '[')
			return false;

		size_t closePosition = FindMatchingDelimiter(text, valuePosition, '[', ']');

		if (closePosition == std::string::npos || closePosition >= endPosition)
			return false;

		outOpenPosition = valuePosition;
		outClosePosition = closePosition;
		return true;
	}

	bool FindObjectRange(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, size_t& outOpenPosition, size_t& outClosePosition)
	{
		size_t valuePosition = 0;

		if (!FindValueStart(text, beginPosition, endPosition, pKey, valuePosition))
			return false;

		if (text[valuePosition] != '{')
			return false;

		size_t closePosition = FindMatchingDelimiter(text, valuePosition, '{', '}');

		if (closePosition == std::string::npos || closePosition >= endPosition)
			return false;

		outOpenPosition = valuePosition;
		outClosePosition = closePosition;
		return true;
	}

	bool ParseNumberArray(const std::string& text, size_t openPosition, size_t closePosition, std::vector<double>& outValues)
	{
		if (openPosition >= closePosition || text[openPosition] != '[' || text[closePosition] != ']')
			return false;

		outValues.clear();
		size_t currentPosition = openPosition + 1;

		while (currentPosition < closePosition)
		{
			SkipWhitespace(text, currentPosition, closePosition);

			if (currentPosition >= closePosition)
				break;

			if (text[currentPosition] == ',')
			{
				++currentPosition;
				continue;
			}

			const char* pNumberStart = text.c_str() + currentPosition;
			char* pNumberEnd = nullptr;
			double value = std::strtod(pNumberStart, &pNumberEnd);

			if (pNumberEnd == pNumberStart)
				return false;

			size_t parsedEndPosition = static_cast<size_t>(pNumberEnd - text.c_str());

			if (parsedEndPosition > closePosition)
				return false;

			outValues.push_back(value);
			currentPosition = parsedEndPosition;
		}

		return true;
	}

	bool FindFloatArrayValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, size_t expectedCount, float* pOutValues)
	{
		if (!pOutValues || expectedCount == 0)
			return false;

		size_t openPosition = 0;
		size_t closePosition = 0;

		if (!FindArrayRange(text, beginPosition, endPosition, pKey, openPosition, closePosition))
			return false;

		std::vector<double> values;

		if (!ParseNumberArray(text, openPosition, closePosition, values) || values.size() != expectedCount)
			return false;

		for (size_t i = 0; i < expectedCount; ++i)
		{
			if (values[i] < -static_cast<double>(std::numeric_limits<float>::max()) ||
				values[i] > static_cast<double>(std::numeric_limits<float>::max()))
			{
				return false;
			}

			pOutValues[i] = static_cast<float>(values[i]);
		}

		return true;
	}

	bool FindUIntArrayValue(const std::string& text, size_t beginPosition, size_t endPosition, const char* pKey, std::vector<UINT>& outValues)
	{
		size_t openPosition = 0;
		size_t closePosition = 0;

		if (!FindArrayRange(text, beginPosition, endPosition, pKey, openPosition, closePosition))
			return false;

		std::vector<double> values;

		if (!ParseNumberArray(text, openPosition, closePosition, values))
			return false;

		outValues.clear();
		outValues.reserve(values.size());

		for (double value : values)
		{
			if (value < 0.0 || value > static_cast<double>(std::numeric_limits<UINT>::max()))
				return false;

			if (std::floor(value) != value)
				return false;

			outValues.push_back(static_cast<UINT>(value));
		}

		return true;
	}

	bool ConvertUtf8ToWide(const std::string& utf8Text, std::wstring& outWideText)
	{
		outWideText.clear();

		if (utf8Text.empty())
			return false;

		int requiredLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Text.c_str(), static_cast<int>(utf8Text.size()), nullptr, 0);

		if (requiredLength <= 0)
			return false;

		outWideText.resize(static_cast<size_t>(requiredLength));

		int convertedLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Text.c_str(), static_cast<int>(utf8Text.size()), outWideText.data(), requiredLength);
		return convertedLength == requiredLength;
	}

	bool ParseParticleTextureID(const std::string& value, ParticleTextureID& outValue)
	{
		if (value == "EXPLOSION") outValue = ParticleTextureID::EXPLOSION;
		else if (value == "SPARK_RIFLE_SMG") outValue = ParticleTextureID::SPARK_RIFLE_SMG;
		else if (value == "SPARK_SHOTGUN") outValue = ParticleTextureID::SPARK_SHOTGUN;
		else return false;

		return true;
	}

	bool ParseParticleBlendMode(const std::string& value, ParticleBlendMode& outValue)
	{
		if (value == "ALPHA") outValue = ParticleBlendMode::ALPHA;
		else if (value == "ADDITIVE") outValue = ParticleBlendMode::ADDITIVE;
		else return false;

		return true;
	}

	bool ParseParticleBillboardMode(const std::string& value, ParticleBillboardMode& outValue)
	{
		if (value == "CAMERA_FACING") outValue = ParticleBillboardMode::CAMERA_FACING;
		else if (value == "VELOCITY_ALIGNED") outValue = ParticleBillboardMode::VELOCITY_ALIGNED;
		else return false;

		return true;
	}

	bool ParseParticleFrameMode(const std::string& value, ParticleFrameMode& outValue)
	{
		if (value == "FIXED_FRAME") outValue = ParticleFrameMode::FIXED_FRAME;
		else if (value == "SEQUENTIAL") outValue = ParticleFrameMode::SEQUENTIAL;
		else if (value == "RANDOM_SELECTED") outValue = ParticleFrameMode::RANDOM_SELECTED;
		else return false;

		return true;
	}

	bool ParseParticleDirectionMode(const std::string& value, ParticleDirectionMode& outValue)
	{
		if (value == "CONFIGURED") outValue = ParticleDirectionMode::CONFIGURED;
		else if (value == "EFFECT_DIRECTION") outValue = ParticleDirectionMode::EFFECT_DIRECTION;
		else return false;

		return true;
	}

	bool ParseParticleRenderGroup(const std::string& value, UINT& outValue)
	{
		if (value == "EXPLOSION_ALPHA") outValue = static_cast<UINT>(ParticleRenderGroup::EXPLOSION_ALPHA);
		else if (value == "EXPLOSION_ADDITIVE") outValue = static_cast<UINT>(ParticleRenderGroup::EXPLOSION_ADDITIVE);
		else if (value == "SHOTGUN_ADDITIVE") outValue = static_cast<UINT>(ParticleRenderGroup::SHOTGUN_ADDITIVE);
		else if (value == "RIFLE_ADDITIVE") outValue = static_cast<UINT>(ParticleRenderGroup::RIFLE_ADDITIVE);
		else if (value == "RIFLE_ALPHA") outValue = static_cast<UINT>(ParticleRenderGroup::RIFLE_ALPHA);
		else return false;

		return true;
	}

	bool ParseEffectID(const std::string& value, EffectID& outValue)
	{
		if (value == "GRENADE_EXPLOSION") outValue = EffectID::GRENADE_EXPLOSION;
		else if (value == "SPARK") outValue = EffectID::SPARK;
		else if (value == "SPARK_SHOTGUN") outValue = EffectID::SPARK_SHOTGUN;
		else if (value == "SPARK_PISTOL") outValue = EffectID::SPARK_PISTOL;
		else return false;

		return true;
	}

	bool ParseAtlasDesc(const std::string& text, size_t beginPosition, size_t endPosition, ParticleAtlasDesc& outDesc)
	{
		if (!FindUIntValue(text, beginPosition, endPosition, "textureWidth", outDesc.textureWidth) ||
			!FindUIntValue(text, beginPosition, endPosition, "textureHeight", outDesc.textureHeight) ||
			!FindUIntValue(text, beginPosition, endPosition, "columns", outDesc.columns) ||
			!FindUIntValue(text, beginPosition, endPosition, "rows", outDesc.rows) ||
			!FindUIntValue(text, beginPosition, endPosition, "frameWidth", outDesc.frameWidth) ||
			!FindUIntValue(text, beginPosition, endPosition, "frameHeight", outDesc.frameHeight) ||
			!FindUIntValue(text, beginPosition, endPosition, "borderX", outDesc.borderX) ||
			!FindUIntValue(text, beginPosition, endPosition, "borderY", outDesc.borderY) ||
			!FindUIntValue(text, beginPosition, endPosition, "spacingX", outDesc.spacingX) ||
			!FindUIntValue(text, beginPosition, endPosition, "spacingY", outDesc.spacingY) ||
			!FindUIntValue(text, beginPosition, endPosition, "validFrameCount", outDesc.validFrameCount))
		{
			return false;
		}

		if (outDesc.textureWidth == 0 || outDesc.textureHeight == 0 ||
			outDesc.columns == 0 || outDesc.rows == 0 ||
			outDesc.frameWidth == 0 || outDesc.frameHeight == 0 ||
			outDesc.validFrameCount == 0)
		{
			return false;
		}

		UINT64 requiredWidth = static_cast<UINT64>(outDesc.borderX) * 2ull +
			static_cast<UINT64>(outDesc.columns) * outDesc.frameWidth +
			static_cast<UINT64>(outDesc.columns - 1) * outDesc.spacingX;

		UINT64 requiredHeight = static_cast<UINT64>(outDesc.borderY) * 2ull +
			static_cast<UINT64>(outDesc.rows) * outDesc.frameHeight +
			static_cast<UINT64>(outDesc.rows - 1) * outDesc.spacingY;

		UINT64 totalFrameCapacity = static_cast<UINT64>(outDesc.columns) * outDesc.rows;

		if (requiredWidth > outDesc.textureWidth || requiredHeight > outDesc.textureHeight)
			return false;

		if (outDesc.validFrameCount > totalFrameCapacity)
			return false;

		return true;
	}

	bool ParseEmitterDesc(const std::string& text, size_t beginPosition, size_t endPosition, ParticleEmitterDesc& outDesc)
	{
		std::string renderGroupText;
		std::string directionModeText;
		std::string billboardModeText;
		std::string frameModeText;

		if (!FindStringValue(text, beginPosition, endPosition, "renderGroup", renderGroupText) ||
			!FindStringValue(text, beginPosition, endPosition, "directionMode", directionModeText) ||
			!FindStringValue(text, beginPosition, endPosition, "billboardMode", billboardModeText) ||
			!FindStringValue(text, beginPosition, endPosition, "frameMode", frameModeText))
		{
			return false;
		}

		if (!ParseParticleRenderGroup(renderGroupText, outDesc.renderGroup) ||
			!ParseParticleDirectionMode(directionModeText, outDesc.directionMode) ||
			!ParseParticleBillboardMode(billboardModeText, outDesc.billboardMode) ||
			!ParseParticleFrameMode(frameModeText, outDesc.frameMode))
		{
			return false;
		}

		if (!FindUIntValue(text, beginPosition, endPosition, "burstCount", outDesc.burstCount) ||
			!FindFloatValue(text, beginPosition, endPosition, "spawnDelayMin", outDesc.spawnDelayMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "spawnDelayMax", outDesc.spawnDelayMax) ||
			!FindFloatValue(text, beginPosition, endPosition, "lifeTimeMin", outDesc.lifeTimeMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "lifeTimeMax", outDesc.lifeTimeMax) ||
			!FindFloatValue(text, beginPosition, endPosition, "speedMin", outDesc.speedMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "speedMax", outDesc.speedMax) ||
			!FindFloatValue(text, beginPosition, endPosition, "coneAngleDegrees", outDesc.coneAngleDegrees) ||
			!FindFloatValue(text, beginPosition, endPosition, "positionOffsetAlongDirection", outDesc.positionOffsetAlongDirection) ||
			!FindFloatValue(text, beginPosition, endPosition, "sizeScaleMin", outDesc.sizeScaleMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "sizeScaleMax", outDesc.sizeScaleMax) ||
			!FindFloatValue(text, beginPosition, endPosition, "rotationMin", outDesc.rotationMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "rotationMax", outDesc.rotationMax) ||
			!FindFloatValue(text, beginPosition, endPosition, "angularVelocityMin", outDesc.angularVelocityMin) ||
			!FindFloatValue(text, beginPosition, endPosition, "angularVelocityMax", outDesc.angularVelocityMax) ||
			!FindUIntValue(text, beginPosition, endPosition, "firstFrame", outDesc.firstFrame) ||
			!FindUIntValue(text, beginPosition, endPosition, "frameCount", outDesc.frameCount) ||
			!FindUIntValue(text, beginPosition, endPosition, "loopAnimation", outDesc.loopAnimation))
		{
			return false;
		}

		float direction[3] = {};
		float positionOffset[3] = {};
		float acceleration[3] = {};
		float startSize[2] = {};
		float endSize[2] = {};
		float startColor[4] = {};
		float endColor[4] = {};

		if (!FindFloatArrayValue(text, beginPosition, endPosition, "direction", 3, direction) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "positionOffset", 3, positionOffset) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "acceleration", 3, acceleration) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "startSize", 2, startSize) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "endSize", 2, endSize) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "startColor", 4, startColor) ||
			!FindFloatArrayValue(text, beginPosition, endPosition, "endColor", 4, endColor))
		{
			return false;
		}

		outDesc.direction = XMFLOAT3(direction[0], direction[1], direction[2]);
		outDesc.positionOffset = XMFLOAT3(positionOffset[0], positionOffset[1], positionOffset[2]);
		outDesc.acceleration = XMFLOAT3(acceleration[0], acceleration[1], acceleration[2]);
		outDesc.startSize = XMFLOAT2(startSize[0], startSize[1]);
		outDesc.endSize = XMFLOAT2(endSize[0], endSize[1]);
		outDesc.startColor = XMFLOAT4(startColor[0], startColor[1], startColor[2], startColor[3]);
		outDesc.endColor = XMFLOAT4(endColor[0], endColor[1], endColor[2], endColor[3]);

		std::vector<UINT> selectedFrames;

		if (!FindUIntArrayValue(text, beginPosition, endPosition, "selectedFrames", selectedFrames))
			return false;

		if (selectedFrames.size() > PARTICLE_SELECTED_FRAME_CAPACITY)
			return false;

		outDesc.selectedFrameCount = static_cast<UINT>(selectedFrames.size());

		for (UINT i = 0; i < outDesc.selectedFrameCount; ++i)
		{
			outDesc.selectedFrames[i] = selectedFrames[i];
		}

		return true;
	}

}

ParticleResource::~ParticleResource()
{
	Release();
}

bool ParticleResource::Load(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pConfigFilePath)
{
	if (!pd3dDevice || !pd3dCommandList || !pConfigFilePath)
	{
		OutputDebugStringW(L"[ParticleResource] Load failed. Device, command list, or config path is null.\n");
		return false;
	}

	if (m_bLoaded)
	{
		OutputDebugStringW(L"[ParticleResource] Resources are already loaded.\n");
		return true;
	}

	Release();

	if (!LoadConfig(pd3dDevice, pd3dCommandList, pConfigFilePath))
	{
		OutputDebugStringW(L"[ParticleResource] Particle JSON config load failed.\n");
		Release();
		return false;
	}

	if (m_Textures.size() != static_cast<size_t>(ParticleTextureID::COUNT) ||
		m_RenderGroupDescs.size() != static_cast<size_t>(ParticleRenderGroup::COUNT) ||
		m_EffectDescs.empty())
	{
		OutputDebugStringW(L"[ParticleResource] Particle JSON config is incomplete.\n");
		Release();
		return false;
	}

	m_bLoaded = true;

	wchar_t debugText[256];
	swprintf_s(debugText, L"[ParticleResource] Particle JSON loaded. Textures=%zu, RenderGroups=%zu, Effects=%zu\n",
		m_Textures.size(), m_RenderGroupDescs.size(), m_EffectDescs.size());
	OutputDebugStringW(debugText);

	return true;
}

bool ParticleResource::LoadConfig(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pConfigFilePath)
{
	std::string jsonText;

	if (!ReadFileToString(pConfigFilePath, jsonText))
	{
		OutputDebugStringW(L"[ParticleResource] Failed to open particle JSON config file.\n");
		return false;
	}

	size_t texturesOpenPosition = 0;
	size_t texturesClosePosition = 0;

	if (!FindArrayRange(jsonText, 0, jsonText.size(), "textures", texturesOpenPosition, texturesClosePosition))
	{
		OutputDebugStringW(L"[ParticleResource] JSON does not contain a valid textures array.\n");
		return false;
	}

	size_t currentPosition = texturesOpenPosition + 1;

	while (currentPosition < texturesClosePosition)
	{
		SkipWhitespace(jsonText, currentPosition, texturesClosePosition);

		if (currentPosition >= texturesClosePosition)
			break;

		if (jsonText[currentPosition] == ',')
		{
			++currentPosition;
			continue;
		}

		if (jsonText[currentPosition] != '{')
		{
			OutputDebugStringW(L"[ParticleResource] Invalid texture object in JSON.\n");
			return false;
		}

		size_t objectClosePosition = FindMatchingDelimiter(jsonText, currentPosition, '{', '}');

		if (objectClosePosition == std::string::npos || objectClosePosition > texturesClosePosition)
		{
			OutputDebugStringW(L"[ParticleResource] Texture object closing bracket was not found.\n");
			return false;
		}

		size_t objectEndPosition = objectClosePosition + 1;
		std::string textureIdText;
		std::string texturePathText;

		if (!FindStringValue(jsonText, currentPosition, objectEndPosition, "id", textureIdText) ||
			!FindStringValue(jsonText, currentPosition, objectEndPosition, "path", texturePathText))
		{
			OutputDebugStringW(L"[ParticleResource] Texture id or path parse failed.\n");
			return false;
		}

		ParticleTextureID textureId = ParticleTextureID::COUNT;

		if (!ParseParticleTextureID(textureIdText, textureId))
		{
			OutputDebugStringW(L"[ParticleResource] Unknown texture id in JSON.\n");
			return false;
		}

		if (m_Textures.find(textureId) != m_Textures.end())
		{
			OutputDebugStringW(L"[ParticleResource] Duplicate texture id in JSON.\n");
			return false;
		}

		size_t atlasOpenPosition = 0;
		size_t atlasClosePosition = 0;

		if (!FindObjectRange(jsonText, currentPosition, objectEndPosition, "atlas", atlasOpenPosition, atlasClosePosition))
		{
			OutputDebugStringW(L"[ParticleResource] Texture atlas object parse failed.\n");
			return false;
		}

		ParticleAtlasDesc atlasDesc;

		if (!ParseAtlasDesc(jsonText, atlasOpenPosition, atlasClosePosition + 1, atlasDesc))
		{
			OutputDebugStringW(L"[ParticleResource] Texture atlas data is invalid.\n");
			return false;
		}

		std::wstring texturePath;

		if (!ConvertUtf8ToWide(texturePathText, texturePath))
		{
			OutputDebugStringW(L"[ParticleResource] Texture path UTF-8 conversion failed.\n");
			return false;
		}

		if (!LoadTexture(textureId, pd3dDevice, pd3dCommandList, texturePath.c_str(), atlasDesc))
			return false;

		currentPosition = objectEndPosition;
	}


	size_t renderGroupsOpenPosition = 0;
	size_t renderGroupsClosePosition = 0;

	if (!FindArrayRange(jsonText, 0, jsonText.size(), "renderGroups", renderGroupsOpenPosition, renderGroupsClosePosition))
	{
		OutputDebugStringW(L"[ParticleResource] JSON does not contain a valid renderGroups array.\n");
		return false;
	}

	currentPosition = renderGroupsOpenPosition + 1;

	while (currentPosition < renderGroupsClosePosition)
	{
		SkipWhitespace(jsonText, currentPosition, renderGroupsClosePosition);

		if (currentPosition >= renderGroupsClosePosition)
			break;

		if (jsonText[currentPosition] == ',')
		{
			++currentPosition;
			continue;
		}

		if (jsonText[currentPosition] != '{')
		{
			OutputDebugStringW(L"[ParticleResource] Invalid render group object in JSON.\n");
			return false;
		}

		size_t objectClosePosition = FindMatchingDelimiter(jsonText, currentPosition, '{', '}');

		if (objectClosePosition == std::string::npos || objectClosePosition > renderGroupsClosePosition)
		{
			OutputDebugStringW(L"[ParticleResource] Render group object closing bracket was not found.\n");
			return false;
		}

		size_t objectEndPosition = objectClosePosition + 1;
		std::string renderGroupText;
		std::string textureIdText;
		std::string blendModeText;

		if (!FindStringValue(jsonText, currentPosition, objectEndPosition, "id", renderGroupText) ||
			!FindStringValue(jsonText, currentPosition, objectEndPosition, "textureId", textureIdText) ||
			!FindStringValue(jsonText, currentPosition, objectEndPosition, "blendMode", blendModeText))
		{
			OutputDebugStringW(L"[ParticleResource] Render group data parse failed.\n");
			return false;
		}

		UINT renderGroupIndex = 0;
		ParticleRenderGroupDesc renderGroupDesc;

		if (!ParseParticleRenderGroup(renderGroupText, renderGroupIndex) ||
			!ParseParticleTextureID(textureIdText, renderGroupDesc.textureId) ||
			!ParseParticleBlendMode(blendModeText, renderGroupDesc.blendMode))
		{
			OutputDebugStringW(L"[ParticleResource] Unknown render group configuration in JSON.\n");
			return false;
		}

		if (renderGroupIndex >= static_cast<UINT>(ParticleRenderGroup::COUNT))
		{
			OutputDebugStringW(L"[ParticleResource] Render group index is out of range.\n");
			return false;
		}

		if (m_RenderGroupDescs.find(renderGroupIndex) != m_RenderGroupDescs.end())
		{
			OutputDebugStringW(L"[ParticleResource] Duplicate render group id in JSON.\n");
			return false;
		}

		if (m_Textures.find(renderGroupDesc.textureId) == m_Textures.end())
		{
			OutputDebugStringW(L"[ParticleResource] Render group references an unloaded texture.\n");
			return false;
		}

		m_RenderGroupDescs[renderGroupIndex] = renderGroupDesc;
		currentPosition = objectEndPosition;
	}

	size_t effectsOpenPosition = 0;
	size_t effectsClosePosition = 0;

	if (!FindArrayRange(jsonText, 0, jsonText.size(), "effects", effectsOpenPosition, effectsClosePosition))
	{
		OutputDebugStringW(L"[ParticleResource] JSON does not contain a valid effects array.\n");
		return false;
	}

	currentPosition = effectsOpenPosition + 1;

	while (currentPosition < effectsClosePosition)
	{
		SkipWhitespace(jsonText, currentPosition, effectsClosePosition);

		if (currentPosition >= effectsClosePosition)
			break;

		if (jsonText[currentPosition] == ',')
		{
			++currentPosition;
			continue;
		}

		if (jsonText[currentPosition] != '{')
		{
			OutputDebugStringW(L"[ParticleResource] Invalid effect object in JSON.\n");
			return false;
		}

		size_t effectObjectClosePosition = FindMatchingDelimiter(jsonText, currentPosition, '{', '}');

		if (effectObjectClosePosition == std::string::npos || effectObjectClosePosition > effectsClosePosition)
		{
			OutputDebugStringW(L"[ParticleResource] Effect object closing bracket was not found.\n");
			return false;
		}

		size_t effectObjectEndPosition = effectObjectClosePosition + 1;
		std::string effectIdText;

		if (!FindStringValue(jsonText, currentPosition, effectObjectEndPosition, "id", effectIdText))
		{
			OutputDebugStringW(L"[ParticleResource] Effect id parse failed.\n");
			return false;
		}

		EffectID effectId = EffectID::NONE;

		if (!ParseEffectID(effectIdText, effectId))
		{
			OutputDebugStringW(L"[ParticleResource] Unknown effect id in JSON.\n");
			return false;
		}

		if (m_EffectDescs.find(effectId) != m_EffectDescs.end())
		{
			OutputDebugStringW(L"[ParticleResource] Duplicate effect id in JSON.\n");
			return false;
		}

		size_t emittersOpenPosition = 0;
		size_t emittersClosePosition = 0;

		if (!FindArrayRange(jsonText, currentPosition, effectObjectEndPosition, "emitters", emittersOpenPosition, emittersClosePosition))
		{
			OutputDebugStringW(L"[ParticleResource] Effect emitters array parse failed.\n");
			return false;
		}

		ParticleEffectDesc effectDesc;
		effectDesc.id = effectId;

		size_t emitterPosition = emittersOpenPosition + 1;

		while (emitterPosition < emittersClosePosition)
		{
			SkipWhitespace(jsonText, emitterPosition, emittersClosePosition);

			if (emitterPosition >= emittersClosePosition)
				break;

			if (jsonText[emitterPosition] == ',')
			{
				++emitterPosition;
				continue;
			}

			if (jsonText[emitterPosition] != '{')
			{
				OutputDebugStringW(L"[ParticleResource] Invalid emitter object in JSON.\n");
				return false;
			}

			size_t emitterClosePosition = FindMatchingDelimiter(jsonText, emitterPosition, '{', '}');

			if (emitterClosePosition == std::string::npos || emitterClosePosition > emittersClosePosition)
			{
				OutputDebugStringW(L"[ParticleResource] Emitter object closing bracket was not found.\n");
				return false;
			}

			ParticleEmitterDesc emitterDesc;

			if (!ParseEmitterDesc(jsonText, emitterPosition, emitterClosePosition + 1, emitterDesc))
			{
				OutputDebugStringW(L"[ParticleResource] Emitter data parse failed.\n");
				return false;
			}

			auto renderGroupIt = m_RenderGroupDescs.find(emitterDesc.renderGroup);

			if (renderGroupIt == m_RenderGroupDescs.end())
			{
				OutputDebugStringW(L"[ParticleResource] Emitter references an unknown render group.\n");
				return false;
			}

			emitterDesc.textureId = renderGroupIt->second.textureId;
			emitterDesc.blendMode = renderGroupIt->second.blendMode;

			if (!ValidateEmitterDesc(emitterDesc))
			{
				OutputDebugStringW(L"[ParticleResource] Emitter data validation failed.\n");
				return false;
			}

			effectDesc.emitters.push_back(emitterDesc);
			emitterPosition = emitterClosePosition + 1;
		}

		if (effectDesc.emitters.empty())
		{
			OutputDebugStringW(L"[ParticleResource] Effect does not contain any emitters.\n");
			return false;
		}

		m_EffectDescs[effectDesc.id] = std::move(effectDesc);
		currentPosition = effectObjectEndPosition;
	}

	return true;
}

bool ParticleResource::LoadTexture(ParticleTextureID textureId, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const wchar_t* texturePath, const ParticleAtlasDesc& atlasDesc)
{
	if (!pd3dDevice || !pd3dCommandList || !texturePath)
	{
		OutputDebugStringW(L"[ParticleResource] Texture load failed. Invalid argument.\n");
		return false;
	}

	auto textureIt = m_Textures.find(textureId);

	if (textureIt != m_Textures.end() && textureIt->second)
	{
		return true;
	}

	auto pTexture = std::make_unique<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
	pTexture->LoadTextureFromDDSFile(pd3dDevice, pd3dCommandList, texturePath, RESOURCE_TEXTURE2D, 0);

	if (!pTexture->GetResource(0))
	{
		OutputDebugStringW(L"[ParticleResource] DDS resource creation failed.\n");
		return false;
	}

	D3D12_RESOURCE_DESC textureDesc = pTexture->GetResource(0)->GetDesc();

	if (textureDesc.Width != atlasDesc.textureWidth || textureDesc.Height != atlasDesc.textureHeight)
	{
		wchar_t debugText[256];
		swprintf_s(debugText, L"[ParticleResource] Texture size mismatch. JSON=%ux%u, DDS=%llux%u\n",
			atlasDesc.textureWidth, atlasDesc.textureHeight, textureDesc.Width, textureDesc.Height);
		OutputDebugStringW(debugText);
		return false;
	}

	m_AtlasDescs[textureId] = atlasDesc;
	m_Textures[textureId] = std::move(pTexture);

	wchar_t debugText[256];
	swprintf_s(debugText, L"[ParticleResource] Texture loaded. ID=%u, Size=%ux%u, Atlas=%ux%u, Frames=%u\n",
		static_cast<UINT>(textureId), atlasDesc.textureWidth, atlasDesc.textureHeight,
		atlasDesc.columns, atlasDesc.rows, atlasDesc.validFrameCount);
	OutputDebugStringW(debugText);

	return true;
}

bool ParticleResource::ValidateEmitterDesc(const ParticleEmitterDesc& emitterDesc) const
{
	auto renderGroupIt = m_RenderGroupDescs.find(emitterDesc.renderGroup);

	if (renderGroupIt == m_RenderGroupDescs.end())
		return false;

	if (renderGroupIt->second.textureId != emitterDesc.textureId ||
		renderGroupIt->second.blendMode != emitterDesc.blendMode)
	{
		return false;
	}

	auto atlasIt = m_AtlasDescs.find(emitterDesc.textureId);

	if (atlasIt == m_AtlasDescs.end())
		return false;

	const ParticleAtlasDesc& atlasDesc = atlasIt->second;

	if (emitterDesc.directionMode != ParticleDirectionMode::CONFIGURED &&
		emitterDesc.directionMode != ParticleDirectionMode::EFFECT_DIRECTION)
	{
		return false;
	}

	if (emitterDesc.spawnDelayMin < 0.0f || emitterDesc.spawnDelayMax < emitterDesc.spawnDelayMin)
		return false;

	if (emitterDesc.lifeTimeMin <= 0.0f || emitterDesc.lifeTimeMax < emitterDesc.lifeTimeMin)
		return false;

	if (emitterDesc.speedMin < 0.0f || emitterDesc.speedMax < emitterDesc.speedMin)
		return false;

	if (emitterDesc.coneAngleDegrees < 0.0f || emitterDesc.coneAngleDegrees > 180.0f)
		return false;

	if (emitterDesc.sizeScaleMin <= 0.0f || emitterDesc.sizeScaleMax < emitterDesc.sizeScaleMin)
		return false;

	if (emitterDesc.rotationMax < emitterDesc.rotationMin)
		return false;

	if (emitterDesc.angularVelocityMax < emitterDesc.angularVelocityMin)
		return false;

	if (emitterDesc.startSize.x < 0.0f || emitterDesc.startSize.y < 0.0f ||
		emitterDesc.endSize.x < 0.0f || emitterDesc.endSize.y < 0.0f)
	{
		return false;
	}

	if (emitterDesc.firstFrame >= atlasDesc.validFrameCount)
		return false;

	switch (emitterDesc.frameMode)
	{
	case ParticleFrameMode::FIXED_FRAME:
		if (emitterDesc.frameCount == 0)
			return false;
		break;

	case ParticleFrameMode::SEQUENTIAL:
		if (emitterDesc.frameCount == 0 || emitterDesc.frameCount > atlasDesc.validFrameCount - emitterDesc.firstFrame)
			return false;
		break;

	case ParticleFrameMode::RANDOM_SELECTED:
		if (emitterDesc.selectedFrameCount == 0 || emitterDesc.selectedFrameCount > PARTICLE_SELECTED_FRAME_CAPACITY)
			return false;

		for (UINT i = 0; i < emitterDesc.selectedFrameCount; ++i)
		{
			if (emitterDesc.selectedFrames[i] >= atlasDesc.validFrameCount)
				return false;
		}
		break;

	default:
		return false;
	}

	return true;
}

void ParticleResource::ReleaseUploadBuffers()
{
	for (auto& texturePair : m_Textures)
	{
		if (texturePair.second)
		{
			texturePair.second->ReleaseUploadBuffers();
		}
	}
}

void ParticleResource::Release()
{
	ReleaseUploadBuffers();

	m_EffectDescs.clear();
	m_RenderGroupDescs.clear();
	m_AtlasDescs.clear();
	m_Textures.clear();

	m_bLoaded = false;
}

const ParticleEffectDesc* ParticleResource::GetEffectDesc(EffectID effectId) const
{
	auto it = m_EffectDescs.find(effectId);

	if (it == m_EffectDescs.end())
	{
		return nullptr;
	}

	return &it->second;
}

CTexture* ParticleResource::GetTexture(ParticleTextureID textureId) const
{
	auto it = m_Textures.find(textureId);

	if (it == m_Textures.end())
	{
		return nullptr;
	}

	return it->second.get();
}

const ParticleAtlasDesc* ParticleResource::GetAtlasDesc(ParticleTextureID textureId) const
{
	auto it = m_AtlasDescs.find(textureId);

	if (it == m_AtlasDescs.end())
	{
		return nullptr;
	}

	return &it->second;
}

const ParticleRenderGroupDesc* ParticleResource::GetRenderGroupDesc(UINT renderGroupIndex) const
{
	auto it = m_RenderGroupDescs.find(renderGroupIndex);

	if (it == m_RenderGroupDescs.end())
	{
		return nullptr;
	}

	return &it->second;
}