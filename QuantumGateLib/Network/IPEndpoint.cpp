// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IPEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	IPEndpoint::IPEndpoint(const Protocol protocol, const sockaddr_storage* addr)
	{
		assert(addr != nullptr);

		m_Protocol = ValidateProtocol(protocol);
		m_Address = IPAddress(addr);

		switch (addr->ss_family)
		{
			case AF_INET:
				m_Port = ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);
				break;
			case AF_INET6:
				m_Port = ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
				break;
			default:
				// IPAddress should already have thrown an exception;
				// this is just in case
				assert(false);
				break;
		}
	}

	String IPEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);

		if (m_Address.GetFamily() == IPAddress::Family::IPv6)
		{
			// IPv6 address should be in brackets according to RFC 3986
			// to separate it from the port
			return Util::FormatString(L"%s:[%s]:%u%s", GetProtocolName(IP::ProtocolToNetwork(m_Protocol)),
									  m_Address.GetString().c_str(), m_Port, rph.c_str());
		}

		return Util::FormatString(L"%s:%s:%u%s", GetProtocolName(IP::ProtocolToNetwork(m_Protocol)),
								  m_Address.GetString().c_str(), m_Port, rph.c_str());
	}

	std::ostream& operator<<(std::ostream& stream, const IPEndpoint& endpoint)
	{
		stream << Util::ToStringA(endpoint.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const IPEndpoint& endpoint)
	{
		stream << endpoint.GetString();
		return stream;
	}
}
