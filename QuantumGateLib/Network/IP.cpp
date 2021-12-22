// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IP.h"

namespace QuantumGate::Implementation::Network::ICMP
{
	UInt16 CalculateChecksum(const BufferView buffer) noexcept
	{
		UInt32 chksum{ 0 };

		auto data = reinterpret_cast<const UInt16*>(buffer.GetBytes());
		auto size = buffer.GetSize();

		while (size > 1)
		{
			chksum += *data++;

			if (size >= sizeof(UInt16))
			{
				size -= sizeof(UInt16);
			}
			else break;
		}

		assert(size == 0 || size == 1);

		if (size == 1)
		{
			chksum += *reinterpret_cast<const UInt8*>(data);
		}

		chksum = (chksum >> 16) + (chksum & 0xffff);
		chksum += (chksum >> 16);

		return static_cast<UInt16>(~chksum);
	}
}