// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	String BTHEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);

		return Util::FormatString(L"%s:%s:%u%s", GetProtocolName(BTH::ProtocolToNetwork(m_Protocol)),
								  m_Address.GetString().c_str(), m_Port, rph.c_str());
	}

	std::ostream& operator<<(std::ostream& stream, const BTHEndpoint& endpoint)
	{
		stream << Util::ToStringA(endpoint.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const BTHEndpoint& endpoint)
	{
		stream << endpoint.GetString();
		return stream;
	}
}