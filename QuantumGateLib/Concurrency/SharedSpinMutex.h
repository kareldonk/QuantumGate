// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>

namespace QuantumGate::Implementation::Concurrency
{
	class SharedSpinMutex
	{
	public:
		SharedSpinMutex() noexcept {}
		SharedSpinMutex(const SharedSpinMutex&) = delete;
		SharedSpinMutex(SharedSpinMutex&&) = delete;
		virtual ~SharedSpinMutex() = default;
		SharedSpinMutex& operator=(const SharedSpinMutex&) = delete;
		SharedSpinMutex& operator=(SharedSpinMutex&&) = delete;

		void lock() noexcept
		{
			auto val = false;
			while (!m_ExclusiveLocked.compare_exchange_strong(val, true, std::memory_order_seq_cst)) { val = false; }

			while (m_SharedCount.load() > 0) {}
		}

		bool try_lock() noexcept
		{
			auto val = false;
			if (m_ExclusiveLocked.compare_exchange_strong(val, true, std::memory_order_seq_cst))
			{
				if (m_SharedCount.load() == 0)
				{
					return true;
				}
				else m_ExclusiveLocked.store(false);
			}

			return false;
		}

		void unlock() noexcept
		{
			assert(m_SharedCount.load() == 0 && m_ExclusiveLocked.load());

			m_ExclusiveLocked.store(false);
		}

		void lock_shared() noexcept
		{
			while (m_ExclusiveLocked.load()) {}

			++m_SharedCount;
		};

		bool try_lock_shared() noexcept
		{
			if (m_ExclusiveLocked.load()) return false;

			++m_SharedCount;

			return true;
		}

		void unlock_shared() noexcept
		{
			assert(m_SharedCount.load() > 0);

			--m_SharedCount;
		}

	private:
		std::atomic_bool m_ExclusiveLocked{ false };
		std::atomic_ulong m_SharedCount{ 0 };
	};
}