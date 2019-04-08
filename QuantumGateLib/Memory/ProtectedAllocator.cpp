// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "ProtectedAllocator.h"

namespace QuantumGate::Implementation::Memory
{
	std::mutex& GetProtectedAllocatorMutex() noexcept
	{
		static std::mutex mutex;
		return mutex;
	}

	bool GetCurrentProcessWorkingSetSize(Size& minsize, Size& maxsize) noexcept
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

	bool SetCurrentProcessWorkingSetSize(const Size minsize, const Size maxsize) noexcept
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

	void* ProtectedAllocatorBase::Allocate(const Size len)
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
					GetProtectedAllocatorMutex().lock();

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

						Size nmin{ 0 };
						Size nmax{ 0 };

						do
						{
							if (GetCurrentProcessWorkingSetSize(nmin, nmax))
							{
								Size nmin2 = nmin * 2;
								Size nmax2 = nmax * 2;

								if (nmin + len > nmin2)
								{
									nmin2 = nmin + len;
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

					GetProtectedAllocatorMutex().unlock();
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

		return memaddr;
	}

	void ProtectedAllocatorBase::Deallocate(void* p, const Size len) noexcept
	{
		// Wipe all data from used memory
		MemClear(p, len);

		// Unlock and free
		::VirtualUnlock(p, len);
		::VirtualFree(p, 0, MEM_RELEASE);
	}
}