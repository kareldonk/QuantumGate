// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\OnlineVariance.h"
#include "..\..\Common\RingList.h"
#include "..\..\Common\Containers.h"

// Use to enable/disable RTT debug console output
// #define UDPCS_RTT_DEBUG

// Use to enable/disable MTU Window Size debug console output
// #define UDPCS_WND_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Statistics final
	{
		struct RTTSample final
		{
			std::chrono::nanoseconds RTT{ 0 };
		};

		using RTTSampleList = RingList<RTTSample, 128>;

		struct MTUWindowSizeSample final
		{
			double MTUWindowSize{ 0 };
		};

		using MTUWindowSampleList = RingList<MTUWindowSizeSample, 128>;

	public:
		Statistics() noexcept {}
		Statistics(const Statistics&) = delete;
		Statistics(Statistics&& other) noexcept = delete;
		~Statistics() = default;
		Statistics& operator=(const Statistics&) = delete;
		Statistics& operator=(Statistics&&) noexcept = delete;

		[[nodiscard]] inline std::chrono::nanoseconds GetRetransmissionTimeout() noexcept
		{
			RecalcRetransmissionTimeout();

			// Retransmission timeout is larger than RTT to avoid premature retransmission,
			// and will be larger when more MTUs get lost
			return std::chrono::duration_cast<std::chrono::nanoseconds>(m_RTT * m_RTTMTULossFactor * 2);
		}

		inline void RecordRTT(const std::chrono::nanoseconds rtt) noexcept
		{
			// Never go below minimum
			const auto ns = std::max(MinRTT.count(), rtt.count());

			m_RTTVariance.AddSample(static_cast<double>(ns));
			m_RTTSamples.Add(RTTSample{ std::chrono::nanoseconds(ns) });
		}

	private:
		void RecalcRetransmissionTimeout() noexcept
		{
			// No new recorded samples
			if (!m_RTTSamples.IsUpdated()) return;

			const auto rtt_minm = m_RTTVariance.GetMinDev();
			const auto rtt_maxm = m_RTTVariance.GetMaxDev();

			std::chrono::nanoseconds total_rtt{ 0 };
			Size total_rtt_count{ 0 };

#ifdef UDPCS_RTT_DEBUG
			std::chrono::nanoseconds min_time{ std::numeric_limits<std::chrono::nanoseconds::rep>::max() };
			std::chrono::nanoseconds max_time{ 0 };
			const auto old_rtt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_RTT);
#endif
			for (const auto& sample : m_RTTSamples.GetList())
			{
				if (rtt_minm <= sample.RTT.count() && sample.RTT.count() <= rtt_maxm)
				{
					total_rtt += sample.RTT;
					++total_rtt_count;
				}

#ifdef UDPCS_RTT_DEBUG
				min_time = std::min(min_time, sample.RTT);
				max_time = std::max(max_time, sample.RTT);
#endif
			}

			if (total_rtt_count > 0)
			{
				// Choosing a value for X close to 1 makes the weighted average immune to changes
				// that last a short time (e.g., a single message that encounters long delay).
				// Choosing a value for X close to 0 makes the weighted average respond to changes
				// in delay very quickly.
				const auto X = m_NoLossYetRecorded ? 0.5 : 0.95;
				const auto new_rtt_sample = (static_cast<double>(total_rtt.count()) / static_cast<double>(total_rtt_count));
				const auto new_rtt = OnlineVariance<double>::WeightedSampleUpdate(static_cast<double>(m_RTT.count()), new_rtt_sample, X);
				m_RTT = std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(new_rtt));
			}

#ifdef UDPCS_RTT_DEBUG
			if (old_rtt_ms != std::chrono::duration_cast<std::chrono::milliseconds>(m_RTT))
			{
				const auto stddev = std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetStdDev()));
				const auto mean = std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetMean()));

				SLogInfo(SLogFmt(FGBrightGreen) << L"UDP connection: RTT: " <<
						 std::chrono::duration_cast<std::chrono::milliseconds>(m_RTT) <<
						 L" - Min: " << std::chrono::duration_cast<std::chrono::milliseconds>(min_time) <<
						 L" - Max: " << std::chrono::duration_cast<std::chrono::milliseconds>(max_time) <<
						 L" - StdDev: " << std::chrono::duration_cast<std::chrono::milliseconds>(stddev) <<
						 L" - Mean: " << std::chrono::duration_cast<std::chrono::milliseconds>(mean) << SLogFmt(Default));
			}
#endif
			m_RTTSamples.Expire();
		}

	public:
		[[nodiscard]] inline Size GetMTUWindowSize() noexcept
		{
			RecalcMTUWindowSize();
			return m_MTUWindowSize;
		}

		inline void RecordMTUAck(const double num_mtu) noexcept
		{
			if (num_mtu == 0.0) return;

			// Part of additive increase/multiplicative decrease (AIMD) algorithm
			if (m_NoLossYetRecorded)
			{
				// Fast start
				m_NewMTUWindowSizeSample += num_mtu;
			}
			else
			{
				if (m_NewMTUWindowSizeSample < m_ThresholdMTUWindowSize)
				{
					// Fast recovery
					m_NewMTUWindowSizeSample += num_mtu;
				}
				else
				{
					m_NewMTUWindowSizeSample += ((1.0 / static_cast<double>(m_MTUWindowSize)) * num_mtu);
				}
			}
		}

		inline void RecordMTULoss(const double num_mtu) noexcept
		{
			if (num_mtu == 0.0)
			{
				if (m_NoLossYetRecorded)
				{
					m_NoLossMTUWindowSize = std::max(MinMTUWindowSize, m_MTUWindowSize / 2);
					m_ThresholdMTUWindowSize = m_NoLossMTUWindowSize;
				}
			}
			else
			{
				const auto now = Util::GetCurrentSteadyTime();

				m_LastLossRecordedSteadyTime = now;

				// Part of additive increase/multiplicative decrease (AIMD) algorithm
				m_NewMTUWindowSizeSample = m_NewMTUWindowSizeSample / std::pow(2.0, num_mtu);

				if (m_NoLossYetRecorded)
				{
					m_MTUWindowSizeSamples.Clear();

					if (m_MTUStart)
					{
						m_MTUWindowSizeVariance.Restart();
						m_MTUWindowSize = std::max(MinMTUWindowSize, m_MTUWindowSize / 2);
						m_MTUStart = false;
					}

					m_NoLossYetRecorded = false;

#ifdef UDPCS_WND_DEBUG
					SLogInfo(SLogFmt(FGBrightMagenta) << L"UDP connection: NoLossMTUWindowSize: " <<
							 m_NoLossMTUWindowSize << L" - MTUWindowSize: " << m_MTUWindowSize << SLogFmt(Default));
#endif
				}

				m_RTTMTULossCount += num_mtu;
				if (now - m_LastRTTMTULossFactorSteadyTime >= m_RTT)
				{
					m_RTTMTULossFactor = 1.0 + (m_RTTMTULossCount / static_cast<double>(m_MTUWindowSize));

					m_RTTMTULossCount = 0;
					m_LastRTTMTULossFactorSteadyTime = now;
				}
			}
		}

		void RecordMTUWindowSizeStats() noexcept
		{
			if (m_OldMTUWindowSizeSample == m_NewMTUWindowSizeSample) return;

			const auto rtt = std::invoke([&]()
			{
				return (m_MTUWindowSizeSamples.IsMaxSize()) ?
					GetRetransmissionTimeout() : m_RTT;
			});

			// Only record every RTT for a good sample
			const auto now = Util::GetCurrentSteadyTime();
			if (now - m_LastMTUWindowSizeSampleSteadyTime >= rtt)
			{
				m_MTUWindowSizeVariance.AddSample(m_NewMTUWindowSizeSample);
				m_MTUWindowSizeSamples.Add(MTUWindowSizeSample{ m_NewMTUWindowSizeSample });

				m_ThresholdMTUWindowSize = std::max(m_NoLossMTUWindowSize, static_cast<Size>(m_MTUWindowSizeVariance.GetMean() / 2.0));

				m_OldMTUWindowSizeSample = m_NewMTUWindowSizeSample;
				m_LastMTUWindowSizeSampleSteadyTime = now;
			}

			if (now - m_LastLossRecordedSteadyTime >= NoLossRestartTimeout)
			{
				m_NoLossYetRecorded = true;
			}
		}

	private:
		void RecalcMTUWindowSize() noexcept
		{
			if (!m_MTUWindowSizeSamples.IsUpdated()) return;

			const auto mtu_minm = m_MTUWindowSizeVariance.GetMinDev();
			const auto mtu_maxm = m_MTUWindowSizeVariance.GetMaxDev();

			double total_mtu{ 0 };
			double total_mtu_num{ 0 };

#ifdef UDPCS_WND_DEBUG
			double min_size{ std::numeric_limits<double>::max() };
			double max_size{ MinMTUWindowSize };
#endif

			for (const auto& sample : m_MTUWindowSizeSamples.GetList())
			{
				if (mtu_minm <= sample.MTUWindowSize && sample.MTUWindowSize <= mtu_maxm)
				{
					total_mtu += sample.MTUWindowSize;
					++total_mtu_num;
				}

#ifdef UDPCS_WND_DEBUG
				min_size = std::min(min_size, sample.MTUWindowSize);
				max_size = std::max(max_size, sample.MTUWindowSize);
#endif
			}

			if (total_mtu_num > 0)
			{
				// Choosing a value for X close to 1 makes the weighted average immune to changes
				// that last a short time. Choosing a value for X close to 0 makes the weighted
				// average respond to changes very quickly.
				constexpr auto X = 0.95;
				const auto new_mtu_sample = (total_mtu / total_mtu_num);
				m_MTUWindowSize = static_cast<Size>(std::ceil(OnlineVariance<double>::WeightedSampleUpdate(static_cast<double>(m_MTUWindowSize), new_mtu_sample, X)));

				// Never go below minimum
				m_MTUWindowSize = std::max(MinMTUWindowSize, m_MTUWindowSize);
			}

#ifdef UDPCS_WND_DEBUG
			SLogInfo(SLogFmt(FGBrightMagenta) << L"UDP connection: MTUWindowSize: " << m_MTUWindowSize <<
					 L" - Min: " << static_cast<Size>(std::ceil(min_size)) << L" - Max: " <<
					 static_cast<Size>(std::ceil(max_size)) << L" - StdDev: " << m_MTUWindowSizeVariance.GetStdDev() <<
					 L" - Mean: " << m_MTUWindowSizeVariance.GetMean() << SLogFmt(Default));
#endif
			m_MTUWindowSizeSamples.Expire();
		}

	public:
		static constexpr Size MinMTUWindowSize{ 1 };

	private:
		static constexpr std::chrono::nanoseconds StartRTT{ 600'000'000 };
		static constexpr std::chrono::nanoseconds MinRTT{ 1'000 };
		static constexpr std::chrono::seconds NoLossRestartTimeout{ 2 };

	private:
		std::chrono::nanoseconds m_RTT{ StartRTT };
		OnlineVariance<double> m_RTTVariance;
		RTTSampleList m_RTTSamples;
		double m_RTTMTULossCount{ 0 };
		double m_RTTMTULossFactor{ 1 };
		SteadyTime m_LastRTTMTULossFactorSteadyTime;

		bool m_MTUStart{ true };
		bool m_NoLossYetRecorded{ true };
		Size m_NoLossMTUWindowSize{ MinMTUWindowSize };
		SteadyTime m_LastLossRecordedSteadyTime;
		Size m_ThresholdMTUWindowSize{ MinMTUWindowSize };
		Size m_MTUWindowSize{ MinMTUWindowSize };
		OnlineVariance<double> m_MTUWindowSizeVariance;
		MTUWindowSampleList m_MTUWindowSizeSamples;
		double m_NewMTUWindowSizeSample{ MinMTUWindowSize };
		double m_OldMTUWindowSizeSample{ MinMTUWindowSize };
		SteadyTime m_LastMTUWindowSizeSampleSteadyTime;
	};
}