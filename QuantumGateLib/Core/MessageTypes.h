// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

namespace QuantumGate::Implementation::Core
{
	enum class MessageType : UInt16
	{
		Unknown = 0,

		BeginMetaExchange = 10,
		EndMetaExchange = 20,
		BeginPrimaryKeyExchange = 30,
		EndPrimaryKeyExchange = 40,
		BeginSecondaryKeyExchange = 50,
		EndSecondaryKeyExchange = 60,
		BeginAuthentication = 70,
		EndAuthentication = 80,
		BeginSessionInit = 90,
		EndSessionInit = 100,

		BeginPrimaryKeyUpdateExchange = 110,
		EndPrimaryKeyUpdateExchange = 120,
		BeginSecondaryKeyUpdateExchange = 130,
		EndSecondaryKeyUpdateExchange = 140,
		KeyUpdateReady = 150,

		ExtenderCommunication = 160,
		ExtenderUpdate = 170,
		Noise = 180,

		RelayCreate = 300,
		RelayStatus = 310,
		RelayData = 320,
		RelayDataAck = 330
	};

	enum class RelayStatusUpdate : UInt8
	{
		Disconnected = 0,
		Connected = 1,
		Suspended = 2,
		Resumed = 3,
		GeneralFailure = 4,
		ConnectionReset = 5,
		NoPeersAvailable = 6,
		HostUnreachable = 7,
		ConnectionRefused = 8,
		TimedOut = 9
	};
}