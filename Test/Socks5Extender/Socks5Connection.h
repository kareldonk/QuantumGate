// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Socks5Protocol.h"
#include "Socks5Socket.h"

namespace QuantumGate::Socks5Extender
{
	class Extender;

	using ConnectionID = UInt64;

	class Connection final
	{
	public:
		enum class Type { Unknown, Incoming, Outgoing };
		enum class Status { Unknown, Handshake, Authenticating, Connecting, Connected, Ready, Disconnecting, Disconnected };

		Connection(Extender& extender, const PeerLUID pluid, Socket&& socket) noexcept;

		Connection(Extender& extender, const PeerLUID pluid,
				   const ConnectionID cid, Socket&& socket) noexcept;

		inline const UInt64 GetKey() const noexcept { return m_Key; }
		static const UInt64 MakeKey(const PeerLUID pluid, const ConnectionID cid) noexcept;

		inline const ConnectionID GetID() const noexcept { return m_ID; }
		inline const PeerLUID GetPeerLUID() const noexcept { return m_PeerLUID; }
		inline const Status GetStatus() const noexcept { return m_Status; }
		inline const Type GetType() const noexcept { return m_Type; }
		inline const Socket& GetSocket() const noexcept { return m_Socket; }
		inline const SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }

		inline const bool IsPeerConnected() const noexcept { return m_PeerConnected; }
		inline void SetPeerConnected(const bool connected) noexcept { m_PeerConnected = connected; }

		inline const void SetDisconnectCondition() noexcept { m_DisconnectCondition = true; }
		inline const bool ShouldDisconnect() const noexcept { return m_DisconnectCondition; }
		void Disconnect();

		inline const bool IsInHandshake() const noexcept { return (GetStatus() <= Status::Connected && GetType() == Type::Incoming); }
		inline const bool IsActive() const noexcept { return (GetStatus() <= Status::Ready); }
		inline const bool IsReady() const noexcept { return (GetStatus() == Status::Ready); }
		inline const bool IsDisconnecting() const noexcept { return (GetStatus() == Status::Disconnecting); }
		inline const bool IsDisconnected() const noexcept { return (GetStatus() == Status::Disconnected); }

		const bool IsTimedOut() const noexcept;

		void SetStatus(Status status) noexcept;

		const bool ProcessEvents(bool& didwork);

		const bool SendSocks5Reply(const Socks5Protocol::Replies reply);
		const bool SendSocks5Reply(const Socks5Protocol::Replies reply,
								   const Socks5Protocol::AddressTypes atype,
								   const BufferView& address, const UInt16 port);

		const bool SendRelayedData(const Buffer&& buffer);

	protected:
		const bool SendAndReceive(bool& didwork);

		const bool HandleReceivedSocks5Messages();
		const bool ProcessSocks5MethodIdentificationMessage();
		const bool ProcessSocks5AuthenticationMessages();
		const bool ProcessSocks5ConnectMessages();
		const bool ProcessSocks5DomainConnectMessage(BufferView buffer);
		const bool ProcessSocks5IPv4ConnectMessage(BufferView buffer);
		const bool ProcessSocks5IPv6ConnectMessage(BufferView buffer);

		const bool RelayReceivedData();

	private:
		ConnectionID m_ID{ 0 };
		PeerLUID m_PeerLUID{ 0 };
		UInt64 m_Key{ 0 };
		Type m_Type{ Type::Unknown };
		Status m_Status{ Status::Unknown };
		bool m_PeerConnected{ false };

		Socket m_Socket;
		Buffer m_ReceiveBuffer;
		Buffer m_SendBuffer;

		bool m_DisconnectCondition{ false };
		SteadyTime m_LastActiveSteadyTime;

		Extender& m_Extender;
	};
}