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
	struct EchoMessageHeader
	{
		UInt8 Type{ 0 };
		UInt8 Code{ 0 };
		UInt16 Checksum{ 0 };
		UInt16 Identifier{ 0 };
		UInt16 SequenceNumber{ 0 };
	};
#pragma pack(pop)


	[[nodiscard]] UInt16 CalculateChecksum(const BufferView buffer) noexcept
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