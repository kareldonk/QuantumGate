// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Memory
{
	template<class T>
	class Allocator
	{
	public:
		using value_type = T;
		using pointer = T* ;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		Allocator() noexcept = default;

		template<class Other>
		Allocator(const Allocator<Other>&) noexcept {}

		Allocator(const Allocator&) = default;
		Allocator(Allocator&&) = default;
		virtual ~Allocator() = default;
		Allocator& operator=(const Allocator&) = default;
		Allocator& operator=(Allocator&&) = default;

		template<class Other>
		inline bool operator==(const Allocator<Other>&) const noexcept
		{
			return true;
		}

		template<class Other>
		inline bool operator!=(const Allocator<Other>&) const noexcept
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

		inline static void MemInit(void* dst, const Size len) noexcept
		{
			memset(dst, 0, len);
		}

		inline static void MemClear(void* dst, const Size len) noexcept
		{
			::SecureZeroMemory(dst, len);
		}
	};
}

#define MemInit(a, l) QuantumGate::Implementation::Memory::Allocator<void>::MemInit(a, l)
#define MemClear(a, l) QuantumGate::Implementation::Memory::Allocator<void>::MemClear(a, l)