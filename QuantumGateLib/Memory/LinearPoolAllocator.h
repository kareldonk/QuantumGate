// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "ProtectedFreeStoreAllocator.h"

namespace QuantumGate::Implementation::Memory
{
	struct NormalPool final {};
	struct ProtectedPool final {};
}

namespace QuantumGate::Implementation::Memory::LinearPoolAllocator
{
	template<typename Type>
	class Export AllocatorBase
	{
	public:
		static void LogStatistics() noexcept;

	protected:
		[[nodiscard]] void* Allocate(const std::size_t len);
		void Deallocate(void* p, const std::size_t len);
	};

	template<typename T, typename Type = NormalPool>
	class Allocator final : public AllocatorBase<Type>
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		Allocator() noexcept = default;

		template<typename Other>
		Allocator(const Allocator<Other, Type>&) noexcept {}

		Allocator(const Allocator&) noexcept = default;
		Allocator(Allocator&&) noexcept = default;
		~Allocator() = default;
		Allocator& operator=(const Allocator&) noexcept = default;
		Allocator& operator=(Allocator&&) noexcept = default;

		template<typename Other>
		inline bool operator==(const Allocator<Other, Type>&) const noexcept
		{
			return true;
		}

		template<typename Other>
		inline bool operator!=(const Allocator<Other, Type>&) const noexcept
		{
			return false;
		}

		[[nodiscard]] pointer allocate(const std::size_t n)
		{
			return static_cast<pointer>(this->Allocate(n * sizeof(T)));
		}

		void deallocate(pointer p, const std::size_t n) noexcept
		{
			assert(p != nullptr);

			this->Deallocate(p, n * sizeof(T));
		}
	};

	template<typename T>
	using ProtectedAllocator = Allocator<T, ProtectedPool>;
}