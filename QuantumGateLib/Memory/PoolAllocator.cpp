// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PoolAllocator.h"
#include "PoolAllocatorImpl.h"

namespace QuantumGate::Implementation::Memory::PoolAllocator
{
	template<typename Type>
	void AllocatorBase<Type>::LogStatistics() noexcept
	{
		auto output = AllocatorStats::FormatString(L"\r\n\r\n%s statistics:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());

		GetMemoryPoolMap<Type>().WithSharedLock([&](const auto& mpm)
		{
			std::size_t total{ 0 };

			for (auto it = mpm.begin(); it != mpm.end(); ++it)
			{
				const auto pool_size = it->second->MemoryBufferPool.WithSharedLock()->size();

				output += AllocatorStats::FormatString(L"Allocation size: %8zu bytes -> Pool size: %8zu (%zu free)\r\n",
													   it->first, pool_size,
													   it->second->FreeBufferPool.WithUniqueLock()->size());

				total += it->first * pool_size;
			}

			output += AllocatorStats::FormatString(L"\r\nTotal in managed pools: %zu bytes\r\n", total);
		});

		DbgInvoke([&]()
		{
			auto& pas = GetAllocatorStats<Type>();

			output += AllocatorStats::FormatString(L"\r\n%s allocation sizes:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());
			output += pas.WithSharedLock()->GetAllSizes();
			output += AllocatorStats::FormatString(L"\r\n%s memory in use:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());
			output += pas.WithSharedLock()->GetMemoryInUse();
		});

		output += L"\r\n";

		SLogInfo(output);
	}

	template<typename Type>
	std::pair<bool, std::size_t> AllocatorBase<Type>::GetAllocationDetails(const std::size_t n) noexcept
	{
		auto len = n;
		auto manage = false;

		if (n >= AllocatorConstants<Type>::PoolAllocationMinimumSize&&
			n <= AllocatorConstants<Type>::PoolAllocationMaximumSize)
		{
			manage = true;
			len = AllocatorConstants<Type>::PoolAllocationMinimumSize;

			while (n > len)
			{
				len *= 2;
			}
		}

		return std::make_pair(manage, len);
	}

	template<typename Type>
	void* AllocatorBase<Type>::AllocateFromPool(const std::size_t n) noexcept
	{
		void* retbuf{ nullptr };

		const auto [manage, len] = GetAllocationDetails(n);

		if (manage)
		{
			const auto GetBuffer = [](MemoryPoolData<Type>* mpd, const std::size_t len) -> void*
			{
				// If we have free buffers reuse one
				{
					auto fbp = mpd->FreeBufferPool.WithUniqueLock();
					if (!fbp->empty())
					{
						auto bufptr = reinterpret_cast<void*>(fbp->front());
						fbp->pop_front();

						return bufptr;
					}
				}

				// No free buffers were available so we
				// try to allocate a new one
				try
				{
					typename MemoryPoolData<Type>::MemoryBufferType buffer(len, Byte{ 0 });

					void* bufptr = buffer.data();

					mpd->MemoryBufferPool.WithUniqueLock()->emplace(reinterpret_cast<std::uintptr_t>(bufptr),
																	std::move(buffer));

					return bufptr;
				}
				catch (...) {}

				return nullptr;
			};

			auto mpm = GetMemoryPoolMap<Type>().WithSharedLock();

			// Get the pool for the allocation size
			if (const auto it = mpm->find(len); it != mpm->end())
			{
				retbuf = GetBuffer(it->second.get(), len);
			}
			else
			{
				mpm.UnlockShared();

				auto mpm2 = GetMemoryPoolMap<Type>().WithUniqueLock();
				if (const auto it2 = mpm2->find(len); it2 != mpm2->end())
				{
					retbuf = GetBuffer(it2->second.get(), len);
				}
				else
				{
					// No pool was available for the allocation size; 
					// allocate a new one
					try
					{
						const auto [it3, inserted] = mpm2->insert({ len, std::make_unique<MemoryPoolData<Type>>() });
						if (inserted)
						{
							retbuf = GetBuffer(it3->second.get(), len);
						}
					}
					catch (...) {}
				}
			}
		}
		else
		{
			try
			{
				retbuf = GetUnmanagedAllocator<Type>().allocate(len);
			}
			catch (...) {}
		}

		DbgInvoke([&]()
		{
			GetAllocatorStats<Type>().WithUniqueLock()->AddAllocation(retbuf, len);
		});

		return retbuf;
	}

	template<typename Type>
	bool AllocatorBase<Type>::FreeToPool(void* p, const std::size_t n) noexcept
	{
		auto found = false;

		const auto [manage, len] = GetAllocationDetails(n);

		if (manage)
		{
			const auto mpm = GetMemoryPoolMap<Type>().WithSharedLock();

			if (const auto it = mpm->find(len); it != mpm->end())
			{
				MemoryPoolData<Type>* mpd = it->second.get();

				mpd->MemoryBufferPool.WithSharedLock([&](const auto& mbp)
				{
					if (const auto it = mbp.find(reinterpret_cast<std::uintptr_t>(p)); it != mbp.end())
					{
						found = true;
					}
				});

				if (found)
				{
					auto reused = false;

					try
					{
						mpd->FreeBufferPool.WithUniqueLock([&](auto& fbp)
						{
							// If we have too many free buffers don't reuse this one
							if (fbp.size() <= AllocatorConstants<Type>::MaximumFreeBuffersPerPool &&
								(fbp.size() * len <= AllocatorConstants<Type>::MaximumFreeBufferPoolSize))
							{
								fbp.emplace_front(reinterpret_cast<std::uintptr_t>(p));
								reused = true;

								if constexpr (std::is_same_v<Type, ProtectedPool>)
								{
									// Wipe all data from used memory
									MemClear(p, len);
								}
							}
						});
					}
					catch (...) {}

					if (!reused)
					{
						// Reuse conditions were not met or exception was thrown; release the memory
						// Memory buffer allocator will wipe memory so we don't do that here
						mpd->MemoryBufferPool.WithUniqueLock()->erase(reinterpret_cast<std::uintptr_t>(p));
					}
				}
			}
		}
		else
		{
			// Unmanaged allocator will wipe memory so we don't do that here
			GetUnmanagedAllocator<Type>().deallocate(static_cast<Byte*>(p), len);

			found = true;
		}

		DbgInvoke([&]()
		{
			if (found)
			{
				GetAllocatorStats<Type>().WithUniqueLock()->RemoveAllocation(p, len);
			}
		});

		return found;
	}

	template<typename Type>
	void AllocatorBase<Type>::FreeUnused() noexcept
	{
		auto mpm = GetMemoryPoolMap<Type>().WithUniqueLock();

		for (auto it = mpm->begin(); it != mpm->end();)
		{
			MemoryPoolData<Type>* mpd = it->second.get();

			auto remove = false;

			auto mbp = mpd->MemoryBufferPool.WithUniqueLock();
			auto fbp = mpd->FreeBufferPool.WithUniqueLock();

			if (mbp->size() == fbp->size())
			{
				// All buffers in the pool are free
				// so we can remove the pool
				remove = true;
			}
			else
			{
				// Remove all free buffers from the pool
				for (auto it2 = fbp->begin(); it2 != fbp->end(); ++it2)
				{
					if (const auto it3 = mbp->find(*it2); it3 != mbp->end())
					{
						mbp->erase(it3);
					}
					else
					{
						// If we don't find the free buffer
						// in the pool something is wrong
						assert(false);
					}
				}

				fbp->clear();

				if (mbp->size() == 0) remove = true;
			}

			if (remove) it = mpm->erase(it);
			else ++it;
		}
	}

	// Specific instantiations
	template Export void AllocatorBase<NormalPool>::LogStatistics() noexcept;
	template Export void AllocatorBase<NormalPool>::FreeUnused() noexcept;
	template Export void* AllocatorBase<NormalPool>::AllocateFromPool(const std::size_t n) noexcept;
	template Export bool AllocatorBase<NormalPool>::FreeToPool(void* p, const std::size_t n) noexcept;

	template Export void AllocatorBase<ProtectedPool>::LogStatistics() noexcept;
	template Export void AllocatorBase<ProtectedPool>::FreeUnused() noexcept;
	template Export void* AllocatorBase<ProtectedPool>::AllocateFromPool(const std::size_t n) noexcept;
	template Export bool AllocatorBase<ProtectedPool>::FreeToPool(void* p, const std::size_t n) noexcept;
}