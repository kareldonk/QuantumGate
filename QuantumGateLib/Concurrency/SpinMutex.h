// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>
#include <thread>

namespace QuantumGate::Implementation::Concurrency
{
	class SpinMutex final
	{
	public:
		SpinMutex() noexcept {}
		SpinMutex(const SpinMutex&) = delete;
		SpinMutex(SpinMutex&&) = delete;
		~SpinMutex() = default;
		SpinMutex& operator=(const SpinMutex&) = delete;
		SpinMutex& operator=(SpinMutex&&) = delete;

        void lock() noexcept
        {
            for (int spin_count = 0; !try_lock(); ++spin_count)
            {
                if (spin_count < 16)
                {
                    _mm_pause();
                }
                else
                {
                    std::this_thread::yield();
                    spin_count = 0;
                }
            }
        }

        bool try_lock() noexcept
        {
            return (!m_Locked.load(std::memory_order_relaxed) && !m_Locked.exchange(true, std::memory_order_acquire));
        }

        void unlock() noexcept
        {
            m_Locked.store(false, std::memory_order_release);
        }

    private:
        std::atomic_bool m_Locked{ false };
	};
}