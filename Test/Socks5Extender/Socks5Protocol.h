// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"

// Socks 5 Protocol according to RFC1928; for details
// see: https://www.ietf.org/rfc/rfc1928.txt and https://www.ietf.org/rfc/rfc1929.txt

namespace QuantumGate::Socks5Extender
{
	namespace Socks5Protocol
	{
		enum class AuthMethods : UInt8
		{
			NoAuthenticationRequired = 0x00,
			GSSAPI = 0x01,
			UsernamePassword = 0x02,
			NoAcceptableMethods = 0xff
		};

		enum class Commands : UInt8
		{
			Unknown = 0x00,
			Connect = 0x01,
			Bind = 0x02,
			UDPAssociate = 0x03
		};

		enum class AddressTypes : UInt8
		{
			Unknown = 0x00,
			IPv4 = 0x01,
			DomainName = 0x03,
			IPv6 = 0x04
		};

		enum class Replies : UInt8
		{
			Succeeded = 0x00,
			GeneralFailure = 0x01,
			ConnectionNotAllowed = 0x02,
			NetworkUnreachable = 0x03,
			HostUnreachable = 0x04,
			ConnectionRefused = 0x05,
			TTLExpired = 0x06,
			UnsupportedCommand = 0x07,
			UnsupportedAddressType = 0x08
		};

#pragma pack(push, 1) // Disable padding bytes
		struct MethodIdentificationMsg final
		{
			UInt8 Version{ 0x05 };
			UInt8 NumMethods{ 0 };
		};

		struct MethodSelectionMsg final
		{
			UInt8 Version{ 0x05 };
			UInt8 Method{ 0 };
		};

		struct RequestMsg final
		{
			UInt8 Version{ 0x05 };
			UInt8 Command{ 0 };
			UInt8 Reserved{ 0 };
			UInt8 AddressType{ 0 };
		};

		struct ReplyMsg final
		{
			UInt8 Version{ 0x05 };
			UInt8 Reply{ 0 };
			UInt8 Reserved{ 0 };
			UInt8 AddressType{ 0 };
		};

		struct IPv4Address final
		{
			UInt8 Address[4]{ 0 };
			UInt16 Port{ 0 };
		};

		struct IPv6Address final
		{
			UInt8 Address[16]{ 0 };
			UInt16 Port{ 0 };
		};

		struct AuthReplyMsg final
		{
			UInt8 Version{ 0x01 };
			UInt8 Reply{ 0 };
		};
#pragma pack(pop)
	}
}