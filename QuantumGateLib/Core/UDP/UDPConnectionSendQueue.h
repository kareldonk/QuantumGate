// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPConnectionCommon.h"
#include "UDPConnectionMTUD.h"

// Use to enable/disable debug console output
// #define UDPSND_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection;

	class SendQueue final
	{
	public:
		struct Item final
		{
			Message::Type MessageType{ Message::Type::Unknown };
			Message::SequenceNumber SequenceNumber{ 0 };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			SteadyTime TimeResent;
			Buffer Data;
			bool Acked{ false };
			SteadyTime TimeAcked;
		};

		SendQueue(Connection& connection) noexcept;
		SendQueue(const SendQueue&) = delete;
		SendQueue(SendQueue&&) noexcept = delete;
		~SendQueue() = default;
		SendQueue& operator=(const SendQueue&) = delete;
		SendQueue& operator=(SendQueue&&) noexcept = delete;
		
		void SetMaxMessageSize(const Size size) noexcept;
		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaxMessageSize; }

		[[nodiscard]] bool Add(Item&& item) noexcept;

		[[nodiscard]] bool Process() noexcept;

		void SetPeerAdvertisedReceiveWindowSizes(const Size num_items, const Size num_bytes) noexcept;
		[[nodiscard]] Size GetAvailableSendWindowByteSize() noexcept;

		[[nodiscard]] inline Message::SequenceNumber GetNextSendSequenceNumber() const noexcept { return m_NextSendSequenceNumber; }

		void ProcessReceivedInSequenceAck(const Message::SequenceNumber seqnum) noexcept;
		void ProcessReceivedAcks(const Vector<Message::SequenceNumber>& acks) noexcept;
		void ProcessReceivedNAcks(const Vector<Message::NAckRange>& nack_ranges) noexcept;

	private:
		void AckItem(Item& item) noexcept;
		[[nodiscard]] Size AckSentMessage(const Message::SequenceNumber seqnum) noexcept;
		void PurgeAcked() noexcept;

		void RecalcPeerReceiveWindowSize() noexcept;
		[[nodiscard]] Size GetSendWindowByteSize() noexcept;

	private:
		using Queue = Containers::List<Item>;

		Connection& m_Connection;
		Size m_NumBytesInQueue{ 0 };
		Queue m_Queue;
		Statistics m_Statistics;

		Message::SequenceNumber m_NextSendSequenceNumber{ 0 };
		Message::SequenceNumber m_LastInSequenceAckedSequenceNumber{ 0 };

		Size m_MaxMessageSize{ MTUDiscovery::MinMessageSize };

		Size m_PeerAdvReceiveWindowItemSize{ MinReceiveWindowItemSize };
		Size m_PeerAdvReceiveWindowByteSize{ MinReceiveWindowItemSize * MTUDiscovery::MinMessageSize };
		Size m_PeerReceiveWindowItemSize{ MinReceiveWindowItemSize };
	};
}