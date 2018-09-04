// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "SpinMutex.h"

namespace QuantumGate::Implementation::Concurrency
{
	class Export RecursiveSharedMutex
	{
	public:
		RecursiveSharedMutex() noexcept {}
		RecursiveSharedMutex(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex(RecursiveSharedMutex&&) = delete;
		virtual ~RecursiveSharedMutex() = default;
		RecursiveSharedMutex& operator=(const RecursiveSharedMutex&) = delete;
		RecursiveSharedMutex& operator=(RecursiveSharedMutex&&) = delete;

		void lock() noexcept;
		const bool try_lock() noexcept;
		void unlock() noexcept;

		void lock_shared() noexcept;
		const bool try_lock_shared() noexcept;
		void unlock_shared() noexcept;

	private:
		template<typename Func>
		void Wait(std::unique_lock<SpinMutex>& lock, Func&& func) noexcept;

	private:
		std::thread::id m_ExclusiveThreadID;
		unsigned long m_ExclusiveLockCount{ 0 };
		unsigned long m_SharedLockCount{ 0 };

		SpinMutex m_Mutex;
	};
}