// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BTH.h"

namespace QuantumGate::Implementation::Network
{
	struct Export BinaryBTHAddress
	{
		using Family = BTH::AddressFamily;

		Family AddressFamily{ Family::Unspecified };
		union
		{
			Byte Bytes[8];
			UInt64 UInt64s{ 0 };
		};

		constexpr BinaryBTHAddress() noexcept {}

		constexpr BinaryBTHAddress(const Family family, const UInt64 u64 = 0) noexcept :
			AddressFamily(family), UInt64s(u64)
		{}

		constexpr BinaryBTHAddress(const BinaryBTHAddress& other) noexcept :
			AddressFamily(other.AddressFamily), UInt64s(other.UInt64s)
		{}

		constexpr BinaryBTHAddress(BinaryBTHAddress&& other) noexcept :
			AddressFamily(other.AddressFamily), UInt64s(other.UInt64s)
		{}

		constexpr BinaryBTHAddress& operator=(const BinaryBTHAddress& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			AddressFamily = other.AddressFamily;
			UInt64s = other.UInt64s;

			return *this;
		}

		constexpr BinaryBTHAddress& operator=(BinaryBTHAddress&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			*this = other;

			return *this;
		}

		constexpr bool operator==(const BinaryBTHAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					UInt64s == other.UInt64s);
		}

		constexpr bool operator!=(const BinaryBTHAddress& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr void Clear() noexcept
		{
			AddressFamily = Family::Unspecified;
			UInt64s = 0;
		}

		std::size_t GetHash() const noexcept;
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryBTHAddress final
	{
		using Family = BTH::AddressFamily;

		Family AddressFamily{ Family::Unspecified };
		union
		{
			Byte Bytes[16];
			UInt64 UInt64s{ 0 };
		};

		SerializedBinaryBTHAddress() noexcept {}
		SerializedBinaryBTHAddress(const BinaryBTHAddress& addr) noexcept { *this = addr; }

		SerializedBinaryBTHAddress& operator=(const BinaryBTHAddress& addr) noexcept
		{
			AddressFamily = addr.AddressFamily;
			UInt64s = addr.UInt64s;
			return *this;
		}

		operator BinaryBTHAddress() const noexcept
		{
			BinaryBTHAddress addr;
			addr.AddressFamily = AddressFamily;
			addr.UInt64s = UInt64s;
			return addr;
		}

		bool operator==(const SerializedBinaryBTHAddress& other) const noexcept
		{
			return (AddressFamily == other.AddressFamily &&
					UInt64s == other.UInt64s);
		}

		bool operator!=(const SerializedBinaryBTHAddress& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}

namespace std
{
	// Specialization for standard hash function for BinaryBTHAddress
	template<> struct hash<QuantumGate::Implementation::Network::BinaryBTHAddress>
	{
		std::size_t operator()(const QuantumGate::Implementation::Network::BinaryBTHAddress& addr) const noexcept
		{
			return addr.GetHash();
		}
	};
}
