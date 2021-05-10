// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\OnlineVariance.h"
#include "..\..\Common\RingList.h"

// Use to enable/disable debug console output
// #define RDRL_DEBUG

namespace QuantumGate::Implementation::Core::Relay
{
	class DataRateLimit final
	{
		static constexpr Size NumMTUsPerWindow{ 2u };

		struct MTUDetails
		{
			RelayMessageID ID{ 0 };
			Size NumBytes{ 0 };
			SteadyTime TimeSent;
		};

		using MTUList = RingList<MTUDetails, NumMTUsPerWindow>;

	public:
		[[nodiscard]] inline RelayMessageID GetNewMessageID() noexcept
		{
			return m_MessageIDCounter++;
		}

		[[nodiscard]] bool AddMTU(const RelayMessageID id, const Size num_bytes, const SteadyTime time_sent) noexcept
		{
			assert(CanAddMTU());

			if (m_MTUList.Add(MTUDetails{ id, num_bytes, time_sent }))
			{
				++m_WindowMTUsInUse;

				LogDbg(L"Relay data rate: added message ID %u, %zu bytes", id, num_bytes);

				return true;
			}

			LogErr(L"Relay data rate: failed to add message ID %u with %zu bytes to relay data limit", id, num_bytes);

			return false;
		}

		[[nodiscard]] bool AckMTU(const RelayMessageID id, const SteadyTime time_ack_received) noexcept
		{
			const auto it = std::find_if(m_MTUList.GetList().begin(), m_MTUList.GetList().end(),
										 [&](const auto& dmd) { return (dmd.ID == id); });
			if (it != m_MTUList.GetList().end())
			{
				assert(time_ack_received > it->TimeSent);

				if (time_ack_received > it->TimeSent)
				{
					const auto rtt = time_ack_received - it->TimeSent;
					RecordMTUAck(rtt, it->NumBytes);

					--m_WindowMTUsInUse;

					LogDbg(L"Relay data rate: received ack for message ID %u, %zu bytes, roundtrip time: %jd ms",
						   id, it->NumBytes, std::chrono::duration_cast<std::chrono::milliseconds>(rtt).count());
				}
				else
				{
					LogErr(L"Relay data rate: failed to update message ID %u on relay data limit; the ACK time was smaller than the sent time", id);
					return false;
				}
			}
			else
			{
				LogErr(L"Relay data rate: received ACK for message ID %u which does not exist", id);
			}

			return true;
		}

		[[nodiscard]] inline bool CanAddMTU() const noexcept
		{
			return (GetAvailableWindowMTUs() > 0);
		}

		[[nodiscard]] inline Size GetWindowSizeInBytes() const noexcept { return NumMTUsPerWindow * GetMTUSize(); }
		[[nodiscard]] inline Size GetMTUSize() const noexcept { return m_MTUSize; }

	private:
		[[nodiscard]] inline Size GetAvailableWindowMTUs() const noexcept
		{
			if (NumMTUsPerWindow > m_WindowMTUsInUse)
			{
				return (NumMTUsPerWindow - m_WindowMTUsInUse);
			}

			return 0;
		}

		void RecordMTUAck(const std::chrono::nanoseconds rtt, const Size num_bytes) noexcept
		{
			const auto now = Util::GetCurrentSteadyTime();

			if (m_RTTVariance.GetCount() > 0)
			{
				const auto threshold = std::min(m_RTTVariance.GetMean() / 2, m_RTTVariance.GetMinDev2());

				if (rtt.count() < threshold || now - m_LastSampleRecordedSteadyTime > SampleRecordingRestartTimeout)
				{
					m_RTTVariance.Restart();
					m_MTUVariance.Restart();

#ifdef RDRL_DEBUG
					const auto meanms = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetMean())));
					const auto stddevms = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetStdDev())));

					SLogInfo(SLogFmt(FGBrightCyan) << L"Relay connection: RTT restart: " <<
							 std::chrono::duration_cast<std::chrono::milliseconds>(rtt) << L" (mean: " << meanms <<
							 L", stddev: " << stddevms << L")" << SLogFmt(Default));
#endif
				}
			}

			m_RTTVariance.AddSample(static_cast<double>(rtt.count()));
			m_MTUVariance.AddSample(static_cast<double>(num_bytes));
			m_LastSampleRecordedSteadyTime = now;

			const auto rttns = static_cast<double>(rtt.count());
			const auto meanns = m_RTTVariance.GetMean();

			const double data_rate_second = (m_MTUVariance.GetMean() / (m_RTTVariance.GetMean() / 1'000'000'000.0));

			Size mtu{ m_MTUSize };

			if (rttns <= meanns)
			{
				const auto mtua = static_cast<Size>(data_rate_second * (1.0 - (rttns / meanns)));
				if (RelayDataMessage::MaxMessageDataSize - mtu > mtua)
				{
					mtu += mtua;
				}
				else mtu = RelayDataMessage::MaxMessageDataSize;
			}
			else
			{
				const auto mtur = static_cast<Size>(data_rate_second * (1.0 - (meanns / rttns)));
				if (mtur < mtu)
				{
					mtu -= mtur;
					mtu = std::max(MinMTUSize, mtu);
				}
				else mtu = MinMTUSize;
			}

			// Choosing a value for X close to 1 makes the weighted average immune to changes
			// that last a short time (e.g., a single message that encounters long delay).
			// Choosing a value for X close to 0 makes the weighted average respond to changes
			// in delay very quickly.
			const auto X{ 0.95 };
			const auto new_mtu = OnlineVariance<double>::WeightedSampleUpdate(static_cast<double>(m_MTUSize), static_cast<double>(mtu), X);
			m_MTUSize = static_cast<Size>(new_mtu);

#ifdef RDRL_DEBUG
			if (now - m_LastLogTime > std::chrono::seconds(1))
			{
				m_LastLogTime = now;

				const auto rttms = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(rttns)));
				const auto meanms = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetMean())));
				const auto stddevms = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(m_RTTVariance.GetStdDev())));

				SLogInfo(SLogFmt(FGBrightGreen) << L"Relay connection: RTT: " << rttms << L" (mean: " << meanms <<
						 L", stddev: " << stddevms << L") - Datarate: " << std::fixed << std::setprecision(2) <<
						 data_rate_second << L" B/s (mean: " << m_MTUVariance.GetMean() << L" B) - MTUSize: " <<
						 m_MTUSize << L" B - WindowSize: " << NumMTUsPerWindow << L" (" << GetWindowSizeInBytes() <<
						 " B), " << m_WindowMTUsInUse << L" used" << SLogFmt(Default));
			}
#endif
		}

	private:
		static constexpr Size MinMTUSize{ 1u << 16 }; // 65KB

		static constexpr std::chrono::seconds SampleRecordingRestartTimeout{ 2 };

	private:
		RelayMessageID m_MessageIDCounter{ 0 };
		
		OnlineVariance<double> m_RTTVariance;
		OnlineVariance<double> m_MTUVariance;
		SteadyTime m_LastSampleRecordedSteadyTime;

		Size m_MTUSize{ MinMTUSize };
		Size m_WindowMTUsInUse{ 0 };

		MTUList m_MTUList;

#ifdef RDRL_DEBUG
		SteadyTime m_LastLogTime;
#endif
	};
}