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
	bool KeyHold(INPUT_KEY key) { return GetState(key) == KEY_STATE::HOLD; }
	bool KeyRelease(INPUT_KEY key) { return GetState(key) == KEY_STATE::UP; }
	bool KeyPress(INPUT_KEY key) { return (GetState(key) == KEY_STATE::DOWN|| GetState(key) == KEY_STATE::HOLD); }
	POINT GetMousePos() { return cur_pos; }
private:
	InputManager() : m_hwnd(nullptr) {}

	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	KEY_STATE GetState(INPUT_KEY key);


	HWND m_hwnd;
	std::vector<KEY_STATE> key_states;
	POINT cur_pos;
};

/*
인풋 매니저 관리를 위해 싱글톤 기법 사용
static InputManager& Instance()를 통해 생성된 inputmanager를 static으로 선언해 
단일객체임을 보장, 코드에서 별도의 객체 생성 없이 Instance()함수 호출을 통해 단일 객체를 이용

싱글톤 관련 내용 공부 필요(면접용)
*/