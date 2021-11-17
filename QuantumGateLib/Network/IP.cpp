// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "IP.h"

namespace QuantumGate::Implementation::Network::IP
{
	AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept
	{
		switch (af)
		{
			case Network::AddressFamily::Unspecified:
				return AddressFamily::Unspecified;
			case Network::AddressFamily::IPv4:
				return AddressFamily::IPv4;
			case Network::AddressFamily::IPv6:
				return AddressFamily::IPv6;
			default:
				assert(false);
				break;
		}

		return AddressFamily::Unspecified;
	}

	Network::AddressFamily AddressFamilyToNetwork(const AddressFamily protocol) noexcept
	{
		switch (protocol)
		{
			case AddressFamily::Unspecified:
				return Network::AddressFamily::Unspecified;
			case AddressFamily::IPv4:
				return Network::AddressFamily::IPv4;
			case AddressFamily::IPv6:
				return Network::AddressFamily::IPv6;
			default:
				assert(false);
				break;
		}

		return Network::AddressFamily::Unspecified;
	}

	Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Network::Protocol::Unspecified:
				return Protocol::Unspecified;
			case Network::Protocol::ICMP:
				return Protocol::ICMP;
			case Network::Protocol::TCP:
				return Protocol::TCP;
			case Network::Protocol::UDP:
				return Protocol::UDP;
			default:
				assert(false);
				break;
		}

		return Protocol::Unspecified;
	}

	Network::Protocol ProtocolToNetwork(const Protocol protocol) noexcept
	{
		switch (protocol)
		{
			case Protocol::Unspecified:
				return Network::Protocol::Unspecified;
			case Protocol::ICMP:
				return Network::Protocol::ICMP;
			case Protocol::TCP:
				return Network::Protocol::TCP;
			case Protocol::UDP:
				return Network::Protocol::UDP;
			default:
				assert(false);
				break;
		}

		return Network::Protocol::Unspecified;
	}
}

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