// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"
#include "AllocatorStats.h"

namespace QuantumGate::Implementation::Memory::LinearPoolAllocator
{
	constexpr std::size_t MaxAllocationSize = MemorySize::_4MB;

	template<typename Type>
	struct MemoryBufferData final
	{
		using AllocatorType = std::conditional_t<std::is_same_v<Type, NormalPool>, FreeStoreAllocator<Byte>,
			std::conditional_t<std::is_same_v<Type, ProtectedPool>, ProtectedFreeStoreAllocator<Byte>, void>>;

		using MemoryBufferType = std::vector<Byte, AllocatorType>;
		using MemoryAllocationList = std::list<std::uintptr_t>;

		MemoryBufferType Buffer;
		MemoryAllocationList Allocations;
		std::size_t FreeOffset{ 0 };
	};

	template<typename Type>
	using MemoryPoolList = std::list<MemoryBufferData<Type>>;

	template<typename Type>
	using MemoryPoolList_ThS = Concurrency::ThreadSafe<MemoryPoolList<Type>, std::mutex>;

	extern MemoryPoolList_ThS<NormalPool> NormalMemoryPool;
	extern MemoryPoolList_ThS<ProtectedPool> ProtectedMemoryPool;

	extern AllocatorStats_ThS NormalAllocatorStats;
	extern AllocatorStats_ThS ProtectedAllocatorStats;

	template<typename Type>
	inline auto& GetMemoryPool() noexcept
	{
		if constexpr (std::is_same_v<Type, NormalPool>)
		{
			return NormalMemoryPool;
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return ProtectedMemoryPool;
		}
		else
		{
			static_assert(false, "Unsupported type used");
		}
	}

	template<typename Type>
	inline auto& GetAllocatorStats() noexcept
	{
		if constexpr (std::is_same_v<Type, NormalPool>)
		{
			return NormalAllocatorStats;
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return ProtectedAllocatorStats;
		}
		else
		{
			static_assert(false, "Unsupported type used");
		}
	}

	template<typename Type>
	inline const WChar* GetAllocatorName() noexcept
	{
		if constexpr (std::is_same_v<Type, NormalPool>)
		{
			return L"LinearPoolAllocator";
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return L"ProtectedLinearPoolAllocator";
		}
		else
		{
			static_assert(false, "Unsupported type used");
		}
	}
}