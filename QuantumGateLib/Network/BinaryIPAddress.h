// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <array>

namespace QuantumGate::Implementation::Network
{
	struct Export BinaryIPAddress
	{
		IPAddressFamily AddressFamily{ IPAddressFamily::Unknown };
		union
		{
			Byte Bytes[16];
			UInt16 UInt16s[8];
			UInt32 UInt32s[4];
			UInt64 UInt64s[2]{ 0, 0 };
		};

		constexpr BinaryIPAddress() noexcept {}

		constexpr BinaryIPAddress(const IPAddressFamily af, const Byte b1 = Byte{ 0 }, const Byte b2 = Byte{ 0 },
								  const Byte b3 = Byte{ 0 }, const Byte b4 = Byte{ 0 }, const Byte b5 = Byte{ 0 },
								  const Byte b6 = Byte{ 0 }, const Byte b7 = Byte{ 0 }, const Byte b8 = Byte{ 0 },
								  const Byte b9 = Byte{ 0 }, const Byte b10 = Byte{ 0 }, const Byte b11 = Byte{ 0 },
								  const Byte b12 = Byte{ 0 }, const Byte b13 = Byte{ 0 }, const Byte b14 = Byte{ 0 },
								  const Byte b15 = Byte{ 0 }, const Byte b16 = Byte{ 0 }) noexcept :
			AddressFamily(af),
			UInt64s{ static_cast<UInt64>(b1) | (static_cast<UInt64>(b2) << 8) | (static_cast<UInt64>(b3) << 16) |
			(static_cast<UInt64>(b4) << 24) | (static_cast<UInt64>(b5) << 32) | (static_cast<UInt64>(b6) << 40) |
			(static_cast<UInt64>(b7) << 48) | (static_cast<UInt64>(b8) << 56),
			static_cast<UInt64>(b9) | (static_cast<UInt64>(b10) << 8) | (static_cast<UInt64>(b11) << 16) |
			(static_cast<UInt64>(b12) << 24) | (static_cast<UInt64>(b13) << 32) | (static_cast<UInt64>(b14) << 40) |
			(static_cast<UInt64>(b15) << 48) | (static_cast<UInt64>(b16) << 56) }
		{}

		constexpr BinaryIPAddress(const IPAddressFamily af, const std::array<Byte, 16>& bytes) :
			BinaryIPAddress(af, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4],
							bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10],
							bytes[11], bytes[12], bytes[13], bytes[14], bytes[15])
		{}

		constexpr BinaryIPAddress(const UInt32 u32) noexcept :
			AddressFamily(IPAddressFamily::IPv4),
			UInt64s{ (static_cast<UInt64>(u32) >> 24) |
			((static_cast<UInt64>(u32) >> 8) & 0x00000000'0000ff00) |
			((static_cast<UInt64>(u32) << 8) & 0x00000000'00ff0000) |
			((static_cast<UInt64>(u32) << 24) & 0x00000000'ff000000), 0 }
		{}

		constexpr BinaryIPAddress(const UInt64 u64_1, const UInt64 u64_2) noexcept :
			AddressFamily(IPAddressFamily::IPv6),
			UInt64s{ (u64_1 >> 56) |
			((u64_1 >> 40) & 0x00000000'0000ff00) |
			((u64_1 >> 24) & 0x00000000'00ff0000) |
			((u64_1 >> 8) & 0x00000000'ff000000) |
			((u64_1 << 8) & 0x000000ff'00000000) |
			((u64_1 << 24) & 0x0000ff00'00000000) |
			((u64_1 << 40) & 0x00ff0000'00000000) |
			((u64_1 << 56) & 0xff000000'00000000),
			(u64_2 >> 56) |
			((u64_2 >> 40) & 0x00000000'0000ff00) |
			((u64_2 >> 24) & 0x00000000'00ff0000) |
			((u64_2 >> 8) & 0x00000000'ff000000) |
			((u64_2 << 8) & 0x000000ff'00000000) |
			((u64_2 << 24) & 0x0000ff00'00000000) |
			((u64_2 << 40) & 0x00ff0000'00000000) |
			((u64_2 << 56) & 0xff000000'00000000) }
		{}

		constexpr BinaryIPAddress(const BinaryIPAddress& other) noexcept :
			AddressFamily(other.AddressFamily), UInt64s{ other.UInt64s[0], other.UInt64s[1] }
		{}

		constexpr BinaryIPAddress(BinaryIPAddress&& other) noexcept :
			AddressFamily(other.AddressFamily), UInt64s{ other.UInt64s[0], other.UInt64s[1] }
		{}

		constexpr BinaryIPAddress& operator=(const BinaryIPAddress& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			AddressFamily = other.AddressFamily;
			UInt64s[0] = other.UInt64s[0];
			UInt64s[1] = other.UInt64s[1];

			return *this;
		}

		constexpr BinaryIPAddress& operator=(BinaryIPAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			*this = other;
			
			return *this;
		}

		constexpr void Clear() noexcept
		{
			AddressFamily = IPAddressFamily::Unknown;
			UInt64s[0] = 0;
			UInt64s[1] = 0;
		}

		constexpr BinaryIPAddress operator~() const noexcept
		{
			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt64s[0] = ~UInt64s[0] & 0x00000000ffffffff;
					break;
				case IPAddressFamily::IPv6:
					addr.UInt64s[0] = ~UInt64s[0];
					addr.UInt64s[1] = ~UInt64s[1];
					break;
				default:
					assert(false);
					break;
			}
			return addr;
		}

		constexpr BinaryIPAddress operator^(const BinaryIPAddress& other) const noexcept
		{
			assert(AddressFamily == other.AddressFamily);

			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt64s[0] = UInt64s[0] ^ other.UInt64s[0];
					break;
				case IPAddressFamily::IPv6:
					addr.UInt64s[0] = UInt64s[0] ^ other.UInt64s[0];
					addr.UInt64s[1] = UInt64s[1] ^ other.UInt64s[1];
					break;
				default:
					assert(false);
					break;
			}
			return addr;
		}

		constexpr BinaryIPAddress& operator^=(const BinaryIPAddress& other) noexcept
		{
			*this = *this ^ other;
			return *this;
		}

		constexpr BinaryIPAddress operator|(const BinaryIPAddress& other) const noexcept
		{
			assert(AddressFamily == other.AddressFamily);

			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt64s[0] = UInt64s[0] | other.UInt64s[0];
					break;
				case IPAddressFamily::IPv6:
					addr.UInt64s[0] = UInt64s[0] | other.UInt64s[0];
					addr.UInt64s[1] = UInt64s[1] | other.UInt64s[1];
					break;
				default:
					assert(false);
					break;
			}
			return addr;
		}

		constexpr BinaryIPAddress& operator|=(const BinaryIPAddress& other) noexcept
		{
			*this = *this | other;
			return *this;
		}

		constexpr BinaryIPAddress operator&(const BinaryIPAddress& other) const noexcept
		{
			assert(AddressFamily == other.AddressFamily);

			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt64s[0] = UInt64s[0] & other.UInt64s[0];
					break;
				case IPAddressFamily::IPv6:
					addr.UInt64s[0] = UInt64s[0] & other.UInt64s[0];
					addr.UInt64s[1] = UInt64s[1] & other.UInt64s[1];
					break;
				default:
					assert(false);
					break;
			}
			return addr;
		}

		constexpr BinaryIPAddress& operator&=(const BinaryIPAddress& other) noexcept
		{
			*this = *this & other;
			return *this;
		}

		constexpr bool operator==(const BinaryIPAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					UInt64s[0] == other.UInt64s[0] &&
					UInt64s[1] == other.UInt64s[1]);
		}

		constexpr bool operator!=(const BinaryIPAddress& other) const noexcept
		{
			return !(*this == other);
		}

		std::size_t GetHash() const noexcept;

		constexpr UInt GetNumAddressBytes() const noexcept
		{
			return GetNumAddressBytes(AddressFamily);
		}

		constexpr Byte GetAddressByte(const UInt n) const noexcept
		{
			assert(n < 16u);

			const auto idx = (n < sizeof(UInt64)) ? 0 : 1;
			const auto bshift = (n < sizeof(UInt64)) ? n : n - sizeof(UInt64);
			return static_cast<Byte>(UInt64s[idx] >> (bshift * 8));
		}

		static constexpr BinaryIPAddress CreateMask(const IPAddressFamily af, const UInt8 cidr_lbits)
		{
			BinaryIPAddress mask;
			if (!CreateMask(af, cidr_lbits, mask))
			{
				throw std::invalid_argument("Couldn't create mask likely because of invalid arguments.");
			}

			return mask;
		}

		[[nodiscard]] static constexpr const bool CreateMask(const IPAddressFamily af,
															 UInt8 cidr_lbits,
															 BinaryIPAddress& bin_mask) noexcept
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

					std::array<Byte, 16> bytes{ Byte{ 0 } };

					const auto r = cidr_lbits % 8;
					const auto n = (cidr_lbits - r) / 8;

					for (auto x = 0; x < n; ++x)
					{
						bytes[x] = Byte{ 0xff };
					}

					if (r > 0) bytes[n] = Byte{ static_cast<UChar>(0xff << (8 - r)) };

					bin_mask = BinaryIPAddress(af, bytes);

					return true;
				}
				default:
				{
					// Shouldn't get here
					assert(false);
					break;
				}
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool IsMask(const BinaryIPAddress& bin_ipaddr) noexcept
		{
			if (const auto numbytes = bin_ipaddr.GetNumAddressBytes(); numbytes > 0u)
			{
				auto on = true;

				for (auto x = 0u; x < numbytes; ++x)
				{
					const auto byte = static_cast<UChar>(bin_ipaddr.GetAddressByte(x));

					if (on)
					{
						switch (byte)
						{
							case 0b00000000:
							case 0b10000000:
							case 0b11000000:
							case 0b11100000:
							case 0b11110000:
							case 0b11111000:
							case 0b11111100:
							case 0b11111110:
								on = false;
								continue;
							case 0b11111111:
								continue;
							default:
								return false;
						}
					}
					else
					{
						if (byte != 0b00000000) return false;
					}
				}

				return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool GetNetwork(const BinaryIPAddress& bin_ipaddr,
															 const UInt8 cidr_lbits,
															 BinaryIPAddress& bin_network) noexcept
		{
			BinaryIPAddress mask;
			if (CreateMask(bin_ipaddr.AddressFamily, cidr_lbits, mask))
			{
				bin_network = bin_ipaddr & mask;
				return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const bool GetNetwork(const BinaryIPAddress& bin_ipaddr,
															 const BinaryIPAddress& bin_mask,
															 BinaryIPAddress& bin_network) noexcept
		{
			assert(bin_ipaddr.AddressFamily == bin_mask.AddressFamily);

			if (bin_ipaddr.AddressFamily == bin_mask.AddressFamily)
			{
				bin_network = bin_ipaddr & bin_mask;
				return true;
			}

			return false;
		}

		[[nodiscard]] static constexpr const std::pair<bool, bool> AreInSameNetwork(const BinaryIPAddress& bin_ipaddr1,
																					const BinaryIPAddress& bin_ipaddr2,
																					const UInt8 cidr_lbits) noexcept
		{
			if (bin_ipaddr1.AddressFamily == bin_ipaddr2.AddressFamily)
			{
				BinaryIPAddress bin_network1, bin_network2;
				if (GetNetwork(bin_ipaddr1, cidr_lbits, bin_network1) &&
					GetNetwork(bin_ipaddr2, cidr_lbits, bin_network2))
				{
					return std::make_pair(true, bin_network1 == bin_network2);
				}
				else return std::make_pair(false, false);
			}

			return std::make_pair(true, false);
		}

		[[nodiscard]] static constexpr const std::pair<bool, bool>
			AreInSameNetwork(const BinaryIPAddress& bin_ipaddr1,
							 const BinaryIPAddress& bin_ipaddr2,
							 const BinaryIPAddress& bin_mask) noexcept
		{
			if (bin_ipaddr1.AddressFamily == bin_ipaddr2.AddressFamily)
			{
				BinaryIPAddress bin_network1, bin_network2;
				if (GetNetwork(bin_ipaddr1, bin_mask, bin_network1) &&
					GetNetwork(bin_ipaddr2, bin_mask, bin_network2))
				{
					return std::make_pair(true, bin_network1 == bin_network2);
				}
				else return std::make_pair(false, false);
			}

			return std::make_pair(true, false);
		}

		static constexpr std::optional<std::pair<BinaryIPAddress, BinaryIPAddress>>
			GetAddressRange(const BinaryIPAddress& bin_ipaddr, const BinaryIPAddress& bin_mask) noexcept
		{
			assert(bin_ipaddr.AddressFamily == bin_mask.AddressFamily);

			if (bin_ipaddr.AddressFamily == bin_mask.AddressFamily)
			{
				return std::make_pair(bin_ipaddr, bin_ipaddr | ~bin_mask);
			}

			return std::nullopt;
		}

		[[nodiscard]] static constexpr const std::pair<bool, bool>
			IsInAddressRange(const BinaryIPAddress& bin_ipaddr,
							 const BinaryIPAddress& bin_range_start,
							 const BinaryIPAddress& bin_range_end) noexcept
		{
			if (bin_ipaddr.AddressFamily == bin_range_start.AddressFamily &&
				bin_ipaddr.AddressFamily == bin_range_end.AddressFamily)
			{
				if (const auto numbytes = bin_ipaddr.GetNumAddressBytes(); numbytes > 0u)
				{
					for (auto x = 0u; x < numbytes; ++x)
					{
						const auto byte = bin_ipaddr.GetAddressByte(x);
						if (!(byte >= bin_range_start.GetAddressByte(x) &&
							  byte <= bin_range_end.GetAddressByte(x)))
						{
							// As soon as we have one mismatch we can stop immediately
							return std::make_pair(true, false);
						}
					}

					return std::make_pair(true, true);
				}
			}

			return std::make_pair(false, false);
		}

		static constexpr UInt GetNumAddressBytes(const IPAddressFamily af) noexcept
		{
			switch (af)
			{
				case IPAddressFamily::IPv4:
					return 4u;
				case IPAddressFamily::IPv6:
					return 16u;
				default:
					assert(false);
					break;
			}

			return 0u;
		}
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryIPAddress
	{
		IPAddressFamily AddressFamily{ IPAddressFamily::Unknown };
		union
		{
			Byte Bytes[16];
			UInt16 UInt16s[8];
			UInt32 UInt32s[4];
			UInt64 UInt64s[2]{ 0, 0 };
		};

		SerializedBinaryIPAddress() noexcept {}
		SerializedBinaryIPAddress(const BinaryIPAddress& addr) noexcept { *this = addr; }

		SerializedBinaryIPAddress& operator=(const BinaryIPAddress& addr) noexcept
		{
			AddressFamily = addr.AddressFamily;
			UInt64s[0] = addr.UInt64s[0];
			UInt64s[1] = addr.UInt64s[1];
			return *this;
		}

		operator BinaryIPAddress() const noexcept
		{
			BinaryIPAddress addr;
			addr.AddressFamily = AddressFamily;
			addr.UInt64s[0] = UInt64s[0];
			addr.UInt64s[1] = UInt64s[1];
			return addr;
		}

		bool operator==(const SerializedBinaryIPAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					UInt64s[0] == other.UInt64s[0] &&
					UInt64s[1] == other.UInt64s[1]);
		}

		bool operator!=(const SerializedBinaryIPAddress& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}

namespace std
{
	// Specialization for standard hash function for BinaryIPAddress
	template<> struct hash<QuantumGate::Implementation::Network::BinaryIPAddress>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::BinaryIPAddress& ip) const noexcept
		{
			return ip.GetHash();
		}
	};
}
