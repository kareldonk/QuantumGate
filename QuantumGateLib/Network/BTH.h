// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Network.h"

namespace QuantumGate::Implementation::Network::BTH
{
	enum class AddressFamily : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::AddressFamily::Unspecified),
		BTH = static_cast<UInt8>(Network::AddressFamily::BTH)
	};

	[[nodiscard]] constexpr AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept
	{
		switch (af)
		{
			case Network::AddressFamily::Unspecified:
				return AddressFamily::Unspecified;
			case Network::AddressFamily::BTH:
				return AddressFamily::BTH;
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
			case AddressFamily::BTH:
				return Network::AddressFamily::BTH;
			default:
				assert(false);
				break;
		}

		return Network::AddressFamily::Unspecified;
	}

	enum class Protocol : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::Protocol::Unspecified),
		RFCOMM = static_cast<UInt8>(Network::Protocol::RFCOMM)
	};

	[[nodiscard]] constexpr Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Network::Protocol::Unspecified:
				return Protocol::Unspecified;
			case Network::Protocol::RFCOMM:
				return Protocol::RFCOMM;
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
			case Protocol::RFCOMM:
				return Network::Protocol::RFCOMM;
			default:
				assert(false);
				break;
		}

		return Network::Protocol::Unspecified;
	}
}