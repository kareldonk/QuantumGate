// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	template<typename T, T MinSize = std::numeric_limits<T>::min(), T MaxSize = std::numeric_limits<T>::max(), bool NoExcept = true>
	class RateLimit final
	{
		static_assert(std::is_arithmetic_v<T>, "T should be an arithmetic type.");
		static_assert(MaxSize >= MinSize, "Maximum size should be greater than or equal to minimum size.");

	public:
		using SizeType = T;

		constexpr RateLimit() noexcept = default;

		template<typename U> requires std::is_arithmetic_v<U>
		constexpr RateLimit(U value) noexcept(NoExcept)
		{
			if constexpr (NoExcept)
			{
				assert((value >= MinSize) && (value <= MaxSize));

				if (value < MinSize) value = MinSize;
				else if (value > MaxSize) value = MaxSize;
			}
			else
			{
				if (!((value >= MinSize) && (value <= MaxSize)))
				{
					throw std::invalid_argument("Value parameter is out of range.");
					return;
				}
			}

			m_CurrentSize = value;
		}

		constexpr RateLimit(const RateLimit&) noexcept = default;
		constexpr RateLimit(RateLimit&&) noexcept = default;
		~RateLimit() = default;
		constexpr RateLimit& operator=(const RateLimit&) noexcept = default;
		constexpr RateLimit& operator=(RateLimit&&) noexcept = default;

		template<typename U> requires std::is_arithmetic_v<U>
		constexpr inline void Add(const U num) noexcept(NoExcept)
		{
			if constexpr (NoExcept)
			{
				assert(CanAdd(num));
			}
			else
			{
				if (!CanAdd(num))
				{
					throw std::invalid_argument("Value parameter is out of range.");
					return;
				}
			}

			m_CurrentSize += num;
		}

		template<typename U> requires std::is_arithmetic_v<U>
		[[nodiscard]] constexpr inline bool CanAdd(const U num) const noexcept
		{
			return ((m_CurrentSize + num >= MinSize) && (m_CurrentSize + num <= MaxSize));
		}

		[[nodiscard]] constexpr inline SizeType GetAvailable() const noexcept { return (MaxSize - m_CurrentSize); }

		template<typename U> requires std::is_arithmetic_v<U>
		constexpr inline void Subtract(const U num) noexcept(NoExcept)
		{
			if constexpr (NoExcept)
			{
				assert(CanSubtract(num));
			}
			else
			{
				if (!CanSubtract(num))
				{
					throw std::invalid_argument("Value parameter is out of range.");
					return;
				}
			}

			m_CurrentSize -= num;
		}

		template<typename U> requires std::is_arithmetic_v<U>
		[[nodiscard]] constexpr inline bool CanSubtract(const U num) const noexcept
		{
			return ((m_CurrentSize - num >= MinSize) && (m_CurrentSize - num <= MaxSize));
		}

		[[nodiscard]] constexpr inline SizeType GetCurrent() const noexcept { return m_CurrentSize; }
		[[nodiscard]] constexpr inline SizeType GetMinimum() const noexcept { return MinSize; }
		[[nodiscard]] constexpr inline SizeType GetMaximum() const noexcept { return MaxSize; }

	private:
		SizeType m_CurrentSize{ MinSize };
	};
}