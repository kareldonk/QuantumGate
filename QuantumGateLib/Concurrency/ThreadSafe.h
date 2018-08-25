// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Common\Traits.h"

#include <shared_mutex>

namespace QuantumGate::Implementation::Concurrency
{
	// Inspired by "Enforcing Correct Mutex Usage with Synchronized Values" by Anthony Williams
	// http://www.drdobbs.com/cpp/enforcing-correct-mutex-usage-with-synch/225200269

	template<typename T, typename M = std::mutex>
	class ThreadSafe
	{
		template<typename ThS, typename Lck = std::unique_lock<M>>
		class Value
		{
		public:
			constexpr Value() = default;
			constexpr Value(ThS* ths) noexcept : m_ThS(ths), m_Lock(ths->m_Mutex) {}
			constexpr Value(ThS* ths, std::adopt_lock_t t) noexcept : m_ThS(ths), m_Lock(ths->m_Mutex, t) {}
			constexpr Value(ThS* ths, std::defer_lock_t t) noexcept : m_ThS(ths), m_Lock(ths->m_Mutex, t) {}
			Value(const Value&) = delete;
			constexpr Value(Value&&) = default;
			~Value() {}
			Value& operator=(const Value&) = delete;
			constexpr Value& operator=(Value&&) = default;

			constexpr std::conditional_t<std::is_const_v<ThS>, const T*, T*> operator->() noexcept { assert(*this); return &m_ThS->m_Data; }

			constexpr std::conditional_t<std::is_const_v<ThS>, const T&, T&> operator*() noexcept { assert(*this); return m_ThS->m_Data; }

			template<typename Arg>
			constexpr auto& operator[](Arg&& arg) { assert(*this); return m_ThS->m_Data[std::forward<Arg>(arg)]; }

			template<typename Arg>
			constexpr const auto& operator[](Arg&& arg) const { assert(*this); return m_ThS->m_Data[std::forward<Arg>(arg)]; }

			constexpr T& operator=(const T& other) noexcept(noexcept(m_ThS->m_Data = other))
			{
				assert(*this);
				m_ThS->m_Data = other; return m_ThS->m_Data;
			}

			constexpr const bool operator==(const T& other) const noexcept
			{
				assert(*this);
				return (m_ThS->m_Data == other);
			}

			constexpr const bool operator!=(const T& other) const noexcept
			{
				assert(*this);
				return !(m_ThS->m_Data == other);
			}

			constexpr explicit operator bool() const noexcept { return (m_ThS != nullptr && m_Lock.owns_lock()); }

			template<typename... Args>
			constexpr void operator()(Args&&... args) const noexcept(noexcept(m_ThS->m_Data(std::forward<Args>(args)...)))
			{
				assert(*this);
				m_ThS->m_Data(std::forward<Args>(args)...);
			}

			constexpr void Reset() noexcept
			{
				m_ThS = nullptr; 
				if (m_Lock.owns_lock()) m_Lock.unlock();
			}

		private:
			ThS* m_ThS{ nullptr };
			Lck m_Lock;
		};

	public:
		using UniqueLockedType = Value<ThreadSafe, std::unique_lock<M>>;
		using ConstUniqueLockedType = Value<const ThreadSafe, std::unique_lock<M>>;
		using SharedLockedType = Value<const ThreadSafe, std::shared_lock<M>>;

		constexpr ThreadSafe() noexcept {}

		template<typename... Args,
			typename = std::enable_if_t<!are_same<ThreadSafe<T, M>, Args...>::value>>
		constexpr ThreadSafe(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
			m_Data(std::forward<Args>(args)...) {}

		ThreadSafe(const ThreadSafe&) = delete;
		ThreadSafe(ThreadSafe&&) = delete;
		virtual ~ThreadSafe() {}
		ThreadSafe& operator=(const ThreadSafe&) = delete;
		ThreadSafe& operator=(ThreadSafe&&) = delete;

		constexpr UniqueLockedType WithUniqueLock() noexcept(noexcept(m_Mutex.lock())) { return { this }; }
		constexpr ConstUniqueLockedType WithUniqueLock() const noexcept(noexcept(m_Mutex.lock())) { return { this }; }
		
		template<typename F>
		constexpr auto WithUniqueLock(F&& function) noexcept(noexcept(function(*WithUniqueLock())))
		{
			return function(*WithUniqueLock());
		}

		template<typename F>
		constexpr auto WithUniqueLock(F&& function) const noexcept(noexcept(function(*WithUniqueLock())))
		{
			return function(*WithUniqueLock());
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock)>>>
		constexpr const bool IfUniqueLock(F&& function) noexcept(noexcept(function(m_Data)))
		{
			if (m_Mutex.try_lock())
			{
				function(m_Data);
				m_Mutex.unlock();
				return true;
			}

			return false;
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock)>>>
		constexpr const bool IfUniqueLock(F&& function) const noexcept(noexcept(function(m_Data)))
		{
			if (m_Mutex.try_lock())
			{
				function(m_Data);
				m_Mutex.unlock();
				return true;
			}

			return false;
		}

		template<typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock)>>>
		constexpr const bool TryUniqueLock(UniqueLockedType& value) noexcept
		{
			if (m_Mutex.try_lock())
			{
				value = UniqueLockedType(this, std::adopt_lock);
				return true;
			}

			return false;
		}

		template<typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::lock_shared)>>>
		constexpr SharedLockedType WithSharedLock() const noexcept(noexcept(m_Mutex.lock_shared()))
		{
			return { this }; 
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::lock_shared)>>>
		constexpr auto WithSharedLock(F&& function) const noexcept(noexcept(function(*WithSharedLock())))
		{
			return function(*WithSharedLock());
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock_shared)>>>
		constexpr const bool IfSharedLock(F&& function) const noexcept(noexcept(function(m_Data)))
		{
			if (m_Mutex.try_lock_shared())
			{
				function(m_Data);
				m_Mutex.unlock_shared();
				return true;
			}

			return false;
		}

	private:
		T m_Data;
		mutable M m_Mutex;
	};
}