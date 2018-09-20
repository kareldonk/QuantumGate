// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPAddress.h"
#include "..\Common\Endian.h"

#include <regex>

namespace QuantumGate::Implementation::Network
{
	const bool IPAddress::TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept
	{
		try
		{
			IPAddress temp_ip(ipaddr_str);
			ipaddr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	const bool IPAddress::TryParse(const BinaryIPAddress& bin_ipaddr, IPAddress& ipaddr) noexcept
	{
		try
		{
			IPAddress temp_ip(bin_ipaddr);
			ipaddr = std::move(temp_ip);
			return true;
		}
		catch (...) {}

		return false;
	}

	const bool IPAddress::TryParseMask(const IPAddressFamily af, const String& mask_str, IPAddress& ipmask) noexcept
	{
		try
		{
			if (mask_str.size() <= IPAddress::MaxIPAddressStringLength)
			{
				// Looks for mask bits specified in the format
				// "/999" in the mask string used in CIDR notations
				// such as "192.168.0.0/16"
				std::wregex r(LR"bits(^\s*\/(\d+)\s*$)bits");
				std::wsmatch m;
				if (std::regex_search(mask_str, m, r))
				{
					auto cidr_lbits = std::stoi(m[1].str());

					return CreateMask(af, cidr_lbits, ipmask);
				}
				else
				{
					// Treats the mask string as an IP address mask
					// e.g. "255.255.255.255"
					IPAddress temp_ip;
					if (TryParse(mask_str, temp_ip) && temp_ip.GetFamily() == af && temp_ip.IsMask())
					{
						ipmask = std::move(temp_ip);
						return true;
					}
				}
			}
		}
		catch (...) {}

		return false;
	}

	void IPAddress::SetAddress(const String& ipaddr_str)
	{
		static_assert(sizeof(m_AddressBinary.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

		if (ipaddr_str.size() <= IPAddress::MaxIPAddressStringLength)
		{
			BinaryIPAddress baddr;
			if (InetPton(AF_INET, ipaddr_str.c_str(), &baddr.Bytes) == 1)
			{
				m_AddressBinary = baddr;
				m_AddressBinary.AddressFamily = IPAddressFamily::IPv4;
				return;
			}
			else
			{
				String ipint = ipaddr_str;

				// Remove link-local address zone index for IPv6 addresses;
				// starts with % on Windows. InetPton does not accept it.
				const auto pos = ipaddr_str.find(L"%");
				if (pos != String::npos) ipint = ipaddr_str.substr(0, pos);

				if (InetPton(AF_INET6, ipint.c_str(), &baddr.Bytes) == 1)
				{
					m_AddressBinary = baddr;
					m_AddressBinary.AddressFamily = IPAddressFamily::IPv6;
					return;
				}
			}
		}

		throw std::invalid_argument("Invalid IP address");

		return;
	}

	String IPAddress::GetString() const noexcept
	{
		try
		{
			auto afws = AF_UNSPEC;

			switch (m_AddressBinary.AddressFamily)
			{
				case IPAddressFamily::IPv4:
					afws = AF_INET;
					break;
				case IPAddressFamily::IPv6:
					afws = AF_INET6;
					break;
				default:
					return {};
			}

			std::array<WChar, IPAddress::MaxIPAddressStringLength> ipstr{ 0 };

			const auto ip = InetNtop(afws, &m_AddressBinary.Bytes, reinterpret_cast<PWSTR>(ipstr.data()), ipstr.size());
			if (ip != NULL)
			{
				return ipstr.data();
			}
		}
		catch (...) {}

		return {};
	}

	std::ostream& operator<<(std::ostream& stream, const IPAddress& ipaddr)
	{
		stream << Util::ToStringA(ipaddr.GetString());
		return stream;
	}

	std::wostream& operator<<(std::wostream& stream, const IPAddress& ipaddr)
	{
		stream << ipaddr.GetString();
		return stream;
	}
}