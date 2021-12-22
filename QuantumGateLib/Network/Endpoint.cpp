// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Endpoint.h"

namespace QuantumGate::Implementation::Network
{
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