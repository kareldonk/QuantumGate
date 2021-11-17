// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "NetworkUtils.h"

namespace QuantumGate::Implementation::Network
{
	Protocol GetEndpointNetworkProtocol(const Endpoint& endpoint) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
				return IP::ProtocolToNetwork(endpoint.GetIPEndpoint().GetProtocol());
			case Endpoint::Type::BTH:
				return BTH::ProtocolToNetwork(endpoint.GetBTHEndpoint().GetProtocol());
			case Endpoint::Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return Protocol::Unspecified;
	}

	AddressFamily GetEndpointNetworkAddressFamily(const Endpoint& endpoint) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
				return IP::AddressFamilyToNetwork(endpoint.GetIPEndpoint().GetIPAddress().GetFamily());
			case Endpoint::Type::BTH:
				return BTH::AddressFamilyToNetwork(endpoint.GetBTHEndpoint().GetBTHAddress().GetFamily());
			case Endpoint::Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return AddressFamily::Unspecified;
	}

	Address GetEndpointAddress(const Endpoint& endpoint) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
				return endpoint.GetIPEndpoint().GetIPAddress();
			case Endpoint::Type::BTH:
				return endpoint.GetBTHEndpoint().GetBTHAddress();
			case Endpoint::Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return {};
	}

	UInt16 GetEndpointPort(const Endpoint& endpoint) noexcept
	{
		switch (endpoint.GetType())
		{
			case Endpoint::Type::IP:
				return endpoint.GetIPEndpoint().GetPort();
			case Endpoint::Type::BTH:
				return endpoint.GetBTHEndpoint().GetPort();
			case Endpoint::Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return 0;
	}
}