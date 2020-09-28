// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\Socket.h"
#include "..\..\Concurrency\ThreadSafe.h"

namespace QuantumGate::Implementation::Core::UDP::Listener
{
	using Socket_ThS = Concurrency::ThreadSafe<Network::Socket, std::shared_mutex>;
}