// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Network
{
	enum class AddressFamily : UInt8
	{
		Unspecified = 0,
		IPv4 = 2,
		IPv6 = 23,
		BTH = 32
	};

	enum class Protocol : UInt8
	{
		Unspecified = 0,
		ICMP = 1,
		TCP = 6,
		UDP = 17,
		BTH = 3
	};

	[[nodiscard]] const WChar* GetProtocolName(const Protocol protocol) noexcept;
}
