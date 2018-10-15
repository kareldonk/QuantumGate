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

		Socket(const QuantumGate::Implementation::Network::IP::AddressFamily af) :
			QuantumGate::Implementation::Network::Socket(af,
														 QuantumGate::Implementation::Network::Socket::Type::Stream,
														 QuantumGate::Implementation::Network::IP::Protocol::TCP)
		{}

		using QuantumGate::Implementation::Network::Socket::Send;
		using QuantumGate::Implementation::Network::Socket::Receive;
	};
}