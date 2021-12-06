// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BTHEndpoint.h"

namespace QuantumGate::Implementation::Network
{
#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBTHEndpoint final
	{
		BTHEndpoint::Protocol Protocol{ BTHEndpoint::Protocol::Unspecified };
		SerializedBinaryBTHAddress BTHAddress;
		UInt16 Port{ 0 };
		GUID ServiceClassID{ 0 };

		SerializedBTHEndpoint() noexcept {}
		SerializedBTHEndpoint(const BTHEndpoint& endpoint) noexcept { *this = endpoint; }

		SerializedBTHEndpoint& operator=(const BTHEndpoint& endpoint) noexcept
		{
			Protocol = endpoint.GetProtocol();
			BTHAddress = endpoint.GetBTHAddress().GetBinary();
			Port = endpoint.GetPort();
			ServiceClassID = endpoint.GetServiceClassID();
			return *this;
		}

		operator BTHEndpoint() const noexcept
		{
			return BTHEndpoint(Protocol, { BTHAddress }, Port, ServiceClassID);
		}

		bool operator==(const SerializedBTHEndpoint& other) const noexcept
		{
			return (Protocol == other.Protocol && BTHAddress == other.BTHAddress &&
					Port == other.Port && ServiceClassID == other.ServiceClassID);
		}

		bool operator!=(const SerializedBTHEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}