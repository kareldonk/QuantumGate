// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include <atomic>

#include "QuantumGate.h"
#include "Concurrency\Event.h"
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
		Socks4ReplyRelay,
		Socks5ReplyRelay,
		DataRelay,
		Disconnect,
		DisconnectAck
	};

	struct Listener final
	{
		Concurrency::Event ShutdownEvent;
		std::thread Thread;
		Network::Socket Socket;
		UInt16 TCPPort{ 9090 };
		std::shared_mutex Mutex;
	};

	using Connection_ThS = Concurrency::ThreadSafe<Connection, std::shared_mutex>;
	using Connections = std::unordered_map<Connection::ID, std::shared_ptr<Connection_ThS>>;
	using Connections_ThS = Concurrency::ThreadSafe<Connections, std::shared_mutex>;

	using PollFD_ThS = Concurrency::ThreadSafe<std::vector<WSAPOLLFD>, std::shared_mutex>;

	using DNSCache = std::unordered_map<String, IPAddress>;
	using DNSCache_ThS = Concurrency::ThreadSafe<DNSCache, std::shared_mutex>;

	struct Peer final
	{
		const PeerLUID ID{ 0 };
		Connections Connections;

		const Size MaxDataRelayDataSize{ 0 };
		static constexpr Size MinSndRcvSize{ 1u << 10 };
		Size MaxSndRcvSize{ 0 };
		Size ActSndRcvSize{ 0 };

		Peer(const PeerLUID pluid, const Size max_datarelay_size) noexcept :
			ID(pluid), MaxDataRelayDataSize(max_datarelay_size)
		{
			CalcMaxSndRcvSize();
		}

		void CalcMaxSndRcvSize() noexcept
		{
			const auto num_conn = Connections.size();
			if (num_conn > 0)
			{
				const auto max_size = (std::max)(static_cast<double>(MaxDataRelayDataSize) / static_cast<double>(num_conn),
												 static_cast<double>(MinSndRcvSize));
				MaxSndRcvSize = static_cast<Size>(max_size);
			}
			else
			{
				MaxSndRcvSize = MaxDataRelayDataSize;
			}
		}
	};

	using Peer_ThS = Concurrency::ThreadSafe<Peer, std::shared_mutex>;
	using Peers = std::unordered_map<PeerLUID, std::shared_ptr<Peer_ThS>>;
	using Peers_ThS = Concurrency::ThreadSafe<Peers, std::shared_mutex>;

	class Extender final : public QuantumGate::Extender
	{
		friend Connection;

		using ThreadPool = Concurrency::ThreadPool<>;

	public:
		Extender() noexcept;
		virtual ~Extender();

		inline void SetUseCompression(const bool compression) noexcept { m_UseCompression = compression; }
		[[nodiscard]] inline bool IsUsingCompression() const noexcept { return m_UseCompression; }

		[[nodiscard]] inline bool IsAuthenticationRequired() const noexcept { return m_RequireAuthentication; }

		[[nodiscard]] bool IsAcceptingIncomingConnections() const noexcept { return m_UseListener; }
		void SetAcceptIncomingConnections(const bool accept);

		[[nodiscard]] bool SetCredentials(const ProtectedStringA& username, const ProtectedStringA& password);
		[[nodiscard]] bool CheckCredentials(const BufferView& username, const BufferView& password) const;

		void SetTCPListenerPort(const UInt16 port) noexcept;
		inline UInt16 GetTCPListenerPort() const noexcept { return m_Listener.TCPPort; }

		[[nodiscard]] bool IsOutgoingIPAllowed(const IPAddress& ip) const noexcept;

	private:
		[[nodiscard]] bool OnStartup();
		void OnPostStartup();
		void OnPreShutdown();
		void OnShutdown();
		void OnPeerEvent(PeerEvent&& event);
		QuantumGate::Extender::PeerEvent::Result OnPeerMessage(PeerEvent&& event);

		[[nodiscard]] bool InitializeIPFilters();
		void DeInitializeIPFilters() noexcept;
		[[nodiscard]] bool StartupListener();
		void ShutdownListener();
		[[nodiscard]] bool StartupThreadPool();
		void ShutdownThreadPool() noexcept;

		static void ListenerThreadLoop(Extender* extender);
		bool MainWorkerThreadWaitProcessor(std::chrono::milliseconds max_wait, const Concurrency::Event& shutdown_event);
		ThreadPool::ThreadCallbackResult MainWorkerThreadLoop(const Concurrency::Event& shutdown_event);
		bool DataRelayWorkerThreadWaitProcessor(std::chrono::milliseconds max_wait, const Concurrency::Event& shutdown_event);
		ThreadPool::ThreadCallbackResult DataRelayWorkerThreadLoop(const Concurrency::Event& shutdown_event);

		[[nodiscard]] std::optional<IPAddress> ResolveDomainIP(const String& domain) noexcept;

		[[nodiscard]] Socks4Protocol::Replies TranslateWSAErrorToSocks4(Int errorcode) const noexcept;
		[[nodiscard]] Socks5Protocol::Replies TranslateWSAErrorToSocks5(Int errorcode) const noexcept;

		[[nodiscard]] bool AddPeer(const PeerLUID pluid) noexcept;
		void RemovePeer(const PeerLUID pluid) noexcept;
		[[nodiscard]] std::shared_ptr<Peer_ThS> GetPeer(const PeerLUID pluid) const noexcept;

		[[nodiscard]] bool AddConnection(const PeerLUID pluid, const Connection::ID cid,
										 std::shared_ptr<Connection_ThS>&& c) noexcept;
		void RemoveConnection(const Connection::Key key) noexcept;
		void RemoveConnections(const std::vector<Connection::Key>& conn_list) noexcept;
		[[nodiscard]] std::shared_ptr<Connection_ThS> GetConnection(const PeerLUID pluid, const Connection::ID cid) const noexcept;

		[[nodiscard]] bool AddConnectionFD(const SOCKET s) noexcept;
		void RemoveConnectionFD(const SOCKET s) noexcept;

		void Disconnect(Connection_ThS& c);
		void Disconnect(Connection& c);
		void DisconnectFor(const PeerLUID pluid);
		void DisconnectAll();

		void AcceptIncomingConnection();
		[[nodiscard]] std::optional<PeerLUID> GetPeerForConnection() const;

		[[nodiscard]] bool MakeOutgoingConnection(const PeerLUID pluid, const Connection::ID cid,
												  const SocksProtocolVersion socks_version,
												  const IPAddress& ip, const UInt16 port);

		[[nodiscard]] bool SendConnectDomain(const PeerLUID pluid, const Connection::ID cid,
											 const SocksProtocolVersion socks_version,
											 const String& domain, const UInt16 port) const noexcept;

		[[nodiscard]] bool SendConnectIP(const PeerLUID pluid, const Connection::ID cid,
										 const SocksProtocolVersion socks_version,
										 const Network::BinaryIPAddress& ip, const UInt16 port) const noexcept;

		[[nodiscard]] bool SendDisconnect(const PeerLUID pluid, const Connection::ID cid) const noexcept;
		[[nodiscard]] bool SendDisconnectAck(const PeerLUID pluid, const Connection::ID cid) const noexcept;

		[[nodiscard]] bool SendSocks4Reply(const PeerLUID pluid, const Connection::ID cid, const Socks4Protocol::Replies reply,
										   const Network::BinaryIPAddress ip = Network::BinaryIPAddress{},
										   const UInt16 port = 0) const noexcept;

		[[nodiscard]] bool SendSocks5Reply(const PeerLUID pluid, const Connection::ID cid, const Socks5Protocol::Replies reply,
										   const Socks5Protocol::AddressTypes atype = Socks5Protocol::AddressTypes::IPv4,
										   const Network::BinaryIPAddress ip = Network::BinaryIPAddress{},
										   const UInt16 port = 0) const noexcept;

		[[nodiscard]] Result<> SendDataRelay(const PeerLUID pluid, const Connection::ID cid,
											 const BufferView& buffer) const noexcept;

		[[nodiscard]] bool Send(const PeerLUID pluid, Buffer&& buffer) const noexcept;

		void SetConnectionSendEvent() noexcept { m_AllConnectionsSendEvent.Set(); }
		void SetConnectionReceiveEvent() noexcept { m_AllConnectionsReceiveEvent.Set(); }

		[[nodiscard]] Size GetDataRelayHeaderSize() const noexcept
		{
			return sizeof(MessageType) +
				sizeof(Connection::ID) +
				9; // 9 bytes for encoded size of buffer
		}

		[[nodiscard]] Size GetMaxDataRelayDataSize() const noexcept
		{
			const Size size{ (1u << 15) - GetDataRelayHeaderSize() };
			assert(size <= GetMaximumMessageDataSize());
			return size;
		}

		[[nodiscard]] bool HandleConnectDomainPeerMessage(const PeerLUID pluid, const Connection::ID cid,
														  const SocksProtocolVersion socks_version,
														  const String& domain, const UInt16 port);

		[[nodiscard]] bool HandleConnectIPPeerMessage(const PeerLUID pluid, const Connection::ID cid,
													  const SocksProtocolVersion socks_version,
													  const Network::BinaryIPAddress& ip, const UInt16 port);

		[[nodiscard]] bool HandleSocks4ReplyRelayPeerMessage(const PeerLUID pluid, const Connection::ID cid,
															 const Socks4Protocol::Replies reply,
															 const BufferView& address, const UInt16 port);

		[[nodiscard]] bool HandleSocks5ReplyRelayPeerMessage(const PeerLUID pluid, const Connection::ID cid,
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
		Connections_ThS m_AllConnections;
		PollFD_ThS m_AllConnectionFDs;
		Concurrency::Event m_AllConnectionsSendEvent;
		Concurrency::Event m_AllConnectionsReceiveEvent;
		DNSCache_ThS m_DNSCache;

		std::atomic_bool m_UseCompression{ true };

		bool m_RequireAuthentication{ false };
		ProtectedBuffer m_Username;
		ProtectedBuffer m_Password;

		Core::Access::IPFilters_ThS m_IPFilters;
	};
}