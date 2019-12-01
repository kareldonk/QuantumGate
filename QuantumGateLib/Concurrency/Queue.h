// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "EventCondition.h"
#include "..\Common\Containers.h"

namespace QuantumGate::Implementation::Concurrency
{
	template<typename T>
	class Queue final
	{
	public:
		using QueueType = Containers::Queue<T>;
	
		Queue() noexcept {}
		Queue(const Queue&) = delete;
		Queue(Queue&&) = default;
		Queue& operator=(const Queue&) = delete;
		~Queue() = default;
		Queue& operator=(Queue&&) = default;

		inline bool Empty() const noexcept
		{
			return m_Queue.empty();
		}

		inline Size GetSize() const noexcept
		{
			return m_Queue.size();
		}

		void Clear() noexcept
		{
			QueueType empty;
			m_Queue.swap(empty);

			m_Event.Reset();
		}

		inline T& Front()
		{
			return m_Queue.front();
		}

		inline const T& Front() const
		{
			return m_Queue.front();
		}

		template<typename F>
		inline void Push(const T& element, F&& function)
		{
			Push(element);
			function();
		}

		template<typename F>
		inline void Push(T&& element, F&& function)
		{
			Push(std::move(element));
			function();
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

		template<typename F>
		inline void Pop(F&& function) noexcept(noexcept(function()))
		{
			Pop();
			function();
		}

		inline void Pop() noexcept
		{
			m_Queue.pop();

			if (Empty()) m_Event.Reset();
		}

		inline EventCondition& Event() noexcept
		{
			return m_Event;
		}

		inline const EventCondition& Event() const noexcept
		{
			return m_Event;
		}

	private:
		QueueType m_Queue;
		EventCondition m_Event{ false };
	};
}

