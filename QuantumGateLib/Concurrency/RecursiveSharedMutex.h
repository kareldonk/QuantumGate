// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SpinMutex.h"

namespace QuantumGate::Implementation::Concurrency
{
	class Export RecursiveSharedMutex final
	{
		using UniqueLockType = std::unique_lock<SpinMutex>;
		using SizeType = unsigned long;

	public:
		RecursiveSharedMutex() noexcept {}
		RecursiveSharedMutex(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex(RecursiveSharedMutex&&) = delete;
		~RecursiveSharedMutex() = default;
		RecursiveSharedMutex& operator=(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex& operator=(RecursiveSharedMutex&&) = delete;

		void lock();
		bool try_lock() noexcept;
		void unlock() noexcept;

		void lock_shared();
		bool try_lock_shared() noexcept;
		void unlock_shared() noexcept;

	private:
		template<typename Func> requires std::is_same_v<std::invoke_result_t<Func>, bool>
		void Wait(UniqueLockType& lock, Func&& func) noexcept;

	private:
		static constexpr SizeType MaxNumLocks{ (std::numeric_limits<SizeType>::max)() };

	private:
		std::thread::id m_ExclusiveThreadID;
		SizeType m_ExclusiveLockCount{ 0 };
		SizeType m_SharedLockCount{ 0 };

		SpinMutex m_Mutex;
	};
}