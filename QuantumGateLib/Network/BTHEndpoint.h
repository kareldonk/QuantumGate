// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "BTHAddress.h"

namespace QuantumGate::Implementation::Network
{
	class Export BTHEndpoint
	{
	public:
		using Protocol = Network::BTH::Protocol;

		constexpr BTHEndpoint() noexcept {}

		constexpr BTHEndpoint(const Protocol protocol, const BTHAddress& addr, const UInt16 port) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port)
		{}


		constexpr BTHEndpoint(const Protocol protocol, const BTHAddress& addr, const GUID& scid) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_ServiceClassID(scid)
		{}

		constexpr BTHEndpoint(const Protocol protocol, const BTHAddress& addr, const UInt16 port, const GUID& scid) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port), m_ServiceClassID(scid)
		{
			if (m_Port != 0 && !AreGUIDsEqual(m_ServiceClassID, GetNullServiceClassID()))
			{
				throw std::invalid_argument("Specify either a port or a Service Class ID, not both");
			}
		}

		constexpr BTHEndpoint(const Protocol protocol, const BTHAddress& addr, const UInt16 port, const GUID& scid,
							  const RelayPort rport, const RelayHop hop) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port), m_ServiceClassID(scid),
			m_RelayPort(rport), m_RelayHop(hop)
		{
			if (m_Port != 0 && !AreGUIDsEqual(m_ServiceClassID, GetNullServiceClassID()))
			{
				throw std::invalid_argument("Specify either a port or a Service Class ID, not both");
			}
		}

		constexpr BTHEndpoint(const BTHEndpoint& other) noexcept :
			m_Protocol(other.m_Protocol), m_Address(other.m_Address), m_Port(other.m_Port),
			m_ServiceClassID(other.m_ServiceClassID), m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		constexpr BTHEndpoint(BTHEndpoint&& other) noexcept :
			m_Protocol(other.m_Protocol), m_Address(std::move(other.m_Address)), m_Port(other.m_Port),
			m_ServiceClassID(other.m_ServiceClassID), m_RelayPort(other.m_RelayPort), m_RelayHop(other.m_RelayHop)
		{}

		BTHEndpoint(const Protocol protocol, const sockaddr_storage* saddr);

		~BTHEndpoint() = default;

		constexpr BTHEndpoint& operator=(const BTHEndpoint& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Protocol = other.m_Protocol;
			m_Address = other.m_Address;
			m_Port = other.m_Port;
			m_ServiceClassID = other.m_ServiceClassID;
			m_RelayPort = other.m_RelayPort;
			m_RelayHop = other.m_RelayHop;

			return *this;
		}

		constexpr BTHEndpoint& operator=(BTHEndpoint&& other) noexcept
		{
			// Check for same object
			if (this == &other) return *this;

			m_Protocol = other.m_Protocol;
			m_Address = std::move(other.m_Address);
			m_Port = other.m_Port;
			m_ServiceClassID = other.m_ServiceClassID;
			m_RelayPort = other.m_RelayPort;
			m_RelayHop = other.m_RelayHop;

			return *this;
		}

		constexpr bool operator==(const BTHEndpoint& other) const noexcept
		{
			return ((m_Protocol == other.m_Protocol) &&
					(m_Address == other.m_Address) &&
					(m_Port == other.m_Port) &&
					AreGUIDsEqual(m_ServiceClassID, other.m_ServiceClassID) &&
					(m_RelayPort == other.m_RelayPort) &&
					(m_RelayHop == other.m_RelayHop));
		}

		constexpr bool operator!=(const BTHEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		[[nodiscard]] constexpr Protocol GetProtocol() const noexcept { return m_Protocol; }
		[[nodiscard]] constexpr const BTHAddress& GetBTHAddress() const noexcept { return m_Address; }
		[[nodiscard]] constexpr UInt16 GetPort() const noexcept { return m_Port; }
		[[nodiscard]] constexpr const GUID& GetServiceClassID() const noexcept { return m_ServiceClassID; }
		[[nodiscard]] constexpr RelayPort GetRelayPort() const noexcept { return m_RelayPort; }
		[[nodiscard]] constexpr RelayHop GetRelayHop() const noexcept { return m_RelayHop; }

		[[nodiscard]] String GetString() const noexcept;

		[[nodiscard]] static constexpr GUID GetQuantumGateServiceClassID() noexcept
		{
			return { 0xCA11AB1E, 0x5AFE, 0xC0DE, 0x20, 0x45, 0x41, 0x2D, 0x45, 0x4E, 0x4B, 0x49 };
		}

		[[nodiscard]] static constexpr GUID GetNullServiceClassID() noexcept { return { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; }

		[[nodiscard]] static constexpr bool AreGUIDsEqual(const GUID& g1, const GUID& g2) noexcept
		{
			if (std::is_constant_evaluated())
			{
				return (g1.Data1 == g2.Data1 &&
						g1.Data2 == g2.Data2 &&
						g1.Data3 == g2.Data3 &&
						g1.Data4[0] == g2.Data4[0] &&
						g1.Data4[1] == g2.Data4[1] &&
						g1.Data4[2] == g2.Data4[2] &&
						g1.Data4[3] == g2.Data4[3] &&
						g1.Data4[4] == g2.Data4[4] &&
						g1.Data4[5] == g2.Data4[5] &&
						g1.Data4[6] == g2.Data4[6] &&
						g1.Data4[7] == g2.Data4[7]);
			}
			else
			{
				return (g1.Data1 == g2.Data1 &&
						g1.Data2 == g2.Data2 &&
						g1.Data3 == g2.Data3 &&
						*(reinterpret_cast<const UInt64*>(&g1.Data4)) == *(reinterpret_cast<const UInt64*>(&g2.Data4)));
			}
		}

		friend Export std::ostream& operator<<(std::ostream& stream, const BTHEndpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const BTHEndpoint& endpoint);

	private:
		[[nodiscard]] constexpr inline Protocol ValidateProtocol(const Protocol protocol)
		{
			switch (protocol)
			{
				case Protocol::RFCOMM:
					return protocol;
				default:
					throw std::invalid_argument("Unsupported Bluetooth protocol");
			}

			return Protocol::Unspecified;
		}

	private:
		Protocol m_Protocol{ Protocol::Unspecified };
		BTHAddress m_Address;
		UInt16 m_Port{ 0 };
		GUID m_ServiceClassID{ 0 };
		RelayPort m_RelayPort{ 0 };
		RelayHop m_RelayHop{ 0 };
	};
}