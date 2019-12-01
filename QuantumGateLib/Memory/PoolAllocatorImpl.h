// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferIO.h"
#include "AllocatorStats.h"

#include <map>

namespace QuantumGate::Implementation::Memory::PoolAllocator
{
	template<typename Type>
	struct AllocatorConstants;

	template<> struct AllocatorConstants<NormalPool>
	{
		static constexpr const std::size_t PoolAllocationMinimumSize{ MemorySize::_65KB };
		static constexpr const std::size_t PoolAllocationMaximumSize{ MemorySize::_4MB };
		static constexpr const std::size_t MaximumFreeBufferPoolSize{ MemorySize::_16MB };
		static constexpr const std::size_t MaximumFreeBuffersPerPool{ 20 };
	};

	template<> struct AllocatorConstants<ProtectedPool>
	{
		static constexpr const std::size_t PoolAllocationMinimumSize{ MemorySize::_1B };
		static constexpr const std::size_t PoolAllocationMaximumSize{ MemorySize::_4MB };
		static constexpr const std::size_t MaximumFreeBufferPoolSize{ MemorySize::_16MB };
		static constexpr const std::size_t MaximumFreeBuffersPerPool{ 20 };
	};

	template<typename MemoryBufferType>
	using MemoryBufferPool_T = std::map<std::uintptr_t, MemoryBufferType>;

	template<typename MemoryBufferType>
	using MemoryBufferPool_ThS = Concurrency::ThreadSafe<MemoryBufferPool_T<MemoryBufferType>, Concurrency::SharedSpinMutex>;

	using FreeBufferPool_T = std::list<std::uintptr_t>;
	using FreeBufferPool_ThS = Concurrency::ThreadSafe<FreeBufferPool_T, Concurrency::SpinMutex>;

	template<typename Type>
	struct MemoryPoolData final
	{
		using AllocatorType = std::conditional_t<std::is_same_v<Type, NormalPool>, LinearPoolAllocator::Allocator<Byte>,
			std::conditional_t<std::is_same_v<Type, ProtectedPool>, LinearPoolAllocator::ProtectedAllocator<Byte>, void>>;

		using MemoryBufferType = std::vector<Byte, AllocatorType>;

		MemoryBufferPool_ThS<MemoryBufferType> MemoryBufferPool;
		FreeBufferPool_ThS FreeBufferPool;
	};

	template<typename Type>
	using MemoryPoolMap_T = std::map<std::size_t, std::unique_ptr<MemoryPoolData<Type>>>;

	template<typename Type>
	using MemoryPoolMap_ThS = Concurrency::ThreadSafe<MemoryPoolMap_T<Type>, std::shared_mutex>;

	extern MemoryPoolMap_ThS<NormalPool> NormalMemoryPoolMap;
	extern MemoryPoolMap_ThS<ProtectedPool> ProtectedMemoryPoolMap;
	
	extern FreeStoreAllocator<Byte> NormalUnmanagedAllocator;
	extern ProtectedFreeStoreAllocator<Byte> ProtectedUnmanagedAllocator;
	
	extern AllocatorStats_ThS NormalPoolAllocatorStats;
	extern AllocatorStats_ThS ProtectedPoolAllocatorStats;

	template<typename Type>
	inline auto& GetMemoryPoolMap() noexcept
	{
		if constexpr (std::is_same_v<Type, NormalPool>)
		{
			return NormalMemoryPoolMap;
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return ProtectedMemoryPoolMap;
		}
		else
		{
			static_assert(false, "Unsupported type used");
		}
	}

	template<typename Type>
	inline auto& GetUnmanagedAllocator() noexcept
	{
		if constexpr (std::is_same_v<Type, NormalPool>)
		{
			return NormalUnmanagedAllocator;
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return ProtectedUnmanagedAllocator;
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
			return NormalPoolAllocatorStats;
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return ProtectedPoolAllocatorStats;
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
			return L"PoolAllocator";
		}
		else if constexpr (std::is_same_v<Type, ProtectedPool>)
		{
			return L"ProtectedPoolAllocator";
		}
		else
		{
			static_assert(false, "Unsupported type used");
		}
	}
}