// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "Socks5Protocol.h"
#include "Socks5Socket.h"

namespace QuantumGate::Socks5Extender
{
	class Extender;

	class Connection final
	{
	public:
		enum class Type { Unknown, Incoming, Outgoing };
		enum class Status { Unknown, Handshake, Authenticating, Connecting, Connected, Ready, Disconnecting, Disconnected };

		using ID = UInt64;
		using Key = UInt64;

		Connection(Extender& extender, const PeerLUID pluid, Socket&& socket) noexcept;
		Connection(Extender& extender, const PeerLUID pluid, const ID cid, Socket&& socket) noexcept;

		[[nodiscard]] inline Key GetKey() const noexcept { return m_Key; }
		[[nodiscard]] static Key MakeKey(const PeerLUID pluid, const ID cid) noexcept;

		[[nodiscard]] inline const ID GetID() const noexcept { return m_ID; }
		[[nodiscard]] inline const PeerLUID GetPeerLUID() const noexcept { return m_PeerLUID; }
		[[nodiscard]] inline const Status GetStatus() const noexcept { return m_Status; }
		[[nodiscard]] inline const Type GetType() const noexcept { return m_Type; }
		[[nodiscard]] inline const Socket& GetSocket() const noexcept { return m_Socket; }
		[[nodiscard]] inline const SteadyTime GetLastActiveSteadyTime() const noexcept { return m_LastActiveSteadyTime; }

		[[nodiscard]] inline bool IsPeerConnected() const noexcept { return m_PeerConnected; }
		inline void SetPeerConnected(const bool connected) noexcept { m_PeerConnected = connected; }

		inline void SetDisconnectCondition() noexcept { m_DisconnectCondition = true; }
		[[nodiscard]] inline bool ShouldDisconnect() const noexcept { return m_DisconnectCondition; }
		void Disconnect();

		[[nodiscard]] inline bool IsInHandshake() const noexcept { return (GetStatus() <= Status::Connected && GetType() == Type::Incoming); }
		[[nodiscard]] inline bool IsActive() const noexcept { return (GetStatus() <= Status::Ready); }
		[[nodiscard]] inline bool IsReady() const noexcept { return (GetStatus() == Status::Ready); }
		[[nodiscard]] inline bool IsDisconnecting() const noexcept { return (GetStatus() == Status::Disconnecting); }
		[[nodiscard]] inline bool IsDisconnected() const noexcept { return (GetStatus() == Status::Disconnected); }

		[[nodiscard]] bool IsTimedOut() const noexcept;

		void SetStatus(Status status) noexcept;

		void ProcessEvents(bool& didwork);
		void ProcessRelayEvents(bool& didwork, const Size max_send, Size& sent);

		[[nodiscard]] bool SendSocks5Reply(const Socks5Protocol::Replies reply);
		[[nodiscard]] bool SendSocks5Reply(const Socks5Protocol::Replies reply,
										   const Socks5Protocol::AddressTypes atype,
										   const BufferView& address, const UInt16 port);

		[[nodiscard]] bool SendRelayedData(const Buffer&& buffer);

	protected:
		[[nodiscard]] bool SendAndReceive(bool& didwork);
		void FlushBuffers();

		[[nodiscard]] bool HandleReceivedSocks5Messages();
		[[nodiscard]] bool ProcessSocks5MethodIdentificationMessage();
		[[nodiscard]] bool ProcessSocks5AuthenticationMessages();
		[[nodiscard]] bool ProcessSocks5ConnectMessages();
		[[nodiscard]] bool ProcessSocks5DomainConnectMessage(BufferView buffer);
		[[nodiscard]] bool ProcessSocks5IPv4ConnectMessage(BufferView buffer);
		[[nodiscard]] bool ProcessSocks5IPv6ConnectMessage(BufferView buffer);

		[[nodiscard]] bool RelayReceivedData(const Size max_send, Size& sent);

	private:
		static constexpr Size MaxReceiveBufferSize{ 1u << 16 };

	private:
		ID m_ID{ 0 };
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