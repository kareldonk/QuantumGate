// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryIPAddress.h"

#include <ws2tcpip.h>
#include <Mstcpip.h>

namespace QuantumGate::Implementation::Network
{
	class Export IPAddress
	{
		struct Block
		{
			constexpr Block(const BinaryIPAddress& addr, const UInt8 cidr_lbits) :
				Address(addr), Mask(BinaryIPAddress::CreateMask(addr.AddressFamily, cidr_lbits))
			{}

			const BinaryIPAddress Address;
			const BinaryIPAddress Mask;
		};

	public:
		constexpr IPAddress() noexcept :
			m_AddressBinary(BinaryIPAddress{ IPAddressFamily::IPv4 }) // Defaults to IPv4 any address
		{}

		constexpr IPAddress(const IPAddress& other) noexcept { *this = other; }
		constexpr IPAddress(IPAddress&& other) noexcept { *this = std::move(other); }

		IPAddress(const String& ipaddr_str) { SetAddress(ipaddr_str); }
		IPAddress(const sockaddr_storage* saddr) { SetAddress(saddr); }
		IPAddress(const sockaddr* saddr) { SetAddress(reinterpret_cast<const sockaddr_storage*>(saddr)); }

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

		[[nodiscard]] constexpr const bool IsMask() const noexcept { return BinaryIPAddress::IsMask(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsLocal() const noexcept { return IsLocal(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsMulticast() const noexcept { return IsMulticast(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsReserved() const noexcept { return IsReserved(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsClassA() const noexcept { return IsClassA(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsClassB() const noexcept { return IsClassB(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsClassC() const noexcept { return IsClassC(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsClassD() const noexcept { return IsClassD(m_AddressBinary); }
		[[nodiscard]] constexpr const bool IsClassE() const noexcept { return IsClassE(m_AddressBinary); }

		friend Export std::ostream& operator<<(std::ostream& stream, const IPAddress& ipaddr);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPAddress& ipaddr);

		static constexpr const IPAddress AnyIPv4() noexcept { return { BinaryIPAddress(IPAddressFamily::IPv4) }; }

		static constexpr const IPAddress AnyIPv6() noexcept { return { BinaryIPAddress(IPAddressFamily::IPv6) }; }

		static constexpr const IPAddress LoopbackIPv4() noexcept
		{
			return { BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 127 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 }) };
		}

		static constexpr const IPAddress LoopbackIPv6() noexcept
		{
			return { BinaryIPAddress(IPAddressFamily::IPv6,
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 }) };
		}

		[[nodiscard]] static const bool TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static const bool TryParse(const BinaryIPAddress& bin_ipaddr, IPAddress& ipaddr) noexcept;

		[[nodiscard]] static const bool TryParseMask(const IPAddressFamily af,
													 const String& mask_str, IPAddress& ipmask) noexcept;

		[[nodiscard]] static constexpr const bool CreateMask(const IPAddressFamily af, UInt8 cidr_lbits,
															 IPAddress& ipmask) noexcept
		{
			BinaryIPAddress mask;
			if (BinaryIPAddress::CreateMask(af, cidr_lbits, mask))
			{
				ipmask.m_AddressBinary = mask;
				return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool IsLocal(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr std::array<Block, 12> local =
			{
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 0 }), 8},						// 0.0.0.0/8 (Local system)
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 169 }, Byte{ 254 }), 16 },		// 169.254.0.0/16 (Link local)
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 127 }), 8 },					// 127.0.0.0/8 (Loopback)
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 192 }, Byte{ 168 }), 16 },		// 192.168.0.0/16 (Local LAN)
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 10 }), 8 },						// 10.0.0.0/8 (Local LAN)
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 172 }, Byte{ 16 }), 12 },		// 172.16.0.0/12 (Local LAN)

				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0 }), 8 },						// ::/8 (Local system)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0xfc }), 7 },					// fc00::/7 (Unique Local Addresses)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0xfd }), 8 },					// fd00::/8 (Unique Local Addresses)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0xfe }, Byte{ 0xc0 }), 10 },	// fec0::/10 (Site local)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0xfe }, Byte{ 0x80 }), 10 },	// fe80::/10 (Link local)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0 }), 127 }						// ::/127 (Inter-Router Links)
			};

			for (const auto& block : local)
			{
				if (IsInBlock(bin_ipaddr, block)) return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool IsMulticast(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr std::array<Block, 2> multicast =
			{
				Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 224 }), 4 },	// 224.0.0.0/4 (Multicast)
				Block{ BinaryIPAddress(IPAddressFamily::IPv6, Byte{ 0xff }), 8 },	// ff00::/8 (Multicast)
			};

			for (const auto& block : multicast)
			{
				if (IsInBlock(bin_ipaddr, block)) return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool IsReserved(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 240 }), 4 }; // 240.0.0.0/4 (Future use)
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr const bool IsClassA(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 0 }), 1 }; // 0.0.0.0/1
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr const bool IsClassB(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 128 }), 2 }; // 128.0.0.0/2
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr const bool IsClassC(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 192 }), 3 }; // 192.0.0.0/3
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr const bool IsClassD(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 224 }), 4 }; // 224.0.0.0/4
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr const bool IsClassE(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(IPAddressFamily::IPv4, Byte{ 240 }), 4 }; // 240.0.0.0/4
			return IsInBlock(bin_ipaddr, block);
		}

	private:
		void SetAddress(const String& ipaddr_str);
		void SetAddress(const sockaddr_storage* saddr);

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

		constexpr void Clear() noexcept { m_AddressBinary.Clear(); }

		[[nodiscard]] static constexpr const bool IsInBlock(const BinaryIPAddress& bin_ipaddr, const Block& block) noexcept
		{
			const auto[success, same_network] = BinaryIPAddress::AreInSameNetwork(bin_ipaddr, block.Address,
																				  block.Mask);
			if (success && same_network) return true;

			return false;
		}

	private:
		static constexpr UInt8 MaxIPAddressStringLength{ 46 }; // Maximum length of IPv6 address

		BinaryIPAddress m_AddressBinary; // In network byte order (big endian)
	};
}
