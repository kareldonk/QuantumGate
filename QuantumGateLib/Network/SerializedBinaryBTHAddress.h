// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BinaryBTHAddress.h"

namespace QuantumGate::Implementation::Network
{
#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBinaryBTHAddress final
	{
		using Family = BTH::AddressFamily;

		Family AddressFamily{ Family::Unspecified };
		union
		{
			Byte Bytes[8];
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
