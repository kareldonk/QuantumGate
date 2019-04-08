// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "DummyMutex.h"

#include <atomic>
#include <condition_variable>

namespace QuantumGate::Implementation::Concurrency
{
	class EventCondition
	{
	public:
		EventCondition(bool singlethread = false) noexcept : m_SingleThread(singlethread), m_State(false) {}
		EventCondition(const EventCondition&) = delete;
		EventCondition(EventCondition&&) = delete;
		virtual ~EventCondition() = default;
		EventCondition& operator=(const EventCondition&) = delete;
		EventCondition& operator=(EventCondition&&) = delete;

		void Set() noexcept
		{
			m_State = true;

			if (m_SingleThread) m_Condition.notify_one();
			else m_Condition.notify_all();
		}

		inline void Reset() noexcept { m_State = false; }
		inline bool IsSet() const noexcept { return m_State; }
		
		bool Wait(const std::chrono::milliseconds& ms)
		{
			DummyMutex mtx;
			return m_Condition.wait_for(mtx, ms, [&]() noexcept -> bool { return m_State; });
		}

		void Wait()
		{
			DummyMutex mtx;
			m_Condition.wait(mtx, [&]() noexcept -> bool { return m_State; });
		}

	private:
		const bool m_SingleThread{ false };
		std::atomic_bool m_State{ false };
		std::condition_variable_any m_Condition;
	};
}
