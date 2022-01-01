// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHEndpoint.h"

namespace QuantumGate::Implementation::Network
{
	BTHEndpoint::BTHEndpoint(const Protocol protocol, const sockaddr_storage* saddr)
	{
		assert(saddr != nullptr);

		m_Protocol = ValidateProtocol(protocol);
		m_Address = BTHAddress(saddr);

		switch (saddr->ss_family)
		{
			case AF_BTH:
			{
				const auto bthaddr = reinterpret_cast<const SOCKADDR_BTH*>(saddr);
				if (bthaddr->port != BT_PORT_ANY)
				{
					m_Port = static_cast<UInt16>(bthaddr->port);
				}
				m_ServiceClassID = bthaddr->serviceClassId;
				break;
			}
			default:
			{
				// BTHAddress should already have thrown an exception;
				// this is just in case
				assert(false);
				break;
			}
		}
	}

	String BTHEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);
		
		constexpr auto null_guid = GetNullServiceClassID();

		String guid;
		if (m_ServiceClassID != null_guid) guid = Util::FormatString(L":%s", Util::ToString(m_ServiceClassID).c_str());

		if (m_Port == 0)
		{
			return Util::FormatString(L"%s:%s%s%s", GetProtocolName(BTH::ProtocolToNetwork(m_Protocol)),
									  m_Address.GetString().c_str(), guid.c_str(), rph.c_str());
		}
		else
		{
			return Util::FormatString(L"%s:%s:%u%s%s", GetProtocolName(BTH::ProtocolToNetwork(m_Protocol)),
									  m_Address.GetString().c_str(), m_Port, guid.c_str(), rph.c_str());
		}
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