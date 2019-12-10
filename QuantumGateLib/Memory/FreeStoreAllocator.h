// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Memory
{
	template<typename T>
	class FreeStoreAllocator final
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		FreeStoreAllocator() noexcept = default;

		template<typename Other>
		FreeStoreAllocator(const FreeStoreAllocator<Other>&) noexcept {}

		FreeStoreAllocator(const FreeStoreAllocator&) noexcept = default;
		FreeStoreAllocator(FreeStoreAllocator&&) noexcept = default;
		~FreeStoreAllocator() = default;
		FreeStoreAllocator& operator=(const FreeStoreAllocator&) noexcept = default;
		FreeStoreAllocator& operator=(FreeStoreAllocator&&) noexcept = default;

		template<typename Other>
		inline bool operator==(const FreeStoreAllocator<Other>&) const noexcept
		{
			return true;
		}

		template<typename Other>
		inline bool operator!=(const FreeStoreAllocator<Other>&) const noexcept
		{
			return false;
		}

		[[nodiscard]] inline pointer allocate(const std::size_t n)
		{
			return static_cast<T*>(::operator new(n * sizeof(T)));
		}

		inline void deallocate(pointer p, const std::size_t n) noexcept
		{
			assert(p != nullptr);
			
			// Clear memory
			MemClear(p, n * sizeof(T));

			::operator delete(p);
		}

		inline static void MemInit(void* dst, const std::size_t len) noexcept
		{
			memset(dst, 0, len);
		}

		inline static void MemClear(void* dst, const std::size_t len) noexcept
		{
			::SecureZeroMemory(dst, len);
		}
	};
}

#define MemInit(a, l) QuantumGate::Implementation::Memory::FreeStoreAllocator<void>::MemInit(a, l)
#define MemClear(a, l) QuantumGate::Implementation::Memory::FreeStoreAllocator<void>::MemClear(a, l)