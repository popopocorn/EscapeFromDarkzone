#include "stdafx.h"
#include "UIText.h"

#include <utility>

UIText::UIText(
	const std::wstring& text,
	const DirectX::XMFLOAT2& position,
	UITextAlign align)
	: m_Text(text),
	m_xmf2Position(position),
	m_eAlign(align)
{
	m_bDirty = true;
}

void UIText::SetText(const std::wstring& text)
{
	if (m_Text == text)
		return;

	m_Text = text;
	m_bDirty = true;
}

void UIText::SetText(std::wstring&& text)
{
	if (m_Text == text)
		return;

	m_Text = std::move(text);
	m_bDirty = true;
}

void UIText::SetPosition(float x, float y)
{
	if (m_xmf2Position.x == x && m_xmf2Position.y == y)
		return;

	m_xmf2Position.x = x;
	m_xmf2Position.y = y;

	m_bDirty = true;
}

void UIText::SetPosition(const DirectX::XMFLOAT2& position)
{
	SetPosition(position.x, position.y);
}

void UIText::SetColor(float r, float g, float b, float a)
{
	if (m_xmf4Color.x == r &&
		m_xmf4Color.y == g &&
		m_xmf4Color.z == b &&
		m_xmf4Color.w == a)
	{
		return;
	}

	m_xmf4Color = DirectX::XMFLOAT4(r, g, b, a);
	m_bDirty = true;
}

void UIText::SetColor(const DirectX::XMFLOAT4& color)
{
	SetColor(color.x, color.y, color.z, color.w);
}

void UIText::SetAlign(UITextAlign align)
{
	if (m_eAlign == align)
		return;

	m_eAlign = align;
	m_bDirty = true;
}

void UIText::SetMaxWidth(float maxWidth)
{
	if (maxWidth < 0.0f)
		maxWidth = 0.0f;

	if (m_fMaxWidth == maxWidth)
		return;

	m_fMaxWidth = maxWidth;
	m_bDirty = true;
}

void UIText::SetLineSpacing(float lineSpacing)
{
	if (lineSpacing < 0.0f)
		lineSpacing = 0.0f;

	if (m_fLineSpacing == lineSpacing)
		return;

	m_fLineSpacing = lineSpacing;
	m_bDirty = true;
}

void UIText::SetVisible(bool visible)
{
	if (m_bVisible == visible)
		return;

	m_bVisible = visible;
	m_bDirty = true;
}