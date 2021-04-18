// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BufferView.h"

#include <cassert>
#include <array>

namespace QuantumGate::Implementation::Memory
{
	class StackBufferOverflowException final : public std::exception
	{
	public:
		StackBufferOverflowException(const char* message) noexcept : std::exception(message) {}
	};

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

		explicit constexpr StackBufferImpl(const Size size) { Allocate(size); }

		constexpr StackBufferImpl(const Byte* data, const Size data_size) { Add(data, data_size); }

		~StackBufferImpl() = default;

		constexpr explicit operator bool() const noexcept { return !IsEmpty(); }

		constexpr Byte& operator[](const Size index)
		{
			assert(index < m_Size);

			return m_Buffer[index];
		}
		
		constexpr const Byte& operator[](const Size index) const
		{
			assert(index < m_Size);

			return m_Buffer[index];
		}

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
			std::copy(buffer.GetBytes(), buffer.GetBytes() + buffer.GetSize(), m_Buffer.data());

			return *this;
		}

		constexpr bool operator==(const StackBufferImpl& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			if (!std::is_constant_evaluated())
			{
				return (std::memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
			}
			else
			{
				for (SizeType x = 0; x < m_Size; ++x)
				{
					if (m_Buffer[x] != other.m_Buffer[x]) return false;
				}

				return true;
			}
		}

		constexpr bool operator==(const BufferView& other) const noexcept
		{
			return (this->operator BufferView() == other);
		}

		constexpr bool operator==(const BufferSpan& other) const noexcept
		{
			return (this->operator BufferSpan() == other);
		}

		constexpr bool operator!=(const StackBufferImpl& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr bool operator!=(const BufferView& other) const noexcept
		{
			return (this->operator BufferView() != other);
		}

		constexpr bool operator!=(const BufferSpan& other) const noexcept
		{
			return (this->operator BufferSpan() != other);
		}

		constexpr operator BufferView() const noexcept { return { GetBytes(), GetSize() }; }
		constexpr operator BufferSpan() noexcept { return { GetBytes(), GetSize() }; }

		constexpr StackBufferImpl& operator+=(const StackBufferImpl& other) { Add(other.GetBytes(), other.GetSize()); return *this; }
		constexpr StackBufferImpl& operator+=(const BufferView& buffer) { Add(buffer.GetBytes(), buffer.GetSize()); return *this; }

		friend constexpr StackBufferImpl operator+(const StackBufferImpl& lhs, const StackBufferImpl& rhs)
		{
			StackBufferImpl val;
			val += lhs;
			val += rhs;
			return val;
		}

		[[nodiscard]] constexpr Byte* GetBytes() noexcept { return m_Buffer.data(); }
		[[nodiscard]] constexpr const Byte* GetBytes() const noexcept { return m_Buffer.data(); }

		[[nodiscard]] constexpr Size GetSize() const noexcept { return m_Size; }
		[[nodiscard]] static constexpr Size GetMaxSize() noexcept { return MaxSize; }

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
			else throw StackBufferOverflowException("Buffer size is larger than maximum.");
		}

		constexpr void Clear() noexcept
		{
			m_Buffer.fill(Byte{ 0 });
			m_Size = 0;
		}

		constexpr void RemoveFirst(const Size num) noexcept
		{
			assert(GetSize() >= num);
			
			std::copy(m_Buffer.data() + num, m_Buffer.data() + m_Size, m_Buffer.data());
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
			else throw StackBufferOverflowException("New buffer size is larger than maximum.");
		}

	private:
		constexpr void Add(const Byte* data, const Size size)
		{
			if (data != nullptr && size > 0)
			{
				assert(m_Size + size <= MaxSize);

				if (m_Size + size <= MaxSize)
				{
					std::copy(data, data + size, m_Buffer.data() + m_Size);
					m_Size += size;
				}
				else throw StackBufferOverflowException("Buffer overflow while trying to add data.");
			}
		}

	private:
		StorageType m_Buffer{ Byte{ 0 } };
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
	using StackBuffer65K = StackBuffer<65536>;
}