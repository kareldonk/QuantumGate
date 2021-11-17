// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\Peer\PeerMessageDetails.h"

#include <variant>

namespace QuantumGate::Implementation::Core::Relay::Events
{
	struct Connect final
	{
		Connect() noexcept = default;
		Connect(const Connect&) = delete;
		Connect(Connect&&) noexcept = default;
		~Connect() = default;
		Connect& operator=(const Connect&) = delete;
		Connect& operator=(Connect&&) noexcept = default;

		RelayPort Port{ 0 };
		RelayHop Hop{ 0 };
		Endpoint ConnectEndpoint;
		struct
		{
			PeerLUID PeerLUID{ 0 };
			Endpoint LocalEndpoint;
			Endpoint PeerEndpoint;
		} Origin;
	};

	struct StatusUpdate final
	{
		StatusUpdate() noexcept = default;
		StatusUpdate(const StatusUpdate&) = delete;
		StatusUpdate(StatusUpdate&&) noexcept = default;
		~StatusUpdate() = default;
		StatusUpdate& operator=(const StatusUpdate&) = delete;
		StatusUpdate& operator=(StatusUpdate&&) noexcept = default;

		RelayPort Port{ 0 };
		RelayStatusUpdate Status{ RelayStatusUpdate::GeneralFailure };
		struct
		{
			PeerLUID PeerLUID{ 0 };
		} Origin;
	};

	struct RelayData final
	{
		RelayData() noexcept = default;
		RelayData(const RelayData&) = delete;
		RelayData(RelayData&&) noexcept = default;
		~RelayData() = default;
		RelayData& operator=(const RelayData&) = delete;
		RelayData& operator=(RelayData&&) noexcept = default;

		RelayPort Port{ 0 };
		RelayMessageID MessageID{ 0 };
		Buffer Data;
		struct
		{
			PeerLUID PeerLUID{ 0 };
		} Origin;
		Peer::MessageDetails::MessageRate MessageRate;
	};

	struct RelayDataAck final
	{
		RelayDataAck() noexcept = default;
		RelayDataAck(const RelayDataAck&) = delete;
		RelayDataAck(RelayDataAck&&) noexcept = default;
		~RelayDataAck() = default;
		RelayDataAck& operator=(const RelayDataAck&) = delete;
		RelayDataAck& operator=(RelayDataAck&&) noexcept = default;

		RelayPort Port{ 0 };
		RelayMessageID MessageID{ 0 };
		struct
		{
			PeerLUID PeerLUID{ 0 };
		} Origin;
	};
}

namespace QuantumGate::Implementation::Core::Relay
{
	using Event = std::variant<Events::Connect, Events::StatusUpdate, Events::RelayData, Events::RelayDataAck>;
}

