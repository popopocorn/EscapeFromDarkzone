#pragma once
class CGameObject;

template<class T>
class State {

public:
	virtual ~State() = default;
	virtual bool Enter(T* Object) { return false; }
	virtual void Update(T* Object, float fTimeElapsed) {}
	virtual void Exit(T* Object) {}
};
