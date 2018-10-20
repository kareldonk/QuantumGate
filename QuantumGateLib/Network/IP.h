// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Network::IP
{
#pragma pack(push, 1) // Disable padding bytes
	struct Header
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

	enum class AddressFamily : UInt8
	{
		Unspecified = 0,
		IPv4 = 2,
		IPv6 = 23
	};

	enum class Protocol : UInt8
	{
		Unspecified = 0,
		ICMP = 1,
		TCP = 6,
		UDP = 17
	};
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
	struct Header
	{
		UInt8 Type{ 0 };
		UInt8 Code{ 0 };
		UInt16 Checksum{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(Header) == 4, "Size of ICMP header should be 4 bytes");

#pragma pack(push, 1) // Disable padding bytes
	struct EchoMessage
	{
		Header Header;
		UInt16 Identifier{ 0 };
		UInt16 SequenceNumber{ 0 };
	};
#pragma pack(pop)

	static_assert(sizeof(EchoMessage) == 8, "Size of EchoMessage should be 8 bytes");

#pragma pack(push, 1) // Disable padding bytes
	struct DestinationUnreachableMessage
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