// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"
#include "Network\Socket.h"

using namespace QuantumGate;

class AttackSocket : public QuantumGate::Implementation::Network::Socket
{
public:
	AttackSocket(const IPAddressFamily af, const Int32 type, const Int32 protocol) noexcept :
		Socket(af, type, protocol)
	{}

	using Socket::Send;
	using Socket::Receive;
};

class Attacks
{
	struct ConnectGarbageData
	{
		std::thread Thread;
		std::atomic_bool Stop{ false };
	};

private:
	Attacks() = default;

public:
	static const bool StartConnectGarbageAttack(const CString& ip, const UInt16 port);
	static void StopConnectGarbageAttack();
	static bool IsConnectGarbageAttackRunning() noexcept;

private:
	static void ConnectGarbageThreadProc(const CString ip, const UInt16 port);

private:
	static ConnectGarbageData m_ConnectGarbageData;
};

