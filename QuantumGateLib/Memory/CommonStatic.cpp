// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "LinearPoolAllocatorImpl.h"
#include "PoolAllocatorImpl.h"

// The order of definition of the below variables is important because
// of initialization dependencies and destruction order

namespace QuantumGate::Implementation::Memory
{
	AllocatorStats_ThS ProtectedFreeStoreAllocatorStats;
}

namespace QuantumGate::Implementation::Memory::LinearPoolAllocator
{
	MemoryPoolList_ThS<NormalPool> NormalMemoryPool;
	MemoryPoolList_ThS<ProtectedPool> ProtectedMemoryPool;

	AllocatorStats_ThS NormalAllocatorStats;
	AllocatorStats_ThS ProtectedAllocatorStats;
}

namespace QuantumGate::Implementation::Memory::PoolAllocator
{
	MemoryPoolMap_ThS<NormalPool> NormalMemoryPoolMap;
	MemoryPoolMap_ThS<ProtectedPool> ProtectedMemoryPoolMap;

	AllocatorStats_ThS NormalPoolAllocatorStats;
	AllocatorStats_ThS ProtectedPoolAllocatorStats;

	FreeStoreAllocator<Byte> NormalUnmanagedAllocator;
	ProtectedFreeStoreAllocator<Byte> ProtectedUnmanagedAllocator;
}