// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Network.h"

namespace QuantumGate::Implementation::Network::IMF
{
	enum class AddressFamily : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::AddressFamily::Unspecified),
		IMF = static_cast<UInt8>(Network::AddressFamily::IMF)
	};

	[[nodiscard]] constexpr AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept
	{
		switch (af)
		{
			case Network::AddressFamily::Unspecified:
				return AddressFamily::Unspecified;
			case Network::AddressFamily::IMF:
				return AddressFamily::IMF;
			default:
				assert(false);
				break;
		}

		return AddressFamily::Unspecified;
	}

	[[nodiscard]] constexpr Network::AddressFamily AddressFamilyToNetwork(const AddressFamily protocol) noexcept
	{
		switch (protocol)
		{
			case AddressFamily::Unspecified:
				return Network::AddressFamily::Unspecified;
			case AddressFamily::IMF:
				return Network::AddressFamily::IMF;
			default:
				assert(false);
				break;
		}

		return Network::AddressFamily::Unspecified;
	}

	enum class Protocol : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::Protocol::Unspecified),
		IMF = static_cast<UInt8>(Network::Protocol::IMF)
	};

	[[nodiscard]] constexpr Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Network::Protocol::Unspecified:
				return Protocol::Unspecified;
			case Network::Protocol::IMF:
				return Protocol::IMF;
			default:
				assert(false);
				break;
		}

		return Protocol::Unspecified;
	}

	[[nodiscard]] constexpr Network::Protocol ProtocolToNetwork(const Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Protocol::Unspecified:
				return Network::Protocol::Unspecified;
			case Protocol::IMF:
				return Network::Protocol::IMF;
			default:
				assert(false);
				break;
		}

		return Network::Protocol::Unspecified;
	}
}