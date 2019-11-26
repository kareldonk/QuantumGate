// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Memory
{
	class Export PoolAllocatorBase
	{
	public:
		static void LogStatistics() noexcept;

	protected:
		static std::pair<bool, std::size_t> GetAllocationDetails(const std::size_t n) noexcept;
		[[nodiscard]] static void* AllocateFromPool(const std::size_t n) noexcept;
		[[nodiscard]] static bool FreeToPool(void* p, const std::size_t n) noexcept;
	};

	template<typename T>
	class PoolAllocator final : public PoolAllocatorBase
	{
	public:
		using value_type = T;
		using pointer = T*;
		using propagate_on_container_move_assignment = std::true_type;
		using propagate_on_container_copy_assignment = std::false_type;
		using propagate_on_container_swap = std::false_type;
		using is_always_equal = std::true_type;

		PoolAllocator() noexcept = default;

		template<typename Other>
		PoolAllocator(const PoolAllocator<Other>&) noexcept {}

		PoolAllocator(const PoolAllocator&) = default;
		PoolAllocator(PoolAllocator&&) = default;
		virtual ~PoolAllocator() = default;
		PoolAllocator& operator=(const PoolAllocator&) = default;
		PoolAllocator& operator=(PoolAllocator&&) = default;

		template<typename Other>
		inline bool operator==(const PoolAllocator<Other>&) const noexcept
		{
			return true;
		}

		template<typename Other>
		inline bool operator!=(const PoolAllocator<Other>&) const noexcept
		{
			return false;
		}

		[[nodiscard]] inline pointer allocate(const std::size_t n)
		{
			auto retval = AllocateFromPool(n * sizeof(T));
			if (retval == nullptr) throw std::bad_alloc();

			return static_cast<T*>(retval);
		}

		inline void deallocate(pointer p, const std::size_t n)
		{
			assert(p != nullptr);

			if (!FreeToPool(p, n * sizeof(T)))
			{
				throw std::invalid_argument("Trying to free memory that wasn't allocated with this allocator.");
			}
		}
	};
}
