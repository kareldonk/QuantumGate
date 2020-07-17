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
	class ThreadSafe final
	{
		template<typename ThS, typename Lck = std::unique_lock<M>>
		class Value final
		{
		public:
			Value() noexcept(std::is_nothrow_default_constructible_v<Lck>) {}
			Value(ThS* ths) noexcept(std::is_nothrow_constructible_v<Lck, M>) : m_ThS(ths), m_Lock(ths->m_Mutex) {}

			Value(ThS* ths, std::adopt_lock_t t) noexcept(std::is_nothrow_constructible_v<Lck, M, std::adopt_lock_t>) :
				m_ThS(ths), m_Lock(ths->m_Mutex, t) {}

			Value(ThS* ths, std::defer_lock_t t) noexcept(std::is_nothrow_constructible_v<Lck, M, std::defer_lock_t>) :
				m_ThS(ths), m_Lock(ths->m_Mutex, t) {}

			Value(const Value&) = delete;
			
			Value(Value&& other) noexcept :
				m_ThS(other.m_ThS), m_Lock(std::move(other.m_Lock))
			{
				other.m_ThS = nullptr;
			}

			~Value() = default;
			Value& operator=(const Value&) = delete;

			Value& operator=(Value&& other) noexcept
			{
				m_ThS = std::exchange(other.m_ThS, nullptr);
				m_Lock = std::move(other.m_Lock);

				return *this;
			}

			inline std::conditional_t<std::is_const_v<ThS>, const T*, T*> operator->() noexcept { assert(*this); return &m_ThS->m_Data; }
			inline const T* operator->() const noexcept { assert(*this); return &m_ThS->m_Data; }

			inline std::conditional_t<std::is_const_v<ThS>, const T&, T&> operator*() noexcept { assert(*this); return m_ThS->m_Data; }
			inline const T& operator*() const noexcept { assert(*this); return m_ThS->m_Data; }

			template<typename Arg, typename ThS2 = ThS, typename = std::enable_if_t<!std::is_const_v<ThS2>>>
			inline auto& operator[](Arg&& arg) noexcept(noexcept(m_ThS->m_Data[std::forward<Arg>(arg)]))
			{
				assert(*this);
				return m_ThS->m_Data[std::forward<Arg>(arg)];
			}

			template<typename Arg, typename ThS2 = ThS, typename = std::enable_if_t<std::is_const_v<ThS2>>>
			inline const auto& operator[](Arg&& arg) const noexcept(noexcept(m_ThS->m_Data[std::forward<Arg>(arg)]))
			{
				assert(*this);
				return m_ThS->m_Data[std::forward<Arg>(arg)];
			}

			template<typename... Args, typename ThS2 = ThS, typename = std::enable_if_t<!std::is_const_v<ThS2>>>
			inline auto operator()(Args&&... args) noexcept(noexcept(m_ThS->m_Data(std::forward<Args>(args)...)))
			{
				assert(*this);
				return m_ThS->m_Data(std::forward<Args>(args)...);
			}

			template<typename... Args, typename ThS2 = ThS, typename = std::enable_if_t<std::is_const_v<ThS2>>>
			inline auto operator()(Args&&... args) const noexcept(noexcept(m_ThS->m_Data(std::forward<Args>(args)...)))
			{
				assert(*this);
				return m_ThS->m_Data(std::forward<Args>(args)...);
			}

			inline Value& operator=(const T& other) noexcept(std::is_nothrow_copy_assignable_v<T>)
			{
				assert(*this);
				m_ThS->m_Data = other;
				return *this;
			}

			inline Value& operator=(T&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
			{
				assert(*this);
				m_ThS->m_Data = std::move(other);
				return *this;
			}

			inline bool operator==(const T& other) const noexcept
			{
				assert(*this);
				return (m_ThS->m_Data == other);
			}

			inline bool operator!=(const T& other) const noexcept
			{
				assert(*this);
				return !(m_ThS->m_Data == other);
			}

			inline explicit operator bool() const noexcept { return (m_ThS != nullptr && m_Lock.owns_lock()); }

			template<typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::unique_lock<M>>>>
			inline void Lock() noexcept(noexcept(m_Lock.lock()))
			{
				assert(!m_Lock.owns_lock());
				m_Lock.lock();
			}

			template<typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::unique_lock<M>>>>
			inline void Unlock() noexcept(noexcept(m_Lock.unlock()))
			{
				assert(m_Lock.owns_lock());
				m_Lock.unlock();
			}

			template<typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::shared_lock<M>>>>
			inline void LockShared() noexcept(noexcept(m_Lock.lock()))
			{
				assert(!m_Lock.owns_lock());
				m_Lock.lock();
			}

			template<typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::shared_lock<M>>>>
			inline void UnlockShared() noexcept(noexcept(m_Lock.unlock()))
			{
				assert(m_Lock.owns_lock());
				m_Lock.unlock();
			}

			inline void Reset() noexcept(noexcept(m_Lock.owns_lock()) &&
										 noexcept(m_Lock.unlock()))
			{
				m_ThS = nullptr;
				if (m_Lock.owns_lock()) m_Lock.unlock();
			}

			template<typename F, typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::unique_lock<M>>>>
			inline void WhileUnlocked(F&& function) noexcept(noexcept(Unlock()) &&
															 noexcept(function()) &&
															 noexcept(Lock()))
			{
				assert(m_Lock.owns_lock());
				Unlock();
				function();
				Lock();
			}

			template<typename F, typename Lck2 = Lck,
				typename = std::enable_if_t<std::is_same_v<Lck2, std::shared_lock<M>>>>
				inline void WhileUnlocked(F&& function) const noexcept(noexcept(UnlockShared()) &&
																	   noexcept(function()) &&
																	   noexcept(LockShared()))
			{
				assert(m_Lock.owns_lock());
				UnlockShared();
				function();
				LockShared();
			}

		private:
			ThS* m_ThS{ nullptr };
			Lck m_Lock;
		};

	public:
		using UniqueLockedType = Value<ThreadSafe, std::unique_lock<M>>;
		using UniqueLockedConstType = Value<const ThreadSafe, std::unique_lock<M>>;
		using SharedLockedConstType = Value<const ThreadSafe, std::shared_lock<M>>;

		constexpr ThreadSafe() noexcept(std::is_nothrow_default_constructible_v<T> &&
										std::is_nothrow_default_constructible_v<M>) {}

		template<typename... Args,
			typename = std::enable_if_t<!AreSameV<ThreadSafe<T, M>, std::decay_t<Args>...>>>
		constexpr ThreadSafe(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...> &&
													  std::is_nothrow_constructible_v<M>) :
			m_Data(std::forward<Args>(args)...) {}

		ThreadSafe(const ThreadSafe&) = delete;
		ThreadSafe(ThreadSafe&&) = delete;
		~ThreadSafe() = default;
		ThreadSafe& operator=(const ThreadSafe&) = delete;
		ThreadSafe& operator=(ThreadSafe&&) = delete;

		inline UniqueLockedType WithUniqueLock() noexcept(std::is_nothrow_constructible_v<UniqueLockedType, ThreadSafe>)
		{
			return { this };
		}

		inline UniqueLockedConstType WithUniqueLock() const noexcept(std::is_nothrow_constructible_v<UniqueLockedConstType, ThreadSafe>)
		{
			return { this };
		}

		template<typename F>
		inline auto WithUniqueLock(F&& function) noexcept(noexcept(function(*WithUniqueLock())))
		{
			return function(*WithUniqueLock());
		}

		template<typename F>
		inline auto WithUniqueLock(F&& function) const noexcept(noexcept(function(*WithUniqueLock())))
		{
			return function(*WithUniqueLock());
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock)>>>
		inline bool IfUniqueLock(F&& function) noexcept(noexcept(m_Mutex.try_lock()) &&
														noexcept(function(m_Data)) &&
														noexcept(m_Mutex.unlock()))
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
		inline bool IfUniqueLock(F&& function) const noexcept(noexcept(m_Mutex.try_lock()) &&
															  noexcept(function(m_Data)) &&
															  noexcept(m_Mutex.unlock()))
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
		inline bool TryUniqueLock(UniqueLockedType& value) noexcept(noexcept(m_Mutex.try_lock()) &&
																	noexcept(UniqueLockedType(this, std::adopt_lock)))
		{
			if (m_Mutex.try_lock())
			{
				value = UniqueLockedType(this, std::adopt_lock);
				return true;
			}

			return false;
		}

		template<typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock)>>>
		inline bool TryUniqueLock(UniqueLockedConstType& value) const noexcept(noexcept(m_Mutex.try_lock()) &&
																			   noexcept(UniqueLockedConstType(this, std::adopt_lock)))
		{
			if (m_Mutex.try_lock())
			{
				value = UniqueLockedConstType(this, std::adopt_lock);
				return true;
			}

			return false;
		}

		template<typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::lock_shared)>>>
		inline SharedLockedConstType WithSharedLock() const noexcept(std::is_nothrow_constructible_v<SharedLockedConstType, ThreadSafe>)
		{
			return { this };
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::lock_shared)>>>
		inline auto WithSharedLock(F&& function) const noexcept(noexcept(function(*WithSharedLock())))
		{
			return function(*WithSharedLock());
		}

		template<typename F, typename M2 = M,
			typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(&M2::try_lock_shared)>>>
		inline bool IfSharedLock(F&& function) const noexcept(noexcept(m_Mutex.try_lock_shared()) &&
															  noexcept(function(m_Data)) &&
															  noexcept(m_Mutex.unlock_shared()))
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