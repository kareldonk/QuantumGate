// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::Relay
{
	class DataRateLimit final
	{
		struct DataMessageDetails
		{
			RelayMessageID ID{ 0 };
			Size NumBytes{ 0 };
			SteadyTime TimeSent;
			std::optional<SteadyTime> TimeAckReceived;
		};

		using DataMessageList = Containers::List<DataMessageDetails>;

	public:
		[[nodiscard]] inline RelayMessageID GetNewDataMessageID() noexcept
		{
			return m_MessageIDCounter++;
		}

		[[nodiscard]] bool AddDataMessage(const RelayMessageID id, const Size num_bytes, const SteadyTime time_sent) noexcept
		{
			assert(CanAddDataMessage(num_bytes));

			try
			{
				m_DataMessageList.emplace_front(DataMessageDetails{ id, num_bytes, time_sent });

				if (m_DataMessageList.size() > MaxDataMessageHistory)
				{
					m_DataMessageList.pop_back();
				}

				m_CurrentWindowSizeInUse += num_bytes;

				LogDbg(L"Relay data rate: added message ID %u, %zu bytes", id, num_bytes);

				return true;
			}
			catch (...) {}

			LogErr(L"Failed to add message ID %u with %zu bytes to relay data limit", id, num_bytes);

			return false;
		}

		[[nodiscard]] bool UpdateDataMessage(const RelayMessageID id, const SteadyTime time_ack_received) noexcept
		{
			const auto it = std::find_if(m_DataMessageList.begin(), m_DataMessageList.end(),
										 [&](const auto& dmd) { return (dmd.ID == id); });
			if (it != m_DataMessageList.end())
			{
				assert(time_ack_received > it->TimeSent);

				if (time_ack_received > it->TimeSent)
				{
					it->TimeAckReceived = time_ack_received;

					if (m_CurrentWindowSizeInUse > it->NumBytes)
					{
						m_CurrentWindowSizeInUse -= it->NumBytes;
					}
					else m_CurrentWindowSizeInUse = 0;

					LogDbg(L"Relay data rate: received ack for message ID %u, %zu bytes, roundtrip time: %jd ms",
						   id, it->NumBytes, std::chrono::duration_cast<std::chrono::milliseconds>(time_ack_received - it->TimeSent).count());

					CalculateDataRate();
				}
				else
				{
					LogErr(L"Failed to update message ID %u on relay data limit; the ACK time was smaller than the sent time", id);
					return false;
				}
			}

			return true;
		}

		[[nodiscard]] inline bool CanAddDataMessage(const Size num_bytes) const noexcept
		{
			return (GetAvailableWindowSize() >= num_bytes);
		}

		[[nodiscard]] inline Size GetAvailableWindowSize() const noexcept
		{
			if (m_MaxWindowSize > m_CurrentWindowSizeInUse)
			{
				return (m_MaxWindowSize - m_CurrentWindowSizeInUse);
			}

			return 0;
		}

		[[nodiscard]] inline Size GetMaxWindowSize() const noexcept { return m_MaxWindowSize; }

	private:
		void CalculateDataRate() noexcept
		{
			Size total_bytes{ 0 };
			std::chrono::nanoseconds total_time{ 0 };

			for (const auto& dmd : m_DataMessageList)
			{
				if (dmd.TimeAckReceived.has_value())
				{
					total_bytes += dmd.NumBytes;
					total_time += (*dmd.TimeAckReceived - dmd.TimeSent);
				}
			}

			// Assuming sending data takes 75% of the RTT and receiving ACK 25% of the RTT
			// we only use 75% (0.75) of the total RTT (rough average estimate)
			const double data_rate_milliseconds =
				(static_cast<double>(total_bytes) / ((static_cast<double>(total_time.count()) * 0.75) / 1'000'000.0));

			m_MaxWindowSize = std::max(static_cast<Size>(data_rate_milliseconds * static_cast<double>(WindowInterval.count())), MinWindowSize);

			if (m_CurrentWindowSizeInUse > m_MaxWindowSize)
			{
				m_CurrentWindowSizeInUse = m_MaxWindowSize;
			}

			LogDbg(L"Relay data rate - total bytes sent: %zu in %jd ms - rate: %.2lf B/ms - MaxWindowSize: %zu bytes",
				   total_bytes, std::chrono::duration_cast<std::chrono::milliseconds>(total_time).count(),
				   data_rate_milliseconds, m_MaxWindowSize);
		}

	private:
		static constexpr Size MaxDataMessageHistory{ 100 };

		static constexpr Size MinWindowSize{ 1u << 12 }; // 4KB

		// Allow 1000ms of data to be in transit before receiving ACK
		static constexpr std::chrono::milliseconds WindowInterval{ 1000 };

	private:
		RelayMessageID m_MessageIDCounter{ 0 };
		Size m_MaxWindowSize{ MinWindowSize };
		Size m_CurrentWindowSizeInUse{ 0 };
		DataMessageList m_DataMessageList;
	};
}