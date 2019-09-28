// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Memory
{
	class BufferView
	{
	public:
		using SizeType = Size;

		constexpr BufferView() noexcept {}
		constexpr BufferView(std::nullptr_t) noexcept {}

		constexpr BufferView(const Byte* buffer, const Size size) noexcept :
			m_Buffer(buffer), m_Size(size)
		{}

		constexpr BufferView(const BufferView&) = default;
		constexpr BufferView(BufferView&&) = default;
		~BufferView() = default;
		constexpr BufferView& operator=(const BufferView&) = default;
		constexpr BufferView& operator=(BufferView&&) = default;

		constexpr const Byte operator[](Size index) const noexcept
		{
			assert(index < GetSize());

			return m_Buffer[index];
		}

		[[nodiscard]] constexpr const Byte* GetBytes() const noexcept { return m_Buffer; }
		[[nodiscard]] constexpr const Size GetSize() const noexcept { return m_Size; }
		[[nodiscard]] constexpr bool IsEmpty() const noexcept { return (m_Buffer == nullptr || m_Size == 0); }

		constexpr explicit operator bool() const noexcept { return !IsEmpty(); }

		constexpr bool operator==(const BufferView& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			return (std::memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
		}

		constexpr bool operator!=(const BufferView& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr BufferView GetFirst(const Size count) const noexcept
		{
			assert(GetSize() >= count);

			return { m_Buffer, count };
		}

		constexpr BufferView GetLast(const Size count) const noexcept
		{
			assert(GetSize() >= count);

			return { m_Buffer + (GetSize() - count), count };
		}

		constexpr BufferView GetSubView(const Size offset, const Size count) const noexcept
		{
			assert(GetSize() >= offset && GetSize() >= offset + count);

			return { m_Buffer + offset, count };
		}

		constexpr void RemoveFirst(const Size count) noexcept
		{
			assert(GetSize() >= count);

			m_Buffer += count;
			m_Size -= count;
		}

		constexpr void RemoveLast(const Size count) noexcept
		{
			assert(GetSize() >= count);

			m_Size -= count;
		}

	private:
		const Byte* m_Buffer{ nullptr };
		Size m_Size{ 0 };
	};
}