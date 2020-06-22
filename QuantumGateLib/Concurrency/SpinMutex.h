// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>
#include <thread>

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

        void lock()
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

        bool try_lock()
        {
            return (!locked.load(std::memory_order_relaxed) && !locked.exchange(true, std::memory_order_acquire));
        }

        void unlock()
        {
            locked.store(false, std::memory_order_release);
        }

    private:
        std::atomic_bool locked{ false };
	};
}