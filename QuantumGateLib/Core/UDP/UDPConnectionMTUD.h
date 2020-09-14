// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "UDPMessage.h"
#include "..\..\Common\Random.h"

// Use to enable/disable MTU discovery debug console output
// #define UDPMTUD_DEBUG

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	class Connection;

	class MTUDiscovery final
	{
		struct MTUDMessageData final
		{
			Message::SequenceNumber SequenceNumber{ 0 };
			UInt NumTries{ 0 };
			SteadyTime TimeSent;
			Buffer Data;
			bool Acked{ false };
		};

		enum class TransmitResult { Success, MessageTooLarge, Failed };

	public:
		enum class Status { Start, Discovery, Finished, Failed };

		MTUDiscovery(Connection& connection) noexcept;
		MTUDiscovery(const MTUDiscovery&) = delete;
		MTUDiscovery(MTUDiscovery&&) noexcept = delete;
		~MTUDiscovery() = default;
		MTUDiscovery& operator=(const MTUDiscovery&) = delete;
		MTUDiscovery& operator=(MTUDiscovery&&) noexcept = delete;

		[[nodiscard]] inline Size GetMaxMessageSize() const noexcept { return m_MaximumMessageSize; }

		[[nodiscard]] bool CreateNewMessage(const Size msg_size) noexcept;
		
		[[nodiscard]] TransmitResult TransmitMessage() noexcept;
		void ProcessTransmitResult(const TransmitResult result) noexcept;
		
		[[nodiscard]] Status Process() noexcept;
		
		void ProcessReceivedAck(const Message::SequenceNumber seqnum) noexcept;

		static void AckReceivedMessage(Connection& connection, const Message::SequenceNumber seqnum) noexcept;

	public:
		// According to RFC 791 IPv4 requires an MTU of 576 octets or greater, while
		// the maximum size of the IP header is 60.
		// According to RFC 8200 IPv6 requires an MTU of 1280 octets or greater, while
		// the minimum IPv6 header size (fixed header) is 40 octets. Recommended configuration
		// is for 1500 octets or greater.
		// Maximum message size is 65467 octets (65535 - 8 octet UDP header - 60 octet IP header).
		static constexpr std::array<Size, 9> MessageSizes{ 508, 1232, 1452, 2048, 4096, 8192, 16384, 32768, 65467 };
		static constexpr Size MinMessageSize{ 508 };
		static constexpr Size MaxMessageSize{  65467 };

	private:
		static constexpr std::chrono::milliseconds MinRetransmissionTimeout{ 600 };
		static constexpr Size MaxNumRetries{ 6 };

	private:
		Connection& m_Connection;
		Status m_Status{ Status::Start };
		std::optional<MTUDMessageData> m_MTUDMessageData;
		Size m_MaximumMessageSize{ MinMessageSize };
		Size m_CurrentMessageSizeIndex{ 0 };
		std::chrono::milliseconds m_RetransmissionTimeout{ MinRetransmissionTimeout };
	};
}
