// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export IPEndpoint
	{
	public:
		IPEndpoint() noexcept {}
		IPEndpoint(const IPEndpoint& other) noexcept;
		IPEndpoint(IPEndpoint&& other) noexcept;
		IPEndpoint(const IPAddress& ipaddr, const UInt16 port) noexcept;
		IPEndpoint(const IPAddress& ipaddr, const UInt16 port, const RelayPort rport, const RelayHop hop) noexcept;
		IPEndpoint(const sockaddr_storage* addr);
		virtual ~IPEndpoint() = default;

		IPEndpoint& operator=(const IPEndpoint& other) noexcept;
		IPEndpoint& operator=(IPEndpoint&& other) noexcept;

		constexpr const bool operator==(const IPEndpoint& other) const noexcept
		{
			return ((m_Address == other.m_Address) && (m_Port == other.m_Port));
		}

		constexpr const bool operator!=(const IPEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		String GetString() const noexcept;
		constexpr const IPAddress& GetIPAddress() const noexcept { return m_Address; }
		constexpr UInt16 GetPort() const noexcept { return m_Port; }
		constexpr RelayPort GetRelayPort() const noexcept { return m_RelayPort; }
		constexpr RelayHop GetRelayHop() const noexcept { return m_RelayHop; }

		friend Export std::ostream& operator<<(std::ostream& stream, const IPEndpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPEndpoint& endpoint);

	private:
		IPAddress m_Address;
		UInt16 m_Port{ 0 };
		RelayPort m_RelayPort{ 0 };
		RelayHop m_RelayHop{ 0 };
	};
}