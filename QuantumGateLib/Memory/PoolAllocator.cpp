// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PoolAllocator.h"
#include "AllocatorStats.h"
#include "Allocator.h"
#include "BufferIO.h"

namespace QuantumGate::Implementation::Memory
{
	static constexpr const std::size_t PoolAllocationMinimumSize{ MaxSize::_65KB };
	static constexpr const std::size_t PoolAllocationMaximumSize{ MaxSize::_4MB };
	static constexpr const std::size_t MaximumFreeBufferPoolSize{ MaxSize::_16MB };
	static constexpr const std::size_t MaximumFreeBuffersPerPool{ 20 };

	using MemoryBuffer = std::vector<Byte, Allocator<Byte>>;
	using MemoryBufferPool_T = std::map<std::uintptr_t, MemoryBuffer>;
	using MemoryBufferPool_ThS = Concurrency::ThreadSafe<MemoryBufferPool_T, Concurrency::SharedSpinMutex>;

	using FreeBufferPool_T = std::list<std::uintptr_t>;
	using FreeBufferPool_ThS = Concurrency::ThreadSafe<FreeBufferPool_T, Concurrency::SpinMutex>;

	struct MemoryPoolData final
	{
		MemoryBufferPool_ThS MemoryBufferPool;
		FreeBufferPool_ThS FreeBufferPool;
	};

	using MemoryPoolMap_T = std::map<std::size_t, std::unique_ptr<MemoryPoolData>>;
	using MemoryPoolMap_ThS = Concurrency::ThreadSafe<MemoryPoolMap_T, Concurrency::SharedSpinMutex>;

	static MemoryPoolMap_ThS MemoryPoolMap;
	static Allocator<Byte> UnmanagedAllocator;

	static AllocatorStats_ThS PoolAllocStats;

	void PoolAllocatorBase::LogStatistics() noexcept
	{
		String output{ L"\r\n\r\nPoolAllocator statistics:\r\n-------------------------------------\r\n" };

		MemoryPoolMap.WithSharedLock([&](const MemoryPoolMap_T& mpdc)
		{
			Size total{ 0 };

			for (auto it = mpdc.begin(); it != mpdc.end(); ++it)
			{
				const auto pool_size = it->second->MemoryBufferPool.WithSharedLock()->size();

				output += Util::FormatString(L"Allocation size: %zu bytes -> Pool size: %zu, Free: %zu\r\n",
											 it->first, pool_size,
											 it->second->FreeBufferPool.WithUniqueLock()->size());

				total += it->first * pool_size;
			}

			output += Util::FormatString(L"\r\nTotal in managed pools: %u bytes\r\n", total);
		});

		DbgInvoke([&]()
		{
			output += PoolAllocStats.WithSharedLock()->GetAllSizes(L"\r\nPoolAllocator allocation sizes:\r\n-------------------------------------\r\n");
			output += PoolAllocStats.WithSharedLock()->GetMemoryInUse(L"\r\nPoolAllocator memory in use:\r\n-------------------------------------\r\n");
		});

		output += L"\r\n";

		SLogInfo(output);
	}

	std::pair<bool, std::size_t> PoolAllocatorBase::GetAllocationDetails(const std::size_t n) noexcept
	{
		auto len = n;
		auto manage = false;

		if (n >= PoolAllocationMinimumSize && n <= PoolAllocationMaximumSize)
		{
			manage = true;
			len = PoolAllocationMinimumSize;

			while (n > len)
			{
				len *= 2;
			}
		}

		return std::make_pair(manage, len);
	}

	void* PoolAllocatorBase::AllocateFromPool(const std::size_t n) noexcept
	{
		void* retbuf{ nullptr };

		const auto [manage, len] = GetAllocationDetails(n);

		if (manage)
		{
			MemoryPoolData* mpd{ nullptr };

			MemoryPoolMap.WithUniqueLock([&](MemoryPoolMap_T& mpdc)
			{
				// Get the pool for the allocation size
				if (const auto it = mpdc.find(len); it != mpdc.end())
				{
					mpd = it->second.get();
				}
				else
				{
					// No pool was available for the allocation size; 
					// allocate a new one
					try
					{
						const auto [it2, inserted] = mpdc.insert({ len, std::make_unique<MemoryPoolData>() });
						if (inserted)
						{
							mpd = it2->second.get();
						}
					}
					catch (...) {}
				}
			});

			if (mpd != nullptr)
			{
				// If we have free buffers reuse one
				mpd->FreeBufferPool.WithUniqueLock([&](FreeBufferPool_T& fbp)
				{
					if (!fbp.empty())
					{
						retbuf = reinterpret_cast<void*>(fbp.front());
						fbp.pop_front();
					}
				});

				if (retbuf == nullptr)
				{
					// No free buffers were available so we
					// try to allocate a new one
					try
					{
						MemoryBuffer buffer(len, Byte{ 0 });

						mpd->MemoryBufferPool.WithUniqueLock([&](MemoryBufferPool_T& mbp)
						{
							retbuf = buffer.data();
							mbp.emplace(reinterpret_cast<std::uintptr_t>(retbuf), std::move(buffer));
						});
					}
					catch (...)
					{
						retbuf = nullptr;
					}
				}
			}
		}
		else
		{
			try
			{
				retbuf = UnmanagedAllocator.allocate(len);
			}
			catch (...) {}
		}

		DbgInvoke([&]()
		{
			PoolAllocStats.WithUniqueLock([&](auto& stats)
			{
				stats.Sizes.insert(n);
				
				if (retbuf != nullptr) stats.MemoryInUse.insert({ reinterpret_cast<std::uintptr_t>(retbuf), len });
			});
		});

		return retbuf;
	}

	bool PoolAllocatorBase::FreeToPool(void* p, const std::size_t n) noexcept
	{
		auto found = false;

		const auto [manage, len] = GetAllocationDetails(n);

		if (manage)
		{
			MemoryPoolData* mpd{ nullptr };

			MemoryPoolMap.WithSharedLock([&](const MemoryPoolMap_T& mpdc)
			{
				if (const auto it = mpdc.find(len); it != mpdc.end())
				{
					mpd = it->second.get();
				}
			});

			if (mpd != nullptr)
			{
				mpd->MemoryBufferPool.WithSharedLock([&](const MemoryBufferPool_T& mbp)
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
						mpd->FreeBufferPool.WithUniqueLock([&](FreeBufferPool_T& fbp)
						{
							// If we have too many free buffers
							// don't reuse this one
							if (fbp.size() <= MaximumFreeBuffersPerPool &&
								(fbp.size() * len <= MaximumFreeBufferPoolSize))
							{
								fbp.emplace_front(reinterpret_cast<std::uintptr_t>(p));
								reused = true;
							}
						});
					}
					catch (...) {}

					if (!reused)
					{
						// Reuse conditions were not met or exception was thrown; release the memory
						mpd->MemoryBufferPool.WithUniqueLock()->erase(reinterpret_cast<std::uintptr_t>(p));
					}
				}
			}
		}
		else
		{
			UnmanagedAllocator.deallocate(static_cast<Byte*>(p), len);

			found = true;
		}

		DbgInvoke([&]()
		{
			if (found)
			{
				PoolAllocStats.WithUniqueLock([&](auto& stats)
				{
					stats.MemoryInUse.erase(reinterpret_cast<std::uintptr_t>(p));
				});
			}
		});

		return found;
	}
}