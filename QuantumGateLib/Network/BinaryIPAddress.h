// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Network
{
	struct Export BinaryIPAddress
	{
		IPAddressFamily AddressFamily{ IPAddressFamily::Unknown };
		union
		{
			Byte Bytes[16]
			{
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 }
			};
			UInt16 UInt16s[8];
			UInt32 UInt32s[4];
			UInt64 UInt64s[2];
		};

		constexpr BinaryIPAddress() noexcept {}

		constexpr BinaryIPAddress(const IPAddressFamily af, const Byte b1 = Byte{ 0 }, const Byte b2 = Byte{ 0 },
								  const Byte b3 = Byte{ 0 }, const Byte b4 = Byte{ 0 }, const Byte b5 = Byte{ 0 },
								  const Byte b6 = Byte{ 0 }, const Byte b7 = Byte{ 0 }, const Byte b8 = Byte{ 0 },
								  const Byte b9 = Byte{ 0 }, const Byte b10 = Byte{ 0 }, const Byte b11 = Byte{ 0 },
								  const Byte b12 = Byte{ 0 }, const Byte b13 = Byte{ 0 }, const Byte b14 = Byte{ 0 },
								  const Byte b15 = Byte{ 0 }, const Byte b16 = Byte{ 0 }) noexcept :
			AddressFamily{ af }, Bytes{ b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16 }
		{}

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
					addr.UInt32s[0] = ~UInt32s[0];
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
			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt32s[0] = UInt32s[0] ^ other.UInt32s[0];
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
			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt32s[0] = UInt32s[0] | other.UInt32s[0];
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
			BinaryIPAddress addr(*this);
			switch (AddressFamily)
			{
				case IPAddressFamily::IPv4:
					addr.UInt32s[0] = UInt32s[0] & other.UInt32s[0];
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

					bin_mask = BinaryIPAddress(af);

					const auto r = cidr_lbits % 8;
					const auto n = (cidr_lbits - r) / 8;

					for (auto x = 0; x < n; ++x)
					{
						bin_mask.Bytes[x] = Byte{ 0xff };
					}

					if (r > 0) bin_mask.Bytes[n] = Byte{ static_cast<UChar>(0xff << (8 - r)) };

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
			const auto numbytes = [&]() constexpr noexcept
			{
				switch (bin_ipaddr.AddressFamily)
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
			}();

			if (numbytes > 0u)
			{
				auto on = true;

				for (auto x = 0u; x < numbytes; ++x)
				{
					if (on)
					{
						switch (static_cast<UChar>(bin_ipaddr.Bytes[x]))
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
						if (static_cast<UChar>(bin_ipaddr.Bytes[x]) != 0b00000000) return false;
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
				const auto numbytes = [&]() constexpr noexcept
				{
					switch (bin_ipaddr.AddressFamily)
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
				}();

				if (numbytes > 0u)
				{
					for (auto x = 0u; x < numbytes; ++x)
					{
						if (!((bin_ipaddr.Bytes[x] >= bin_range_start.Bytes[x]) &&
							(bin_ipaddr.Bytes[x] <= bin_range_end.Bytes[x])))
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
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryIPAddress
	{
		IPAddressFamily AddressFamily{ IPAddressFamily::Unknown };
		union
		{
			Byte Bytes[16]
			{
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 },
				Byte{ 0 }, Byte{ 0 }, Byte{ 0 }, Byte{ 0 }
			};
			UInt16 UInt16s[8];
			UInt32 UInt32s[4];
			UInt64 UInt64s[2];
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
