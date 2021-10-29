// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IPAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export IPEndpoint
	{
	public:
		using Protocol = IP::Protocol;

		constexpr IPEndpoint() noexcept {}

		constexpr IPEndpoint(const Protocol protocol, const IPAddress& ipaddr, const UInt16 port) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(ipaddr), m_Port(port)
		{}

		constexpr IPEndpoint(const Protocol protocol, const IPAddress& ipaddr, const UInt16 port,
							 const RelayPort rport, const RelayHop hop) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(ipaddr), m_Port(port), m_RelayPort(rport), m_RelayHop(hop)
		{}

		constexpr IPEndpoint(const IPEndpoint& other) noexcept :
			m_Protocol(other.m_Protocol), m_Address(other.m_Address), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr IPEndpoint(IPEndpoint&& other) noexcept :
			m_Protocol(other.m_Protocol), m_Address(std::move(other.m_Address)), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		IPEndpoint(const Protocol protocol, const sockaddr_storage* addr)
		{
			assert(addr != nullptr);

			m_Protocol = ValidateProtocol(protocol);
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

		~IPEndpoint() = default;

		constexpr IPEndpoint& operator=(const IPEndpoint& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Protocol = other.m_Protocol;
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

			m_Protocol = other.m_Protocol;
			m_Address = std::move(other.m_Address);
			m_Port = other.m_Port;
			m_RelayPort = other.m_RelayPort;
			m_RelayHop = other.m_RelayHop;

			return *this;
		}

		constexpr bool operator==(const IPEndpoint& other) const noexcept
		{
			return ((m_Protocol == other.m_Protocol) &&
					(m_Address == other.m_Address) &&
					(m_Port == other.m_Port) &&
					(m_RelayPort == other.m_RelayPort) &&
					(m_RelayHop == other.m_RelayHop));
		}

		constexpr bool operator!=(const IPEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr Protocol GetProtocol() const noexcept { return m_Protocol; }
		constexpr const IPAddress& GetIPAddress() const noexcept { return m_Address; }
		constexpr UInt16 GetPort() const noexcept { return m_Port; }
		constexpr RelayPort GetRelayPort() const noexcept { return m_RelayPort; }
		constexpr RelayHop GetRelayHop() const noexcept { return m_RelayHop; }

		String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const IPEndpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const IPEndpoint& endpoint);

	private:
		constexpr inline Protocol ValidateProtocol(const Protocol protocol)
		{
			switch (protocol)
			{
				case Protocol::ICMP:
				case Protocol::UDP:
				case Protocol::TCP:
					return protocol;
				default:
					throw std::invalid_argument("Unsupported internetwork protocol");
			}

			return Protocol::Unspecified;
		}

	private:
		Protocol m_Protocol{ Protocol::Unspecified };
		IPAddress m_Address;
		UInt16 m_Port{ 0 };
		RelayPort m_RelayPort{ 0 };
		RelayHop m_RelayHop{ 0 };
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedIPEndpoint final
	{
		IPEndpoint::Protocol Protocol{ IPEndpoint::Protocol::Unspecified };
		SerializedBinaryIPAddress IPAddress;
		UInt16 Port{ 0 };

		SerializedIPEndpoint() noexcept {}
		SerializedIPEndpoint(const IPEndpoint& endpoint) noexcept { *this = endpoint; }

		SerializedIPEndpoint& operator=(const IPEndpoint& endpoint) noexcept
		{
			Protocol = endpoint.GetProtocol();
			IPAddress = endpoint.GetIPAddress().GetBinary();
			Port = endpoint.GetPort();
			return *this;
		}

		operator IPEndpoint() const noexcept
		{
			return IPEndpoint(Protocol, { IPAddress }, Port);
		}

		bool operator==(const SerializedIPEndpoint& other) const noexcept
		{
			return (Protocol == other.Protocol && IPAddress == other.IPAddress && Port == other.Port);
		}

		bool operator!=(const SerializedIPEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}