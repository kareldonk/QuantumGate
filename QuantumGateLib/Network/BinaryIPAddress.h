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
			Byte	Bytes[16];
			UInt16	UInt16s[8];
			UInt32	UInt32s[4];
			UInt64	UInt64s[2]{ 0, 0 };
		};

		constexpr void Clear() noexcept
		{
			AddressFamily = IPAddressFamily::Unknown;
			UInt64s[0] = 0;
			UInt64s[1] = 0;
		}

		constexpr BinaryIPAddress operator~() const noexcept
		{
			BinaryIPAddress addr(*this);
			addr.UInt64s[0] = ~UInt64s[0];
			addr.UInt64s[1] = ~UInt64s[1];
			return addr;
		}

		constexpr BinaryIPAddress operator^(const BinaryIPAddress& other) const noexcept
		{
			BinaryIPAddress addr(*this);
			addr.UInt64s[0] = UInt64s[0] ^ other.UInt64s[0];
			addr.UInt64s[1] = UInt64s[1] ^ other.UInt64s[1];
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
			addr.UInt64s[0] = UInt64s[0] | other.UInt64s[0];
			addr.UInt64s[1] = UInt64s[1] | other.UInt64s[1];
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
			addr.UInt64s[0] = UInt64s[0] & other.UInt64s[0];
			addr.UInt64s[1] = UInt64s[1] & other.UInt64s[1];
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
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryIPAddress
	{
		IPAddressFamily AddressFamily{ IPAddressFamily::Unknown };

		union
		{
			Byte	Bytes[16];
			UInt16	UInt16s[8];
			UInt32	UInt32s[4];
			UInt64	UInt64s[2]{ 0, 0 };
		};

		constexpr SerializedBinaryIPAddress() noexcept {}
		constexpr SerializedBinaryIPAddress(const BinaryIPAddress& addr) noexcept { *this = addr; }

		constexpr SerializedBinaryIPAddress& operator=(const BinaryIPAddress& addr) noexcept
		{
			AddressFamily = addr.AddressFamily;
			UInt64s[0] = addr.UInt64s[0];
			UInt64s[1] = addr.UInt64s[1];
			return *this;
		}

		constexpr operator BinaryIPAddress() const noexcept
		{
			BinaryIPAddress addr;
			addr.AddressFamily = AddressFamily;
			addr.UInt64s[0] = UInt64s[0];
			addr.UInt64s[1] = UInt64s[1];
			return addr;
		}

		constexpr bool operator==(const SerializedBinaryIPAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					UInt64s[0] == other.UInt64s[0] &&
					UInt64s[1] == other.UInt64s[1]);
		}

		constexpr bool operator!=(const SerializedBinaryIPAddress& other) const noexcept
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
