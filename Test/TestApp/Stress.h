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
		String IP;
		UInt16 Port{ 0 };
		RelayHop Hops{ 0 };
		std::optional<PeerLUID> RelayPeer;
		ProtectedBuffer GlobalSharedSecret;
		std::atomic_bool Stop{ false };
		std::atomic_bool Connected{ false };
	};

	struct MultiInstanceStressData final
	{
		std::thread Thread;
		StartupParameters StartupParams;
		String IP;
		UInt16 Port{ 0 };
		ProtectedBuffer GlobalSharedSecret;
		std::atomic_bool Stop{ false };
	};

private:
	Stress() = default;

public:
	static bool StartExtenderStartupShutdownStress(Local& qg);
	static void StopExtenderStartupShutdownStress();
	static bool IsExtenderStartupShutdownStressRunning() noexcept;

	static bool StartConnectStress(Local& qg, const CString& ip, const UInt16 port, const RelayHop hops,
								   const std::optional<PeerLUID>& rpeer, const ProtectedBuffer& gsecret);
	static void StopConnectStress();
	static bool IsConnectStressRunning() noexcept;

	static bool StartMultiInstanceStress(const StartupParameters& startup_params,
										 const CString& ip, const UInt16 port, const ProtectedBuffer& gsecret);
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

