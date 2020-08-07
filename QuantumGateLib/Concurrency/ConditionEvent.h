// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Event.h"
#include "CriticalSection.h"
#include "ConditionVariable.h"

namespace QuantumGate::Implementation::Concurrency
{
	class ConditionEvent final
	{
		using LockGuardType = std::lock_guard<CriticalSection>;

	public:
		ConditionEvent() noexcept = default;
		ConditionEvent(const ConditionEvent&) = delete;
		ConditionEvent(ConditionEvent&&) = delete;
		~ConditionEvent() = default;
		ConditionEvent& operator=(const ConditionEvent&) = delete;
		ConditionEvent& operator=(ConditionEvent&&) = delete;

		inline void Set() noexcept
		{
			{
				LockGuardType lock(m_CriticalSection);
				m_Flag = true;
			}

			m_WaitCondition.NotifyAll();
		}

		inline void Reset() noexcept
		{
			LockGuardType lock(m_CriticalSection);
			m_Flag = false;
		}

		[[nodiscard]] inline bool IsSet() const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_Flag;
		}

		inline void InterruptWait() const noexcept
		{
			m_WaitCondition.NotifyAll();
		}

		inline bool Wait(const std::chrono::milliseconds time, const Event& interrupt_event) const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_WaitCondition.Wait(m_CriticalSection, time, [&]()
			{
				return (m_Flag || interrupt_event.IsSet());
			});
		}

		inline bool Wait(const Event& interrupt_event) const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_WaitCondition.Wait(m_CriticalSection, [&]()
			{
				return (m_Flag || interrupt_event.IsSet());
			});
		}

	private:
		bool m_Flag{ false };
		mutable CriticalSection m_CriticalSection;
		mutable ConditionVariable m_WaitCondition;
	};
}
