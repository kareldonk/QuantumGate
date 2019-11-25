// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "PoolAllocator.h"
#include "BufferIO.h"
#include "..\Concurrency\ThreadSafe.h"
#include "..\Concurrency\SharedSpinMutex.h"

#include <map>

namespace QuantumGate::Implementation::Memory
{
	static constexpr const std::size_t PoolAllocationMinimumSize{ MaxSize::_65KB };
	static constexpr const std::size_t PoolAllocationMaximumSize{ MaxSize::_4MB };
	static constexpr const std::size_t MaximumFreeBufferPoolSize{ MaxSize::_16MB };
	static constexpr const std::size_t MaximumFreeBuffersPerPool{ 20 };

	using PoolVector = std::vector<Byte, Allocator<Byte>>;
	using MemoryPool_T = std::map<std::uintptr_t, PoolVector>;
	using MemoryPool_ThS = Concurrency::ThreadSafe<MemoryPool_T, Concurrency::SharedSpinMutex>;

	using FreeBufferPool_T = std::list<std::uintptr_t>;
	using FreeBufferPool_ThS = Concurrency::ThreadSafe<FreeBufferPool_T, Concurrency::SpinMutex>;

	struct PoolAllocStats_T final
	{
		std::set<std::size_t> Sizes;
	};

	using PoolAllocStats_ThS = Concurrency::ThreadSafe<PoolAllocStats_T, Concurrency::SharedSpinMutex>;

	struct MemoryPoolData final
	{
		MemoryPool_ThS MemoryPool;
		FreeBufferPool_ThS FreeBufferPool;
	};

	using MemoryPoolMap_T = std::map<std::size_t, std::unique_ptr<MemoryPoolData>>;
	using MemoryPoolMap_ThS = Concurrency::ThreadSafe<MemoryPoolMap_T, Concurrency::SharedSpinMutex>;

	static MemoryPoolMap_ThS MemoryPoolMap;
	static PoolAllocStats_ThS PoolAllocStats;

	void PoolAllocatorBase::LogStatistics() noexcept
	{
		String output{ L"\r\n\r\nPoolAllocator statistics:\r\n\r\n" };

		MemoryPoolMap.WithSharedLock([&](const MemoryPoolMap_T& mpdc)
		{
			for (auto it = mpdc.begin(); it != mpdc.end(); ++it)
			{
				output += Util::FormatString(L"Allocation size: %zu bytes -> Pool size: %zu, Free: %zu\r\n",
											 it->first, it->second->MemoryPool.WithSharedLock()->size(),
											 it->second->FreeBufferPool.WithUniqueLock()->size());
			}
		});

		DbgInvoke([&]()
		{
			PoolAllocStats.WithSharedLock([&](const PoolAllocStats_T& stats)
			{
				output += L"\r\nPoolAllocator allocation sizes:\r\n";

				for (const auto size : stats.Sizes)
				{
					output += Util::FormatString(L"%u\r\n", size);
				}
			});
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
						PoolVector buffer(len, Byte{ 0 });

						mpd->MemoryPool.WithUniqueLock([&](MemoryPool_T& mp)
						{
							retbuf = buffer.data();
							mp.emplace(reinterpret_cast<std::uintptr_t>(retbuf), std::move(buffer));
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
				retbuf = ::operator new(len);
			}
			catch (...) {}
		}

		DbgInvoke([&]()
		{
			PoolAllocStats.WithUniqueLock()->Sizes.insert(n);
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
				mpd->MemoryPool.WithSharedLock([&](const MemoryPool_T& mp)
				{
					if (const auto it = mp.find(reinterpret_cast<std::uintptr_t>(p)); it != mp.end())
					{
						found = true;
					}
				});

				if (found)
				{
					// Note that we don't clear the memory in this case because
					// PoolVector's allocator will do that for us upon release

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
						mpd->MemoryPool.WithUniqueLock()->erase(reinterpret_cast<std::uintptr_t>(p));
					}
				}
			}
		}
		else
		{
			// Clear memory
			MemClear(p, len);

			::operator delete(p);

			found = true;
		}

		return found;
	}
}