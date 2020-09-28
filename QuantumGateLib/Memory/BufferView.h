// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Memory
{
	template<bool Const = false>
	class BufferSpanImpl
	{
	public:
		using SizeType = Size;

		constexpr BufferSpanImpl() noexcept {}
		constexpr BufferSpanImpl(std::nullptr_t) noexcept {}

		constexpr BufferSpanImpl(std::conditional_t<Const, const Byte*, Byte*> buffer, const Size size) noexcept :
			m_Buffer(buffer), m_Size(size)
		{}

		constexpr BufferSpanImpl(const BufferSpanImpl&) noexcept = default;
		constexpr BufferSpanImpl(BufferSpanImpl&&) noexcept = default;
		~BufferSpanImpl() = default;
		constexpr BufferSpanImpl& operator=(const BufferSpanImpl&) noexcept = default;
		constexpr BufferSpanImpl& operator=(BufferSpanImpl&&) noexcept = default;

		constexpr operator BufferSpanImpl<true>() const noexcept requires(!Const) { return { m_Buffer, m_Size }; }

		constexpr std::conditional_t<Const, const Byte&, Byte&> operator[](const Size index) noexcept
		{
			assert(index < GetSize());

			return m_Buffer[index];
		}

		constexpr const Byte& operator[](const Size index) const noexcept
		{
			assert(index < GetSize());

			return m_Buffer[index];
		}

		[[nodiscard]] constexpr std::conditional_t<Const, const Byte*, Byte*> GetBytes() noexcept { return m_Buffer; }
		[[nodiscard]] constexpr const Byte* GetBytes() const noexcept { return m_Buffer; }
		[[nodiscard]] constexpr Size GetSize() const noexcept { return m_Size; }
		[[nodiscard]] constexpr bool IsEmpty() const noexcept { return (m_Buffer == nullptr || m_Size == 0); }

		constexpr explicit operator bool() const noexcept { return !IsEmpty(); }

		constexpr bool operator==(const BufferSpanImpl& other) const noexcept
		{
			if (GetSize() != other.GetSize()) return false;

			return (std::memcmp(GetBytes(), other.GetBytes(), GetSize()) == 0);
		}

		constexpr bool operator!=(const BufferSpanImpl& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr BufferSpanImpl GetFirst(const Size count) const noexcept
		{
			assert(GetSize() >= count);

			return { m_Buffer, count };
		}

		constexpr BufferSpanImpl GetLast(const Size count) const noexcept
		{
			assert(GetSize() >= count);

			return { m_Buffer + (GetSize() - count), count };
		}

		constexpr BufferSpanImpl GetSub(const Size offset, const Size count) const noexcept
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
		std::conditional_t<Const, const Byte*, Byte*> m_Buffer{ nullptr };
		Size m_Size{ 0 };
	};

	using BufferView = BufferSpanImpl<true>;
	using BufferSpan = BufferSpanImpl<false>;
}