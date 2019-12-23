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
		struct Block final
		{
			constexpr Block(const BinaryIPAddress& addr, const UInt8 cidr_lbits) :
				Address(addr), Mask(BinaryIPAddress::CreateMask(addr.AddressFamily, cidr_lbits))
			{}

			const BinaryIPAddress Address;
			const BinaryIPAddress Mask;
		};

	public:
		using Family = BinaryIPAddress::Family;

		explicit constexpr IPAddress() noexcept :
			m_BinaryAddress(BinaryIPAddress{ BinaryIPAddress::Family::IPv4 }) // Defaults to IPv4 any address
		{}

		constexpr IPAddress(const IPAddress& other) noexcept : m_BinaryAddress(other.m_BinaryAddress) {}
		constexpr IPAddress(IPAddress&& other) noexcept : m_BinaryAddress(std::move(other.m_BinaryAddress)) {}

		explicit IPAddress(const WChar* ipaddr_str) { SetAddress(ipaddr_str); }
		explicit IPAddress(const String& ipaddr_str) { SetAddress(ipaddr_str.c_str()); }
		explicit IPAddress(const sockaddr_storage* saddr) { SetAddress(saddr); }
		explicit IPAddress(const sockaddr* saddr) { SetAddress(reinterpret_cast<const sockaddr_storage*>(saddr)); }

		constexpr IPAddress(const BinaryIPAddress& bin_ipaddr) { SetAddress(bin_ipaddr); }

		~IPAddress() = default;

		constexpr IPAddress& operator=(const IPAddress& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_BinaryAddress = other.m_BinaryAddress;

			return *this;
		}

		constexpr IPAddress& operator=(IPAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			*this = other;

			return *this;
		}

		constexpr bool operator==(const IPAddress& other) const noexcept
		{
			return (m_BinaryAddress == other.m_BinaryAddress);
		}

		constexpr bool operator!=(const IPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr bool operator==(const BinaryIPAddress& other) const noexcept
		{
			return (m_BinaryAddress == other);
		}

		constexpr bool operator!=(const BinaryIPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		String GetString() const noexcept;
		constexpr const BinaryIPAddress& GetBinary() const noexcept { return m_BinaryAddress; }
		constexpr const Family GetFamily() const noexcept { return m_BinaryAddress.AddressFamily; }

		[[nodiscard]] constexpr bool IsMask() const noexcept { return BinaryIPAddress::IsMask(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsLocal() const noexcept { return IsLocal(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsMulticast() const noexcept { return IsMulticast(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsReserved() const noexcept { return IsReserved(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsPublic() const noexcept { return IsPublic(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsClassA() const noexcept { return IsClassA(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsClassB() const noexcept { return IsClassB(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsClassC() const noexcept { return IsClassC(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsClassD() const noexcept { return IsClassD(m_BinaryAddress); }
		[[nodiscard]] constexpr bool IsClassE() const noexcept { return IsClassE(m_BinaryAddress); }

		friend Export std::ostream& operator<<(std::ostream& stream, const IPAddress& ipaddr);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPAddress& ipaddr);

		static constexpr const IPAddress AnyIPv4() noexcept { return { BinaryIPAddress(BinaryIPAddress::Family::IPv4) }; }

		static constexpr const IPAddress AnyIPv6() noexcept { return { BinaryIPAddress(BinaryIPAddress::Family::IPv6) }; }

		static constexpr const IPAddress LoopbackIPv4() noexcept
		{
			return { BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 127 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 }) };
		}

		static constexpr const IPAddress LoopbackIPv6() noexcept
		{
			return { BinaryIPAddress(BinaryIPAddress::Family::IPv6,
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
									 Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 1 }) };
		}

		[[nodiscard]] static bool TryParse(const WChar* ipaddr_str, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static bool TryParse(const String& ipaddr_str, IPAddress& ipaddr) noexcept;
		[[nodiscard]] static bool TryParse(const BinaryIPAddress& bin_ipaddr, IPAddress& ipaddr) noexcept;

		[[nodiscard]] static bool TryParseMask(const IPAddress::Family af,
											   const WChar* mask_str, IPAddress& ipmask) noexcept;
		[[nodiscard]] static bool TryParseMask(const IPAddress::Family af,
											   const String& mask_str, IPAddress& ipmask) noexcept;

		[[nodiscard]] static constexpr bool CreateMask(const IPAddress::Family af, UInt8 cidr_lbits,
													   IPAddress& ipmask) noexcept
		{
			BinaryIPAddress mask;
			if (BinaryIPAddress::CreateMask(af, cidr_lbits, mask))
			{
				ipmask.m_BinaryAddress = mask;
				return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr bool IsLocal(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr std::array<Block, 12> local =
			{
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 0 }), 8 },						// 0.0.0.0/8 (Local system)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 169 }, Byte{ 254 }), 16 },		// 169.254.0.0/16 (Link local)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 127 }), 8 },					// 127.0.0.0/8 (Loopback)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }, Byte{ 168 }), 16 },		// 192.168.0.0/16 (Local LAN)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 10 }), 8 },						// 10.0.0.0/8 (Local LAN)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 172 }, Byte{ 16 }), 12 },		// 172.16.0.0/12 (Local LAN)

				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0 }), 8 },						// ::/8 (Local system)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0xfc }), 7 },					// fc00::/7 (Unique Local Addresses)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0xfd }), 8 },					// fd00::/8 (Unique Local Addresses)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0xfe }, Byte{ 0xc0 }), 10 },	// fec0::/10 (Site local)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0xfe }, Byte{ 0x80 }), 10 },	// fe80::/10 (Link local)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0 }), 127 }						// ::/127 (Inter-Router Links)
			};

			for (const auto& block : local)
			{
				if (IsInBlock(bin_ipaddr, block)) return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr bool IsMulticast(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr std::array<Block, 2> multicast =
			{
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 224 }), 4 },	// 224.0.0.0/4 (Multicast)
				Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv6, Byte{ 0xff }), 8 },	// ff00::/8 (Multicast)
			};

			for (const auto& block : multicast)
			{
				if (IsInBlock(bin_ipaddr, block)) return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr bool IsReserved(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 240 }), 4 }; // 240.0.0.0/4 (Future use)
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr bool IsPublic(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			return (!IsLocal(bin_ipaddr) && !IsMulticast(bin_ipaddr) && !IsReserved(bin_ipaddr));
		}

		[[nodiscard]] static constexpr bool IsClassA(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 0 }), 1 }; // 0.0.0.0/1
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr bool IsClassB(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 128 }), 2 }; // 128.0.0.0/2
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr bool IsClassC(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 192 }), 3 }; // 192.0.0.0/3
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr bool IsClassD(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 224 }), 4 }; // 224.0.0.0/4
			return IsInBlock(bin_ipaddr, block);
		}

		[[nodiscard]] static constexpr bool IsClassE(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			constexpr auto block = Block{ BinaryIPAddress(BinaryIPAddress::Family::IPv4, Byte{ 240 }), 4 }; // 240.0.0.0/4
			return IsInBlock(bin_ipaddr, block);
		}

	private:
		void SetAddress(const WChar* ipaddr_str);
		void SetAddress(const sockaddr_storage* saddr);

		constexpr void SetAddress(const BinaryIPAddress& bin_ipaddr)
		{
			switch (bin_ipaddr.AddressFamily)
			{
				case BinaryIPAddress::Family::IPv4:
				case BinaryIPAddress::Family::IPv6:
					m_BinaryAddress = bin_ipaddr;
					break;
				default:
					throw std::invalid_argument("Unsupported internetwork address family");
			}

			return;
		}

		constexpr void Clear() noexcept { m_BinaryAddress.Clear(); }

		[[nodiscard]] static constexpr bool IsInBlock(const BinaryIPAddress& bin_ipaddr, const Block& block) noexcept
		{
			const auto [success, same_network] = BinaryIPAddress::AreInSameNetwork(bin_ipaddr, block.Address,
																				   block.Mask);
			if (success && same_network) return true;

			return false;
		}

	private:
		static constexpr UInt8 MaxIPAddressStringLength{ 46 }; // Maximum length of IPv6 address

		BinaryIPAddress m_BinaryAddress; // In network byte order (big endian)
	};
}
