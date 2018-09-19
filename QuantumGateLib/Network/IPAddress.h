// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryIPAddress.h"

#include <array>

#include <ws2tcpip.h>
#include <Mstcpip.h>

namespace QuantumGate::Implementation::Network
{
	class Export IPAddress
	{
		struct Block
		{
			const BinaryIPAddress Address;
			const UInt8 CIDRLeadingBits{ 0 };
		};

	public:
		constexpr IPAddress() noexcept :
			m_AddressBinary(BinaryIPAddress{ IPAddressFamily::IPv4 }) // Defaults to IPv4 any address
		{}

		constexpr IPAddress(const IPAddress& other) noexcept { *this = other; }
		constexpr IPAddress(IPAddress&& other) noexcept { *this = std::move(other); }
		
		IPAddress(const String& ipaddr_str) { SetAddress(ipaddr_str); }
		
		constexpr IPAddress(const sockaddr_storage* saddr) { SetAddress(saddr); }
		constexpr IPAddress(const sockaddr* saddr) { SetAddress(reinterpret_cast<const sockaddr_storage*>(saddr)); }
		constexpr IPAddress(const BinaryIPAddress& bin_ipaddr) { SetAddress(bin_ipaddr); }

		constexpr IPAddress& operator=(const IPAddress& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_AddressBinary = other.m_AddressBinary;

			return *this;
		}

		constexpr IPAddress& operator=(IPAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			*this = other;

			other.Clear();

			return *this;
		}

		constexpr const bool operator==(const IPAddress& other) const noexcept
		{
			return (m_AddressBinary == other.m_AddressBinary);
		}

		constexpr const bool operator!=(const IPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr const bool operator==(const BinaryIPAddress& other) const noexcept
		{
			return (m_AddressBinary == other);
		}

		constexpr const bool operator!=(const BinaryIPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		String GetString() const noexcept;
		constexpr const BinaryIPAddress& GetBinary() const noexcept { return m_AddressBinary; }
		constexpr const IPAddressFamily GetFamily() const noexcept { return m_AddressBinary.AddressFamily; }

		[[nodiscard]] inline const bool IsMask() const noexcept { return BinaryIPAddress::IsMask(m_AddressBinary); }
		[[nodiscard]] inline const bool IsLocal() const noexcept { return IsLocal(m_AddressBinary); }
		[[nodiscard]] inline const bool IsMulticast() const noexcept { return IsMulticast(m_AddressBinary); }
		[[nodiscard]] inline const bool IsReserved() const noexcept { return IsReserved(m_AddressBinary); }

		friend Export std::ostream& operator<<(std::ostream& stream, const IPAddress& ipaddr);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPAddress& ipaddr);

		static constexpr const IPAddress AnyIPv4() noexcept { return { BinaryIPAddress{ IPAddressFamily::IPv4 } }; }

		static constexpr const IPAddress AnyIPv6() noexcept { return { BinaryIPAddress{ IPAddressFamily::IPv6 } }; }

		static constexpr const IPAddress LoopbackIPv4() noexcept
		{
			return { BinaryIPAddress{ IPAddressFamily::IPv4, { Byte{ 127 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 } } } };
		}

		static constexpr const IPAddress LoopbackIPv6() noexcept
		{
			auto bin_ip = BinaryIPAddress{ IPAddressFamily::IPv6 };
			bin_ip.Bytes[15] = Byte{ 1 };
			return { bin_ip };
		}

		[[nodiscard]] static const bool IsLocal(const BinaryIPAddress& bin_ipaddr) noexcept;
		[[nodiscard]] static const bool IsMulticast(const BinaryIPAddress& bin_ipaddr) noexcept;
		[[nodiscard]] static const bool IsReserved(const BinaryIPAddress& bin_ipaddr) noexcept;

		[[nodiscard]] static const bool TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static const bool TryParse(const BinaryIPAddress& bin_ipaddr, IPAddress& ipaddr) noexcept;

		[[nodiscard]] static const bool TryParseMask(const IPAddressFamily af,
													 const String& mask_str, IPAddress& ipmask) noexcept;

		[[nodiscard]] static const bool CreateMask(const IPAddressFamily af,
												   UInt8 cidr_lbits, IPAddress& ipmask) noexcept;

	private:
		void SetAddress(const String& ipaddr_str);

		constexpr void SetAddress(const BinaryIPAddress& bin_ipaddr)
		{
			switch (bin_ipaddr.AddressFamily)
			{
				case IPAddressFamily::IPv4:
					[[fallthrough]];
				case IPAddressFamily::IPv6:
					m_AddressBinary = bin_ipaddr;
					break;
				default:
					throw std::invalid_argument("Unsupported internetwork address family");
			}

			return;
		}

		constexpr void SetAddress(const sockaddr_storage* saddr)
		{
			assert(saddr != nullptr);

			switch (saddr->ss_family)
			{
				case AF_INET:
				{
					static_assert(sizeof(m_AddressBinary.Bytes) >= sizeof(in_addr), "IP Address length mismatch");

					m_AddressBinary.AddressFamily = IPAddressFamily::IPv4;

					auto ip4 = reinterpret_cast<const sockaddr_in*>(saddr);
					memcpy(&m_AddressBinary.Bytes, &ip4->sin_addr, sizeof(ip4->sin_addr));

					break;
				}
				case AF_INET6:
				{
					static_assert(sizeof(m_AddressBinary.Bytes) >= sizeof(in6_addr), "IP Address length mismatch");

					m_AddressBinary.AddressFamily = IPAddressFamily::IPv6;

					auto ip6 = reinterpret_cast<const sockaddr_in6*>(saddr);
					memcpy(&m_AddressBinary.Bytes, &ip6->sin6_addr, sizeof(ip6->sin6_addr));

					break;
				}
				default:
				{
					throw std::invalid_argument("Unsupported internetwork address family");
				}
			}

			return;
		}

		constexpr void Clear() noexcept { m_AddressBinary.Clear(); }

		[[nodiscard]] static const bool IsInBlock(const BinaryIPAddress& bin_ipaddr, const Block& block) noexcept;

	private:
		static constexpr UInt8 MaxIPAddressStringLength{ 46 }; // Maximum length of IPv6 address

		BinaryIPAddress m_AddressBinary; // In network byte order (big endian)
	};
}
