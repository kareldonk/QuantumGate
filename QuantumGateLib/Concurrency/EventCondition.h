// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SpinMutex.h"

#include <condition_variable>

namespace QuantumGate::Implementation::Concurrency
{
	class EventCondition final
	{
	public:
		enum class NotifyOptions { One, All };

		EventCondition() noexcept {}
		EventCondition(const EventCondition&) = delete;
		EventCondition(EventCondition&&) = delete;
		~EventCondition() = default;
		EventCondition& operator=(const EventCondition&) = delete;
		EventCondition& operator=(EventCondition&&) = delete;

		void Set(const NotifyOptions notify_opt = NotifyOptions::All) noexcept
		{
			{
				std::lock_guard<SpinMutex> lock(m_Mutex);
				m_State = true;
			}

			if (notify_opt == NotifyOptions::All) m_Condition.notify_all();
			else m_Condition.notify_one();
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
		bool m_State{ false };
		mutable SpinMutex m_Mutex;
		std::condition_variable_any m_Condition;
	};

	// TODO: This can maybe be implemented in terms of std::atomic_flag with wait() in CPP20
}
