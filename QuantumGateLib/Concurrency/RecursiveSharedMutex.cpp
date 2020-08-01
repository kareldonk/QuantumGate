// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "RecursiveSharedMutex.h"

using namespace std::literals;

namespace QuantumGate::Implementation::Concurrency
{
	void RecursiveSharedMutex::lock()
	{
		const auto id = std::this_thread::get_id();

		LockGuardType lock(m_CriticalSection);

		if (m_ExclusiveThreadID == id)
		{
			assert(m_ExclusiveLockCount > 0);

			if (m_ExclusiveLockCount < MaxNumLocks)
			{
				// Recursive locking
				++m_ExclusiveLockCount;
			}
			else
			{
				throw std::system_error(std::make_error_code(std::errc::value_too_large),
										"RecursiveSharedMutex recursion too deep.");
			}
		}
		else
		{
			if (m_ExclusiveLockCount != 0)
			{
				// Wait for other thread to release exclusive lock
				m_Condition1.Wait(m_CriticalSection, [&]() { return (m_ExclusiveLockCount == 0); });
			}

			m_ExclusiveThreadID = id;
			m_ExclusiveLockCount = 1;

			if (m_SharedLockCount != 0)
			{
				// Wait for other threads to release their shared locks
				m_Condition2.Wait(m_CriticalSection, [&]() { return (m_SharedLockCount == 0); });
			}
		}
	}
	
	bool RecursiveSharedMutex::try_lock() noexcept
	{
		const auto id = std::this_thread::get_id();

		LockGuardType lock(m_CriticalSection);

		if (m_ExclusiveThreadID == id)
		{
			assert(m_ExclusiveLockCount > 0);

			if (m_ExclusiveLockCount < MaxNumLocks)
			{
				// Recursive locking
				++m_ExclusiveLockCount;

				return true;
			}
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
		bool notify = false;

		{
			LockGuardType lock(m_CriticalSection);

			// Only the thread with exclusive lock should call unlock
			assert(m_ExclusiveThreadID == std::this_thread::get_id());

			// Should have an exclusive lock before unlocking
			assert(m_ExclusiveLockCount > 0 && m_SharedLockCount == 0);

			--m_ExclusiveLockCount;

			if (m_ExclusiveLockCount == 0)
			{
				m_ExclusiveThreadID = std::thread::id();

				notify = true;
			}
		}

		if (notify) m_Condition1.NotifyAll();
	}
	
	void RecursiveSharedMutex::lock_shared() noexcept
	{
		LockGuardType lock(m_CriticalSection);

		// Thread with exclusive lock may not get shared lock
		assert(m_ExclusiveThreadID != std::this_thread::get_id());

		if (m_ExclusiveLockCount != 0 || m_SharedLockCount == MaxNumLocks)
		{
			// Wait for other threads to release (exclusive) locks
			m_Condition1.Wait(m_CriticalSection, [&]()
			{
				return (m_ExclusiveLockCount == 0 && m_SharedLockCount < MaxNumLocks);
			});
		}

		++m_SharedLockCount;
	}
	
	bool RecursiveSharedMutex::try_lock_shared() noexcept
	{
		LockGuardType lock(m_CriticalSection);

		// Thread with exclusive lock may not get shared lock
		assert(m_ExclusiveThreadID != std::this_thread::get_id());

		if (m_ExclusiveLockCount > 0 || m_SharedLockCount == MaxNumLocks)
		{
			return false;
		}

		++m_SharedLockCount;

		return true;
	}
	
	void RecursiveSharedMutex::unlock_shared() noexcept
	{
		auto wake = 0u;

		{
			LockGuardType lock(m_CriticalSection);

			// Should have shared lock before unlocking
			assert(m_SharedLockCount > 0);

			--m_SharedLockCount;

			if (m_ExclusiveLockCount > 0)
			{
				if (m_SharedLockCount == 0)
				{
					wake = 1u;
				}
			}
			else
			{
				if (m_SharedLockCount == MaxNumLocks - 1)
				{
					wake = 2u;
				}
			}
		}

		if (wake == 1u) m_Condition2.NotifyOne();
		else if (wake == 2u) m_Condition1.NotifyOne();
	}
}
