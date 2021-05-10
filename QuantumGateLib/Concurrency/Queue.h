// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Event.h"
#include "CriticalSection.h"
#include "ConditionVariable.h"
#include "..\Common\Containers.h"

namespace QuantumGate::Implementation::Concurrency
{
	template<typename T>
	class Queue final
	{
		using LockGuardType = std::lock_guard<CriticalSection>;

	public:
		using QueueType = Containers::Queue<T>;
	
		Queue() noexcept = default;
		Queue(const Queue&) = delete;
		Queue(Queue&&) noexcept = default;
		Queue& operator=(const Queue&) = delete;
		~Queue() = default;
		Queue& operator=(Queue&&) noexcept = default;

		[[nodiscard]] inline bool IsEmpty() const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_Queue.empty();
		}

		[[nodiscard]] inline Size GetSize() const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_Queue.size();
		}

		inline void Clear() noexcept
		{
			LockGuardType lock(m_CriticalSection);
			QueueType empty;
			m_Queue.swap(empty);
		}

		template<typename F> requires std::is_same_v<std::invoke_result_t<F, T&>, bool>
		inline void PopFrontIf(F&& function) noexcept(noexcept(function(std::declval<T&>())))
		{
			LockGuardType lock(m_CriticalSection);
			if (!m_Queue.empty())
			{
				if (function(m_Queue.front()))
				{
					m_Queue.pop();
				}
			}
		}

		inline void Push(const T& element)
		{
			{
				LockGuardType lock(m_CriticalSection);
				m_Queue.push(element);
			}

			m_WaitCondition.NotifyOne();
		}

		template<typename F>
		inline void Push(const T& element, F&& function)
		{
			{
				LockGuardType lock(m_CriticalSection);
				m_Queue.push(element);
				function();
			}

			m_WaitCondition.NotifyOne();
		}

		inline void Push(T&& element)
		{
			{
				LockGuardType lock(m_CriticalSection);
				m_Queue.push(std::move(element));
			}

			m_WaitCondition.NotifyOne();
		}

		template<typename F>
		inline void Push(T&& element, F&& function)
		{
			{
				LockGuardType lock(m_CriticalSection);
				m_Queue.push(std::move(element));
				function();
			}

			m_WaitCondition.NotifyOne();
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
				return (!m_Queue.empty() || interrupt_event.IsSet());
			});
		}

		inline bool Wait(const Event& interrupt_event) const noexcept
		{
			LockGuardType lock(m_CriticalSection);
			return m_WaitCondition.Wait(m_CriticalSection, [&]()
			{
				return (!m_Queue.empty() || interrupt_event.IsSet());
			});
		}

	private:
		QueueType m_Queue;
		mutable CriticalSection m_CriticalSection;
		mutable ConditionVariable m_WaitCondition;
	};
}

