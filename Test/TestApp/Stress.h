// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"

using namespace QuantumGate;

class Stress final
{
	struct ExtenderStartupShutdownStressData final
	{
		std::thread Thread;
		std::atomic_bool Stop{ false };
	};

	struct ConnectStressData final
	{
		std::thread Thread;
		Endpoint Endpoint;
		RelayHop Hops{ 0 };
		bool ReuseConnection{ false };
		bool BTHAuthentication{ true };
		std::optional<PeerLUID> RelayPeer;
		ProtectedBuffer GlobalSharedSecret;
		std::atomic_bool Stop{ false };
		std::atomic_bool Connected{ false };
	};

	struct MultiInstanceStressData final
	{
		std::thread Thread;
		StartupParameters StartupParams;
		Endpoint Endpoint;
		ProtectedBuffer GlobalSharedSecret;
		std::atomic_bool Stop{ false };
	};

private:
	Stress() noexcept = default;

public:
	static bool StartExtenderStartupShutdownStress(Local& qg);
	static void StopExtenderStartupShutdownStress();
	static bool IsExtenderStartupShutdownStressRunning() noexcept;

	static bool StartConnectStress(Local& qg, const Endpoint& endpoint, const bool bthauth, const RelayHop hops, const bool reuse,
								   const std::optional<PeerLUID>& rpeer, const ProtectedBuffer& gsecret);
	static void StopConnectStress();
	static bool IsConnectStressRunning() noexcept;

	static bool StartMultiInstanceStress(const StartupParameters& startup_params, const Endpoint& endpoint,
										 const ProtectedBuffer& gsecret);
	static void StopMultiInstanceStress();
	static bool IsMultiInstanceStressRunning() noexcept;

private:
	static void ExtenderStartupShutdownStressThreadProc(Local* qg);
	static void ConnectStressThreadProc(Local* qg);
	static void MultiInstanceStressThreadProc();

private:
	static ExtenderStartupShutdownStressData m_ExtenderStartupShutdownStressData;
	static ConnectStressData m_ConnectStressData;
	static MultiInstanceStressData m_MultiInstanceStressData;
};

