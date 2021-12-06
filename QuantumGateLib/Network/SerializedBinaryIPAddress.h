// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryIPAddress.h"

namespace QuantumGate::Implementation::Network
{
#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryIPAddress final
	{
		BinaryIPAddress::Family AddressFamily{ BinaryIPAddress::Family::Unspecified };
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
