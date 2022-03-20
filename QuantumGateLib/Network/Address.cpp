// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "Address.h"

namespace QuantumGate::Implementation::Network
{
	bool Address::TryParse(const WChar* addr_str, Address& addr) noexcept
	{
		IPAddress ipaddr;
		BTHAddress bthaddr;

		if (IPAddress::TryParse(addr_str, ipaddr))
		{
			addr = ipaddr;
			return true;
		}
		else if (BTHAddress::TryParse(addr_str, bthaddr))
		{
			addr = bthaddr;
			return true;
		}

		return false;
	}

	bool Address::TryParse(const String& addr_str, Address& addr) noexcept
	{
		return TryParse(addr_str.c_str(), addr);
	}

	std::size_t Address::GetHash() const noexcept
	{
		switch (m_Type)
		{
			case Type::IP:
				return m_IPAddress.GetHash();
			case Type::BTH:
				return m_BTHAddress.GetHash();
			case Type::IMF:
				return m_IMFAddress.GetHash();
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
			case Type::IMF:
				return m_IMFAddress.GetString();
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