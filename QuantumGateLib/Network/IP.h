// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Network.h"

namespace QuantumGate::Implementation::Network::IP
{
	enum class AddressFamily : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::AddressFamily::Unspecified),
		IPv4 = static_cast<UInt8>(Network::AddressFamily::IPv4),
		IPv6 = static_cast<UInt8>(Network::AddressFamily::IPv6)
	};

	[[nodiscard]] constexpr AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept
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

	[[nodiscard]] constexpr Network::AddressFamily AddressFamilyToNetwork(const AddressFamily protocol) noexcept
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

	enum class Protocol : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::Protocol::Unspecified),
		ICMP = static_cast<UInt8>(Network::Protocol::ICMP),
		TCP = static_cast<UInt8>(Network::Protocol::TCP),
		UDP = static_cast<UInt8>(Network::Protocol::UDP)
	};

	[[nodiscard]] constexpr Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept
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

	[[nodiscard]] constexpr Network::Protocol ProtocolToNetwork(const Protocol protocol) noexcept
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

#pragma pack(push, 1) // Disable padding bytes
	struct Header final
	{
		UInt8 Version_HeaderLength{ 0 };
		UInt8 ServiceType{ 0 };
		UInt16 TotalLength{ 0 };
		UInt16 Identification{ 0 };
		UInt16 Flags_FragmentOffset{ 0 };
		UInt8 TTL{ 0 };
		UInt8 Protocol{ 0 };
		UInt16 HeaderChecksum{ 0 };
		UInt32 SourceAddress{ 0 };
		UInt32 DestinationAddress{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(Header) == 20, "Size of IP header should be 20 bytes");
}

namespace QuantumGate::Implementation::Network::ICMP
{
	enum class MessageType : UInt8
	{
		EchoReply = 0,
		DestinationUnreachable = 3,
		SourceQuench = 4,
		Redirect = 5,
		Echo = 8,
		TimeExceeded = 11,
		ParameterProblem = 12,
		Timestamp = 13,
		TimestampReply = 14,
		InformationRequest = 15,
		InformationReply = 16
	};

#pragma pack(push, 1) // Disable padding bytes
	struct Header final
	{
		UInt8 Type{ 0 };
		UInt8 Code{ 0 };
		UInt16 Checksum{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(Header) == 4, "Size of ICMP header should be 4 bytes");

#pragma pack(push, 1) // Disable padding bytes
	struct EchoMessage final
	{
		Header Header;
		UInt16 Identifier{ 0 };
		UInt16 SequenceNumber{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(EchoMessage) == 8, "Size of EchoMessage should be 8 bytes");

#pragma pack(push, 1) // Disable padding bytes
	struct DestinationUnreachableMessage final
	{
		Header Header;
		UInt32 Unused{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(DestinationUnreachableMessage) == 8, "Size of DestinationUnreachableMessage should be 8 bytes");

	enum class DestinationUnreachableCode : UInt8
	{
		NetUnreachable = 0,
		HostUnreachable = 1,
		ProtocolUnreachable = 2,
		PortUnreachable = 3,
		FragmentationNeeded = 4,
		SourceRouteFailed = 5
	};

	using TimeExceededMessage = DestinationUnreachableMessage;

	enum class TimeExceededCode : UInt8
	{
		TTLExceeded = 0,
		FragmentReassemblyTimeExceeded = 1
	};

	[[nodiscard]] UInt16 CalculateChecksum(const BufferView buffer) noexcept;
}