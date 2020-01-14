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
			UInt64 ID{ 0 };
			Size NumBytes{ 0 };
			SteadyTime TimeSent;
			std::optional<SteadyTime> TimeAckReceived;
		};

		using DataMessageList = Containers::List<DataMessageDetails>;

	public:
		[[nodiscard]] inline UInt64 GetNewDataMessageID() noexcept
		{
			return m_MessageIDCounter++;
		}

		[[nodiscard]] bool AddDataMessage(const UInt64 id, const Size num_bytes, const SteadyTime time_sent) noexcept
		{
			assert(CanAddDataMessage(num_bytes));

			try
			{
				m_DataMessageList.emplace_front(DataMessageDetails{ id, num_bytes, time_sent });

				if (m_DataMessageList.size() > MaxDataMessageHistory)
				{
					m_DataMessageList.pop_back();
				}

				m_CurrentNumBytes += num_bytes;

				LogDbg(L"Relay data rate: added message ID %llu, %zu bytes", id, num_bytes);

				return true;
			}
			catch (...) {}

			LogErr(L"Failed to add message ID %llu with %zu bytes to relay data limit", id, num_bytes);

			return false;
		}

		[[nodiscard]] bool UpdateDataMessage(const UInt64 id, const SteadyTime time_ack_received) noexcept
		{
			const auto it = std::find_if(m_DataMessageList.begin(), m_DataMessageList.end(),
										 [&](const auto& dmd) { return (dmd.ID == id); });
			if (it != m_DataMessageList.end())
			{
				assert(time_ack_received > it->TimeSent);

				if (time_ack_received > it->TimeSent)
				{
					it->TimeAckReceived = time_ack_received;

					if (m_CurrentNumBytes >= it->NumBytes)
					{
						m_CurrentNumBytes -= it->NumBytes;
					}
					else m_CurrentNumBytes = 0;

					LogDbg(L"Relay data rate: received ack for message ID %llu, %zu bytes, roundtrip time: %llu ns",
						   id, it->NumBytes, std::chrono::duration_cast<std::chrono::nanoseconds>(time_ack_received - it->TimeSent).count());

					CalculateDataRate();
				}
				else
				{
					LogErr(L"Failed to update message ID %llu on relay data limit; the ACK time was smaller than the sent time", id);
					return false;
				}
			}

			return true;
		}

		[[nodiscard]] inline bool CanAddDataMessage(const Size num_bytes) const noexcept
		{
			return (GetNumBytesAvailable() >= num_bytes);
		}

		[[nodiscard]] inline Size GetNumBytesAvailable() const noexcept
		{
			if (m_MaxNumBytes > m_CurrentNumBytes)
			{
				return (m_MaxNumBytes - m_CurrentNumBytes);
			}

			return 0;
		}

		[[nodiscard]] inline Size GetMaxNumBytes() const noexcept { return m_MaxNumBytes; }

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

			const double data_rate_milliseconds =
				(static_cast<double>(total_bytes) / (static_cast<double>(total_time.count()) / 2.0 / 1'000'000.0));

			m_MaxNumBytes = (std::max)(static_cast<Size>(data_rate_milliseconds * static_cast<double>(DataRateInterval.count())), DataRateMinNumBytes);

			if (m_CurrentNumBytes > m_MaxNumBytes)
			{
				m_CurrentNumBytes = m_MaxNumBytes;
			}

			LogDbg(L"Relay data rate - total bytes sent: %zu in %llu ns - rate: %.2lf b/ms - MaxNumBytes: %zu",
				   total_bytes, total_time.count(), data_rate_milliseconds, m_MaxNumBytes);
		}

	private:
		static constexpr Size MaxDataMessageHistory{ 100 };

		static constexpr Size DataRateMinNumBytes{ 1u << 12 }; // 4KB;
		static constexpr std::chrono::milliseconds DataRateInterval{ 500 };

	private:
		UInt64 m_MessageIDCounter{ 0 };
		Size m_MaxNumBytes{ 1u << 16 };
		Size m_CurrentNumBytes{ 0 };
		DataMessageList m_DataMessageList;
	};
}