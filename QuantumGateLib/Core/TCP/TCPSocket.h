// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Network\Socket.h"

namespace QuantumGate::Implementation::Core::TCP
{
	class Socket final : public Network::Socket
	{
	public:
		Socket() noexcept = default;

		Socket(const Network::AddressFamily af) :
			Network::Socket(af, Network::Socket::Type::Stream, Network::Protocol::TCP)
		{}

		Socket(const Socket&) = delete;
		Socket(Socket&&) noexcept = default;
		~Socket() = default;
		Socket& operator=(const Socket&) = delete;
		Socket& operator=(Socket&&) noexcept = default;
	};
}