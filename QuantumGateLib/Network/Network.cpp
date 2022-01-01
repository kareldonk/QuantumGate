// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Network.h"

namespace QuantumGate::Implementation::Network
{
	const WChar* GetProtocolName(const Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Protocol::ICMP:
				return L"ICMP";
			case Protocol::UDP:
				return L"UDP";
			case Protocol::TCP:
				return L"TCP";
			case Protocol::RFCOMM:
				return L"RFCOMM";
			case Protocol::Unspecified:
				return L"Unspecified";
			default:
				assert(false);
				break;
		}

		return L"Unknown";
	}
}