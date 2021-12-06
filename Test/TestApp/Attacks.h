// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"
#include "Network\Socket.h"

using namespace QuantumGate;
using namespace QuantumGate::Implementation::Network;

class AttackSocket final : public QuantumGate::Implementation::Network::Socket
{
public:
	AttackSocket(const AddressFamily af, const Type type, const Protocol protocol) :
		Socket(af, type, protocol)
	{}

	using Socket::Send;
	using Socket::Receive;
};

class Attacks
{
	struct ThreadData final
	{
		std::thread Thread;
		std::atomic_bool Stop{ false };
	};

private:
	Attacks() noexcept = default;

public:
	static bool StartConnectGarbageAttack(const Endpoint& endpoint);
	static void StopConnectGarbageAttack();
	static bool IsConnectGarbageAttackRunning() noexcept;

	static bool StartConnectAttack(const Endpoint& endpoint);
	static void StopConnectAttack();
	static bool IsConnectAttackRunning() noexcept;

	static bool StartConnectWaitAttack(const Endpoint& endpoint);
	static void StopConnectWaitAttack();
	static bool IsConnectWaitAttackRunning() noexcept;

private:
	static void ConnectGarbageThreadProc(const Endpoint& endpoint);
	static void ConnectThreadProc(const Endpoint& endpoint);
	static void ConnectWaitThreadProc(const Endpoint& endpoint);

private:
	static ThreadData m_ConnectGarbageData;
	static ThreadData m_ConnectData;
	static ThreadData m_ConnectWaitData;
};

