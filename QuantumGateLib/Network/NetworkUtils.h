// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Address.h"
#include "Endpoint.h"

namespace QuantumGate::Implementation::Network
{
	[[nodiscard]] Protocol GetEndpointNetworkProtocol(const Endpoint& endpoint) noexcept;
	[[nodiscard]] AddressFamily GetEndpointNetworkAddressFamily(const Endpoint& endpoint) noexcept;
	[[nodiscard]] Address GetEndpointAddress(const Endpoint& endpoint) noexcept;
	[[nodiscard]] UInt16 GetEndpointPort(const Endpoint& endpoint) noexcept;
}