// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "BTHEndpoint.h"

#include <initguid.h>

DEFINE_GUID(QuantumGateBluetoothServiceClassGUID, 0xCA11AB1E, 0x5AFE, 0xC0DE, 0x20, 0x45, 0x41, 0x2D, 0x45, 0x4E, 0x4B, 0x49);
DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

namespace QuantumGate::Implementation::Network
{
	const GUID& BTHEndpoint::GetQuantumGateServiceClassID() noexcept
	{
		return QuantumGateBluetoothServiceClassGUID;
	}

	const GUID& BTHEndpoint::GetNullServiceClassID() noexcept
	{
		return GUID_NULL;
	}
	
	String BTHEndpoint::GetString() const noexcept
	{
		String rph;
		if (m_RelayPort != 0) rph = Util::FormatString(L":%llu:%u", m_RelayPort, m_RelayHop);
		
		String guid;
		if (m_ServiceClassID != GUID_NULL) guid = Util::FormatString(L":%s", Util::ToString(m_ServiceClassID).c_str());

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