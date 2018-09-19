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

	const bool IPAddress::CreateMask(const IPAddressFamily af, UInt8 cidr_lbits, IPAddress& ipmask) noexcept
	{
		BinaryIPAddress mask;
		if (BinaryIPAddress::CreateMask(af, cidr_lbits, mask))
		{
			ipmask.SetAddress(mask);
			return true;
		}

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

	const bool IPAddress::IsLocal(const BinaryIPAddress& bin_ipaddr) noexcept
	{
		const std::array<Block, 12> local =
		{
			Block{ { IPAddressFamily::IPv4, { Byte{ 0 } } }, 8 },					// 0.0.0.0/8 (Local system)
			Block{ { IPAddressFamily::IPv4, { Byte{ 169 }, Byte{ 254 } } }, 16 },	// 169.254.0.0/16 (Link local)
			Block{ { IPAddressFamily::IPv4, { Byte{ 127 } } }, 8 },					// 127.0.0.0/8 (Loopback)
			Block{ { IPAddressFamily::IPv4, { Byte{ 192 }, Byte{ 168 } } }, 16 },	// 192.168.0.0/16 (Local LAN)
			Block{ { IPAddressFamily::IPv4, { Byte{ 10 } } }, 8 },					// 10.0.0.0/8 (Local LAN)
			Block{ { IPAddressFamily::IPv4, { Byte{ 172 }, Byte{ 16 } } }, 12 },	// 172.16.0.0/12 (Local LAN)

			Block{ { IPAddressFamily::IPv6, { Byte{ 0 } } }, 8 },					// ::/8 (Local system)
			Block{ { IPAddressFamily::IPv6, { Byte{ 0xfc } } }, 7 },				// fc00::/7 (Unique Local Addresses)
			Block{ { IPAddressFamily::IPv6, { Byte{ 0xfd } } }, 8 },				// fd00::/8 (Unique Local Addresses)
			Block{ { IPAddressFamily::IPv6, { Byte{ 0xfe }, Byte{ 0xc0 } } }, 10 },	// fec0::/10 (Site local)
			Block{ { IPAddressFamily::IPv6, { Byte{ 0xfe }, Byte{ 0x80 } } }, 10 },	// fe80::/10 (Link local)
			Block{ { IPAddressFamily::IPv6, { Byte{ 0 } } }, 127 }					// ::/127 (Inter-Router Links)
		};

		for (const auto& block : local)
		{
			if (IsInBlock(bin_ipaddr, block)) return true;
		}

		return false;
	}

	const bool IPAddress::IsMulticast(const BinaryIPAddress& bin_ipaddr) noexcept
	{
		const std::array<Block, 2> multicast =
		{
			Block{ { IPAddressFamily::IPv4, { Byte{ 224 } } }, 4 },		// 224.0.0.0/4 Multicast
			Block{ { IPAddressFamily::IPv6, { Byte{ 0xff } } }, 8 },	// ff00::/8 Multicast
		};

		for (const auto& block : multicast)
		{
			if (IsInBlock(bin_ipaddr, block)) return true;
		}

		return false;
	}

	const bool IPAddress::IsReserved(const BinaryIPAddress& bin_ipaddr) noexcept
	{
		return IsInBlock(bin_ipaddr,
						 Block{ { IPAddressFamily::IPv4, { Byte{ 240 } } }, 4 }); // 240.0.0.0/4 Future use
	}

	const bool IPAddress::IsInBlock(const BinaryIPAddress& bin_ipaddr, const Block& block) noexcept
	{
		const auto[success, same_network] = BinaryIPAddress::AreInSameNetwork(bin_ipaddr, block.Address,
																			  block.CIDRLeadingBits);
		if (success && same_network) return true;

		return false;
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