#include "InputManager.h"

void InputManager::init(HWND hwnd)
{
	m_hwnd = hwnd;
	key_states.resize(256, KEY_STATE::NONE);
}

void InputManager::update()
{

	if (::GetFocus() != m_hwnd)
	{
		Reset();
		return;
	}

	
	for (UINT32 key = 0; key < 256; key++)
	{
	
		if (::GetAsyncKeyState(key) & 0x8000)
		{
			KEY_STATE& state = key_states[key];

			if (state == KEY_STATE::HOLD || state == KEY_STATE::DOWN)
				state = KEY_STATE::HOLD;
			else
				state = KEY_STATE::DOWN;
		}
		else
		{
			KEY_STATE& state = key_states[key];
			if (state == KEY_STATE::HOLD || state == KEY_STATE::DOWN)
				state = KEY_STATE::UP;
			else
				state = KEY_STATE::NONE;
		}
		/*
		if (key_states[key] != KEY_STATE::NONE) {
			wchar_t buffer[128];
			swprintf_s(buffer, L"Key: %u, State: %d\n", key, key_states[key]);
			OutputDebugStringW(buffer);
		}//*/
	}


	::GetCursorPos(&cur_pos);
	::ScreenToClient(m_hwnd, &cur_pos);
}

void InputManager::Reset()
{
	// 포커스 밖에서 놓친 키가 복귀 후 눌린 상태로 남지 않도록 초기화한다.
	std::fill(key_states.begin(), key_states.end(), KEY_STATE::NONE);
}

KEY_STATE InputManager::GetState(INPUT_KEY key)
{
	size_t idx = static_cast<size_t>(key);
	if (idx >= key_states.size())
		return KEY_STATE::NONE;
	return key_states[idx];

}
