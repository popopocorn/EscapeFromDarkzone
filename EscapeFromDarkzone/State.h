#pragma once

#include <memory>
#include <typeinfo>

class CGameObject;

template<class T>
class State
{
public:
	virtual ~State() = default;

	virtual bool Enter(T* Object) { return false; }
	virtual void Update(T* Object, float fTimeElapsed) {}
	virtual void Exit(T* Object) {}
};


template<class T>
class StateMachine
{
public:
	void ChangeState(T* pOwner, std::unique_ptr<State<T>> pNewState)
	{
		if (!pOwner || !pNewState)
			return;

		if (m_pCurrentState && typeid(*m_pCurrentState) == typeid(*pNewState))
			return;

		if (m_pCurrentState)
			m_pCurrentState->Exit(pOwner);

		m_pCurrentState = std::move(pNewState);
		m_pCurrentState->Enter(pOwner);
	}

	void Update(T* pOwner, float fTimeElapsed)
	{
		if (!pOwner || !m_pCurrentState)
			return;

		m_pCurrentState->Update(pOwner, fTimeElapsed);
	}

	void Reset(T* pOwner)
	{
		if (m_pCurrentState && pOwner)
			m_pCurrentState->Exit(pOwner);

		m_pCurrentState.reset();
	}

	State<T>* GetCurrentState() const
	{
		return m_pCurrentState.get();
	}

	bool HasState() const
	{
		return m_pCurrentState != nullptr;
	}

	template<class TState>
	bool IsCurrentState() const
	{
		if (!m_pCurrentState)
			return false;

		return typeid(*m_pCurrentState) == typeid(TState);
	}

private:
	std::unique_ptr<State<T>> m_pCurrentState;
};