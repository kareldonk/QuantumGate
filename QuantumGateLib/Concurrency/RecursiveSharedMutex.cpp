// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "RecursiveSharedMutex.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Concurrency
{
	void RecursiveSharedMutex::lock() noexcept
	{
		const auto id = std::this_thread::get_id();

		UniqueLockType lock(m_Mutex);

		if (m_ExclusiveThreadID == id)
		{
			assert(m_ExclusiveLockCount > 0);

			// Recursive locking
			++m_ExclusiveLockCount;
		}
		else
		{
			if (m_ExclusiveLockCount != 0)
			{
				// Wait for other thread to release exclusive lock
				Wait(lock, [&]() { return (m_ExclusiveLockCount == 0); });
			}

			m_ExclusiveThreadID = id;
			m_ExclusiveLockCount = 1;

			if (m_SharedLockCount != 0)
			{
				// Wait for other threads to release their shared locks
				Wait(lock, [&]() { return (m_SharedLockCount == 0); });
			}
		}
	}
	
	bool RecursiveSharedMutex::try_lock() noexcept
	{
		const auto id = std::this_thread::get_id();

		UniqueLockType lock(m_Mutex);

		if (m_ExclusiveThreadID == id)
		{
			assert(m_ExclusiveLockCount > 0);

			// Recursive locking
			++m_ExclusiveLockCount;

			return true;
		}
		else
		{
			// Normal locking
			if (m_ExclusiveLockCount == 0 && m_SharedLockCount == 0)
			{
				m_ExclusiveThreadID = id;
				m_ExclusiveLockCount = 1;

				return true;
			}
		}

		return false;
	}
	
	void RecursiveSharedMutex::unlock() noexcept
	{
		UniqueLockType lock(m_Mutex);

		// Only the thread with exclusive lock should call unlock
		assert(m_ExclusiveThreadID == std::this_thread::get_id());

		// Should have an exclusive lock before unlocking
		assert(m_ExclusiveLockCount > 0);

		--m_ExclusiveLockCount;
		
		if (m_ExclusiveLockCount == 0)
		{
			m_ExclusiveThreadID = std::thread::id();
		}
	}
	
	void RecursiveSharedMutex::lock_shared() noexcept
	{
		UniqueLockType lock(m_Mutex);

		// Thread with exclusive lock may not get shared lock
		assert(m_ExclusiveThreadID != std::this_thread::get_id());

		if (m_ExclusiveLockCount != 0)
		{
			// Wait for other thread to release exclusive lock
			Wait(lock, [&]() { return (m_ExclusiveLockCount == 0); });
		}

		++m_SharedLockCount;
	}
	
	bool RecursiveSharedMutex::try_lock_shared() noexcept
	{
		UniqueLockType lock(m_Mutex);

		// Thread with exclusive lock may not get shared lock
		assert(m_ExclusiveThreadID != std::this_thread::get_id());

		if (m_ExclusiveLockCount > 0) return false;

		++m_SharedLockCount;

		return true;
	}
	
	void RecursiveSharedMutex::unlock_shared() noexcept
	{
		UniqueLockType lock(m_Mutex);

		// Should have shared lock before unlocking
		assert(m_SharedLockCount > 0);

		--m_SharedLockCount;
	}

	template<typename Func>
	void RecursiveSharedMutex::Wait(UniqueLockType& lock, Func&& func) noexcept
	{
		auto count = 0u;

		while (func() == false)
		{
			lock.unlock();

			std::this_thread::yield();

			++count;
			if (count > 10)
			{
				count = 0;
				std::this_thread::sleep_for(1ms);
			}

			lock.lock();
		}
	}
}
