// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>

namespace QuantumGate::Implementation::Concurrency
{
	class SpinMutex
	{
	public:
		SpinMutex() noexcept {}
		SpinMutex(const SpinMutex&) = delete;
		SpinMutex(SpinMutex&&) = delete;
		virtual ~SpinMutex() = default;
		SpinMutex& operator=(const SpinMutex&) = delete;
		SpinMutex& operator=(SpinMutex&&) = delete;

		void lock() noexcept { while (m_ExclusiveLock.test_and_set(std::memory_order_acquire)) {} }
		const bool try_lock() noexcept { return !m_ExclusiveLock.test_and_set(std::memory_order_acquire); }
		void unlock() noexcept { m_ExclusiveLock.clear(std::memory_order_release); }

	private:
		std::atomic_flag m_ExclusiveLock = ATOMIC_FLAG_INIT;
	};
}