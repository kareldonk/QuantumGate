// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "ThreadSafe.h"
#include "SpinMutex.h"

#include <random>

namespace QuantumGate::Implementation::Concurrency
{
	template<typename T, typename M = SpinMutex, UInt64 ID = 0>
	class ThreadLocalCache
	{
		static_assert(std::is_default_constructible_v<T>, "Type must be default constructible.");
		static_assert(std::is_copy_assignable_v<T>, "Type must be copy assignable.");
		static_assert(std::is_member_function_pointer<decltype(&M::try_lock)>::value, "Value mutex should support try_lock.");

	public:
		using CacheType = T;

		template<typename... Args>
		ThreadLocalCache(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
			m_Value(std::forward<Args>(args)...)
		{
			std::random_device rnd_dev;
			std::mt19937 rnd_alg(rnd_dev());
			m_ValueUpdateFlag = rnd_alg(); 
		};
		
		ThreadLocalCache(const ThreadLocalCache&) = delete;
		ThreadLocalCache(ThreadLocalCache&&) = delete;
		virtual ~ThreadLocalCache() = default;
		ThreadLocalCache& operator=(const ThreadLocalCache&) = delete;
		ThreadLocalCache& operator=(ThreadLocalCache&&) = delete;

		inline const CacheType* operator->() const noexcept(noexcept(GetCache()))
		{
			return &GetCache();
		}

		inline const CacheType& operator*() const noexcept(noexcept(GetCache()))
		{
			return GetCache();
		}

		inline const CacheType& GetCache(const bool latest = true) const noexcept(noexcept(UpdateCache()))
		{
			if (latest && IsCacheExpired()) UpdateCache();

			return Cache();
		}

		template<typename F>
		void UpdateValue(F&& function) noexcept(noexcept(m_Value.WithUniqueLock(function)))
		{
			m_Value.WithUniqueLock([&](T& value) noexcept(noexcept(function(value)))
			{
				function(value);
			});

			// No problem if this wraps/overflows
			// because what matters is the change
			++m_ValueUpdateFlag;
		}

	private:
		ForceInline static CacheType& Cache() noexcept
		{
			// Static object for use by the current thread
			static thread_local CacheType m_Cache;
			return m_Cache;
		}

		ForceInline static UInt& CacheUpdateFlag() noexcept
		{
			// Static object for use by the current thread
			static thread_local UInt m_CacheUpdateFlag{ 0 };
			return m_CacheUpdateFlag;
		}

		ForceInline void UpdateCache() const noexcept(std::is_nothrow_copy_assignable_v<T>)
		{
			m_Value.WithUniqueLock([&](const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
			{
				Cache() = value;
				CacheUpdateFlag() = m_ValueUpdateFlag.load();
			});
		}

		bool TryUpdateCache() const noexcept(std::is_nothrow_copy_assignable_v<T>)
		{
			return m_Value.IfUniqueLock([&](T& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
			{
				Cache() = value;
				CacheUpdateFlag() = m_ValueUpdateFlag.load();
			});
		}

		ForceInline const bool IsCacheExpired() const noexcept
		{
			return (m_ValueUpdateFlag.load() != CacheUpdateFlag());
		}

	private:
		ThreadSafe<T, M> m_Value;
		std::atomic<UInt> m_ValueUpdateFlag{ 0 };
	};
}