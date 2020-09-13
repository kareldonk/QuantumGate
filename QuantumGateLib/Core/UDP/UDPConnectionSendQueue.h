// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPConnectionCommon.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class SendQueue final
	{
	public:
		struct Item final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			bool IsSyn{ false };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			SteadyTime TimeResent;
			Buffer Data;
			bool Acked{ false };
			SteadyTime TimeAcked;
		};

		SendQueue() noexcept
		{
			m_NextSendSequenceNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
		}

		SendQueue(const SendQueue&) = delete;
		SendQueue(SendQueue&&) noexcept = delete;
		~SendQueue() = default;
		SendQueue& operator=(const SendQueue&) = delete;
		SendQueue& operator=(SendQueue&&) noexcept = delete;
		
		inline void SetMaxMessageSize(const Size size) noexcept
		{
			m_MaxMessageSize = size;
			
			RecalcPeerReceiveWindowSize();
		}

		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaxMessageSize; }

		inline void SetPeerAdvReceiveWindowSizes(const Size num_items, const Size num_bytes) noexcept
		{
			m_PeerAdvReceiveWindowItemSize = num_items;
			m_PeerAdvReceiveWindowByteSize = num_bytes;

			RecalcPeerReceiveWindowSize();
		}

		[[nodiscard]] bool Add(Item&& item, const IPEndpoint& endpoint, Network::Socket& socket,
							   const PeerConnectionType connection_type, ConnectionData_ThS& connection_data) noexcept
		{
			try
			{
				Network::Socket* s = &socket;
				if (item.IsSyn && connection_type == PeerConnectionType::Inbound)
				{
					LogWarn(L"Using listener socket to send UDP msg");
					s = connection_data.WithUniqueLock()->GetListenerSocket();
				}

				const auto result = s->SendTo(endpoint, item.Data);
				if (result.Succeeded()) item.NumTries = 1;

				const auto size = item.Data.GetSize();

				m_Queue.emplace_back(std::move(item));

				m_NumBytesInQueue += size;

				IncrementSendSequenceNumber();

				return true;
			}
			catch (...) {}

			return false;
		}

		[[nodiscard]] Buffer GetFreeBuffer() noexcept
		{
			if (!m_FreeBufferList.empty())
			{
				Buffer buffer = std::move(m_FreeBufferList.front());
				m_FreeBufferList.pop_front();
				return buffer;
			}

			return {};
		}

		[[nodiscard]] bool Send(Network::Socket& socket, const Status connection_status,
								const PeerConnectionType connection_type, ConnectionData_ThS& connection_data) noexcept
		{
			if (m_Queue.empty()) return true;

			m_Statistics.RecalcRetransmissionTimeout();

			const auto rtt_timeout = std::invoke([&]()
			{
				return (connection_status < Status::Connected) ? ConnectRetransmissionTimeout : m_Statistics.GetRetransmissionTimeout();
			});

			Size loss_num{ 0 };
			Size loss_bytes{ 0 };

			const auto endpoint = connection_data.WithSharedLock()->GetPeerEndpoint();

			for (auto it = m_Queue.begin(); it != m_Queue.end(); ++it)
			{
				if (it->NumTries == 0 || (Util::GetCurrentSteadyTime() - it->TimeResent >= rtt_timeout * it->NumTries))
				{
					if (it->NumTries > 0)
					{
						SLogDbg(SLogFmt(FGBrightCyan) << L"UDP connection: retransmitting (" << it->NumTries <<
								") message with sequence number " <<  it->SequenceNumber << L" (timeout " <<
								rtt_timeout.count() * it->NumTries << L"ms)" << SLogFmt(Default));
						
						++loss_num;
						loss_bytes += it->Data.GetSize();
					}
					else
					{
						LogDbg(L"UDP connection: sending message with sequence number %u", it->SequenceNumber);
					}

					Network::Socket* s = &socket;
					if (it->IsSyn && connection_type == PeerConnectionType::Inbound)
					{
						LogWarn(L"UDP connection: using listener socket to send UDP msg");
						s = connection_data.WithUniqueLock()->GetListenerSocket();
					}

					const auto result = s->SendTo(endpoint, it->Data);
					if (result.Succeeded())
					{
						// If data was actually sent, otherwise buffer may
						// temporarily be full/unavailable
						if (*result == it->Data.GetSize())
						{
							// We'll wait for ack or else continue sending
							it->TimeResent = Util::GetCurrentSteadyTime();
							++it->NumTries;
						}
						else
						{
							// We'll try again later
							break;
						}
					}
					else
					{
						LogErr(L"UDP connection: send failed for peer %s (%s)",
							   endpoint.GetString().c_str(), result.GetErrorString().c_str());
						return false;
					}
				}
			}

			m_Statistics.RecordMTULoss(static_cast<double>(loss_bytes) / static_cast<double>(m_MaxMessageSize));
			m_Statistics.RecordMTUWindowSizeStats();

			DbgInvoke([&]()
			{
				if (loss_num > 0)
				{
					LogWarn(L"UDP connection: retransmitted %zu items (%zu bytes) (queue size %zu, MTU window size %zu (%zu bytes), RTT %jdms)",
							loss_num, loss_bytes, m_Queue.size(), m_Statistics.GetMTUWindowSize(),
							GetSendWindowByteSize(), m_Statistics.GetRetransmissionTimeout().count());
				}
			});

			return true;
		}

		inline Size GetAvailableSendWindowByteSize() noexcept
		{
			if (m_Queue.size() >= m_PeerReceiveWindowItemSize) return 0;

			const auto send_wnd_size = GetSendWindowByteSize();
			if (send_wnd_size > m_NumBytesInQueue)
			{
				return (send_wnd_size - m_NumBytesInQueue);
			}
			else return 0;
		}

		inline Message::SequenceNumber GetNextSendSequenceNumber() const noexcept { return m_NextSendSequenceNumber; }

		void ProcessReceivedInSequenceAck(const Message::SequenceNumber seqnum) noexcept
		{
			if (m_LastInSequenceAckedSequenceNumber == seqnum) return;

			m_LastInSequenceAckedSequenceNumber = seqnum;

			auto it = std::find_if(m_Queue.begin(), m_Queue.end(), [&](const auto& itm)
			{
				return (itm.SequenceNumber == seqnum);
			});

			if (it != m_Queue.end())
			{
				auto purge_acked{ false };
				Size num_bytes{ 0 };

				for (auto it2 = m_Queue.begin();;)
				{
					if (it2->NumTries > 0)
					{
						if (!it2->Acked)
						{
							AckItem(*it2);

							num_bytes += it2->Data.GetSize();
							purge_acked = true;
						}
					}

					if (it2->SequenceNumber == it->SequenceNumber) break;
					else ++it2;
				}

				m_Statistics.RecordMTUAck(static_cast<double>(num_bytes) / static_cast<double>(m_MaxMessageSize));

				if (purge_acked)
				{
					PurgeAcked();
				}
			}
		}

		void ProcessReceivedAcks(const Vector<Message::SequenceNumber>& acks) noexcept
		{
			auto purge_acked{ false };
			Size num_bytes{ 0 };

			for (const auto ack_num : acks)
			{
				const auto msg_size = AckSentMessage(ack_num);
				num_bytes += msg_size;
				purge_acked = true;
			}

			m_Statistics.RecordMTUAck(static_cast<double>(num_bytes) / static_cast<double>(m_MaxMessageSize));

			if (purge_acked)
			{
				PurgeAcked();
			}
		}

	private:
		void AckItem(Item& item) noexcept
		{
			item.Acked = true;
			item.TimeAcked = Util::GetCurrentSteadyTime();

			// Only record RTT for items that have not been
			// retransmitted as per Karn's Algorithm
			if (item.NumTries == 1)
			{
				m_Statistics.RecordRTT(std::chrono::duration_cast<std::chrono::milliseconds>(item.TimeAcked - item.TimeSent));
			}
		}

		void PurgeAcked() noexcept
		{
			// Remove all acked messages from the front of the list
			// to make room for new messages in the send window
			for (auto it = m_Queue.begin(); it != m_Queue.end();)
			{
				if (it->Acked)
				{
					const auto size = it->Data.GetSize();
					
					try
					{
						m_FreeBufferList.emplace_front(std::move(it->Data));
					}
					catch (...) {}

					it = m_Queue.erase(it);

					m_NumBytesInQueue -= size;
				}
				else break;
			}
		}

		Size AckSentMessage(const Message::SequenceNumber seqnum) noexcept
		{
			auto it = std::find_if(m_Queue.begin(), m_Queue.end(), [&](const auto& itm)
			{
				return (itm.SequenceNumber == seqnum);
			});

			if (it != m_Queue.end())
			{
				LogDbg(L"UDP connection: received ack for message with seq# %u", seqnum);

				if (!it->Acked)
				{
					AckItem(*it);

					return it->Data.GetSize();
				}
			}

			return 0;
		}

		inline void RecalcPeerReceiveWindowSize() noexcept
		{
			const auto wndsize = std::max(MinReceiveWindowItemSize, m_PeerAdvReceiveWindowByteSize / m_MaxMessageSize);
			m_PeerReceiveWindowItemSize = std::min(wndsize, m_PeerAdvReceiveWindowItemSize);

			LogWarn(L"UDP connection: PeerAdvReceiveWindowSizeBytes: %zu - PeerAdvReceiveWindowSize: %zu - PeerReceiveWindowSize: %zu",
					m_PeerAdvReceiveWindowByteSize, m_PeerAdvReceiveWindowItemSize, m_PeerReceiveWindowItemSize);
		}

		inline Size GetSendWindowByteSize() noexcept
		{
			m_Statistics.RecalcMTUWindowSize();
			return std::min(m_Statistics.GetMTUWindowSize() * m_MaxMessageSize, m_PeerAdvReceiveWindowByteSize);
		}

		inline void IncrementSendSequenceNumber() noexcept
		{
			m_NextSendSequenceNumber = Message::GetNextSequenceNumber(m_NextSendSequenceNumber);
		}

	private:
		using Queue = Containers::List<Item>;
		using BufferList = Containers::ForwardList<Buffer>;

		Size m_NumBytesInQueue{ 0 };
		Queue m_Queue;
		BufferList m_FreeBufferList;
		Statistics m_Statistics;

		Message::SequenceNumber m_NextSendSequenceNumber{ 0 };
		Message::SequenceNumber m_LastInSequenceAckedSequenceNumber{ 0 };

		Size m_MaxMessageSize{ MTUDiscovery::MinMessageSize };

		Size m_PeerAdvReceiveWindowItemSize{ MinReceiveWindowItemSize };
		Size m_PeerAdvReceiveWindowByteSize{ MinReceiveWindowItemSize * MTUDiscovery::MinMessageSize };
		Size m_PeerReceiveWindowItemSize{ MinReceiveWindowItemSize };
	};
}