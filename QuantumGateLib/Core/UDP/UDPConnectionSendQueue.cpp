// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "UDPConnectionSendQueue.h"
#include "UDPConnection.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	SendQueue::SendQueue(Connection& connection) noexcept : m_Connection(connection)
	{
		m_NextSendSequenceNumber = static_cast<Message::SequenceNumber>(Random::GetPseudoRandomNumber());
	}

	void SendQueue::SetMaxMessageSize(const Size size) noexcept
	{
		m_MaxMessageSize = size;

		RecalcPeerReceiveWindowSize();
	}

	Size SendQueue::GetMaxMessageSize() const noexcept
	{
		// Message may not be larger than the peer is willing to receive
		return std::min(m_PeerAdvReceiveWindowByteSize, m_MaxMessageSize);
	}

	void SendQueue::SetPeerAdvertisedReceiveWindowSizes(const Size num_items, const Size num_bytes) noexcept
	{
		m_PeerAdvReceiveWindowItemSize = num_items;
		m_PeerAdvReceiveWindowByteSize = num_bytes;

		RecalcPeerReceiveWindowSize();
	}

	bool SendQueue::Add(Item&& item) noexcept
	{
		try
		{
			const bool use_listener_socket = (item.MessageType == Message::Type::Syn &&
											  m_Connection.GetType() == PeerConnectionType::Inbound);

			const auto result = m_Connection.Send(item.TimeSent, item.Data, use_listener_socket);
			if (result.Succeeded()) item.NumTries = 1;

			const auto size = item.Data.GetSize();

			m_Queue.emplace_back(std::move(item));

			m_NumBytesInQueue += size;

			m_NextSendSequenceNumber = Message::GetNextSequenceNumber(m_NextSendSequenceNumber);

			return true;
		}
		catch (...) {}

		return false;
	}

	bool SendQueue::Process() noexcept
	{
		if (m_Queue.empty()) return true;

		const auto rtt_timeout = std::invoke([&]()
		{
			return (m_Connection.GetStatus() < Status::Connected) ?
				m_Connection.GetSettings().UDP.ConnectRetransmissionTimeout :
				m_Statistics.GetRetransmissionTimeout();
		});

#ifdef UDPSND_DEBUG
		Size loss_num{ 0 };
#endif	
		Size loss_bytes{ 0 };

		const auto now = Util::GetCurrentSteadyTime();

		for (auto it = m_Queue.begin(); it != m_Queue.end(); ++it)
		{
			if (it->NumTries == 0 || (now - it->TimeResent >= rtt_timeout * it->NumTries))
			{
				if (it->NumTries > 0)
				{
#ifdef UDPSND_DEBUG
					SLogInfo(SLogFmt(FGBrightCyan) << L"UDP connection: retransmitting (" << it->NumTries <<
							 ") message with sequence number " <<  it->SequenceNumber << L" (timeout " <<
							 std::chrono::duration_cast<std::chrono::milliseconds>(rtt_timeout).count() * it->NumTries <<
							 L"ms) for connection " << m_Connection.GetID() << SLogFmt(Default));

					++loss_num;
#endif	
					loss_bytes += it->Data.GetSize();
				}

				const bool use_listener_socket = (it->MessageType == Message::Type::Syn &&
												  m_Connection.GetType() == PeerConnectionType::Inbound);
				
				const auto result = m_Connection.Send(now, it->Data, use_listener_socket);
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
					LogErr(L"UDP connection: send failed on connection %llu (%s)",
						   m_Connection.GetID(), result.GetErrorString().c_str());
					return false;
				}
			}
		}

		m_Statistics.RecordMTULoss(static_cast<double>(loss_bytes) / static_cast<double>(GetMaxMessageSize()));
		m_Statistics.RecordMTUWindowSizeStats();

#ifdef UDPSND_DEBUG
		if (loss_num > 0)
		{
			SLogWarn(SLogFmt(FGBrightCyan) << L"UDP connection: retransmitted " << loss_num <<
					 " items (" <<  loss_bytes << L" bytes), queue size " << m_Queue.size() << L", MTU window size " <<
					 m_Statistics.GetMTUWindowSize() << L" (" << GetSendWindowByteSize() << L" bytes), RTT " <<
					 m_Statistics.GetRetransmissionTimeout().count() << L"ms" << SLogFmt(Default));
		}
#endif
		return true;
	}

	Size SendQueue::GetAvailableSendWindowByteSize() noexcept
	{
		if (m_Queue.size() >= m_PeerReceiveWindowItemSize) return 0;

		const auto send_wnd_size = GetSendWindowByteSize();
		if (send_wnd_size > m_NumBytesInQueue)
		{
			return (send_wnd_size - m_NumBytesInQueue);
		}
		else return 0;
	}

	void SendQueue::ProcessReceivedInSequenceAck(const Message::SequenceNumber seqnum) noexcept
	{
		if (m_LastInSequenceAckedSequenceNumber == seqnum) return;

		m_LastInSequenceAckedSequenceNumber = seqnum;

		auto it = std::find_if(m_Queue.begin(), m_Queue.end(), [&](const auto& itm)
		{
			return (itm.SequenceNumber == seqnum);
		});

		if (it != m_Queue.end())
		{
			const auto now = Util::GetCurrentSteadyTime();

			auto purge_acked{ false };
			Size num_bytes{ 0 };

			for (auto it2 = m_Queue.begin();;)
			{
				if (it2->NumTries > 0)
				{
					if (!it2->Acked)
					{
						AckItem(*it2, now);

						num_bytes += it2->Data.GetSize();
						purge_acked = true;
					}
				}

				if (it2->SequenceNumber == it->SequenceNumber) break;
				else ++it2;
			}

			m_Statistics.RecordMTUAck(static_cast<double>(num_bytes) / static_cast<double>(GetMaxMessageSize()));

			if (purge_acked)
			{
				PurgeAcked();
			}
		}
	}

	void SendQueue::ProcessReceivedAcks(const Vector<Message::AckRange>& ack_ranges) noexcept
	{
		const auto now = Util::GetCurrentSteadyTime();

		auto purge_acked{ false };
		Size num_bytes{ 0 };

		for (const auto& ack_range : ack_ranges)
		{
			for (auto seqnum = ack_range.Begin; seqnum <= ack_range.End;)
			{
				const auto [acked, msg_size] = AckSentMessage(seqnum, now);
				if (acked)
				{
					num_bytes += msg_size;
					purge_acked = true;
				}

				if (seqnum < std::numeric_limits<Message::SequenceNumber>::max())
				{
					++seqnum;
				}
				else break;
			}
		}

		m_Statistics.RecordMTUAck(static_cast<double>(num_bytes) / static_cast<double>(GetMaxMessageSize()));

		if (purge_acked)
		{
			PurgeAcked();
		}
	}

	void SendQueue::AckItem(Item& item, const SteadyTime& now) noexcept
	{
		item.Acked = true;
		item.TimeAcked = now;

		// Only record RTT for items that have not been
		// retransmitted as per Karn's Algorithm
		if (item.NumTries == 1)
		{
			m_Statistics.RecordRTT(std::chrono::duration_cast<std::chrono::nanoseconds>(item.TimeAcked - item.TimeSent));
		}
	}

	void SendQueue::PurgeAcked() noexcept
	{
		// Remove all acked messages from the front of the list
		// to make room for new messages in the send window
		for (auto it = m_Queue.begin(); it != m_Queue.end();)
		{
			if (it->Acked)
			{
				m_NumBytesInQueue -= it->Data.GetSize();
				
				it = m_Queue.erase(it);
			}
			else break;
		}
	}

	std::pair<bool, Size> SendQueue::AckSentMessage(const Message::SequenceNumber seqnum, const SteadyTime& now) noexcept
	{
		auto it = std::find_if(m_Queue.begin(), m_Queue.end(), [&](const auto& itm)
		{
			return (itm.SequenceNumber == seqnum);
		});

		if (it != m_Queue.end())
		{
			Dbg(L"UDP connection: received ack for message with seq# %u for connection %llu",
				seqnum, m_Connection.GetID());

			if (!it->Acked)
			{
				AckItem(*it, now);

				return std::make_pair(true, it->Data.GetSize());
			}
		}

		return std::make_pair(false, 0);
	}

	void SendQueue::RecalcPeerReceiveWindowSize() noexcept
	{
		const auto wndsize = std::max(MinReceiveWindowItemSize, m_PeerAdvReceiveWindowByteSize / GetMaxMessageSize());
		m_PeerReceiveWindowItemSize = std::min(wndsize, m_PeerAdvReceiveWindowItemSize);

#ifdef UDPSND_DEBUG
		SLogInfo(SLogFmt(FGBrightCyan) << L"UDP connection: PeerAdvReceiveWindowSizeBytes: " <<
				 m_PeerAdvReceiveWindowByteSize << " - PeerAdvReceiveWindowItemSize: " <<  m_PeerAdvReceiveWindowItemSize <<
				 L" - PeerReceiveWindowItemSize: " << m_PeerReceiveWindowItemSize << SLogFmt(Default));
#endif
	}

	Size SendQueue::GetSendWindowByteSize() noexcept
	{
		return std::min(m_Statistics.GetMTUWindowSize() * GetMaxMessageSize(), m_PeerAdvReceiveWindowByteSize);
	}
}