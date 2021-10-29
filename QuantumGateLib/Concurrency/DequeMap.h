// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Event.h"
#include "CriticalSection.h"
#include "ConditionVariable.h"
#include "..\Common\Containers.h"
#include "..\Common\ScopeGuard.h"

namespace QuantumGate::Implementation::Concurrency
{
	template<typename K, typename T>
	class DequeMap final
	{
		using LockGuardType = std::lock_guard<CriticalSection>;
		using DequeType = Containers::Deque<T>;
		using MapType = Containers::Map<K, DequeType>;

		template<typename DM>
		class Locked final
		{
			friend DequeMap<K, T>;

		public:
			Locked() noexcept {}

		private:
			Locked(DM* dm) noexcept : m_DequeMap(dm)
			{
				m_DequeMap->m_CriticalSection.lock();
			}

		public:
			Locked(const Locked&) = delete;

			Locked(Locked&& other) noexcept :
				m_DequeMap(other.m_DequeMap), m_Notify(other.m_Notify)
			{
				other.m_DequeMap = nullptr;
				other.m_Notify = false;
			}

			~Locked()
			{
				Release();
			}

			Locked& operator=(const Locked&) = delete;

			Locked& operator=(Locked&& other) noexcept
			{
				m_DequeMap = std::exchange(other.m_DequeMap, nullptr);
				m_Notify = std::exchange(other.m_Notify, false);

				return *this;
			}

			inline explicit operator bool() const noexcept
			{
				return (m_DequeMap != nullptr);
			}

			inline void Release() noexcept
			{
				if (m_DequeMap != nullptr)
				{
					m_DequeMap->m_CriticalSection.unlock();
					
					if (m_Notify) m_DequeMap->m_WaitCondition.NotifyOne();
					
					m_DequeMap = nullptr;
					m_Notify = false;
				}
			}

			[[nodiscard]] inline bool HasElements() const noexcept
			{
				assert(m_DequeMap != nullptr);
				for (auto& deque : m_DequeMap->m_Map)
				{
					if (!deque.second.empty()) return false;
				}
				return true;
			}

			[[nodiscard]] inline Size GetKeyCount() const noexcept
			{
				assert(m_DequeMap != nullptr);
				return m_DequeMap->m_Map.size();
			}

			[[nodiscard]] inline Size GetElementCount() const noexcept
			{
				assert(m_DequeMap != nullptr);
				Size size{ 0 };
				for (auto& deque : m_DequeMap->m_Map)
				{
					size += deque.size();
				}
				return size;
			}

			inline void Clear() noexcept requires (!std::is_const_v<DM>)
			{
				assert(m_DequeMap != nullptr);
				m_DequeMap->m_Map.clear();
			}

			inline void Insert(const K& key) noexcept requires (!std::is_const_v<DM>)
			{
				assert(m_DequeMap != nullptr);
				m_DequeMap->m_Map.insert({ key, DequeType{} });
			}

			inline Size Erase(const K& key) noexcept requires (!std::is_const_v<DM>)
			{
				assert(m_DequeMap != nullptr);
				return m_DequeMap->m_Map.erase(key);
			}

			template<typename F> requires (std::is_same_v<std::invoke_result_t<F, T&>, bool> && !std::is_const_v<DM>)
			inline void PopFrontIf(F&& function) noexcept(noexcept(function(std::declval<T&>())))
			{
				assert(m_DequeMap != nullptr);
				if (m_DequeMap->m_NextDeque >= m_DequeMap->m_Map.size()) m_DequeMap->m_NextDeque = 0;

				const auto begin = std::next(m_DequeMap->m_Map.begin(), m_DequeMap->m_NextDeque);

				for (auto it = begin; it != m_DequeMap->m_Map.end(); ++it)
				{
					++m_DequeMap->m_NextDeque;

					if (!it->second.empty())
					{
						if (function(it->second.front()))
						{
							it->second.pop_front();
						}

						return;
					}
				}
			}

			inline void PushBack(const K& key, const T& element) requires (!std::is_const_v<DM>)
			{
				Push<false>(key, element);
			}

			template<typename F>
			inline void PushBack(const K& key, const T& element, F&& function) requires (!std::is_const_v<DM>)
			{
				Push<false>(key, element, std::forward<F>(function));
			}

			inline void PushBack(const K& key, T&& element) requires (!std::is_const_v<DM>)
			{
				Push<false>(key, std::move(element));
			}

			template<typename F>
			inline void PushBack(const K& key, T&& element, F&& function) requires (!std::is_const_v<DM>)
			{
				Push<false>(key, std::move(element), std::forward<F>(function));
			}

			inline void PushFront(const K& key, const T& element) requires (!std::is_const_v<DM>)
			{
				Push<true>(key, element);
			}

			template<typename F>
			inline void PushFront(const K& key, const T& element, F&& function) requires (!std::is_const_v<DM>)
			{
				Push<true>(key, element, std::forward<F>(function));
			}

			inline void PushFront(const K& key, T&& element) requires (!std::is_const_v<DM>)
			{
				Push<true>(key, std::move(element));
			}

			template<typename F>
			inline void PushFront(const K& key, T&& element, F&& function) requires (!std::is_const_v<DM>)
			{
				Push<true>(key, std::move(element), std::forward<F>(function));
			}

			inline bool Wait(const std::chrono::milliseconds time, const Event& interrupt_event) const noexcept
			{
				return m_DequeMap->m_WaitCondition.Wait(m_DequeMap->m_CriticalSection, time, [&]()
				{
					return (!HasElements() || interrupt_event.IsSet());
				});
			}

			inline bool Wait(const Event& interrupt_event) const noexcept
			{
				return m_DequeMap->m_WaitCondition.Wait(m_DequeMap->m_CriticalSection, [&]()
				{
					return (!HasElements() || interrupt_event.IsSet());
				});
			}

		private:
			template<bool front>
			inline void Push(const K& key, const T& element)
			{
				assert(m_DequeMap != nullptr);
				if (auto it = m_DequeMap->m_Map.find(key); it != m_DequeMap->m_Map.end())
				{
					if constexpr (front) it->second.push_front(element);
					else it->second.push_back(element);
					m_Notify = true;
				}
				else
				{
					throw std::invalid_argument("The key does not exist.");
				}
			}

			template<bool front, typename F>
			inline void Push(const K& key, const T& element, F&& function)
			{
				assert(m_DequeMap != nullptr);
				if (auto it = m_DequeMap->m_Map.find(key); it != m_DequeMap->m_Map.end())
				{
					if constexpr (front)
					{
						it->second.push_front(element);
						auto sg = MakeScopeGuard([&] { it->second.pop_front(); });
						function();
						m_Notify = true;
						sg.Deactivate();
					}
					else
					{
						it->second.push_back(element);
						auto sg = MakeScopeGuard([&] { it->second.pop_back(); });
						function();
						m_Notify = true;
						sg.Deactivate();
					}
				}
				else
				{
					throw std::invalid_argument("The key does not exist.");
				}
			}

			template<bool front>
			inline void Push(const K& key, T&& element)
			{
				assert(m_DequeMap != nullptr);
				if (auto it = m_DequeMap->m_Map.find(key); it != m_DequeMap->m_Map.end())
				{
					if constexpr (front) it->second.push_front(std::move(element));
					else it->second.push_back(std::move(element));
					m_Notify = true;
				}
				else
				{
					throw std::invalid_argument("The key does not exist.");
				}
			}

			template<bool front, typename F>
			inline void Push(const K& key, T&& element, F&& function)
			{
				assert(m_DequeMap != nullptr);
				if (auto it = m_DequeMap->m_Map.find(key); it != m_DequeMap->m_Map.end())
				{
					if constexpr (front)
					{
						it->second.push_front(std::move(element));
						auto sg = MakeScopeGuard([&] { element = std::move(it->second.front()); it->second.pop_front(); });
						function();
						m_Notify = true;
						sg.Deactivate();
					}
					else
					{
						it->second.push_back(std::move(element));
						auto sg = MakeScopeGuard([&] { element = std::move(it->second.back()); it->second.pop_back(); });
						function();
						m_Notify = true;
						sg.Deactivate();
					}
				}
				else
				{
					throw std::invalid_argument("The key does not exist.");
				}
			}

		private:
			DM* m_DequeMap{ nullptr };
			bool m_Notify{ false };
		};

		friend Locked<DequeMap>;
		friend Locked<const DequeMap>;

	public:
		using LockedType = Locked<DequeMap>;
		using LockedConstType = Locked<const DequeMap>;

		DequeMap() noexcept = default;
		DequeMap(const DequeMap&) = delete;
		DequeMap(DequeMap&&) noexcept = default;
		~DequeMap() = default;
		DequeMap& operator=(const DequeMap&) = delete;
		DequeMap& operator=(DequeMap&&) noexcept = default;

		[[nodiscard]] inline LockedType GetLocked() noexcept
		{
			return { this };
		}

		[[nodiscard]] inline LockedConstType GetLocked() const noexcept
		{
			return { this };
		}

		[[nodiscard]] inline bool HasElements() const noexcept
		{
			return GetLocked().HasElements();
		}

		[[nodiscard]] inline Size GetKeyCount() const noexcept
		{
			return GetLocked().GetKeyCount();
		}

		[[nodiscard]] inline Size GetElementCount() const noexcept
		{
			return GetLocked().GetElementCount();
		}

		inline void Clear() noexcept
		{
			GetLocked().Clear();
		}

		inline void Insert(const K& key)
		{
			GetLocked().Insert(key);
		}

		inline Size Erase(const K& key) noexcept
		{
			return GetLocked().Erase(key);
		}

		template<typename F> requires std::is_same_v<std::invoke_result_t<F, T&>, bool>
		inline void PopFrontIf(F&& function) noexcept(noexcept(function(std::declval<T&>())))
		{
			GetLocked().PopFrontIf(std::forward<F>(function));
		}

		inline void PushBack(const K& key, const T& element)
		{
			GetLocked().PushBack(key, element);
		}

		template<typename F>
		inline void PushBack(const K& key, const T& element, F&& function)
		{
			GetLocked().PushBack(key, element, std::forward<F>(function));
		}

		inline void PushBack(const K& key, T&& element)
		{
			GetLocked().PushBack(key, std::move(element));
		}

		template<typename F>
		inline void PushBack(const K& key, T&& element, F&& function)
		{
			GetLocked().PushBack(key, std::move(element), std::forward<F>(function));
		}

		inline void PushFront(const K& key, const T& element)
		{
			GetLocked().PushFront(key, element);
		}

		template<typename F>
		inline void PushFront(const K& key, const T& element, F&& function)
		{
			GetLocked().PushFront(key, element, std::forward<F>(function));
		}

		inline void PushFront(const K& key, T&& element)
		{
			GetLocked().PushFront(key, std::move(element));
		}

		template<typename F>
		inline void PushFront(const K& key, T&& element, F&& function)
		{
			GetLocked().PushFront(key, std::move(element), std::forward<F>(function));
		}

		inline void InterruptWait() const noexcept
		{
			m_WaitCondition.NotifyAll();
		}

		inline bool Wait(const std::chrono::milliseconds time, const Event& interrupt_event) const noexcept
		{
			return GetLocked().Wait(time, interrupt_event);
		}

		inline bool Wait(const Event& interrupt_event) const noexcept
		{
			return GetLocked().Wait(interrupt_event);
		}

	private:
		MapType m_Map;
		Size m_NextDeque{ 0 };
		mutable CriticalSection m_CriticalSection;
		mutable ConditionVariable m_WaitCondition;
	};
}

