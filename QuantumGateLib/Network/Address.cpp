// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Address.h"

namespace QuantumGate::Implementation::Network
{
	std::size_t Address::GetHash() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return m_IPAddress.GetHash();
			case Type::BTH:
				return m_BTHAddress.GetHash();
			case Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return 0;
	}

	String Address::GetString() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return m_IPAddress.GetString();
			case Type::BTH:
				return m_BTHAddress.GetString();
			case Type::Unspecified:
				break;
			default:
				assert(false);
				break;
		}

		return L"Unspecified";
	}

	std::ostream& operator<<(std::ostream& stream, const Address& addr)
	{
		stream << Util::ToStringA(addr.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const Address& addr)
	{
		stream << addr.GetString();
		return stream;
	}
}