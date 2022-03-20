// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "IMFAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export IMFEndpoint
	{
	public:
		using Protocol = Network::IMF::Protocol;

		constexpr IMFEndpoint() noexcept {}

		constexpr IMFEndpoint(const Protocol protocol, const IMFAddress& addr, const UInt16 port) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port)
		{}


		constexpr IMFEndpoint(const Protocol protocol, const IMFAddress& addr, const UInt16 port,
							  const RelayPort rport, const RelayHop hop) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port),
			m_RelayPort(rport), m_RelayHop(hop)
		{}

		constexpr IMFEndpoint(const IMFEndpoint& other) :
			m_Protocol(other.m_Protocol), m_Address(other.m_Address), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr IMFEndpoint(IMFEndpoint&& other) noexcept :
			m_Protocol(other.m_Protocol), m_Address(std::move(other.m_Address)), m_Port(other.m_Port),
			m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr ~IMFEndpoint() = default;

		constexpr IMFEndpoint& operator=(const IMFEndpoint& other)
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

		constexpr IMFEndpoint& operator=(IMFEndpoint&& other) noexcept
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

		constexpr bool operator==(const IMFEndpoint& other) const noexcept
		{
			return ((m_Protocol == other.m_Protocol) &&
					(m_Address == other.m_Address) &&
					(m_Port == other.m_Port) &&
					(m_RelayPort == other.m_RelayPort) &&
					(m_RelayHop == other.m_RelayHop));
		}

		constexpr bool operator!=(const IMFEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Protocol GetProtocol() const noexcept { return m_Protocol; }
		[[nodiscard]] constexpr const IMFAddress& GetIMFAddress() const noexcept { return m_Address; }
		[[nodiscard]] constexpr UInt16 GetPort() const noexcept { return m_Port; }
		[[nodiscard]] constexpr RelayPort GetRelayPort() const noexcept { return m_RelayPort; }
		[[nodiscard]] constexpr RelayHop GetRelayHop() const noexcept { return m_RelayHop; }

		[[nodiscard]] String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const IMFEndpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const IMFEndpoint& endpoint);

	private:
		[[nodiscard]] constexpr inline Protocol ValidateProtocol(const Protocol protocol)
		{
			switch (protocol)
			{
				case Protocol::IMF:
					return protocol;
				default:
					throw std::invalid_argument("Unsupported Internet Message Format protocol");
			}

			return Protocol::Unspecified;
		}

	private:
		Protocol m_Protocol{ Protocol::Unspecified };
		IMFAddress m_Address;
		UInt16 m_Port{ 0 };
		RelayPort m_RelayPort{ 0 };
		RelayHop m_RelayHop{ 0 };
	};
}