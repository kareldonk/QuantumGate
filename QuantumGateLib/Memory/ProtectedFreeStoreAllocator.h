// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "FreeStoreAllocator.h"

namespace QuantumGate::Implementation::Memory
{
	class BadAllocException final : public std::exception
	{
	public:
		BadAllocException(const char* message) noexcept : std::exception(message) {}
	};

	class Export ProtectedFreeStoreAllocatorBase
	{
	public:
		static void LogStatistics() noexcept;

	protected:
		void* Allocate(const std::size_t len);
		void Deallocate(void* p, const std::size_t len) noexcept;
	};

	template<typename T>
	class ProtectedFreeStoreAllocator final : public ProtectedFreeStoreAllocatorBase
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		ProtectedFreeStoreAllocator() noexcept = default;

		template<typename Other>
		ProtectedFreeStoreAllocator(const ProtectedFreeStoreAllocator<Other>&) noexcept {}

		ProtectedFreeStoreAllocator(const ProtectedFreeStoreAllocator&) noexcept = default;
		ProtectedFreeStoreAllocator(ProtectedFreeStoreAllocator&&) noexcept = default;
		~ProtectedFreeStoreAllocator() = default;
		ProtectedFreeStoreAllocator& operator=(const ProtectedFreeStoreAllocator&) noexcept = default;
		ProtectedFreeStoreAllocator& operator=(ProtectedFreeStoreAllocator&&) noexcept = default;

		template<typename Other>
		inline bool operator==(const ProtectedFreeStoreAllocator<Other>&) const noexcept
		{
			return true;
		}

		template<typename Other>
		inline bool operator!=(const ProtectedFreeStoreAllocator<Other>&) const noexcept
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