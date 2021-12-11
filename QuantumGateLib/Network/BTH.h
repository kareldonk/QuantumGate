// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Network.h"

namespace QuantumGate::Implementation::Network::BTH
{
	enum class AddressFamily : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::AddressFamily::Unspecified),
		BTH = static_cast<UInt8>(Network::AddressFamily::BTH)
	};

	[[nodiscard]] AddressFamily AddressFamilyFromNetwork(const Network::AddressFamily af) noexcept;
	[[nodiscard]] Network::AddressFamily AddressFamilyToNetwork(const AddressFamily protocol) noexcept;

	enum class Protocol : UInt8
	{
		Unspecified = static_cast<UInt8>(Network::Protocol::Unspecified),
		RFCOMM = static_cast<UInt8>(Network::Protocol::RFCOMM)
	};

	[[nodiscard]] Protocol ProtocolFromNetwork(const Network::Protocol protocol) noexcept;
	[[nodiscard]] Network::Protocol ProtocolToNetwork(const Protocol protocol) noexcept;
}