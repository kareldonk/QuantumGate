// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPEndpoint.h"

#include <ws2tcpip.h>
#include <Mstcpip.h>

namespace QuantumGate::Implementation::Network
{
	String IPEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);

		if (m_Address.GetFamily() == IPAddress::Family::IPv6)
		{
			// IPv6 address should be in brackets according to RFC 3986
			// to separate it from the port
			return Util::FormatString(L"[%s]:%u%s", m_Address.GetString().c_str(), m_Port, rph.c_str());
		}

		return Util::FormatString(L"%s:%u%s", m_Address.GetString().c_str(), m_Port, rph.c_str());
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
