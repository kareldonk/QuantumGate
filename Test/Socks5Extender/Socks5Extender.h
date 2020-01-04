// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "QuantumGate.h"
#include "Concurrency\EventCondition.h"
#include "Concurrency\ThreadSafe.h"
#include "Concurrency\ThreadPool.h"
#include "Core\Access\IPFilters.h"
#include "Socks5Connection.h"

namespace QuantumGate::Socks5Extender
{
	using namespace QuantumGate::Implementation;

	enum class MessageType : UInt16
	{
		Unknown = 0,
		ConnectDomain,
		ConnectIP,
		Socks5ReplyRelay,
		DataRelay,
		Disconnect,
		DisconnectAck
	};

	struct Listener final
	{
		Concurrency::EventCondition ShutdownEvent{ false };
		std::thread Thread;
		Network::Socket Socket;
		std::shared_mutex Mutex;
	};

	using Connection_ThS = Concurrency::ThreadSafe<Connection, std::shared_mutex>;
	using Connections = std::unordered_map<ConnectionID, std::unique_ptr<Connection_ThS>>;
	using Connections_ThS = Concurrency::ThreadSafe<Connections, std::shared_mutex>;

	struct Peer final
	{
		PeerLUID ID{ 0 };
	};

	using Peers = std::unordered_map<PeerLUID, Peer>;
	using Peers_ThS = Concurrency::ThreadSafe<Peers, std::shared_mutex>;

	class Extender final : public QuantumGate::Extender
	{
		friend Connection;

		using ThreadPool = Concurrency::ThreadPool<>;

	public:
		Extender() noexcept;
		virtual ~Extender();

		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		inline bool IsUsingCompression() const noexcept { return m_UseCompression; }

		inline bool IsAuthenticationRequired() const noexcept { return m_RequireAuthentication; }

		bool IsAcceptingIncomingConnections() const noexcept { return m_UseListener; }
		void SetAcceptIncomingConnections(const bool accept);

		bool SetCredentials(const ProtectedStringA& username, const ProtectedStringA& password);
		bool CheckCredentials(const BufferView& username, const BufferView& password) const;

		bool IsOutgoingIPAllowed(const IPAddress& ip) const noexcept;

	private:
		bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		QuantumGate::Extender::PeerEvent::Result OnPeerMessage(PeerEvent&& event);

		bool InitializeIPFilters();
		void DeInitializeIPFilters() noexcept;
		bool StartupListener();
		void ShutdownListener();
		bool StartupThreadPool();
		void ShutdownThreadPool() noexcept;

		static void ListenerThreadLoop(Extender* extender);
		ThreadPool::ThreadCallbackResult MainWorkerThreadLoop(const Concurrency::EventCondition& shutdown_event);

		std::optional<IPAddress> ResolveDomainIP(const String& domain) const noexcept;
		const Socks5Protocol::Replies TranslateWSAErrorToSocks5(Int errorcode) const noexcept;

		bool AddConnection(const PeerLUID pluid, const ConnectionID cid, std::unique_ptr<Connection_ThS>&& c) noexcept;
		Connection_ThS* GetConnection(const PeerLUID pluid, const ConnectionID cid) const noexcept;
		void Disconnect(Connection_ThS& c);
		void Disconnect(Connection& c);
		void DisconnectFor(const PeerLUID pluid);
		void DisconnectAll();

		void AcceptIncomingConnection();
		std::optional<PeerLUID> GetPeerForConnection() const;

		bool MakeOutgoingConnection(const PeerLUID pluid, const ConnectionID cid,
									const IPAddress& ip, const UInt16 port);

		bool SendConnectDomain(const PeerLUID pluid, const ConnectionID cid,
							   const String& domain, const UInt16 port) const noexcept;

		bool SendConnectIP(const PeerLUID pluid, const ConnectionID cid,
						   const Network::BinaryIPAddress& ip, const UInt16 port) const noexcept;

		bool SendDisconnect(const PeerLUID pluid, const ConnectionID cid) const noexcept;
		bool SendDisconnectAck(const PeerLUID pluid, const ConnectionID cid) const noexcept;

		bool SendSocks5Reply(const PeerLUID pluid, const ConnectionID cid, const Socks5Protocol::Replies reply,
							 const Socks5Protocol::AddressTypes atype = Socks5Protocol::AddressTypes::IPv4,
							 const Network::BinaryIPAddress ip = Network::BinaryIPAddress{},
							 const UInt16 port = 0) const noexcept;

		Result<> SendDataRelay(const PeerLUID pluid, const ConnectionID cid, const BufferView& buffer) const noexcept;

		bool SendMessage(const PeerLUID pluid, Buffer&& buffer) const noexcept;

		constexpr Size GetDataRelayHeaderSize() const noexcept
		{
			return sizeof(MessageType) +
				sizeof(ConnectionID) +
				9; // 9 bytes for encoded size of buffer
		}

		Size GetMaxDataRelayDataSize() const noexcept
		{
			return GetMaximumMessageDataSize() - GetDataRelayHeaderSize();
		}

		bool HandleConnectDomainPeerMessage(const PeerLUID pluid, const ConnectionID cid,
											const String& domain, const UInt16 port);

		bool HandleConnectIPPeerMessage(const PeerLUID pluid, const ConnectionID cid,
										const Network::BinaryIPAddress& ip, const UInt16 port);

		bool HandleSocks5ReplyRelayPeerMessage(const PeerLUID pluid, const ConnectionID cid,
											   const Socks5Protocol::Replies reply,
											   const Socks5Protocol::AddressTypes atype,
											   const BufferView& address, const UInt16 port);

	public:
		inline static ExtenderUUID UUID{ L"20a86749-7e9e-297d-1e1c-3a7ddc723f66" };

	private:
		bool m_UseListener{ false };
		Listener m_Listener;

		ThreadPool m_ThreadPool;
		Peers_ThS m_Peers;
		Connections_ThS m_Connections;

		std::atomic_bool m_UseCompression{ true };

		bool m_RequireAuthentication{ false };
		ProtectedBuffer m_Username;
		ProtectedBuffer m_Password;

		Core::Access::IPFilters_ThS m_IPFilters;
	};
}