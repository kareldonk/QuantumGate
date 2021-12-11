// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Endpoint.h"

namespace QuantumGate::Implementation::Network
{
	AddressFamily Endpoint::GetAddressFamily() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return IP::AddressFamilyToNetwork(m_IPEndpoint.GetIPAddress().GetFamily());
			case Type::BTH:
				return BTH::AddressFamilyToNetwork(m_BTHEndpoint.GetBTHAddress().GetFamily());
			case Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return AddressFamily::Unspecified;
	}

	Protocol Endpoint::GetProtocol() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return IP::ProtocolToNetwork(m_IPEndpoint.GetProtocol());
			case Type::BTH:
				return BTH::ProtocolToNetwork(m_BTHEndpoint.GetProtocol());
			case Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return Protocol::Unspecified;
	}

	String Endpoint::GetString() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return m_IPEndpoint.GetString();
			case Type::BTH:
				return m_BTHEndpoint.GetString();
			case Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return L"Unspecified";
	}

	std::ostream& operator<<(std::ostream& stream, const Endpoint& endpoint)
	{
		stream << Util::ToStringA(endpoint.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const Endpoint& endpoint)
	{
		stream << endpoint.GetString();
		return stream;
	}
}