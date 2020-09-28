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

	static constexpr Size MinReceiveWindowItemSize{ Statistics::MinMTUWindowSize };
	static constexpr Size MaxReceiveWindowItemSize{ std::numeric_limits<Message::SequenceNumber>::max() / 2 };
	static constexpr Size MaxReceiveWindowBytes{ 1 << 20 };
}