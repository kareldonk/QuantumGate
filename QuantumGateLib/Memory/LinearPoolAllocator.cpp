// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "LinearPoolAllocator.h"
#include "LinearPoolAllocatorImpl.h"

namespace QuantumGate::Implementation::Memory::LinearPoolAllocator
{
	template<typename Type>
	void AllocatorBase<Type>::LogStatistics() noexcept
	{
		auto output = Util::FormatString(L"\r\n\r\n%s statistics:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());

		GetMemoryPool<Type>().WithUniqueLock([&](const auto& mp)
		{
			std::size_t total{ 0 };

			for (auto it = mp.begin(); it != mp.end(); ++it)
			{
				const auto pool_size = it->Buffer.size();

				output += Util::FormatString(L"Pool size: %zu bytes, Free: %zu bytes, Num allocs in use: %zu\r\n",
											 it->Buffer.size(), it->Buffer.size() - it->FreeOffset,
											 it->Allocations.size());
			}
		});

		DbgInvoke([&]()
		{
			auto& as = GetAllocatorStats<Type>();

			output += Util::FormatString(L"\r\n%s allocation sizes:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());
			output += as.WithSharedLock()->GetAllSizes();
			output += Util::FormatString(L"\r\n%s memory in use:\r\n-----------------------------------------------\r\n", GetAllocatorName<Type>());
			output += as.WithSharedLock()->GetMemoryInUse();
		});

		output += L"\r\n";

		SLogInfo(output);
	}

	template<typename Type>
	void* AllocatorBase<Type>::Allocate(const std::size_t len)
	{
		assert(len <= MaxAllocationSize);

		void* retbuf{ nullptr };

		if (len > MaxAllocationSize)
		{
			throw std::invalid_argument("Attempt to allocate more than the maximum allowed allocation size");
		}
		else
		{
			GetMemoryPool<Type>().WithUniqueLock([&](auto& mp)
			{
				// Check if there is a buffer in the pool with enough
				// free space that can accomodate the requested size,
				// and if so, use space from that buffer
				for (auto& mbd : mp)
				{
					if (mbd.Buffer.size() - mbd.FreeOffset >= len)
					{
						retbuf = mbd.Buffer.data() + mbd.FreeOffset;
						mbd.FreeOffset += len;

						try
						{
							mbd.Allocations.emplace_front(reinterpret_cast<std::uintptr_t>(retbuf));
							break;
						}
						catch (...)
						{
							// Undo
							mbd.FreeOffset -= len;
							retbuf = nullptr;
							return;
						}
					}
				}

				// If no buffer was found with enough free space
				// allocate a new buffer
				if (retbuf == nullptr)
				{
					try
					{
						MemoryBufferData<Type> mbd;
						mbd.Buffer.resize(MaxAllocationSize, Byte{ 0 });

						retbuf = mbd.Buffer.data();
						mbd.FreeOffset += len;

						mbd.Allocations.emplace_front(reinterpret_cast<std::uintptr_t>(retbuf));

						mp.emplace_front(std::move(mbd));
					}
					catch (...)
					{
						retbuf = nullptr;
					}
				}
			});
		}

		DbgInvoke([&]()
		{
			auto& as = GetAllocatorStats<Type>();

			as.WithUniqueLock([&](auto& stats)
			{
				stats.Sizes.insert(len);

				if (retbuf != nullptr) stats.MemoryInUse.insert({ reinterpret_cast<std::uintptr_t>(retbuf), len });
			});
		});

		return retbuf;
	}

	template<typename Type>
	void AllocatorBase<Type>::Deallocate(void* p, const std::size_t len)
	{
		auto found = false;

		GetMemoryPool<Type>().WithUniqueLock([&](auto& mp)
		{
			for (auto it = mp.begin(); it != mp.end(); ++it)
			{
				if (it->Allocations.remove(reinterpret_cast<std::uintptr_t>(p)) > 0)
				{
					found = true;

					if constexpr (std::is_same_v<Type, ProtectedPool>)
					{
						// Wipe all data from used memory
						MemClear(p, len);
					}

					// If we don't have any allocations for
					// this pool we can release it
					if (it->Allocations.size() == 0)
					{
						mp.erase(it);
					}

					break;
				}
			}
		});

		if (!found)
		{
			throw std::invalid_argument("Trying to free memory that wasn't allocated with this allocator.");
		}

		DbgInvoke([&]()
		{
			if (found)
			{
				auto& as = GetAllocatorStats<Type>();

				as.WithUniqueLock([&](auto& stats)
				{
					stats.MemoryInUse.erase(reinterpret_cast<std::uintptr_t>(p));
				});
			}
		});
	}

	// Specific instantiations
	template Export void AllocatorBase<NormalPool>::LogStatistics() noexcept;
	template Export void* AllocatorBase<NormalPool>::Allocate(const std::size_t len);
	template Export void AllocatorBase<NormalPool>::Deallocate(void* p, const std::size_t len);

	template Export void AllocatorBase<ProtectedPool>::LogStatistics() noexcept;
	template Export void* AllocatorBase<ProtectedPool>::Allocate(const std::size_t len);
	template Export void AllocatorBase<ProtectedPool>::Deallocate(void* p, const std::size_t len);
}