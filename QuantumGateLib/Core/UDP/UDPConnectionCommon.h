#pragma once

#include "UDPMessage.h"
#include "UDPConnectionData.h"
#include "UDPConnectionMTUD.h"
#include "UDPConnectionStats.h"

namespace QuantumGate::Implementation::Core::UDP::Connection
{
	enum class Status { Open, Handshake, Connected, Closed };

	enum class CloseCondition
	{
		None, GeneralFailure, TimedOutError, ReceiveError, SendError, UnknownMessageError,
		LocalCloseRequest, PeerCloseRequest
	};

	static constexpr std::chrono::seconds ConnectTimeout{ 30 };
	static constexpr std::chrono::milliseconds ConnectRetransmissionTimeout{ 1000 };
	static constexpr std::chrono::seconds MinKeepAliveTimeout{ 0 };
	static constexpr std::chrono::seconds MaxKeepAliveTimeout{ 45 };

	static constexpr Size MinReceiveWindowItemSize{ Statistics::MinMTUWindowSize };
	static constexpr Size MaxReceiveWindowItemSize{ std::numeric_limits<Message::SequenceNumber>::max() / 2 };
	static constexpr Size MaxReceiveWindowBytes{ 1 << 20 };
}