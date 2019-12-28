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
			case GateType::Socket:
			{
				static_assert(sizeof(Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) Socket();
				m_Type = GateType::Socket;
				SetCallbacks();
				break;
			}
			case GateType::RelaySocket:
			{
				static_assert(sizeof(Relay::Socket) <= sizeof(m_SocketStorage),
							  "Type is too large for SocketStorage variable; increase size.");

				m_Socket = new (&m_SocketStorage) Relay::Socket();
				m_Type = GateType::RelaySocket;
				SetCallbacks();
				break;
			}
			default:
			{
				assert(false);
				break;
			}
		}
	}

	Gate::Gate(const IP::AddressFamily af, const Socket::Type type, const IP::Protocol protocol)
	{
		static_assert(sizeof(Socket) <= sizeof(m_SocketStorage),
					  "Type is too large for SocketStorage variable; increase size.");

		m_Socket = new (&m_SocketStorage) Socket(af, type, protocol);
		m_Type = GateType::Socket;
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