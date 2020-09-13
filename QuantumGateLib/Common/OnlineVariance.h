// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation
{
	template<typename T = double>
	class OnlineVariance final
	{
		static_assert(std::is_signed_v<T> && std::is_floating_point_v<T>, "T should be a signed floating point type.");

	public:
		constexpr void AddSample(const T sample) noexcept
		{
			if (m_Count == std::numeric_limits<T>::max())
			{
				Restart();
				AddSample(sample);
			}

			const auto new_count = m_Count + 1;
			const auto delta = sample - m_Mean;
			const auto new_mean = m_Mean + (delta / new_count);
			const auto delta2 = sample - new_mean;
			const auto D2 = delta * delta2;

			if (std::numeric_limits<T>::max() - D2 >= m_M2)
			{
				m_M2 += D2;
				m_Count = new_count;
				m_Mean = new_mean;
			}
			else
			{
				Restart();
				AddSample(sample);
			}
		}

		[[nodiscard]] constexpr T GetCount() const noexcept { return m_Count; }

		[[nodiscard]] constexpr T GetMean() const noexcept { return m_Mean; }

		[[nodiscard]] constexpr T GetVariance() const noexcept
		{
			assert(m_Count > 0);
			return m_M2 / m_Count;
		}

		[[nodiscard]] constexpr T GetStdDev() const noexcept
		{
			assert(m_Count > 0);
			return std::sqrt(m_M2 / m_Count);
		}

		[[nodiscard]] constexpr T GetMinDev() const noexcept
		{
			const auto d = GetStdDev() / 2;
			if (std::numeric_limits<T>::lowest() + d <= m_Mean)
			{
				return m_Mean - d;
			}
			else return std::numeric_limits<T>::lowest();
		}
		
		[[nodiscard]] constexpr T GetMaxDev() const noexcept
		{
			const auto d = GetStdDev() / 2;
			if (std::numeric_limits<T>::max() - d >= m_Mean)
			{
				return m_Mean + d;
			}
			else return std::numeric_limits<T>::max();
		}

		constexpr void Restart() noexcept
		{
			if (m_Count > 0)
			{
				m_M2 = m_M2 / m_Count;
				m_Count = 1;
			}
			else
			{
				Clear();
			}
		}

		constexpr void Clear()
		{
			m_Count = 0;
			m_Mean = 0;
			m_M2 = 0;
		}

		static constexpr T WeightedSampleUpdate(const T old_sample, const T new_sample, const T X) noexcept
		{
			// Choosing a value for X close to 1 makes the weighted average immune to changes
			// that last a short time. Choosing a value for X close to 0 makes the weighted
			// average respond to changes very quickly.
			return (X * old_sample) + ((1.0 - X) * new_sample);
		}

	private:
		T m_Count{ 0 };
		T m_Mean{ 0 };
		T m_M2{ 0 };
	};
}