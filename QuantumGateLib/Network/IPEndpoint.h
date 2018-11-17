// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export IPEndpoint
	{
	public:
		constexpr IPEndpoint() noexcept {}

		constexpr IPEndpoint(const IPEndpoint& other) noexcept :
			m_Address(other.m_Address), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr IPEndpoint(IPEndpoint&& other) noexcept :
			m_Address(std::move(other.m_Address)), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr IPEndpoint(const IPAddress& ipaddr, const UInt16 port) noexcept :
			m_Address(ipaddr), m_Port(port)
		{}

		constexpr IPEndpoint(const IPAddress& ipaddr, const UInt16 port,
							 const RelayPort rport, const RelayHop hop) noexcept :
			m_Address(ipaddr), m_Port(port), m_RelayPort(rport), m_RelayHop(hop)
		{}

		IPEndpoint(const sockaddr_storage* addr)
		{
			assert(addr != nullptr);

			m_Address = IPAddress(addr);

			switch (addr->ss_family)
			{
				case AF_INET:
					m_Port = reinterpret_cast<const sockaddr_in*>(addr)->sin_port;
					break;
				case AF_INET6:
					m_Port = reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port;
					break;
				default:
					// IPAddress should already have thrown an exception;
					// this is just in case
					assert(false);
					break;
			}
		}

		constexpr IPEndpoint& operator=(const IPEndpoint& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Address = other.m_Address;
			m_Port = other.m_Port;
			m_RelayPort = other.m_RelayPort;
			m_RelayHop = other.m_RelayHop;

			return *this;
		}

		constexpr IPEndpoint& operator=(IPEndpoint&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Address = std::move(other.m_Address);
			m_Port = other.m_Port;
			m_RelayPort = other.m_RelayPort;
			m_RelayHop = other.m_RelayHop;

			return *this;
		}

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

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedIPEndpoint final
	{
		SerializedBinaryIPAddress IPAddress;
		UInt16 Port{ 0 };

		SerializedIPEndpoint() noexcept {}
		SerializedIPEndpoint(const IPEndpoint& endpoint) noexcept { *this = endpoint; }

		SerializedIPEndpoint& operator=(const IPEndpoint& endpoint) noexcept
		{
			IPAddress = endpoint.GetIPAddress().GetBinary();
			Port = endpoint.GetPort();
			return *this;
		}

		operator IPEndpoint() const
		{
			return IPEndpoint({ IPAddress }, Port);
		}

		bool operator==(const SerializedIPEndpoint& other) const noexcept
		{
			return (IPAddress == other.IPAddress && Port == other.Port);
		}

		bool operator!=(const SerializedIPEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}