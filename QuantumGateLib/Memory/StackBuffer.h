// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferView.h"

#include <cassert>
#include <array>

namespace QuantumGate::Implementation::Memory
{
	template<Size MaxSize>
	class StackBufferImpl
	{
	public:
		using StorageType = std::array<Byte, MaxSize>;
		using SizeType = Size;

		constexpr StackBufferImpl() noexcept {}

		constexpr StackBufferImpl(const StackBufferImpl& other) :
			m_Buffer(other.m_Buffer), m_Size(other.m_Size)
		{}

		constexpr StackBufferImpl(const BufferView& other) { *this += other; }

		constexpr StackBufferImpl(StackBufferImpl&& other) noexcept :
			m_Buffer(std::move(other.m_Buffer)), m_Size(std::exchange(other.m_Size, 0))
		{}

		constexpr StackBufferImpl(const Size size) { Allocate(size); }

		constexpr StackBufferImpl(const Byte* data, const Size data_size) { Add(data, data_size); }

		~StackBufferImpl() { Clear(); }

		constexpr explicit operator bool() const noexcept { return !IsEmpty(); }

		constexpr Byte& operator[](const Size index) { return m_Buffer[index]; }
		constexpr const Byte& operator[](const Size index) const { return m_Buffer[index]; }

		constexpr StackBufferImpl& operator=(const StackBufferImpl& other)
		{
			// Check for same object
			if (this == &other) return *this;

			m_Buffer = other.m_Buffer;
			m_Size = other.m_Size;

			return *this;
		}

		constexpr StackBufferImpl& operator=(StackBufferImpl&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Buffer = std::move(other.m_Buffer);
			m_Size = std::exchange(other.m_Size, 0);

			return *this;
		}

		constexpr StackBufferImpl& operator=(const BufferView& buffer)
		{
			Allocate(buffer.GetSize());
			std::memcpy(GetBytes(), buffer.GetBytes(), buffer.GetSize());

			return *this;
		}

		constexpr bool operator==(const StackBufferImpl& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			return (std::memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
		}

		constexpr bool operator!=(const StackBufferImpl& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr operator BufferView() const noexcept { return { GetBytes(), GetSize() }; }
		constexpr operator BufferSpan() noexcept { return { GetBytes(), GetSize() }; }

		constexpr StackBufferImpl& operator+=(const StackBufferImpl& other) { Add(other.GetBytes(), other.GetSize()); return *this; }
		constexpr StackBufferImpl& operator+=(const BufferView& buffer) { Add(buffer.GetBytes(), buffer.GetSize()); return *this; }

		[[nodiscard]] constexpr Byte* GetBytes() noexcept { return m_Buffer.data(); }
		[[nodiscard]] constexpr const Byte* GetBytes() const noexcept { return m_Buffer.data(); }

		[[nodiscard]] constexpr Size GetSize() const noexcept { return m_Size; }

		[[nodiscard]] constexpr bool IsEmpty() const noexcept { return (m_Size == 0); }

		constexpr void Swap(StackBufferImpl& other) noexcept
		{
			m_Buffer.swap(other.m_Buffer);
			std::swap(m_Size, other.m_Size);
		}

		constexpr void Allocate(const Size size)
		{
			assert(size <= MaxSize);
			
			if (size <= MaxSize)
			{
				m_Size = size;
			}
			else throw BadAllocException("Buffer size is larger than maximum.");
		}

		constexpr void Clear() noexcept
		{
			m_Buffer.fill(Byte{ 0 });
			m_Size = 0;
		}

		constexpr void RemoveFirst(const Size num) noexcept
		{
			assert(GetSize() >= num);
			
			std::memcpy(m_Buffer.data(), m_Buffer.data() + num, m_Size);
			m_Size -= num;
		}

		constexpr void RemoveLast(const Size num) noexcept
		{
			assert(GetSize() >= num);

			m_Size -= num;
		}

		constexpr void Resize(const Size new_size)
		{
			assert(new_size <= MaxSize);
			
			if (new_size <= MaxSize)
			{
				m_Size = new_size;
			}
			else throw BadAllocException("New buffer size is larger than maximum.");
		}

	private:
		constexpr void Add(const Byte* data, const Size size)
		{
			if (data != nullptr && size > 0)
			{
				assert(m_Size + size <= MaxSize);

				if (m_Size + size <= MaxSize)
				{
					std::memcpy(m_Buffer.data() + m_Size, data, size);
					m_Size += size;
				}
				else throw BadAllocException("Buffer overflow.");
			}
		}

	private:
		StorageType m_Buffer;
		SizeType m_Size{ 0 };
	};

	template<Size MaxSize>
	using StackBuffer = StackBufferImpl<MaxSize>;

	using StackBuffer32 = StackBuffer<32>;
	using StackBuffer64 = StackBuffer<64>;
	using StackBuffer128 = StackBuffer<128>;
	using StackBuffer256 = StackBuffer<256>;
	using StackBuffer512 = StackBuffer<512>;
	using StackBuffer1024 = StackBuffer<1024>;
	using StackBuffer2048 = StackBuffer<2048>;
}