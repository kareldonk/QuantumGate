// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Containers.h"

// Use to enable/disable RTT debug console output
// #define UDPCS_RTT_DEBUG

// Use to enable/disable Send Window Size debug console output
// #define UDPCS_WND_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Statistics final
	{
		class OnlineVariance final
		{
		public:
			void Update(const double value) noexcept
			{
				if (m_Count == std::numeric_limits<double>::max())
				{
					Reset();
					Update(value);
				}

				const auto new_count = m_Count + 1;
				const auto delta = value - m_Mean;
				const auto new_mean = m_Mean + (delta / new_count);
				const auto delta2 = value - new_mean;
				const auto D2 = delta * delta2;

				if (std::numeric_limits<double>::max() - D2 >= m_M2)
				{
					m_M2 += D2;
					m_Count = new_count;
					m_Mean = new_mean;
				}
				else
				{
					Reset();
					Update(value);
				}
			}

			[[nodiscard]] double GetMean() const noexcept { return m_Mean; }
			[[nodiscard]] double GetStdDev() const noexcept { assert(m_Count > 0); return std::sqrt(m_M2 / m_Count); }
			[[nodiscard]] double GetMin() const noexcept { return m_Mean - (GetStdDev() / 2.0); }
			[[nodiscard]] double GetMax() const noexcept { return m_Mean + (GetStdDev() / 2.0); }

			void Reset() noexcept
			{
				if (m_Count > 0)
				{
					m_M2 = m_M2 / m_Count;
					m_Count = 1;
				}
				else
				{
					m_Count = 0;
					m_Mean = 0;
					m_M2 = 0;
				}
			}

		private:
			double m_Count{ 0 };
			double m_Mean{ 0 };
			double m_M2{ 0 };
		};

		template<typename T, Size MaxSize>
		class RingList final
		{
			using ListType = Containers::List<T>;

		public:
			bool Add(T&& value) noexcept
			{
				try
				{
					if (m_List.size() == MaxSize)
					{
						ListType tmp;
						tmp.splice(tmp.begin(), m_List, m_List.begin());
						*tmp.begin() = std::move(value);
						m_List.splice(m_List.end(), tmp);
					}
					else
					{
						m_List.emplace_back(std::move(value));
					}

					m_New = true;
				}
				catch (...) { return false; }

				return true;
			}

			[[nodiscard]] bool HasNew() const noexcept { return m_New; }
			void Expire() noexcept { m_New = false; }

			[[nodiscard]] bool IsEmpty() const noexcept { return m_List.empty(); }
			[[nodiscard]] Size GetSize() const noexcept { return m_List.size(); }
			[[nodiscard]] bool IsMaxSize() const noexcept { return m_List.size() == MaxSize; }
			[[nodiscard]] const ListType& GetList() const noexcept { return m_List; }

		private:
			bool m_New{ false };
			ListType m_List;
		};

		struct RTTSample final
		{
			std::chrono::milliseconds RTT{ 0 };
		};

		using RTTSampleList = RingList<RTTSample, 128>;

		struct SendWindowSizeSample final
		{
			double WindowSize{ 0 };
		};

		using SendWindowSampleList = RingList<SendWindowSizeSample, 128>;

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
			if (!m_RTTSamples.HasNew()) return;

			std::chrono::milliseconds total_time{ 0 };

#ifdef UDPCS_RTT_DEBUG
			std::chrono::milliseconds min_time{ std::numeric_limits<std::chrono::milliseconds::rep>::max() };
			std::chrono::milliseconds max_time{ 0 };
#endif

			const auto rtt_minm = m_RTTVariance.GetMin();
			const auto rtt_maxm = m_RTTVariance.GetMax();

			std::chrono::milliseconds total_rtt{ 0 };
			Size total_rtt_count{ 0 };

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
				const auto new_rtt_sample = (total_rtt.count() / total_rtt_count);
				const auto new_rtt = X * m_RTT.count() + ((1.0 - X) * new_rtt_sample);
				m_RTT = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(new_rtt));
			}

#ifdef UDPCS_RTT_DEBUG
			SLogInfo(SLogFmt(FGBrightGreen) << L"UDP connection: RTT: " << m_RTT.count() <<
					 L"ms - Min: " << min_time.count() << L"ms - Max: " << max_time.count() << L"ms - StdDev: " <<
					 m_RTTVariance.GetStdDev() << L"ms - Mean: " << m_RTTVariance.GetMean() << L"ms" << SLogFmt(Default));
#endif
			m_RTTSamples.Expire();
		}

		[[nodiscard]] inline Size GetSendWindowSize() const noexcept
		{
			return m_SendWindowSize;
		}

		inline void RecordPacketRTT(const std::chrono::milliseconds rtt) noexcept
		{
			RecordRTTStats(rtt);
		}

		inline void RecordPacketAck(const Size num_packets) noexcept
		{
			// Part of additive increase/multiplicative decrease (AIMD) algorithm
			if (m_NoLossYetRecorded)
			{
				// Fast start
				m_NewSendWindowSizeSample += num_packets;
			}
			else
			{
				if (m_NewSendWindowSizeSample < m_NoLossSendWindowSize)
				{
					// Fast recovery
					m_NewSendWindowSizeSample += num_packets;
				}
				else
				{
					m_NewSendWindowSizeSample += ((1.0 / static_cast<double>(m_SendWindowSize)) * num_packets);
				}
			}
		}

		inline void RecordPacketLoss(const Size num_packets) noexcept
		{
			if (num_packets == 0)
			{
				if (m_NoLossYetRecorded)
				{
					m_NoLossSendWindowSize = std::max(MinSendWindowSize, m_SendWindowSize / 2);
				}
			}
			else
			{
				// Part of additive increase/multiplicative decrease (AIMD) algorithm
				m_NewSendWindowSizeSample = m_NewSendWindowSizeSample / std::pow(2.0, num_packets);

				if (m_NoLossYetRecorded)
				{
					m_SendWindowSizeVariance.Reset();
					m_SendWindowSize = std::max(MinSendWindowSize, m_SendWindowSize / 2);
					m_NoLossYetRecorded = false;

#ifdef UDPCS_WND_DEBUG
					SLogInfo(SLogFmt(FGBrightMagenta) << L"UDP connection: NoLossSendWindowSize: " <<
							 m_NoLossSendWindowSize << L" - SendWindowSize: " << m_SendWindowSize << SLogFmt(Default));
#endif
				}
			}
		}

		void RecordSendWindowSizeStats() noexcept
		{
			if (m_OldSendWindowSizeSample == m_NewSendWindowSizeSample) return;

			const auto rtt = std::invoke([&]()
			{
				return (m_SendWindowSizeSamples.IsMaxSize()) ?
					GetRetransmissionTimeout() : GetRetransmissionTimeout() / 2;
			});

			// Only record every RTT for a good sample
			const auto now = Util::GetCurrentSteadyTime();
			if (now - m_LastSendWindowSampleSteadyTime >= rtt)
			{
				m_SendWindowSizeVariance.Update(m_NewSendWindowSizeSample);
				m_SendWindowSizeSamples.Add(SendWindowSizeSample{ m_NewSendWindowSizeSample });

				m_OldSendWindowSizeSample = m_NewSendWindowSizeSample;
				m_LastSendWindowSampleSteadyTime = now;
			}
		}

		void RecalcSendWindowSize() noexcept
		{
			if (!m_SendWindowSizeSamples.HasNew()) return;

			const auto wins_minm = m_SendWindowSizeVariance.GetMin();
			const auto wins_maxm = m_SendWindowSizeVariance.GetMax();

			double total_wins{ 0 };
			double total_wins_count{ 0 };

#ifdef UDPCS_WND_DEBUG
			double min_size{ std::numeric_limits<double>::max() };
			double max_size{ MinSendWindowSize };
#endif

			for (const auto& sample : m_SendWindowSizeSamples.GetList())
			{
				if (wins_minm <= sample.WindowSize && sample.WindowSize <= wins_maxm)
				{
					total_wins += sample.WindowSize;
					++total_wins_count;
				}

#ifdef UDPCS_WND_DEBUG
				min_size = std::min(min_size, sample.WindowSize);
				max_size = std::max(max_size, sample.WindowSize);
#endif
			}

			if (total_wins_count > 0)
			{
				// Choosing a value for X close to 1 makes the weighted average immune to changes
				// that last a short time. Choosing a value for X close to 0 makes the weighted
				// average respond to changes very quickly.
				constexpr auto X = 0.95;
				const auto new_wins_sample = (total_wins / total_wins_count);
				const auto old_size = m_SendWindowSize;
				m_SendWindowSize = static_cast<Size>(std::ceil(X * m_SendWindowSize + ((1.0 - X) * new_wins_sample)));

				// Never go below minimum
				m_SendWindowSize = std::max(MinSendWindowSize, m_SendWindowSize);
			}

#ifdef UDPCS_WND_DEBUG
			SLogInfo(SLogFmt(FGBrightMagenta) << L"UDP connection: SendWindowSize: " << m_SendWindowSize <<
					 L" - Min: " << static_cast<Size>(std::ceil(min_size)) << L" - Max: " <<
					 static_cast<Size>(std::ceil(max_size)) << L" - StdDev: " << m_SendWindowSizeVariance.GetStdDev() <<
					 L" - Mean: " << m_SendWindowSizeVariance.GetMean() << SLogFmt(Default));
#endif
			m_SendWindowSizeSamples.Expire();
		}

	private:
		void RecordRTTStats(const std::chrono::milliseconds rtt) noexcept
		{
			m_RTTVariance.Update(static_cast<double>(rtt.count()));
			m_RTTSamples.Add(RTTSample{ rtt });
		}

	public:
		static constexpr Size MinSendWindowSize{ 8 };

	private:
		static constexpr std::chrono::milliseconds StartRTT{ 600 };
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 1 };

		static constexpr Size MaxSendWindowSizeSamples{ 128 };

	private:
		std::chrono::milliseconds m_RTT{ StartRTT };
		OnlineVariance m_RTTVariance;
		RTTSampleList m_RTTSamples;

		bool m_NoLossYetRecorded{ true };
		Size m_NoLossSendWindowSize{ MinSendWindowSize };
		Size m_SendWindowSize{ MinSendWindowSize };
		OnlineVariance m_SendWindowSizeVariance;
		SendWindowSampleList m_SendWindowSizeSamples;
		double m_NewSendWindowSizeSample{ MinSendWindowSize };
		double m_OldSendWindowSizeSample{ MinSendWindowSize };
		SteadyTime m_LastSendWindowSampleSteadyTime;
	};
}