// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core::Relay::Events
{
	struct Connect final
	{
		Connect() = default;
		Connect(const Connect&) = delete;
		Connect(Connect&&) = default;
		~Connect() = default;
		Connect& operator=(const Connect&) = delete;
		Connect& operator=(Connect&&) = default;

		RelayPort Port{ 0 };
		RelayHop Hop{ 0 };
		IPEndpoint Endpoint;
		struct
		{
			PeerLUID PeerLUID{ 0 };
			IPEndpoint LocalEndpoint;
			IPEndpoint PeerEndpoint;
		} Origin;
	};

	struct StatusUpdate final
	{
		StatusUpdate() = default;
		StatusUpdate(const StatusUpdate&) = delete;
		StatusUpdate(StatusUpdate&&) = default;
		~StatusUpdate() = default;
		StatusUpdate& operator=(const StatusUpdate&) = delete;
		StatusUpdate& operator=(StatusUpdate&&) = default;

		RelayPort Port{ 0 };
		RelayStatusUpdate Status{ RelayStatusUpdate::GeneralFailure };
		struct
		{
			PeerLUID PeerLUID{ 0 };
		} Origin;
	};

	struct RelayData final
	{
		RelayData() = default;
		RelayData(const RelayData&) = delete;
		RelayData(RelayData&&) = default;
		~RelayData() = default;
		RelayData& operator=(const RelayData&) = delete;
		RelayData& operator=(RelayData&&) = default;

		RelayPort Port{ 0 };
		Buffer Data;
		struct
		{
			PeerLUID PeerLUID{ 0 };
		} Origin;
	};
}

