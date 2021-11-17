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

		constexpr BTHEndpoint(const Protocol protocol, const BTHAddress& addr, const UInt16 port,
							  const RelayPort rport, const RelayHop hop) :
			m_Protocol(ValidateProtocol(protocol)), m_Address(addr), m_Port(port), m_RelayPort(rport), m_RelayHop(hop)
		{}

		constexpr bool operator==(const BTHEndpoint& other) const noexcept
		{
			return false;
		}

		constexpr bool operator!=(const BTHEndpoint& other) const noexcept
		{
			return !(*this == other);
		}

		constexpr Protocol GetProtocol() const noexcept { return Protocol::BTH; }
		constexpr const BTHAddress& GetBTHAddress() const noexcept { return m_Address; }
		constexpr UInt16 GetPort() const noexcept { return m_Port; }
		constexpr RelayPort GetRelayPort() const noexcept { return m_RelayPort; }
		constexpr RelayHop GetRelayHop() const noexcept { return m_RelayHop; }

		String GetString() const noexcept;

		friend Export std::ostream& operator<<(std::ostream& stream, const BTHEndpoint& endpoint);
		friend Export std::wostream& operator<<(std::wostream& stream, const BTHEndpoint& endpoint);

	private:
		constexpr inline Protocol ValidateProtocol(const Protocol protocol)
		{
			switch (protocol)
			{
				case Protocol::BTH:
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
		RelayPort m_RelayPort{ 0 };
		RelayHop m_RelayHop{ 0 };
		GUID m_ServiceClassID{ 0 };
	};

#pragma pack(push, 1) // Disable padding bytes
	struct SerializedBTHEndpoint final
	{
		BTHEndpoint::Protocol Protocol{ BTHEndpoint::Protocol::Unspecified };
		SerializedBinaryBTHAddress BTHAddress;
		UInt16 Port{ 0 };

		SerializedBTHEndpoint() noexcept {}
		SerializedBTHEndpoint(const BTHEndpoint& endpoint) noexcept { *this = endpoint; }

		SerializedBTHEndpoint& operator=(const BTHEndpoint& endpoint) noexcept
		{
			Protocol = endpoint.GetProtocol();
			BTHAddress = endpoint.GetBTHAddress().GetBinary();
			Port = endpoint.GetPort();
			return *this;
		}

		operator BTHEndpoint() const noexcept
		{
			return BTHEndpoint(Protocol, { BTHAddress }, Port);
		}

		bool operator==(const SerializedBTHEndpoint& other) const noexcept
		{
			return (Protocol == other.Protocol && BTHAddress == other.BTHAddress && Port == other.Port);
		}

		bool operator!=(const SerializedBTHEndpoint& other) const noexcept
		{
			return !(*this == other);
		}
	};
#pragma pack(pop)
}