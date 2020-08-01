// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "ConditionVariable.h"

namespace QuantumGate::Implementation::Concurrency
{
	class Export RecursiveSharedMutex final
	{
		using LockGuardType = std::lock_guard<CriticalSection>;
		using SizeType = unsigned long;

	public:
		RecursiveSharedMutex() noexcept {}
		RecursiveSharedMutex(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex(RecursiveSharedMutex&&) = delete;
		~RecursiveSharedMutex() = default;
		RecursiveSharedMutex& operator=(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex& operator=(RecursiveSharedMutex&&) = delete;

		void lock();
		[[nodiscard]] bool try_lock() noexcept;
		void unlock() noexcept;

		void lock_shared() noexcept;
		[[nodiscard]] bool try_lock_shared() noexcept;
		void unlock_shared() noexcept;

	private:
		static constexpr SizeType MaxNumLocks{ (std::numeric_limits<SizeType>::max)() };

	private:
		std::thread::id m_ExclusiveThreadID;
		SizeType m_ExclusiveLockCount{ 0 };
		SizeType m_SharedLockCount{ 0 };

		CriticalSection m_CriticalSection;
		ConditionVariable m_Condition1;
		ConditionVariable m_Condition2;
	};
}