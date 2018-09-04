// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "IPAddress.h"
#include "..\Common\Endian.h"

#include <regex>

#include <ws2tcpip.h>
#include <Mstcpip.h>

namespace QuantumGate::Implementation::Network
{
	IPAddress::IPAddress() noexcept
	{
		// Defaults to IPv4 any address
		SetAddress(L"0.0.0.0");
	}

	IPAddress::IPAddress(const IPAddress& other) noexcept
	{
		*this = other;
	}

	IPAddress::IPAddress(IPAddress&& other) noexcept
	{
		*this = std::move(other);
	}

	IPAddress::IPAddress(const String& ipaddr_str)
	{
		SetAddress(ipaddr_str);
	}

	IPAddress::IPAddress(const sockaddr_storage* saddr)
	{
		SetAddress(saddr);
	}

	IPAddress::IPAddress(const sockaddr* saddr)
	{
		SetAddress(reinterpret_cast<const sockaddr_storage*>(saddr));
	}

	IPAddress::IPAddress(const BinaryIPAddress& bin_ipaddr)
	{
		SetAddress(bin_ipaddr);
	}

	IPAddress& IPAddress::operator=(const IPAddress& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		m_AddressString = other.m_AddressString;
		m_AddressBinary = other.m_AddressBinary;

		return *this;
	}

	IPAddress& IPAddress::operator=(IPAddress&& other) noexcept
	{
		// Check for same object
		if (this == &other) return *this;

		*this = other;

		other.Clear();

		return *this;
	}

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

	const bool IPAddress::TryParseMask(const IPAddressFamily af, const String& mask, IPAddress& ipaddr) noexcept
	{
		try
		{
			if (mask.size() <= IPAddress::MaxIPAddressStringLength)
			{
				// Looks for mask bits specified in the format
				// "/999" in the mask string used in CIDR notations
				// such as "192.168.0.0/16"
				std::wregex r(LR"bits(^\s*\/(\d+)\s*$)bits");
				std::wsmatch m;
				if (std::regex_search(mask, m, r))
				{
					auto cidr_lbits = std::stoi(m[1].str());

					return TryParseMask(af, cidr_lbits, ipaddr);
				}
				else
				{
					// Treats the mask string as an IP address mask
					// e.g. "255.255.255.255"
					if (TryParse(mask, ipaddr) && ipaddr.GetFamily() == af)
					{
						return true;
					}
				}
			}
		}
		catch (...) {}

		return false;
	}

	const bool IPAddress::TryParseMask(const IPAddressFamily af, UInt8 cidr_lbits, IPAddress& ipaddr) noexcept
	{
		BinaryIPAddress mask;
		if (TryParseMask(af, cidr_lbits, mask))
		{
			ipaddr.SetAddress(mask);
			return true;
		}

		return false;
	}

	const bool IPAddress::TryParseMask(const IPAddressFamily af, UInt8 cidr_lbits, BinaryIPAddress& bin_ipaddr) noexcept
	{
		switch (af)
		{
			case IPAddressFamily::IPv4:
			{
				if (cidr_lbits > 32) return false;
				[[fallthrough]];
			}
			case IPAddressFamily::IPv6:
			{
				if (cidr_lbits > 128) return false;

				auto i = 0u;
				while (cidr_lbits > 0)
				{
					for (UInt8 x = 0; x < cidr_lbits && x < 8; ++x)
					{
						bin_ipaddr.Bytes[i] |= static_cast<Byte>(0b10000000 >> x);
					}

					if (cidr_lbits >= 8) cidr_lbits -= 8;
					else cidr_lbits = 0;

					++i;
				}

				bin_ipaddr.AddressFamily = af;

				return true;
			}
			default:
			{
				break;
			}
		}

		return false;
	}

	void IPAddress::Clear() noexcept
	{
		std::fill(m_AddressString.begin(), m_AddressString.end(), 0);
		m_AddressBinary.AddressFamily = IPAddressFamily::Unknown;
		m_AddressBinary.UInt64s[0] = 0;
		m_AddressBinary.UInt64s[1] = 0;
	}

	void IPAddress::SetAddress(const String& ipaddr_str)
	{
		static_assert(sizeof(m_AddressBinary.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

		if (ipaddr_str.size() <= IPAddress::MaxIPAddressStringLength)
		{
			BinaryIPAddress baddr;
			if (InetPton(AF_INET, ipaddr_str.c_str(), &baddr.Bytes) == 1)
			{
				assert(ipaddr_str.size() < m_AddressString.size());
				std::fill(m_AddressString.begin(), m_AddressString.end(), 0);
				std::copy(ipaddr_str.begin(), ipaddr_str.end(), m_AddressString.begin());

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
					assert(ipint.size() < m_AddressString.size());
					std::fill(m_AddressString.begin(), m_AddressString.end(), 0);
					std::copy(ipint.begin(), ipint.end(), m_AddressString.begin());

					m_AddressBinary = baddr;
					m_AddressBinary.AddressFamily = IPAddressFamily::IPv6;
					return;
				}
			}
		}

		throw std::invalid_argument("Invalid IP address");

		return;
	}

	void IPAddress::SetAddress(const BinaryIPAddress& bin_ipaddr)
	{
		static_assert(sizeof(bin_ipaddr.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

		auto afws = AF_UNSPEC;
		
		switch (bin_ipaddr.AddressFamily)
		{
			case IPAddressFamily::IPv4:
				afws = AF_INET;
				break;
			case IPAddressFamily::IPv6:
				afws = AF_INET6;
				break;
			default:
				throw std::invalid_argument("Unsupported internetwork address family");
		}

		std::array<WChar, IPAddress::MaxIPAddressStringLength> ipstr{ 0 };

		const auto ip = InetNtop(afws, &bin_ipaddr.Bytes, reinterpret_cast<PWSTR>(ipstr.data()), ipstr.size());
		if (ip != NULL)
		{
			m_AddressBinary = bin_ipaddr;
			m_AddressString = ipstr;
		}
		else throw std::invalid_argument("Invalid IP address");

		return;
	}

	void IPAddress::SetAddress(const sockaddr_storage* saddr)
	{
		assert(saddr != nullptr);

		switch (saddr->ss_family)
		{
			case AF_INET:
			{
				BinaryIPAddress baddr;
				baddr.AddressFamily = IPAddressFamily::IPv4;
				auto ip4 = reinterpret_cast<const sockaddr_in*>(saddr);

				static_assert(sizeof(baddr.Bytes) >= sizeof(in_addr), "IP Address length mismatch");

				memcpy(&baddr.Bytes, &ip4->sin_addr, sizeof(ip4->sin_addr));

				SetAddress(baddr);

				break;
			}
			case AF_INET6:
			{
				BinaryIPAddress baddr;
				baddr.AddressFamily = IPAddressFamily::IPv6;
				auto ip6 = reinterpret_cast<const sockaddr_in6*>(saddr);

				static_assert(sizeof(baddr.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

				memcpy(&baddr.Bytes, &ip6->sin6_addr, sizeof(ip6->sin6_addr));

				SetAddress(baddr);

				break;
			}
			default:
			{
				throw std::invalid_argument("Unsupported internetwork address family");
			}
		}

		return;
	}

	const IPAddress IPAddress::AnyIPv4() noexcept
	{
		return IPAddress(L"0.0.0.0");
	}
	
	const IPAddress IPAddress::AnyIPv6() noexcept
	{
		return IPAddress(L"::");
	}

	const IPAddress IPAddress::LoopbackIPv4() noexcept
	{
		return IPAddress(L"127.0.0.1");
	}
	
	const IPAddress IPAddress::LoopbackIPv6() noexcept
	{
		return IPAddress(L"::1");
	}
	
	const IPAddress IPAddress::Broadcast() noexcept
	{
		return IPAddress(L"255.255.255.255");
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