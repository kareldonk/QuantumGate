// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "pch.h"
#include "PeerGate.h"

namespace QuantumGate::Implementation::Core::Peer
{
	Gate::Gate(const GateType type) noexcept
	{
		switch (type)
		{
			case GateType::TCPSocket:
			{
				static_assert(sizeof(TCP::Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) TCP::Socket();
				m_Type = GateType::TCPSocket;
				break;
			}
			case GateType::UDPSocket:
			{
				static_assert(sizeof(UDP::Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) UDP::Socket();
				m_Type = GateType::UDPSocket;
				break;
			}
			case GateType::BTHSocket:
			{
				static_assert(sizeof(BTH::Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) BTH::Socket();
				m_Type = GateType::BTHSocket;
				break;
			}
			case GateType::RelaySocket:
			{
				static_assert(sizeof(Relay::Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) Relay::Socket();
				m_Type = GateType::RelaySocket;
				break;
			}
			default:
			{
				// Shouldn't get here
				assert(false);
				break;
			}
		}

		SetCallbacks();
	}

	Gate::Gate(const AddressFamily af, const Protocol protocol)
	{
		static_assert(sizeof(TCP::Socket) <= sizeof(m_SocketStorage),
					  "Type is too large for SocketStorage variable; increase size.");

		static_assert(sizeof(UDP::Socket) <= sizeof(m_SocketStorage),
					  "Type is too large for SocketStorage variable; increase size.");

		static_assert(sizeof(BTH::Socket) <= sizeof(m_SocketStorage),
					  "Type is too large for SocketStorage variable; increase size.");

		switch (protocol)
		{
			case Protocol::TCP:
				m_Socket = new (&m_SocketStorage) TCP::Socket(af);
				m_Type = GateType::TCPSocket;
				break;
			case Protocol::UDP:
				m_Socket = new (&m_SocketStorage) UDP::Socket();
				m_Type = GateType::UDPSocket;
				break;
			case Protocol::RFCOMM:
				m_Socket = new (&m_SocketStorage) BTH::Socket(af);
				m_Type = GateType::BTHSocket;
				break;
			default:
				// Shouldn't get here
				assert(false);
				break;
		}

		SetCallbacks();
	}

	Gate::~Gate()
	{
		if (m_Socket)
		{
			m_Socket->~SocketBase();
			m_Socket = nullptr;
		}
	}

	void Gate::SetCallbacks() noexcept
	{
		assert(m_Socket != nullptr);

		m_Socket->SetConnectingCallback(MakeCallback(this, &Gate::OnConnecting));
		m_Socket->SetAcceptCallback(MakeCallback(this, &Gate::OnAccept));
		m_Socket->SetConnectCallback(MakeCallback(this, &Gate::OnConnect));
		m_Socket->SetCloseCallback(MakeCallback(this, &Gate::OnClose));
	}
}