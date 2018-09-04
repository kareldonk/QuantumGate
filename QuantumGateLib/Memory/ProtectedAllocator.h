// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Allocator.h"

#include "..\Concurrency\SpinMutex.h"

namespace QuantumGate::Implementation::Memory
{
	class BadAllocException : public std::exception
	{
	public:
		BadAllocException(const char* message) noexcept : std::exception(message) {}
	};

	class Export ProtectedAllocatorBase
	{
	protected:
		static Concurrency::SpinMutex& GetProtectedAllocatorMutex() noexcept;
		[[nodiscard]] static const bool GetCurrentProcessWorkingSetSize(Size& minsize, Size& maxsize) noexcept;
		[[nodiscard]] static const bool SetCurrentProcessWorkingSetSize(const Size minsize, const Size maxsize) noexcept;
	};

	template<class T>
	class ProtectedAllocator: public ProtectedAllocatorBase
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		ProtectedAllocator() noexcept = default;

		template<class Other>
		ProtectedAllocator(const ProtectedAllocator<Other>&) noexcept {}

		ProtectedAllocator(const ProtectedAllocator&) = default;
		ProtectedAllocator(ProtectedAllocator&&) = default;
		virtual ~ProtectedAllocator() = default;
		ProtectedAllocator& operator=(const ProtectedAllocator&) = default;
		ProtectedAllocator& operator=(ProtectedAllocator&&) = default;

		template<class Other> 
		inline const bool operator==(const ProtectedAllocator<Other>&) const noexcept
		{
			return true;
		}

		template<class Other>
		inline const bool operator!=(const ProtectedAllocator<Other>&) const noexcept
		{
			return false;
		}

		[[nodiscard]] pointer allocate(const std::size_t n)
		{
			const std::size_t len = (n * sizeof(T));
			
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
						if (::VirtualLock(memaddr, len) != 0) succeeded = true;
						else if(::GetLastError() == ERROR_WORKING_SET_QUOTA)
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
							} while (retry < 3);
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

			return static_cast<pointer>(memaddr);
		}

		void deallocate(pointer p, const std::size_t n) noexcept
		{
			assert(p != nullptr);

			const std::size_t len = (n * sizeof(T));

			// Clear memory
			MemClear(p, len);

			// Unlock and free
			::VirtualUnlock(p, len);
			::VirtualFree(p, 0, MEM_RELEASE);
		}
	};
}