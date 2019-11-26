// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Allocator.h"

namespace QuantumGate::Implementation::Memory
{
	class BadAllocException final : public std::exception
	{
	public:
		BadAllocException(const char* message) noexcept : std::exception(message) {}
	};

	class Export ProtectedAllocatorBase
	{
	public:
		static void LogStatistics() noexcept;

	protected:
		void* Allocate(const Size len);
		void Deallocate(void* p, const Size len) noexcept;
	};

	template<class T>
	class ProtectedAllocator final : public ProtectedAllocatorBase
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		ProtectedAllocator() noexcept = default;

		template<class Other>
		ProtectedAllocator(const ProtectedAllocator<Other>&) noexcept {}

		ProtectedAllocator(const ProtectedAllocator&) = default;
		ProtectedAllocator(ProtectedAllocator&&) = default;
		virtual ~ProtectedAllocator() = default;
		ProtectedAllocator& operator=(const ProtectedAllocator&) = default;
		ProtectedAllocator& operator=(ProtectedAllocator&&) = default;

		template<class Other>
		inline bool operator==(const ProtectedAllocator<Other>&) const noexcept
		{
			return true;
		}

		template<class Other>
		inline bool operator!=(const ProtectedAllocator<Other>&) const noexcept
		{
			return false;
		}

		[[nodiscard]] pointer allocate(const std::size_t n)
		{
			return static_cast<pointer>(Allocate(n * sizeof(T)));
		}

		void deallocate(pointer p, const std::size_t n) noexcept
		{
			assert(p != nullptr);

			Deallocate(p, n * sizeof(T));
		}
	};
}