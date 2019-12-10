// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SpinMutex.h"

#include <condition_variable>

namespace QuantumGate::Implementation::Concurrency
{
	class EventCondition
	{
	public:
		EventCondition(const bool singlethread = false) noexcept : m_SingleThread(singlethread), m_State(false) {}
		EventCondition(const EventCondition&) = delete;
		EventCondition(EventCondition&&) = delete;
		virtual ~EventCondition() = default;
		EventCondition& operator=(const EventCondition&) = delete;
		EventCondition& operator=(EventCondition&&) = delete;

		void Set() noexcept
		{
			{
				std::lock_guard<SpinMutex> lock(m_Mutex);
				m_State = true;
			}

			if (m_SingleThread) m_Condition.notify_one();
			else m_Condition.notify_all();
		}

		inline void Reset() noexcept
		{
			std::lock_guard<SpinMutex> lock(m_Mutex);
			m_State = false;
		}

		[[nodiscard]] inline bool IsSet() const noexcept
		{
			std::lock_guard<SpinMutex> lock(m_Mutex);
			return m_State;
		}
		
		inline bool Wait(const std::chrono::milliseconds& ms)
		{
			std::unique_lock<SpinMutex> lock(m_Mutex);
			return m_Condition.wait_for(lock, ms, [&]() noexcept -> bool { return m_State; });
		}

		inline void Wait()
		{
			std::unique_lock<SpinMutex> lock(m_Mutex);
			m_Condition.wait(lock, [&]() noexcept -> bool { return m_State; });
		}

	private:
		const bool m_SingleThread{ false };
		bool m_State{ false };
		mutable SpinMutex m_Mutex;
		std::condition_variable_any m_Condition;
	};
}
