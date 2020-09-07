// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Statistics final
	{
		struct RTTSample final
		{
			std::chrono::milliseconds RTT{ 0 };
		};

		using RTTSampleList = Containers::List<RTTSample>;

		struct SendWindowSizeSample final
		{
			double WindowSize{ 0 };
		};

		using SendWindowSampleList = Containers::List<SendWindowSizeSample>;

	public:
		Statistics() noexcept {}
		Statistics(const Statistics&) = delete;
		Statistics(Statistics&& other) noexcept = delete;
		~Statistics() = default;
		Statistics& operator=(const Statistics&) = delete;
		Statistics& operator=(Statistics&&) noexcept = delete;

		[[nodiscard]] inline std::chrono::milliseconds GetRetransmissionTimeout() const noexcept
		{
			return std::max(MinRetransmissionTimeout, m_RTT * 2);
		}

		void RecalcRetransmissionTimeout() noexcept
		{
			if (!m_RTTSamplesDirty || m_RTTSamples.empty()) return;

			std::chrono::milliseconds total_time{ 0 };
			std::chrono::milliseconds min_time{ std::numeric_limits<std::chrono::milliseconds::rep>::max() };
			std::chrono::milliseconds max_time{ 0 };

			for (const auto& sample : m_RTTSamples)
			{
				total_time += sample.RTT;

				DbgInvoke([&]()
				{
					min_time = std::min(min_time, sample.RTT);
					max_time = std::max(max_time, sample.RTT);
				});
			}

			const std::chrono::milliseconds rtt_mean = total_time / m_RTTSamples.size();
			std::chrono::milliseconds total_rtt_diffmsq{ 0 };

			for (const auto& sample : m_RTTSamples)
			{
				const auto diff = sample.RTT - rtt_mean;
				const auto diffsq = std::chrono::milliseconds(diff.count() * diff.count());
				total_rtt_diffmsq += diffsq;
			}

			const auto rtt_stddev = std::chrono::milliseconds(
				static_cast<std::chrono::milliseconds::rep>(std::sqrt(total_rtt_diffmsq.count() / m_RTTSamples.size())));
			const auto rtt_minm = rtt_mean - rtt_stddev;
			const auto rtt_maxm = rtt_mean + rtt_stddev;

			std::chrono::milliseconds total_rtt{ 0 };
			Size total_rtt_count{ 0 };

			for (const auto& sample : m_RTTSamples)
			{
				if (rtt_minm <= sample.RTT && sample.RTT <= rtt_maxm)
				{
					total_rtt += sample.RTT;
					++total_rtt_count;
				}
			}

			if (total_rtt_count > 0)
			{
				// Choosing a value for X close to 1 makes the weighted average immune to changes
				// that last a short time (e.g., a single message that encounters long delay).
				// Choosing a value for X close to 0 makes the weighted average respond to changes
				// in delay very quickly.
				constexpr auto X = 0.95;
				const auto new_rtt_sample = (total_rtt.count() / total_rtt_count);
				const auto new_rtt = X * m_RTT.count() + ((1.0 - X) * new_rtt_sample);
				m_RTT = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(new_rtt));
			}

			SLogDbg(SLogFmt(FGBrightGreen) << L"UDP connection: RTT: " << m_RTT.count() <<
					L"ms - MinRTT: " << min_time.count() << L"ms - MaxRTT: " << max_time.count() << L"ms" << SLogFmt(Default));

			m_RTTSamplesDirty = false;
		}

		[[nodiscard]] inline Size GetSendWindowSize() const noexcept
		{
			return m_SendWindowSize;
		}

		inline void RecordPacketAck(const std::chrono::milliseconds rtt) noexcept
		{
			RecordRTTStats(rtt);

			// Part of additive increase/multiplicative decrease (AIMD) algorithm
			++m_NewSendWindowSizeSample;
		}

		inline void RecordPacketLoss() noexcept
		{
			// Part of additive increase/multiplicative decrease (AIMD) algorithm
			m_NewSendWindowSizeSample = m_NewSendWindowSizeSample / 2.0;
		}

		void RecordSendWindowSizeStats() noexcept
		{
			// No new data
			if (m_OldSendWindowSizeSample == m_NewSendWindowSizeSample) return;

			const auto rtt = std::invoke([&]()
			{
				return (m_SendWindowSizeSamples.size() == MaxSendWindowSizeSamples) ?
					GetRetransmissionTimeout() : GetRetransmissionTimeout() / 2;
			});

			// Only record every RTT for a good sample
			const auto now = Util::GetCurrentSteadyTime();
			if (now - m_LastSendWindowSampleSteadyTime >= rtt)
			{
				// Never go below minimum
				const auto size = std::max(static_cast<double>(MinSendWindowSize), m_NewSendWindowSizeSample);

				m_SendWindowSizeSamples.emplace_front(SendWindowSizeSample{ size });

				if (m_SendWindowSizeSamples.size() > MaxSendWindowSizeSamples)
				{
					m_SendWindowSizeSamples.pop_back();
				}

				m_OldSendWindowSizeSample = m_NewSendWindowSizeSample;
				m_LastSendWindowSampleSteadyTime = now;
				m_SendWindowSizeSamplesDirty = true;
			}
		}

		void RecalcSendWindowSize() noexcept
		{
			if (!m_SendWindowSizeSamplesDirty || m_SendWindowSizeSamples.empty()) return;

			double total_wnd_size{ 0 };
			double min_size{ std::numeric_limits<double>::max() };
			double max_size{ MinSendWindowSize };

			for (const auto& sample : m_SendWindowSizeSamples)
			{
				total_wnd_size += sample.WindowSize;

				DbgInvoke([&]()
				{
					min_size = std::min(min_size, sample.WindowSize);
					max_size = std::max(max_size, sample.WindowSize);
				});
			}

			const auto wins_mean = total_wnd_size / m_RTTSamples.size();
			double total_wins_diffmsq{ 0 };

			for (const auto& sample : m_SendWindowSizeSamples)
			{
				const auto diff = sample.WindowSize - wins_mean;
				const auto diffsq = diff * diff;
				total_wins_diffmsq += diffsq;
			}

			const auto wins_stddev = std::sqrt(total_wins_diffmsq / m_RTTSamples.size());
			const auto wins_minm = wins_mean - wins_stddev;
			const auto wins_maxm = wins_mean + wins_stddev;

			double total_wins{ 0 };
			double total_wins_count{ 0 };

			for (const auto& sample : m_SendWindowSizeSamples)
			{
				if (wins_minm <= sample.WindowSize && sample.WindowSize <= wins_maxm)
				{
					total_wins += sample.WindowSize;
					++total_wins_count;
				}
			}

			if (total_wins_count > 0)
			{
				// Choosing a value for X close to 1 makes the weighted average immune to changes
				// that last a short time. Choosing a value for X close to 0 makes the weighted
				// average respond to changes very quickly.
				constexpr auto X = 0.95;
				const auto new_wins_sample = (total_wins / total_wins_count);
				m_SendWindowSize = static_cast<Size>(std::ceil(X * m_SendWindowSize + ((1.0 - X) * new_wins_sample)));
			}

			SLogDbg(SLogFmt(FGBrightMagenta) << L"UDP connection: SendWindowSize: " << m_SendWindowSize <<
					L" - Min: " << static_cast<Size>(std::ceil(min_size)) << L" - Max: " <<
					static_cast<Size>(std::ceil(max_size)) << SLogFmt(Default));

			m_SendWindowSizeSamplesDirty = false;
		}

	private:
		void RecordRTTStats(const std::chrono::milliseconds rtt) noexcept
		{
			m_RTTSamples.emplace_front(RTTSample{ rtt });

			if (m_RTTSamples.size() > MaxRTTSamples)
			{
				m_RTTSamples.pop_back();
			}

			m_RTTSamplesDirty = true;
		}

	private:
		static constexpr std::chrono::milliseconds StartRTT{ 600 };
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 1 };
		static constexpr Size MaxRTTSamples{ 128 };

		static constexpr Size MinSendWindowSize{ 2 };
		static constexpr Size MaxSendWindowSizeSamples{ 128 };

	private:
		std::chrono::milliseconds m_RTT{ StartRTT };
		RTTSampleList m_RTTSamples;
		bool m_RTTSamplesDirty{ false };

		Size m_SendWindowSize{ MinSendWindowSize };
		SendWindowSampleList m_SendWindowSizeSamples;
		bool m_SendWindowSizeSamplesDirty{ false };
		double m_NewSendWindowSizeSample{ MinSendWindowSize };
		double m_OldSendWindowSizeSample{ MinSendWindowSize };
		SteadyTime m_LastSendWindowSampleSteadyTime;
	};
}