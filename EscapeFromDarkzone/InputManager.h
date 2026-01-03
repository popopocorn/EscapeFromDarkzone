#pragma once
#include"stdafx.h"

class InputManager
{
public:
	static InputManager& Instance() {
		static InputManager instance;
		return instance;
	}
	void init(HWND hwnd);
	void update();
	bool KeyDown(INPUT_KEY key) { return GetState(key) == KEY_STATE::DOWN; }
	bool KeyPress(INPUT_KEY key) { return GetState(key) == KEY_STATE::PRESS; }
	bool KeyRelease(INPUT_KEY key) { return GetState(key) == KEY_STATE::UP; }

private:
	InputManager() : m_hwnd(nullptr) {}

	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	KEY_STATE GetState(INPUT_KEY key);


	HWND m_hwnd;
	std::vector<KEY_STATE> key_states;
	POINT cur_pos;
};

