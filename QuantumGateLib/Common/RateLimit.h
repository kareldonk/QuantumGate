// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	template<typename T, T MinSize = std::numeric_limits<T>::min(), T MaxSize = std::numeric_limits<T>::max()>
	class RateLimit final
	{
		static_assert(std::is_arithmetic_v<T>, "T should be an arithmetic type.");
		static_assert(MaxSize >= MinSize, "Maximum size should be greater than or equal to minimum size.");

	public:
		using SizeType = T;

		constexpr RateLimit() noexcept = default;

		constexpr RateLimit(const T value) noexcept : m_CurrentSize(value)
		{
			assert(m_CurrentSize >= MinSize && m_CurrentSize <= MaxSize);
		}

		constexpr RateLimit(const RateLimit&) noexcept = default;
		constexpr RateLimit(RateLimit&&) noexcept = default;
		~RateLimit() = default;
		constexpr RateLimit& operator=(const RateLimit&) noexcept = default;
		constexpr RateLimit& operator=(RateLimit&&) noexcept = default;

		constexpr inline void Add(const SizeType num) noexcept
		{
			assert(CanAdd(num));
			m_CurrentSize += num;
		}

		[[nodiscard]] constexpr inline bool CanAdd(const SizeType num) const noexcept
		{
			return (GetAvailable() >= num);
		}

		[[nodiscard]] constexpr inline SizeType GetAvailable() const noexcept { return (MaxSize - m_CurrentSize); }

		constexpr inline void Subtract(const SizeType num) noexcept
		{
			assert(CanSubtract(num));
			m_CurrentSize -= num;
		}

		[[nodiscard]] constexpr inline bool CanSubtract(const SizeType num) const noexcept
		{
			return ((m_CurrentSize >= num) && (m_CurrentSize - num >= MinSize));
		}

		[[nodiscard]] constexpr inline SizeType GetCurrent() const noexcept { return m_CurrentSize; }
		[[nodiscard]] constexpr inline SizeType GetMinimum() const noexcept { return MinSize; }
		[[nodiscard]] constexpr inline SizeType GetMaximum() const noexcept { return MaxSize; }

	private:
		SizeType m_CurrentSize{ MinSize };
	};
}