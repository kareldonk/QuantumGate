// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "PoolAllocator.h"

namespace QuantumGate::Implementation::Memory
{
	template<typename T>
	using DefaultAllocator = PoolAllocator::Allocator<T>;

	template<typename T>
	using DefaultProtectedAllocator = PoolAllocator::ProtectedAllocator<T>;
}