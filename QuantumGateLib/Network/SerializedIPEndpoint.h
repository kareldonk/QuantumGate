// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPEndpoint.h"

namespace QuantumGate::Implementation::Network
{
#pragma pack(push, 1) // Disable padding bytes
	struct SerializedIPEndpoint final
	{
		IPEndpoint::Protocol Protocol{ IPEndpoint::Protocol::Unspecified };
		SerializedBinaryIPAddress IPAddress;
		UInt16 Port{ 0 };

		SerializedIPEndpoint() noexcept {}
		SerializedIPEndpoint(const IPEndpoint& endpoint) noexcept { *this = endpoint; }

		SerializedIPEndpoint& operator=(const IPEndpoint& endpoint) noexcept
		{
			Protocol = endpoint.GetProtocol();
			IPAddress = endpoint.GetIPAddress().GetBinary();
			Port = endpoint.GetPort();
			return *this;
		}

		operator IPEndpoint() const noexcept
		{
			return IPEndpoint(Protocol, { IPAddress }, Port);
		}

		bool operator==(const SerializedIPEndpoint& other) const noexcept
		{
			return (Protocol == other.Protocol && IPAddress == other.IPAddress && Port == other.Port);
		}

		bool operator!=(const SerializedIPEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}