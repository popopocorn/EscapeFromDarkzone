#include "InputManager.h"

void InputManager::init(HWND hwnd)
{
	m_hwnd = hwnd;
	key_states.resize(256, KEY_STATE::NONE);
}

void InputManager::update()
{

	BYTE asciiKeys[256] = {};
	if (::GetKeyboardState(asciiKeys) == false)
		return;
	for (UINT32 key = 0; key < 256; key++)
	{
		// 키가 눌려 있으면 true
		if (asciiKeys[key] & 0x80)
		{
			KEY_STATE& state = key_states[key];

			if (state == KEY_STATE::PRESS || state == KEY_STATE::DOWN)
				state = KEY_STATE::PRESS;
			else
				state = KEY_STATE::DOWN;
		}
		else
		{
			KEY_STATE& state = key_states[key];
			if (state == KEY_STATE::PRESS || state == KEY_STATE::DOWN)
				state = KEY_STATE::UP;
			else
				state = KEY_STATE::NONE;
		}
		/*if (key_states[key] != KEY_STATE::NONE) {
			wchar_t buffer[128];
			swprintf_s(buffer, L"Key: %u, State: %d\n", key, key_states[key]);
			OutputDebugStringW(buffer);
		}*/
	}
	
	::GetCursorPos(&cur_pos);
	::ScreenToClient(m_hwnd, &cur_pos);
}

KEY_STATE InputManager::GetState(INPUT_KEY key)
{
	size_t idx = static_cast<size_t>(key);
	if (idx >= key_states.size())
		return KEY_STATE::NONE;
	return key_states[idx];

}
