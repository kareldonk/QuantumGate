// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "EventCondition.h"

#include <queue>

namespace QuantumGate::Implementation::Concurrency
{
	template<typename T, typename F>
	class PriorityQueue
	{
	public:
		PriorityQueue() = delete;
		PriorityQueue(F&& function) noexcept(std::is_nothrow_constructible_v<T, F>) :
			m_Queue(std::forward<F>(function)) {}
		PriorityQueue(const PriorityQueue&) = delete;
		PriorityQueue(PriorityQueue&&) = default;
		virtual ~PriorityQueue() {}
		PriorityQueue& operator=(const PriorityQueue&) = delete;
		PriorityQueue& operator=(PriorityQueue&&) = default;

		inline const bool Empty() const noexcept
		{
			return m_Queue.empty();
		}

		void Clear() noexcept
		{
			std::priority_queue<T> empty;
			m_Queue.swap(empty);

			m_Event.Reset();
		}

		inline const T& Top() const
		{
			return m_Queue.top();
		}

		inline void Push(const T& element)
		{
			m_Queue.push(element);

			m_Event.Set();
		}

		inline void Push(T&& element)
		{
			m_Queue.push(std::move(element));

			m_Event.Set();
		}

		inline void Pop() noexcept
		{
			m_Queue.pop();

			if (Empty()) m_Event.Reset();
		}

		inline Concurrency::EventCondition& Event() noexcept
		{
			return m_Event;
		}

		inline const Concurrency::EventCondition& Event() const noexcept
		{
			return m_Event;
		}

	private:
		std::priority_queue<T, std::vector<T>, F> m_Queue;
		Concurrency::EventCondition m_Event{ false };
	};
}

