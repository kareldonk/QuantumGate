// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IMFEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	String IMFEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);

		return Util::FormatString(L"%s:%s:%u%s", GetProtocolName(IMF::ProtocolToNetwork(m_Protocol)),
								  m_Address.GetString().c_str(), m_Port, rph.c_str());
	}

	std::ostream& operator<<(std::ostream& stream, const IMFEndpoint& endpoint)
	{
		stream << Util::ToStringA(endpoint.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const IMFEndpoint& endpoint)
	{
		stream << endpoint.GetString();
		return stream;
	}
}