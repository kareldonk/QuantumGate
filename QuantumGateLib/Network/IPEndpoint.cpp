// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPEndpoint.h"

#include <ws2tcpip.h>
#include <Mstcpip.h>

namespace QuantumGate::Implementation::Network
{
	IPEndpoint::IPEndpoint(const IPEndpoint& other) noexcept
	{
		*this = other;
	}

	IPEndpoint::IPEndpoint(IPEndpoint&& other) noexcept
	{
		*this = std::move(other);
	}

	IPEndpoint::IPEndpoint(const IPAddress& ipaddr, const UInt16 port) noexcept
	{
		m_Address = ipaddr;
		m_Port = port;
	}

	IPEndpoint::IPEndpoint(const IPAddress& ipaddr, const UInt16 port, const RelayPort rport, const RelayHop hop) noexcept
	{
		m_Address = ipaddr;
		m_Port = port;
		m_RelayPort = rport;
		m_RelayHop = hop;
	}

	IPEndpoint::IPEndpoint(const sockaddr_storage* addr)
	{
		assert(addr != nullptr);

		m_Address = IPAddress(addr);

		switch (addr->ss_family)
		{
			case AF_INET:
				m_Port = reinterpret_cast<const sockaddr_in*>(addr)->sin_port;
				break;
			case AF_INET6:
				m_Port = reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port;
				break;
			default:
				// IPAddress should already have thrown an exception;
				// this is just in case
				assert(false);
		}
	}

	IPEndpoint& IPEndpoint::operator=(const IPEndpoint& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_Address = other.m_Address;
		m_Port = other.m_Port;
		m_RelayPort = other.m_RelayPort;
		m_RelayHop = other.m_RelayHop;

		return *this;
	}

	IPEndpoint& IPEndpoint::operator=(IPEndpoint&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_Address = std::move(other.m_Address);
		m_Port = std::exchange(other.m_Port, 0);
		m_RelayPort = std::exchange(other.m_RelayPort, 0);
		m_RelayHop = std::exchange(other.m_RelayHop, 0);

		return *this;
	}

	String IPEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);

		if (m_Address.GetFamily() == IPAddressFamily::IPv6)
		{
			// IPv6 address should be in brackets according to RFC 3986
			// to separate it from the port
			return Util::FormatString(L"[%s]:%u%s", m_Address.GetCString(), m_Port, rph.c_str());
		}

		return Util::FormatString(L"%s:%u%s", m_Address.GetCString(), m_Port, rph.c_str());
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
