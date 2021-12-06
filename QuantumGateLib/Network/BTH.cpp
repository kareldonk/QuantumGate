// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTH.h"

namespace QuantumGate::Implementation::Network::BTH
{
	AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept
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

	Network::AddressFamily AddressFamilyToNetwork(const AddressFamily protocol) noexcept
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

	Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Network::Protocol::Unspecified:
				return Protocol::Unspecified;
			case Network::Protocol::BTH:
				return Protocol::RFCOMM;
			default:
				assert(false);
				break;
		}

		return Protocol::Unspecified;
	}

	Network::Protocol ProtocolToNetwork(const Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Protocol::Unspecified:
				return Network::Protocol::Unspecified;
			case Protocol::RFCOMM:
				return Network::Protocol::BTH;
			default:
				assert(false);
				break;
		}

		return Network::Protocol::Unspecified;
	}
}