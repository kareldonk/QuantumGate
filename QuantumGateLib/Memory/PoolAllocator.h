// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "LinearPoolAllocator.h"

namespace QuantumGate::Implementation::Memory::PoolAllocator
{
	template<typename Type>
	class Export AllocatorBase
	{
	public:
		static void LogStatistics() noexcept;
		static void FreeUnused() noexcept;

	protected:
		static std::pair<bool, std::size_t> GetAllocationDetails(const std::size_t n) noexcept;
		[[nodiscard]] static void* AllocateFromPool(const std::size_t n) noexcept;
		[[nodiscard]] static bool FreeToPool(void* p, const std::size_t n) noexcept;
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

		[[nodiscard]] inline pointer allocate(const std::size_t n)
		{
			auto retval = this->AllocateFromPool(n * sizeof(T));
			if (retval == nullptr) throw std::bad_alloc();

			return static_cast<T*>(retval);
		}

		inline void deallocate(pointer p, const std::size_t n)
		{
			assert(p != nullptr);

			if (!this->FreeToPool(p, n * sizeof(T)))
			{
				throw std::invalid_argument("Trying to free memory that wasn't allocated with this allocator.");
			}
		}
	};

	template<typename T>
	using ProtectedAllocator = Allocator<T, ProtectedPool>;
}
