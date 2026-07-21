#pragma once

#include <DirectXMath.h>

#include <string>

enum class UITextAlign
{
	LEFT,
	CENTER
};

class UIText
{
public:
	UIText() = default;

	UIText(
		const std::wstring& text,
		const DirectX::XMFLOAT2& position,
		UITextAlign align = UITextAlign::LEFT
	);

	~UIText() = default;

	UIText(const UIText&) = delete;
	UIText& operator=(const UIText&) = delete;

	UIText(UIText&&) noexcept = default;
	UIText& operator=(UIText&&) noexcept = default;

public:
	void SetText(const std::wstring& text);
	void SetText(std::wstring&& text);

	const std::wstring& GetText() const
	{
		return m_Text;
	}

	void SetPosition(float x, float y);
	void SetPosition(const DirectX::XMFLOAT2& position);

	const DirectX::XMFLOAT2& GetPosition() const
	{
		return m_xmf2Position;
	}

	void SetColor(float r, float g, float b, float a = 1.0f);
	void SetColor(const DirectX::XMFLOAT4& color);

	const DirectX::XMFLOAT4& GetColor() const
	{
		return m_xmf4Color;
	}

	void SetAlign(UITextAlign align);

	UITextAlign GetAlign() const
	{
		return m_eAlign;
	}

	void SetMaxWidth(float maxWidth);

	float GetMaxWidth() const
	{
		return m_fMaxWidth;
	}

	void SetLineSpacing(float lineSpacing);

	float GetLineSpacing() const
	{
		return m_fLineSpacing;
	}

	void SetVisible(bool visible);

	bool IsVisible() const
	{
		return m_bVisible;
	}

	bool IsDirty() const
	{
		return m_bDirty;
	}

	void MarkDirty()
	{
		m_bDirty = true;
	}

	void ClearDirty()
	{
		m_bDirty = false;
	}

private:
	std::wstring m_Text;

	// 화면 좌상단을 (0, 0)으로 사용하는 픽셀 좌표
	DirectX::XMFLOAT2 m_xmf2Position = DirectX::XMFLOAT2(0.0f, 0.0f);

	DirectX::XMFLOAT4 m_xmf4Color =
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	UITextAlign m_eAlign = UITextAlign::LEFT;

	// 0 이하이면 자동 줄바꿈을 사용하지 않음
	float m_fMaxWidth = 0.0f;

	// 0이면 FontResource의 기본 lineHeight 사용
	float m_fLineSpacing = 0.0f;

	bool m_bVisible = true;
	bool m_bDirty = true;
};