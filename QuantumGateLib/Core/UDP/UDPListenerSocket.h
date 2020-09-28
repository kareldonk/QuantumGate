// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\Socket.h"
#include "..\..\Concurrency\ThreadSafe.h"
#include "..\..\Common\Containers.h"

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	class Socket final : public Network::Socket
	{
		using Network::Socket::Socket;
	};

	struct SendQueueItem final
	{
		IPEndpoint Endpoint;
		Buffer Data;
	};

	using SendQueue_ThS = Concurrency::ThreadSafe<Containers::Queue<SendQueueItem>, std::shared_mutex>;
}