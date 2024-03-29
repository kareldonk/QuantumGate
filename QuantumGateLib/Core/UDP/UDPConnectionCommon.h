#pragma once

#include "UDPMessage.h"
#include "UDPConnectionData.h"
#include "UDPConnectionStats.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	enum class Status { Open, Handshake, Connected, Suspended, Closed };

	enum class CloseCondition
	{
		None, GeneralFailure, TimedOutError, ReceiveError, SendError, UnknownMessageError,
		LocalCloseRequest, PeerCloseRequest, PeerNotAllowed
	};

	struct UDPMessageSizes
	{
		// According to RFC 791 IPv4 requires an MTU of 576 octets or greater, while
		// the maximum size of the IP header is 60 (minimum is 20 octets).
		// According to RFC 8200 IPv6 requires an MTU of 1280 octets or greater, while
		// the minimum IPv6 header size (fixed header) is 40 octets. Recommended configuration
		// is for 1500 octets or greater.
		// Maximum message size is 65467 octets (65535 - 8 octet UDP header - 60 octet maximum IP header),
		// or 65507 octets (65535 - 8 octet UDP header - 20 octet minumum IP header)
		static constexpr Size Min{ 508 };
		static constexpr Size Max{ 65507 };
		static constexpr std::array<Size, 13> All{ Min, 548, 1232, 1452, 1460, 1472, 2048, 4096, 8192, 16384, 32768, 65467, Max };
	};

	static constexpr Size MinReceiveWindowItemSize{ Statistics::MinMTUWindowSize };
	static constexpr Size MaxReceiveWindowItemSize{ std::numeric_limits<Message::SequenceNumber>::max() / 2 };
	static constexpr Size MaxReceiveWindowBytes{ 1 << 20 };
}