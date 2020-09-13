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
			std::chrono::milliseconds RTT{ 0 };
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

		[[nodiscard]] inline std::chrono::milliseconds GetRetransmissionTimeout() const noexcept
		{
			return std::max(MinRetransmissionTimeout, m_RTT * 2);
		}

		inline void RecordRTT(const std::chrono::milliseconds rtt) noexcept
		{
			RecordRTTStats(rtt);
		}

		void RecalcRetransmissionTimeout() noexcept
		{
			if (!m_RTTSamples.IsUpdated()) return;

			const auto rtt_minm = m_RTTVariance.GetMinDev();
			const auto rtt_maxm = m_RTTVariance.GetMaxDev();

			std::chrono::milliseconds total_rtt{ 0 };
			Size total_rtt_count{ 0 };

#ifdef UDPCS_RTT_DEBUG
			std::chrono::milliseconds min_time{ std::numeric_limits<std::chrono::milliseconds::rep>::max() };
			std::chrono::milliseconds max_time{ 0 };
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
				m_RTT = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(new_rtt));
			}

#ifdef UDPCS_RTT_DEBUG
			SLogInfo(SLogFmt(FGBrightGreen) << L"UDP connection: RTT: " << m_RTT.count() <<
					 L"ms - Min: " << min_time.count() << L"ms - Max: " << max_time.count() << L"ms - StdDev: " <<
					 m_RTTVariance.GetStdDev() << L"ms - Mean: " << m_RTTVariance.GetMean() << L"ms" << SLogFmt(Default));
#endif
			m_RTTSamples.Expire();
		}

		[[nodiscard]] inline Size GetMTUWindowSize() const noexcept
		{
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
				m_LastLossRecordedSteadyTime = Util::GetCurrentSteadyTime();

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
			}
		}

		void RecordMTUWindowSizeStats() noexcept
		{
			if (m_OldMTUWindowSizeSample == m_NewMTUWindowSizeSample) return;

			const auto rtt = std::invoke([&]()
			{
				return (m_MTUWindowSizeSamples.IsMaxSize()) ?
					GetRetransmissionTimeout() : GetRetransmissionTimeout() / 2;
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

	private:
		void RecordRTTStats(const std::chrono::milliseconds rtt) noexcept
		{
			m_RTTVariance.AddSample(static_cast<double>(rtt.count()));
			m_RTTSamples.Add(RTTSample{ rtt });
		}

	public:
		static constexpr Size MinMTUWindowSize{ 8 };

	private:
		static constexpr std::chrono::milliseconds StartRTT{ 600 };
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 1 };
		static constexpr std::chrono::seconds NoLossRestartTimeout{ 2 };

	private:
		std::chrono::milliseconds m_RTT{ StartRTT };
		OnlineVariance<double> m_RTTVariance;
		RTTSampleList m_RTTSamples;

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