// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ProtectedFreeStoreAllocator.h"
#include "AllocatorStats.h"

#include <mutex>

namespace QuantumGate::Implementation::Memory
{
	extern AllocatorStats_ThS ProtectedFreeStoreAllocatorStats;

	auto& GetProtectedFreeStoreAllocatorStats() noexcept
	{
		return ProtectedFreeStoreAllocatorStats;
	}

	auto& GetProtectedFreeStoreAllocatorMutex() noexcept
	{
		static std::mutex mutex;
		return mutex;
	}

	bool GetCurrentProcessWorkingSetSize(std::size_t& minsize, std::size_t& maxsize) noexcept
	{
		SIZE_T tminsize{ 0 };
		SIZE_T tmaxsize{ 0 };

		if (::GetProcessWorkingSetSize(GetCurrentProcess(), &tminsize, &tmaxsize))
		{
			minsize = tminsize;
			maxsize = tmaxsize;

			LogInfo(L"Process memory working set size is %llu (min) / %llu (max)",
					static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));

			return true;
		}
		else LogErr(L"Could not get process memory working set size");

		return false;
	}

	bool SetCurrentProcessWorkingSetSize(const std::size_t minsize, const std::size_t maxsize) noexcept
	{
		if (::SetProcessWorkingSetSize(::GetCurrentProcess(), minsize, maxsize))
		{
			LogInfo(L"Process memory working set size changed to %llu (min) / %llu (max)",
					static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));
			return true;
		}
		else LogErr(L"Could not change process memory working set size to %llu (min) / %llu (max)",
					static_cast<UInt64>(minsize), static_cast<UInt64>(maxsize));

		return false;
	}

	void ProtectedFreeStoreAllocatorBase::LogStatistics() noexcept
	{
		DbgInvoke([&]()
		{
			String output{ L"\r\n\r\nProtectedFreeStoreAllocator allocation sizes:\r\n-----------------------------------------------\r\n" };
			output += GetProtectedFreeStoreAllocatorStats().WithSharedLock()->GetAllSizes();
			output += L"\r\nProtectedFreeStoreAllocator memory in use:\r\n-----------------------------------------------\r\n";
			output += GetProtectedFreeStoreAllocatorStats().WithSharedLock()->GetMemoryInUse();
			output += L"\r\n";

			SLogInfo(output);
		});
	}

	void* ProtectedFreeStoreAllocatorBase::Allocate(const std::size_t len)
	{
		auto memaddr = ::VirtualAlloc(NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (memaddr != NULL)
		{
			// Lock in physical memory and prevent from being swapped to pagefile (on disk)
			if (::VirtualLock(memaddr, len) == 0)
			{
				auto succeeded = false;

				// If the failure was caused by a low quota
				// we try to increase it if possible
				if (::GetLastError() == ERROR_WORKING_SET_QUOTA)
				{
					GetProtectedFreeStoreAllocatorMutex().lock();

					// Previous lock may have caused a delay while other threads
					// increased the working set size; so try again before
					// proceeding to increase the working set size
					if (::VirtualLock(memaddr, len) != 0)
					{
						succeeded = true;
					}
					else if (::GetLastError() == ERROR_WORKING_SET_QUOTA)
					{
						auto retry = 0u;

						std::size_t nmin{ 0 };
						std::size_t nmax{ 0 };

						do
						{
							if (GetCurrentProcessWorkingSetSize(nmin, nmax))
							{
								std::size_t nmin2 = nmin * 2;
								if (nmin + len > nmin2)
								{
									nmin2 = nmin + len;
								}

								std::size_t nmax2 = nmax;
								if (nmax2 <= nmin2)
								{
									nmax2 = nmin2 * 2;
								}

								if (SetCurrentProcessWorkingSetSize(nmin2, nmax2))
								{
									if (::VirtualLock(memaddr, len) != 0)
									{
										succeeded = true;
										break;
									}
								}
								else break;
							}
							else break;

							++retry;
						}
						while (retry < 3);
					}

					GetProtectedFreeStoreAllocatorMutex().unlock();
				}

				if (!succeeded)
				{
					::VirtualFree(memaddr, 0, MEM_RELEASE);

					std::string error = "Memory allocation error; could not lock memory: GetLastError() returned " + std::to_string(::GetLastError());
					throw BadAllocException(error.c_str());
				}
			}
		}
		else
		{
			std::string error = "Could not allocate memory: GetLastError() returned " + std::to_string(::GetLastError());
			throw BadAllocException(error.c_str());
		}

		DbgInvoke([&]()
		{
			GetProtectedFreeStoreAllocatorStats().WithUniqueLock([&](auto& stats)
			{
				stats.Sizes.insert(len);

				if (memaddr != nullptr) stats.MemoryInUse.insert({ reinterpret_cast<std::uintptr_t>(memaddr), len });
			});
		});

		return memaddr;
	}

	void ProtectedFreeStoreAllocatorBase::Deallocate(void* p, const std::size_t len) noexcept
	{
		// Wipe all data from used memory
		MemClear(p, len);

		// Unlock and free
		::VirtualUnlock(p, len);
		::VirtualFree(p, 0, MEM_RELEASE);

		DbgInvoke([&]()
		{
			GetProtectedFreeStoreAllocatorStats().WithUniqueLock([&](auto& stats)
			{
				stats.MemoryInUse.erase(reinterpret_cast<std::uintptr_t>(p));
			});
		});
	}
}