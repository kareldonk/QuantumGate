// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Network\Socket.h"

namespace QuantumGate::Socks5Extender
{
	class Socket : public QuantumGate::Implementation::Network::Socket
	{
	public:
		Socket() noexcept : QuantumGate::Implementation::Network::Socket() {}

		Socket(const IPAddressFamily af) noexcept :
			QuantumGate::Implementation::Network::Socket(af, SOCK_STREAM, IPPROTO_TCP)
		{}

		using QuantumGate::Implementation::Network::Socket::Send;
		using QuantumGate::Implementation::Network::Socket::Receive;
	};
}