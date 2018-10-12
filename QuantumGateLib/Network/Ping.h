// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IP.h"
#include "Socket.h"

namespace QuantumGate::Implementation::Network
{
	class Export Ping
	{
	public:
		Ping(const IPAddress& ip, const UInt8 num_packets) noexcept :
			m_IPAddress(ip), m_NumPackets(num_packets)
		{}

		[[nodiscard]] const bool Process() noexcept;
		[[nodiscard]] const bool GetResult() noexcept;

	private:
		IPAddress m_IPAddress;
		UInt8 m_NumPackets{ 0 };
		Socket m_Socket;
	};
}