// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPAddress.h"
#include "..\Common\Endian.h"

#include <regex>

namespace QuantumGate::Implementation::Network
{
	const bool IPAddress::TryParse(const WChar* ipaddr_str, IPAddress& ipaddr) noexcept
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

	const bool IPAddress::TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept
	{
		return TryParse(ipaddr_str.c_str(), ipaddr);
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

	const bool IPAddress::TryParseMask(const IPAddress::Family af, const WChar* mask_str, IPAddress& ipmask) noexcept
	{
		try
		{
			if (std::wcslen(mask_str) <= IPAddress::MaxIPAddressStringLength)
			{
				// Looks for mask bits specified in the format
				// "/999" in the mask string used in CIDR notations
				// such as "192.168.0.0/16"
				std::wregex r(LR"bits(^\s*\/(\d+)\s*$)bits");
				std::wcmatch m;
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

	const bool IPAddress::TryParseMask(const IPAddress::Family af, const String& mask_str, IPAddress& ipmask) noexcept
	{
		return TryParseMask(af, mask_str.c_str(), ipmask);
	}

	void IPAddress::SetAddress(const WChar* ipaddr_str)
	{
		static_assert(sizeof(m_BinaryAddress.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

		if (std::wcslen(ipaddr_str) <= IPAddress::MaxIPAddressStringLength)
		{
			BinaryIPAddress baddr;
			if (InetPton(AF_INET, ipaddr_str, &baddr.Bytes) == 1)
			{
				m_BinaryAddress = baddr;
				m_BinaryAddress.AddressFamily = BinaryIPAddress::Family::IPv4;
				return;
			}
			else
			{
				const WChar* ipaddr_ptr = ipaddr_str;
				std::array<WChar, IPAddress::MaxIPAddressStringLength + 1> ipaddr_str2{ 0 };

				// Remove link-local address zone index for IPv6 addresses;
				// starts with % on Windows. InetPton does not accept it.
				const auto pos = std::wcsstr(ipaddr_str, L"%");
				if (pos != nullptr)
				{
					std::memcpy(ipaddr_str2.data(), ipaddr_str, (pos - ipaddr_str) * sizeof(WChar));
					ipaddr_ptr = ipaddr_str2.data();
				}

				if (InetPton(AF_INET6, ipaddr_ptr, &baddr.Bytes) == 1)
				{
					m_BinaryAddress = baddr;
					m_BinaryAddress.AddressFamily = BinaryIPAddress::Family::IPv6;
					return;
				}
			}
		}

		throw std::invalid_argument("Invalid IP address");

		return;
	}

	void IPAddress::SetAddress(const sockaddr_storage* saddr)
	{
		assert(saddr != nullptr);

		switch (saddr->ss_family)
		{
			case AF_INET:
			{
				static_assert(sizeof(m_BinaryAddress.Bytes) >= sizeof(in_addr), "IP Address length mismatch");

				m_BinaryAddress.AddressFamily = BinaryIPAddress::Family::IPv4;

				auto ip4 = reinterpret_cast<const sockaddr_in*>(saddr);
				memcpy(&m_BinaryAddress.Bytes, &ip4->sin_addr, sizeof(ip4->sin_addr));

				break;
			}
			case AF_INET6:
			{
				static_assert(sizeof(m_BinaryAddress.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

				m_BinaryAddress.AddressFamily = BinaryIPAddress::Family::IPv6;

				auto ip6 = reinterpret_cast<const sockaddr_in6*>(saddr);
				memcpy(&m_BinaryAddress.Bytes, &ip6->sin6_addr, sizeof(ip6->sin6_addr));

				break;
			}
			default:
			{
				throw std::invalid_argument("Unsupported internetwork address family");
			}
		}

		return;
	}

	String IPAddress::GetString() const noexcept
	{
		try
		{
			auto afws = AF_UNSPEC;

			switch (m_BinaryAddress.AddressFamily)
			{
				case BinaryIPAddress::Family::IPv4:
					afws = AF_INET;
					break;
				case BinaryIPAddress::Family::IPv6:
					afws = AF_INET6;
					break;
				default:
					return {};
			}

			std::array<WChar, IPAddress::MaxIPAddressStringLength> ipstr{ 0 };

			const auto ip = InetNtop(afws, &m_BinaryAddress.Bytes, reinterpret_cast<PWSTR>(ipstr.data()), ipstr.size());
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